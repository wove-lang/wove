# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Lower the first Wove ownership forms to C11."""

from __future__ import annotations

from collections.abc import Iterator
from contextlib import contextmanager
from dataclasses import dataclass, field
from itertools import repeat
import os
from pathlib import Path
import re

from bridge_types import (
	C_READONLY_BYTES, C_WRITABLE_BYTES, NativeType, c_bridges, declarations, native_pragmas, read_native_function, read_native_type,
)
from syntax import IDENTIFIER, WORD, Atom, Form, Node, SourceError, form, identifier, parse
from target import compiler_target


I8 = "i8"
I32 = "i32"
I64 = "i64"
U8 = "u8"
U16 = "u16"
U32 = "u32"
U64 = "u64"
BOOL = "bool"
VOID = "void"
RUNTIME_INCLUDE = '"wove_runtime.h"'
HEADER_PATH = re.compile(r"[A-Za-z0-9_./-]+")
INTEGER_LITERAL = re.compile(r"-?(?:0[xX][0-9a-fA-F]+|[0-9]+)")
INTEGER_SUFFIX = re.compile(rf"({INTEGER_LITERAL.pattern})([iu][0-9]+)")
UNSIGNED_LITERAL_MACROS = {U32: "UINT32_C", U64: "UINT64_C"}
SCALAR_LIMITS = {
	I8: (-128, 127),
	I32: (-(2**31), 2**31 - 1),
	I64: (-(2**63), 2**63 - 1),
	U8: (0, 255),
	U16: (0, 2**16 - 1),
	U32: (0, 2**32 - 1),
	U64: (0, 2**64 - 1),
}
SCALAR_TYPES = (*SCALAR_LIMITS, BOOL)
OWNED = "owned-array"
SHARED = "shared-array"
MUT = "mut-array"
C_TYPE = {
	I8: "int8_t",
	I32: "int32_t",
	I64: "int64_t",
	U8: "uint8_t",
	U16: "uint16_t",
	U32: "uint32_t",
	U64: "uint64_t",
	BOOL: "bool",
	VOID: "void",
	OWNED: "wvrt_array *",
	SHARED: "wvrt_slice",
	MUT: "wvrt_mut_slice",
}
WRAPPING_OPS = {
	"i32.add": (I32, "wvrt_i32_add_wrapping"),
	"i32.sub": (I32, "wvrt_i32_sub_wrapping"),
	"u32.add": (U32, "wvrt_u32_add_wrapping"),
	"u32.sub": (U32, "wvrt_u32_sub_wrapping"),
}
STEP_OPS = {"inc": "add", "dec": "sub"}
BINARY_OPS = {
	"u8.eq": (U8, BOOL, "=="),
	"u8.not-eq": (U8, BOOL, "!="),
	"u8.lt": (U8, BOOL, "<"),
	"u8.not-lt": (U8, BOOL, ">="),
	"u16.eq": (U16, BOOL, "=="),
	"i32.eq": (I32, BOOL, "=="),
	"i32.not-eq": (I32, BOOL, "!="),
	"i32.lt": (I32, BOOL, "<"),
	"i32.not-lt": (I32, BOOL, ">="),
	"i32.le": (I32, BOOL, "<="),
	"i32.gt": (I32, BOOL, ">"),
	"i32.ge": (I32, BOOL, ">="),
	"i32.mul": (I32, I32, "*"),
	"i32.div": (I32, I32, "/"),
	"i32.rem": (I32, I32, "%"),
	"i64.eq": (I64, BOOL, "=="),
	"i64.lt": (I64, BOOL, "<"),
	"i64.gt": (I64, BOOL, ">"),
	"i64.add": (I64, I64, "+"),
	"i64.sub": (I64, I64, "-"),
	"i64.mul": (I64, I64, "*"),
	"u32.eq": (U32, BOOL, "=="),
	"u32.not-eq": (U32, BOOL, "!="),
	"u32.lt": (U32, BOOL, "<"),
	"u32.not-lt": (U32, BOOL, ">="),
	"u32.gt": (U32, BOOL, ">"),
	"u32.mul": (U32, U32, "*"),
	"u32.div": (U32, U32, "/"),
	"u32.rem": (U32, U32, "%"),
	"u32.and": (U32, U32, "&"),
	"u32.or": (U32, U32, "|"),
	"u32.xor": (U32, U32, "^"),
	"u64.eq": (U64, BOOL, "=="),
	"u64.not-eq": (U64, BOOL, "!="),
	"u64.lt": (U64, BOOL, "<"),
	"u64.not-lt": (U64, BOOL, ">="),
	"u64.gt": (U64, BOOL, ">"),
	"u64.add": (U64, U64, "+"),
	"u64.sub": (U64, U64, "-"),
	"u64.mul": (U64, U64, "*"),
	"u64.and": (U64, U64, "&"),
	"u64.or": (U64, U64, "|"),
	"u64.xor": (U64, U64, "^"),
}
ZERO_PREDICATES = {
	"eqz": ("eq", False),
	"not-eqz": ("not-eq", False),
	"ltz": ("lt", False),
	"not-ltz": ("not-lt", False),
	"gtz": ("lt", True),
	"not-gtz": ("not-lt", True),
	"lez": ("le", False),
}
SHIFT_OPS = {"shl": "<<", "shr": ">>"}
BYTE_DECODER_WIDTHS = {U16: 2, U32: 4}


@dataclass(frozen=True)
class RecordField:
	kind: str | NativeType | Record | BufferType | Form
	c_name: str
	line: int = 0


# Record equality follows the defining declaration, including across file scopes.
@dataclass(eq=False, frozen=True)
class Record:
	c_name: str
	fields: dict[str, RecordField]
	native: bool = False


NamedType = Record | NativeType


@dataclass(frozen=True)
class SharedRef:
	record: Record


@dataclass(frozen=True)
class NativeRef:
	owner: NativeType
	writable: bool = False


@dataclass(frozen=True)
class NativePointerOwner:
	owner: NativeType


@dataclass(frozen=True)
class NativeStorage:
	owner: NativeType


@dataclass(frozen=True)
class NativePlaceRef:
	kind: str | NamedType | NativeStorage
	writable: bool


@dataclass(frozen=True)
class BufferType:
	access: str
	element: str | NamedType | BufferType


@dataclass(frozen=True)
class OptionalOwner:
	payload: BufferType


@dataclass(frozen=True)
class FixedArrayType:
	element: str
	count: int


Type = (str | NamedType | SharedRef | NativeRef | NativePointerOwner | NativeStorage | NativePlaceRef |
	BufferType | OptionalOwner | FixedArrayType)


_BYTE_VIEW = BufferType(SHARED, U8)
_MUT_BYTE_VIEW = BufferType(MUT, U8)
_OWNED_BYTE_ROW = BufferType(OWNED, U8)
_C_STRING_ROWS = BufferType(SHARED, _OWNED_BYTE_ROW)
_OWNED_STRING_ROWS = BufferType(OWNED, _OWNED_BYTE_ROW)
_HOST_ARGV = BufferType(SHARED, _BYTE_VIEW)


@dataclass(frozen=True)
class Value:
	kind: Type
	c: str


@dataclass(eq=False)
class Binding:
	value: Value
	writable: bool = False
	native: bool = False
	snapshot_vector: bool = False


@dataclass
class LoopScope:
	depth: int
	incoming: set[Binding]
	break_moved: set[Binding] = field(default_factory=set)
	found_break: bool = False


@dataclass(frozen=True)
class ModuleValue:
	binding: Binding
	initializer: str | bytes


@dataclass(frozen=True)
class Function:
	c_name: str
	params: tuple[tuple[str, Type], ...]
	result: Type
	body: Form
	is_entry: bool = False


@dataclass(frozen=True)
class NativeStringResult:
	release: str | None = None
	field: str | None = None
	extent: str | None = None


@dataclass(frozen=True)
class NativeFunction:
	c_name: str
	params: tuple[tuple[str, Type], ...]
	result: Type
	byte_extents: tuple[tuple[int, int | None, int], ...]
	c_strings: tuple[int, ...]
	null_only: frozenset[int]
	nullable: frozenset[int]
	initializers: frozenset[int]
	snapshot: NativeStringResult | None
	macro: bool
	variadic_start: int | None
	constants: dict[int, str]
	array_extents: tuple[tuple[int, int | str], ...]
	vectors: tuple[int, ...]


@dataclass(frozen=True)
class Module:
	filename: str
	function_index: int
	records: dict[str, Record] = field(default_factory=dict)
	function_declarations: dict[str, Form] = field(default_factory=dict)
	uses: list[Form] = field(default_factory=list)
	type_bindings: dict[str, NamedType] = field(default_factory=dict)
	functions: dict[str, Function] = field(default_factory=dict)
	bindings: dict[str, Function | NativeFunction] = field(default_factory=dict)
	values: dict[str, ModuleValue] = field(default_factory=dict)
	value_bindings: dict[str, Binding] = field(default_factory=dict)
	syntax: Form | None = None


# Identify values whose storage must transfer or be released.
def owns(kind: Type) -> bool:
	# By-value records release their owned fields recursively.
	if isinstance(kind, Record):
		return any(owns(field.kind) for field in kind.fields.values())
	return isinstance(kind, (OptionalOwner, NativePointerOwner, NativeStorage)) or (isinstance(kind, BufferType) and kind.access == OWNED)


# Scalars and records without owners can be read by value.
def copyable(kind: Type) -> bool:
	return (isinstance(kind, (Record, FixedArrayType, NativeType)) or kind in SCALAR_TYPES) and not owns(kind)


# Arrays and slices share descriptor layouts while values retain their native types.
def c_type(kind: Type) -> str:
	# The descriptor depends on the access mode and carries a typed element count.
	if isinstance(kind, BufferType):
		return C_TYPE[kind.access]
	# A shared record reference carries the address of the original value.
	if isinstance(kind, SharedRef):
		return f"const {c_type(kind.record)} *"
	# Owned and borrowed handles share the exact pointer representation from their definition.
	if isinstance(kind, (NativePointerOwner, NativeRef)):
		return kind.owner.c_name
	# A C storage owner keeps the C object and its activation state together.
	if isinstance(kind, NativeStorage):
		return "wvrt_c_storage *"
	# A native object loan points at the exact caller-owned type.
	if isinstance(kind, NativePlaceRef):
		# Qualify the whole underlying object, including when its native type is a pointer typedef.
		if isinstance(kind.kind, NativeStorage):
			native = kind.kind.owner.c_name
			return f"{native} *" if kind.writable else f"const __typeof__({native}) *"
		qualifier = "" if kind.writable else "const "
		return f"{qualifier}{c_type(kind.kind)} *"
	# A nullable owner uses the same pointer ABI as its payload.
	if isinstance(kind, OptionalOwner):
		return c_type(kind.payload)
	# Inline arrays are passed by value through their generated C type.
	if isinstance(kind, FixedArrayType):
		return f"wv_seed_fixed_{kind.element}_{kind.count}"
	return kind.c_name if isinstance(kind, (Record, NativeType)) else C_TYPE[kind]


# Normalize a suffixed integer atom to the equivalent typed form.
def value_form(node: Node) -> Form:
	# Quoted strings and other atoms retain the ordinary form check.
	if isinstance(node, Atom) and not node.quoted:
		literal = INTEGER_SUFFIX.fullmatch(node.text)
		# Reuse the typed form's range check and C emission.
		if literal is not None and literal[2] in SCALAR_LIMITS:
			return Form(literal[2], (Atom(literal[1], node.line),), node.line)
	return checked_form(node)


# Validate a typed integer literal before emitting its C spelling.
def read_integer_literal(node: Form) -> Value:
	literal, = form(node, arity=1).operands
	# The grammar limits the radix, digits, and sign before Python parses the value.
	if not isinstance(literal, Atom) or literal.quoted or not INTEGER_LITERAL.fullmatch(literal.text):
		raise SourceError(node.line)
	low, high = SCALAR_LIMITS[node.tag]
	hexadecimal = literal.text.removeprefix("-").startswith(("0x", "0X"))
	# An unsigned hexadecimal literal cannot carry a minus, even for zero.
	if hexadecimal and low == 0 and literal.text.startswith("-"):
		raise SourceError(node.line)
	# Convert both radices to the same decimal C spelling.
	try:
		number = int(literal.text, 16 if hexadecimal else 10)
	except ValueError:
		raise SourceError(node.line) from None
	# Check the declared type before handing the literal to C.
	if not low <= number <= high:
		raise SourceError(node.line)
	# C parses a leading minus separately, so spell the lowest signed word as a macro.
	if node.tag == I64:
		return Value(I64, "INT64_MIN" if number == low else f"INT64_C({number})")
	macro = UNSIGNED_LITERAL_MACROS.get(node.tag)
	spelling = str(number) if macro is None else f"{macro}({number})"
	return Value(node.tag, spelling)


# Read an inline integer array shape with an exact nonnegative extent.
def read_fixed_array_type(node: Node) -> FixedArrayType:
	element_node, count_node = checked_form(node, "array", 2).operands
	element = identifier(element_node)
	# Only supported scalar elements can be copied by the seed's inline array representation.
	if element not in SCALAR_TYPES:
		raise SourceError(element_node.line)
	# The extent is a decimal atom in the fixed array type form.
	if not isinstance(count_node, Atom) or count_node.quoted or not re.fullmatch(r"[0-9]+", count_node.text):
		raise SourceError(count_node.line)
	digits = count_node.text.lstrip("0") or "0"
	# Keep the extent representable in the runtime's signed length field.
	if len(digits) > 10 or (count := int(digits)) > SCALAR_LIMITS[I32][1]:
		raise SourceError(count_node.line)
	return FixedArrayType(element, count)


# Accept the scalar literals available in static array initializers.
def read_scalar_literal(node: Form) -> Value:
	# Validate boolean literals independently of integer ranges.
	if node.tag == BOOL:
		literal, = form(node, arity=1).operands
		spelling = identifier(literal)
		# The boolean domain has two named values.
		if spelling not in ("true", "false"):
			raise SourceError(literal.line)
		return Value(BOOL, spelling)
	# Static data cannot evaluate arbitrary expressions.
	if node.tag not in SCALAR_LIMITS:
		raise SourceError(node.line)
	return read_integer_literal(node)


# Resolve a fixed initializer's shape and element operands before lowering it.
def fixed_array_initializer(node: Form) -> tuple[FixedArrayType, tuple[Node, ...]]:
	# The first operand supplies the element type and extent.
	if not node.operands:
		raise SourceError(node.line)
	kind = read_fixed_array_type(node.operands[0])
	elements = node.operands[1:]
	expected = kind.count if node.tag == "array.new" else 1
	# A repetition takes one value, whereas an explicit initializer lists every element.
	if len(elements) != expected:
		raise SourceError(node.line)
	return kind, elements


# Emit a portable C initializer for an inline array's backing field.
def fixed_array_braces(elements: Iterator[str]) -> str:
	return "{{" + (", ".join(elements) or "0") + "}}"


# Track both the spelling used by the source and the resolved file used for reading.
def track_file(imports: dict[str, None], candidate: Path, target: Path, absolute: bool) -> None:
	# Use the same checkout-relative spelling as the generated C by default.
	for dependency in (candidate, target):
		name = os.path.normpath(dependency) if absolute else os.path.relpath(dependency)
		imports[name] = None


# Load static bytes relative to the declaring Wove file.
def read_embed(node: Form, filename: str, imports: dict[str, None] | None) -> bytes:
	operand, = form(node, "embed", 1).operands
	# An embed takes a quoted path whose final component denotes a file.
	if not isinstance(operand, Atom) or not operand.quoted or operand.text.rsplit("/", 1)[-1] in ("", ".", ".."):
		raise SourceError(operand.line)
	candidate = Path(filename).resolve().parent / operand.text
	# Resolve aliases for dependency tracking and read binary data without decoding it.
	try:
		target = candidate.resolve(strict=True)
		data = target.read_bytes()
	except (OSError, RuntimeError, ValueError):
		raise SourceError(operand.line) from None
	# Shared views use signed i32 lengths.
	if len(data) > SCALAR_LIMITS[I32][1]:
		raise SourceError(operand.line)
	# Track the asset through the same dependency channel as source imports.
	if imports is not None:
		track_file(imports, candidate, target, Path(filename).is_absolute())
	return data


# Read an immutable scalar or array without evaluating runtime expressions.
def read_module_value(
	node: Form, index: int, filename: str, imports: dict[str, None] | None
) -> tuple[str, ModuleValue]:
	initializer, = form(node, "let", 1).operands
	name_node, = node.names
	name = identifier(name_node)
	c_name = f"wv_seed_binding_{index}"
	# A quoted string supplies the exact bytes of a shared u8 slice.
	if isinstance(initializer, Atom) and initializer.quoted:
		# Reject string contents that cannot be represented as UTF-8.
		try:
			data = initializer.text.encode("utf-8")
		except UnicodeEncodeError:
			raise SourceError(initializer.line) from None
		# Slice lengths fit the signed i32 domain.
		if len(data) > SCALAR_LIMITS[I32][1]:
			raise SourceError(initializer.line)
		return name, ModuleValue(Binding(Value(_BYTE_VIEW, c_name)), data)
	literal = value_form(initializer)
	# An embedded asset supplies the same representation as a static byte string.
	if literal.tag == "embed":
		data = read_embed(literal, filename, imports)
		return name, ModuleValue(Binding(Value(_BYTE_VIEW, c_name)), data)
	# Static inline arrays carry their own element storage.
	if literal.tag in ("array.new", "array.repeat"):
		kind, elements = fixed_array_initializer(literal)
		values = []
		# Check each literal against the element type before constructing the C initializer.
		for element in elements:
			value = read_scalar_literal(value_form(element))
			# Array initialization does not convert element values.
			if value.kind != kind.element:
				raise SourceError(element.line)
			values.append(value.c)
		items = iter(values) if literal.tag == "array.new" else repeat(values[0], kind.count)
		return name, ModuleValue(Binding(Value(kind, c_name)), fixed_array_braces(items))
	value = read_scalar_literal(literal)
	return name, ModuleValue(Binding(Value(value.kind, c_name)), value.c)


# Give static bytes a read-only backing array and a shared view.
def static_byte_view(name: str, data: bytes) -> list[str]:
	lines = []
	items = "NULL"
	# An empty view needs no backing storage.
	if data:
		items = f"{name}_bytes"
		bytes_c = ", ".join(str(byte) for byte in data)
		lines.append(f"static const uint8_t {items}[{len(data)}] = {{{bytes_c}}};")
	lines.append(f"static const wvrt_slice {name} = {{{items}, {len(data)}, sizeof(uint8_t)}};")
	return lines


# Declare each module binding using its immutable storage representation.
def module_value_definition(declaration: ModuleValue) -> list[str]:
	value = declaration.binding.value
	initializer = declaration.initializer
	# A scalar or fixed array is stored directly in its declared C type.
	if isinstance(initializer, str):
		return [f"static const {c_type(value.kind)} {value.c} = {initializer};"]
	return static_byte_view(value.c, initializer)


# Record the field type names until the defining module has bound its imports.
def read_record(node: Form, index: int) -> tuple[str, Record]:
	fields_node, = form(node, "struct", 1).operands
	name_node, = node.names
	name = identifier(name_node)
	# Keep named records distinct from builtin value and loan types.
	if name in C_TYPE:
		raise SourceError(name_node.line)
	fields = {}
	# Retain the declaration order and source line of each field.
	for field_index, field_node in enumerate(checked_form(fields_node, "fields").operands):
		field = checked_form(field_node, arity=1)
		kind_node, = field.operands
		# An owned array field retains its element type form until imports bind.
		kind = kind_node if isinstance(kind_node, Form) and kind_node.tag == OWNED else identifier(kind_node)
		# A field name is an ordinary identifier and selects exactly one member.
		if not IDENTIFIER.fullmatch(field.tag) or field.tag in fields:
			raise SourceError(field.line)
		fields[field.tag] = RecordField(kind, f"wv_field_{field_index}", kind_node.line)
	return name, Record(f"wv_seed_record_{index}", fields)


# Give each named record a distinct C type with its fields in declaration order.
def record_definition(record: Record) -> list[str]:
	fields = [
		f"    {c_type(field.kind)} {field.c_name};"
		for field in record.fields.values()
	]
	# C11 needs at least one member even when the Wove record has no fields.
	return ["typedef struct {", *(fields or ["    uint8_t wv_empty;"]), f"}} {record.c_name};", ""]


# Release record fields in reverse layout order and skip values without owners.
def drop_statements(kind: Type, expression: str) -> Iterator[str]:
	# Inline record members keep their original descriptors.
	if isinstance(kind, Record):
		# Each member recursively drops any owned values still in its slot.
		for member in reversed(kind.fields.values()):
			yield from drop_statements(member.kind, f"{expression}.{member.c_name}")
	# An absent or already moved pointer has no native destructor to call.
	elif isinstance(kind, NativePointerOwner):
		yield f"if ({expression} != NULL) {{"
		yield f"    (void){kind.owner.pointer_drop}({expression});"
		yield "}"
	# Destroy only an initialized native object, then release its backing storage.
	elif isinstance(kind, NativeStorage):
		native = kind.owner.c_name
		drop = kind.owner.storage_drop
		yield f"if (wvrt_c_storage_active({expression})) {{"
		yield f"    (void){drop}(({native} *)wvrt_c_storage_data({expression}));"
		yield "}"
		yield f"wvrt_c_storage_free({expression});"
	# Moved or absent array owners carry a null descriptor.
	elif owns(kind):
		yield f"wvrt_array_drop({expression});"


# Give an owning-record array one slot type and a destructor matching that type.
def record_slot_definition(record: Record) -> list[str]:
	name = record.c_name
	slot = f"{name}_slot"
	lines = [f"static inline void {name}_drop({name} *value) {{"]
	lines.extend(f"    {statement}" for statement in drop_statements(record, "(*value)"))
	lines.extend(["}", f"WVRT_DEFINE_OWNED_SLOT({name}, {slot}, {slot}_drop, {name}_drop)", ""])
	return lines


# Build a view from the named descriptor fields for calls and runtime macros.
def array_view(fields: str, access: str) -> str:
	return f"(({C_TYPE[access]}){{{fields}wvrt_items, {fields}wvrt_length, {fields}wvrt_element_size}})"


# Resolve the owned element layouts needed by the compiler's seed source.
def read_owned_element(node: Node, records: dict[str, NamedType]) -> str | Record | BufferType:
	# The runtime has a recursive owner for one layer of byte rows.
	if isinstance(node, Form) and node.tag == OWNED:
		child, = checked_form(node, OWNED, 1).operands
		# The row runtime stores pointers to byte array owners.
		if identifier(child) == U8:
			return _OWNED_BYTE_ROW
		raise SourceError(child.line)
	name = identifier(node)
	element = records.get(name, name)
	# Scalars and declared records have native storage layouts.
	if element in (U8, I32) or isinstance(element, Record):
		return element
	raise SourceError(node.line)


# Preserve the declared element type when an owned array may be absent.
def read_optional_owner(payload: Node, records: dict[str, NamedType]) -> OptionalOwner:
	element, = checked_form(payload, OWNED, 1).operands
	return OptionalOwner(BufferType(OWNED, read_owned_element(element, records)))


# Read a value input or a shared or mutable view of a supported element.
def parameter_type(node: Node, records: dict[str, NamedType]) -> Type:
	# Copyable types are written as bare symbols.
	if isinstance(node, Atom) and not node.quoted:
		kind = records.get(node.text, node.text)
		# Records keep their identity and transfer if they carry ownership.
		if isinstance(kind, Record) or copyable(kind):
			return kind
	# An exact inline array shape gives the parameter its own value storage.
	if isinstance(node, Form) and node.tag == "array":
		return read_fixed_array_type(node)
	view = checked_form(node, arity=1)
	element_node, = view.operands
	# A shared record parameter borrows the original Wove record.
	if view.tag == "shared-ref":
		record = records.get(identifier(element_node))
		# This reference uses a Wove value layout rather than a bridge ABI.
		if not isinstance(record, Record) or record.native:
			raise SourceError(element_node.line)
		return SharedRef(record)
	# Slice parameters specify a read or write loan.
	if view.tag not in (SHARED, MUT):
		raise SourceError(node.line)
	# Permit exactly one layer of shared byte rows through the host entry and its helpers.
	if view.tag == SHARED and isinstance(element_node, Form) and element_node.tag == SHARED:
		row, = checked_form(element_node, SHARED, 1).operands
		# Keep the nested form limited to shared byte descriptors.
		if identifier(row) == U8:
			return _HOST_ARGV
		raise SourceError(element_node.line)
	# The row owner uses a slice with the same pointer slots as its backing array.
	if isinstance(element_node, Form) and element_node.tag == OWNED:
		return BufferType(view.tag, read_owned_element(element_node, records))
	element = identifier(element_node)
	# Keep the declared element type as part of the slice type.
	if element not in SCALAR_TYPES and element not in records:
		raise SourceError(node.line)
	return BufferType(view.tag, records.get(element, element))


# Read the signature before lowering calls, including forward calls.
def read_function(
	name: str, node: Form, index: int, records: dict[str, NamedType], *, entry_module: bool
) -> Function:
	fields = {}
	# Keep each tagged field at its original line for validation and lowering.
	for child in node.operands:
		field = form(child)
		# An unknown or repeated field is invalid at that occurrence.
		if field.tag not in ("params", "returns", "body") or field.tag in fields:
			raise SourceError(field.line)
		fields[field.tag] = field
	# An absent slot has no field location, so use the declaration line.
	if len(fields) != 3:
		raise SourceError(node.line)
	params_node, returns_node, body_node = (fields[tag] for tag in ("params", "returns", "body"))

	params = {}
	# Preserve parameter order for the generated C signature.
	for param in checked_form(params_node, "params").operands:
		field = checked_form(param, arity=1)
		# Each ordinary parameter name introduces a distinct binding.
		if not IDENTIFIER.fullmatch(field.tag) or field.tag in params:
			raise SourceError(param.line)
		params[field.tag] = parameter_type(field.operands[0], records)
	result_node, = checked_form(returns_node, "returns", 1).operands
	# An owning result transfers the allocation before local owners are released.
	if isinstance(result_node, Form) and result_node.tag == OWNED:
		element_node, = checked_form(result_node, OWNED, 1).operands
		result = BufferType(OWNED, read_owned_element(element_node, records))
	# Optional array owners return a pointer that may be null.
	elif isinstance(result_node, Form) and result_node.tag == "optional":
		payload, = checked_form(result_node, "optional", 1).operands
		result = read_optional_owner(payload, records)
	# Scalar results copy by value. Void has no return operand.
	else:
		result_name = identifier(result_node)
		result = records.get(result_name, result_name)
		# Borrowed results have no seed return representation.
		if result != VOID and not isinstance(result, Record) and not copyable(result):
			raise SourceError(result_node.line)
	entry = entry_module and name == "main"
	# The application entry implements the shared host argument ABI.
	if entry and (result != I32 or tuple(params.values()) != (_HOST_ARGV,)):
		raise SourceError(node.line)

	c_name = f"wv_seed_fn_{index}"
	return Function(c_name, tuple(params.items()), result, checked_form(body_node, "body"), entry)


# Encode a source filename as a C literal, including arbitrary path bytes.
def c_string(text: str) -> str:
	parts = ['"']
	# Keep the literal safe for C even when the filename contains special bytes.
	for byte in os.fsencode(text):
		# Print ordinary characters directly so source locations stay readable.
		if 32 <= byte <= 126 and byte not in b'"\\?':
			parts.append(chr(byte))
		# Use fixed-width escapes for quotes, trigraph characters, and nonprintable bytes.
		else:
			parts.append(f"\\{byte:03o}")
	parts.append('"')
	return "".join(parts)


# Give each parameter a C name independently of its source spelling.
def signature(function: Function) -> str:
	params = ", ".join(
		f"{c_type(kind)} wv_value_{index}"
		for index, (_, kind) in enumerate(function.params)
	)
	result = f"static {c_type(function.result)}"
	return f"{result} {function.c_name}({params or 'void'})"


class FunctionEmitter:
	# Initialize the function scope, C names, and ownership state.
	def __init__(
		self, function: Function, module: Module, fixed_arrays: set[FixedArrayType],
		owned_slots: set[Record], imports: dict[str, None] | None
	):
		self.function = function
		self.functions = module.bindings
		self.records = module.type_bindings
		self.values = module.value_bindings
		self.filename = module.filename
		self.source = c_string(module.filename)
		self.fixed_arrays = fixed_arrays
		self.owned_slots = owned_slots
		self.imports = imports
		self.lines = []
		self.scopes = [{
			name: Binding(Value(kind, f"wv_value_{index}"))
			for index, (name, kind) in enumerate(function.params)
		}]
		self.next_value = len(function.params)
		self.moved: set[Binding] = set()
		self.loans: list[tuple[Binding, str]] = []
		self.loops: list[LoopScope] = []

	# Indent generated statements at the current lexical scope.
	def emit_line(self, line: str) -> None:
		self.lines.append("    " * len(self.scopes) + line)

	# Reserve a unique slot for the next emitted value.
	def value_slot(self, kind: Type) -> Value:
		name = f"wv_value_{self.next_value}"
		self.next_value += 1
		return Value(kind, name)

	# Evaluate each operation in source order before emitting its consumer.
	def temporary(self, kind: Type, expression: str) -> Value:
		value = self.value_slot(kind)
		self.emit_line(f"{c_type(kind)} {value.c} = {expression};")
		return value

	# Find the first visible declaration without checking its use.
	def find_binding(self, symbol: str) -> Binding | None:
		# The nearest declaration shadows any outer binding with the same name.
		for scope in reversed(self.scopes):
			binding = scope.get(symbol)
			# A moved binding still shadows an outer declaration.
			if binding is not None:
				return binding
		return self.values.get(symbol)

	# Resolve the nearest live binding at the written operand.
	def binding(self, node: Node, *, read: bool = False) -> Binding:
		binding = self.find_binding(identifier(node))
		# A vector snapshot is a fresh owner on each read and cannot be used as a named place.
		if binding is None or binding in self.moved or (binding.snapshot_vector and not read):
			raise SourceError(node.line)
		return binding

	# Require the exact result type at the operation that consumes it.
	def typed(self, node: Node, kind: Type) -> Value:
		value = self.expression(node)
		# Scalar arithmetic and calls need an explicit conversion between types.
		if value.kind != kind:
			raise SourceError(node.line)
		return value

	# Check an access against loans held during operand evaluation.
	def check_access(self, binding: Binding, exclusive: bool, line: int) -> None:
		# Shared access can coexist with other shared access to the same place.
		for parent, loan in self.loans:
			# Any exclusive access conflicts with an outstanding loan of that place.
			if parent is binding and (exclusive or loan == MUT):
				raise SourceError(line)

	# Resolve the binding and descriptor for a permitted array access.
	def array_access(self, node: Node, line: int, writable: bool = False) -> tuple[Binding, BufferType, str]:
		# A selected byte row can serve directly as a read-only array receiver.
		if isinstance(node, Form) and node.tag == "array.load" and not writable:
			value = self.typed(node, _BYTE_VIEW)
			return Binding(value), _BYTE_VIEW, value.c + "."
		binding, value = self.project(node, named=True)
		kind = value.kind
		# Inline arrays can be accessed through a temporary view of their storage.
		if isinstance(kind, FixedArrayType):
			view = self.fixed_view(binding, writable, line)
			return binding, view.kind, view.c + "."
		# Only array owners and views carry an element descriptor.
		if not isinstance(kind, BufferType):
			raise SourceError(line)
		owned = kind.access == OWNED
		# Writes require the owner's mutable binding or an exclusive parameter.
		if writable and not (binding.writable if owned else kind.access == MUT):
			raise SourceError(line)
		self.check_access(binding, writable, line)
		return binding, kind, value.c + ("->" if owned else ".")

	# Form a checked view of a named inline array's storage.
	def fixed_view(self, binding: Binding, exclusive: bool, line: int) -> Value:
		value = binding.value
		kind = value.kind
		# Only a mutable local permits exclusive access to its inline storage.
		if exclusive and not binding.writable:
			raise SourceError(line)
		self.check_access(binding, exclusive, line)
		access = MUT if exclusive else SHARED
		view = BufferType(access, kind.element)
		return self.temporary(view,
			f"({C_TYPE[access]}){{{value.c}.items, {kind.count}, sizeof({c_type(kind.element)})}}")

	# Index a typed C element only after the runtime verifies the signed index.
	def typed_element(
		self, fields: str, element: Type, index: Value, line: int, *, writable: bool = False
	) -> str:
		native = c_type(element)
		pointer = native if writable else f"const {native}"
		offset = f"wvrt_slice_offset({fields}wvrt_length, {index.c}, {self.site(line)})"
		return f"(({pointer} *){fields}wvrt_items)[{offset}]"

	# Supply the source line to runtime checks emitted for this operation.
	def site(self, line: int) -> str:
		return f"(wvrt_location){{{self.source}, {line}, 1}}"

	# Register a loan for a call argument or infer the type of a local shared view.
	def borrow(self, node: Node, kind: BufferType | None) -> Value:
		exclusive = kind is not None and kind.access == MUT
		tag = "borrow-mut" if exclusive else "borrow-shared"
		target, = checked_form(node, tag, 1).operands
		# A byte row can be borrowed directly from either kind of outer view.
		if kind in (None, _BYTE_VIEW) and isinstance(target, Form) and target.tag == "array.load":
			receiver, subscript = checked_form(target, "array.load", 2).operands
			binding, rows, fields = self.array_access(receiver, target.line)
			# Reserve the containing array before the subscript evaluates.
			if rows.element not in (_BYTE_VIEW, _OWNED_BYTE_ROW):
				raise SourceError(target.line)
			self.loans.append((binding, SHARED))
			index = self.typed(subscript, I32)
			# The host already provides a descriptor per argument.
			if rows.element == _BYTE_VIEW:
				return self.temporary(_BYTE_VIEW, self.typed_element(fields, _BYTE_VIEW, index, target.line))
			view = array_view(fields, SHARED)
			return self.temporary(_BYTE_VIEW,
				f"wvrt_array_borrow_row_at_span({view}, {index.c}, -1, {target.line}, {target.line})")
		source = target
		range_bounds = None
		# A range projects a half-open part of the named source.
		if isinstance(target, Form) and target.tag == "array.range":
			# Direct call ranges use the seed's byte-slice ABI.
			if kind is not None and kind.element != U8:
				raise SourceError(node.line)
			source, start, end = form(target, arity=3).operands
			range_bounds = (start, end)
		binding, origin = self.project(source, named=True)
		origin_kind = origin.kind
		# A local borrow retains the element type of its backing descriptor.
		if kind is None:
			# Infer the local view only from a typed array descriptor.
			if not isinstance(origin_kind, BufferType):
				raise SourceError(node.line)
			kind = BufferType(SHARED, origin_kind.element)
		# The loan and its source must agree on the exact element type.
		if not isinstance(origin_kind, (BufferType, FixedArrayType)) or origin_kind.element != kind.element:
			raise SourceError(node.line)
		# Inline storage requires a descriptor before it can be projected.
		if isinstance(origin_kind, FixedArrayType):
			view = self.fixed_view(binding, exclusive, node.line)
			# Whole-place borrows can use the descriptor directly.
			if range_bounds is None:
				self.loans.append((binding, kind.access))
				return view
			fields = view.c + "."
		# Owners and received views already carry a descriptor.
		else:
			owned = origin_kind.access == OWNED
			writable = binding.writable if owned else origin_kind.access == MUT
			# Exclusive access comes from a writable owner or a mutable slice type.
			if exclusive and not writable:
				raise SourceError(node.line)
			self.check_access(binding, exclusive, node.line)
			fields = origin.c + ("->" if owned else ".")

		# Keep the backing storage live while evaluating the two endpoints.
		if range_bounds is not None:
			checkpoint = len(self.loans)
			self.loans.append((binding, SHARED))
			# End the reservation before registering the result's access mode.
			try:
				start = self.typed(range_bounds[0], I32)
				end = self.typed(range_bounds[1], I32)
			finally:
				del self.loans[checkpoint:]
			# The byte helper keeps the seed's path-and-line runtime diagnostic.
			if kind.element == U8:
				helper = "wvrt_mut_slice_range_u8" if exclusive else "wvrt_slice_range_u8"
				expression = (
					f"{helper}({fields}wvrt_items, {fields}wvrt_length, "
					f"{start.c}, {end.c}, {self.site(target.line)})"
				)
			# The typed helper advances by each element's descriptor stride.
			else:
				view = array_view(fields, SHARED)
				expression = f"wvrt_slice_range_at_span({view}, {start.c}, {end.c}, -1, {target.line}, {target.line})"
		# A whole-place borrow keeps the original pointer and extent.
		else:
			expression = (
				f"({C_TYPE[kind.access]}){{{fields}wvrt_items, "
				f"{fields}wvrt_length, {fields}wvrt_element_size}}"
			)
		self.loans.append((binding, kind.access))
		return self.temporary(kind, expression)

	# Borrow a stable record place for the remaining arguments and the call.
	def borrow_record(self, node: Node, kind: SharedRef) -> Value:
		target, = checked_form(node, "borrow-shared", 1).operands
		# An array element loan leaves the containing array as the owner.
		if isinstance(target, Form) and target.tag == "array.load":
			receiver, subscript = checked_form(target, "array.load", 2).operands
			binding, rows, fields = self.array_access(receiver, target.line)
			# The exact record identity determines the element layout.
			if rows.element is not kind.record:
				raise SourceError(target.line)
			self.loans.append((binding, SHARED))
			index = self.typed(subscript, I32)
			# An owning record occupies a slot that may still be uninitialized.
			if owns(kind.record):
				self.owned_slots.add(kind.record)
				slot = f"{kind.record.c_name}_slot"
				view = array_view(fields, SHARED)
				return self.temporary(kind,
					f"wvrt_owned_array_borrow({slot}, {view}, {index.c}, -1, {target.line}, {target.line})")
			element = self.typed_element(fields, kind.record, index, target.line)
			return self.temporary(kind, f"&({element})")

		binding, value = self.project(target, named=True)
		# Taking the address of a record and reborrowing its existing address use the same loan.
		if value.kind is kind.record:
			expression = f"&({value.c})"
		elif value.kind == kind:
			expression = value.c
		else:
			raise SourceError(target.line)
		self.check_access(binding, False, target.line)
		self.loans.append((binding, SHARED))
		return self.temporary(kind, expression)

	# Lend a checked non-null pointer without transferring its native destructor.
	def borrow_native_handle(self, node: Node, kind: NativeRef) -> Value:
		tag = "borrow-mut" if kind.writable else "borrow-shared"
		target, = checked_form(node, tag, 1).operands
		binding, value = self.project(target, named=True)
		# Only an owner of this exact handle type can supply its borrowed representation.
		if value.kind != NativePointerOwner(kind.owner) or (kind.writable and not binding.writable):
			raise SourceError(target.line)
		self.check_access(binding, kind.writable, target.line)
		self.loans.append((binding, MUT if kind.writable else SHARED))
		return self.require_native_handle(kind, value.c, target.line)

	# Reject absence before passing a live handle to a C consumer.
	def require_native_handle(self, kind: NativeRef | NativePointerOwner, value: str, line: int) -> Value:
		return self.temporary(kind, f"({c_type(kind)})wvrt_require_borrow_at_span({value}, -1, {line}, {line})")

	# Lend the original scalar or record place using the declared native pointer type.
	def borrow_native_place(self, node: Node, kind: NativePlaceRef, *, initializing: bool = False) -> Value:
		tag = "borrow-mut" if kind.writable else "borrow-shared"
		target, = checked_form(node, tag, 1).operands
		binding, value = self.project(target, named=True)
		# Only a live place of the exact type can satisfy this pointer argument.
		if value.kind != kind.kind or (kind.writable and not binding.writable):
			raise SourceError(target.line)
		self.check_access(binding, kind.writable, target.line)
		self.loans.append((binding, MUT if kind.writable else SHARED))
		# A storage loan keeps the owner available for its checked address and possible activation.
		if isinstance(kind.kind, NativeStorage):
			mode = "true" if initializing else "false"
			self.emit_line(f"wvrt_c_storage_access({value.c}, {mode}, -1, {target.line}, {target.line});")
			return Value(kind, value.c)
		return self.temporary(kind, f"&({value.c})")

	# Evaluate arguments left to right and end only this call's loans.
	def call(self, node: Form) -> Value:
		# A call needs a named callee before its arguments.
		if not node.operands:
			raise SourceError(node.line)
		name, *args = node.operands
		symbol = identifier(name)
		# A value shadows a function. The seed only supports named calls.
		if self.find_binding(symbol) is not None:
			raise SourceError(name.line)
		callee = self.functions.get(symbol)
		# Neither an unavailable selection nor the host entry can be called.
		if callee is None or (isinstance(callee, Function) and callee.is_entry):
			raise SourceError(node.line)
		parameters = tuple(kind for _, kind in callee.params)
		native = callee if isinstance(callee, NativeFunction) else None
		constants = native.constants if native is not None else {}
		# Hidden header constants occupy a C formal without consuming a Wove operand.
		if len(args) != len(parameters) - len(constants):
			raise SourceError(node.line)
		checkpoint = len(self.loans)
		values = []
		sites = []
		arguments = iter(args)
		# Each argument may acquire a loan that constrains later arguments.
		for index, kind in enumerate(parameters):
			# Let Clang type-check header constants using the original declaration.
			if index in constants:
				value = Value(kind, constants[index])
				self.check_native_value(value)
				values.append(value.c)
				sites.append(node.line)
				continue
			argument = next(arguments)
			sites.append(argument.line)
			# Literal null is admitted only where the bridge declares that representation.
			literal_null = isinstance(argument, Atom) and not argument.quoted and argument.text == "null"
			if native is not None and (index in native.null_only or literal_null and index in native.nullable):
				# A null-only formal cannot borrow caller storage.
				if not literal_null:
					raise SourceError(argument.line)
				values.append("NULL")
				continue
			# A borrow preserves the original array or record storage.
			if isinstance(kind, BufferType):
				value = self.borrow(argument, kind)
			elif isinstance(kind, SharedRef):
				value = self.borrow_record(argument, kind)
			elif isinstance(kind, NativePlaceRef):
				value = self.borrow_native_place(argument, kind, initializing=index in native.initializers)
			elif isinstance(kind, NativeRef) and isinstance(argument, Form):
				value = self.borrow_native_handle(argument, kind)
			else:
				value = self.typed(argument, kind)
				# A consumed owner must be present before the native call can take responsibility for it.
				if isinstance(kind, NativePointerOwner):
					value = self.require_native_handle(kind, value.c, argument.line)
			values.append(value.c)
		# Apply C adaptations only after evaluating the original source arguments.
		if native is not None:
			result = self.native_call(native, parameters, sites, values, node.line)
		else:
			result = self.call_result(callee.result, f"{callee.c_name}({', '.join(values)})")
		del self.loans[checkpoint:]
		return result

	# Materialize a call result before emitting its cleanup or following statement.
	def call_result(self, kind: Type, expression: str) -> Value:
		# A void call is a statement and supplies no C value to a consumer.
		if kind == VOID:
			self.emit_line(f"{expression};")
			return Value(VOID, "")
		return self.temporary(kind, expression)

	# Check bridge extents, prepare temporary strings, invoke, and release the scratch.
	def native_call(self, callee: NativeFunction, parameters: tuple[Type, ...], sites: list[int],
		values: list[str], line: int) -> Value:
		# A function-like macro consumes captured scalar arguments without a function address.
		if callee.macro:
			for value in values:
				self.emit_line(f"(void)({value});")
			return self.call_result(callee.result, f"{callee.c_name}({', '.join(values)})")
		site = f"-1, {line}, {line}"
		actuals = values.copy()
		abi = [c_type(kind) for kind in parameters]
		# Convert checked storage loans to their original C object addresses.
		for index, kind in enumerate(parameters):
			if isinstance(kind, NativePlaceRef) and isinstance(kind.kind, NativeStorage) and values[index] != "NULL":
				actuals[index] = f"({abi[index]})wvrt_c_storage_data({values[index]})"
		# Verify every byte view against its captured operands before entering C.
		for buffer, size, count in callee.byte_extents:
			mutable = parameters[buffer] == _MUT_BYTE_VIEW
			view = f"wvrt_share_mut_slice({values[buffer]})" if mutable else values[buffer]
			width = "1" if size is None else values[size]
			self.emit_line(f"wvrt_check_c_bytes_native({view}, {width}, {values[count]}, {site});")
			actuals[buffer] = f"{values[buffer]}.wvrt_items"
			abi[buffer] = C_WRITABLE_BYTES if mutable else C_READONLY_BYTES
		# A native array uses the element type declared by its checked view.
		for buffer, count in callee.array_extents:
			kind = parameters[buffer]
			mutable = kind.access == MUT
			view = f"wvrt_share_mut_slice({values[buffer]})" if mutable else values[buffer]
			length = values[count] if isinstance(count, int) else count
			element = c_type(kind.element)
			self.emit_line(f"wvrt_check_c_array_native({view}, sizeof({element}), {length}, {site});")
			actuals[buffer] = f"{values[buffer]}.wvrt_items"
			abi[buffer] = f"{element} *" if mutable else f"const {element} *"
		converted = []
		# Every converted string uses a byte view and its own source location.
		for index in callee.c_strings:
			mutable = parameters[index] == _MUT_BYTE_VIEW
			abi[index] = "char *" if mutable else "const char *"
			# A null-only string formal needs no descriptor or temporary backing storage.
			if index not in callee.null_only:
				view = f"wvrt_share_mut_slice({values[index]})" if mutable else values[index]
				converted.append((index, mutable, view, sites[index]))
		# Detached row tables use the same C vector type as the original prototype.
		for index in callee.vectors:
			abi[index] = "char *const *"
		# Clang checks the fixed prefix and applies its native variadic promotions to the actual tail.
		if callee.variadic_start is not None:
			abi[callee.variadic_start:] = ["..."]
		formals = ", ".join(abi) or "void"
		# Verify every newly acquired native owner against its declared C destructor.
		if isinstance(callee.result, NativePointerOwner):
			owner = callee.result.owner
			self.emit_line(f"WVRT_CHECK_C_POINTER_OWNER({owner.c_name}, {owner.pointer_drop});")
		# A single allocation keeps every checked input live until the C result is captured.
		if converted:
			scratch = self.value_slot(_BYTE_VIEW).c
			descriptors = f"{scratch}_arguments"
			self.emit_line(f"wvrt_cstr_argument {descriptors}[] = {{")
			# The runtime checks each original view for embedded NUL before calling C.
			for index, mutable, view, arg_line in converted:
				self.emit_line(f"    {{{view}, true, -1, {arg_line}, {arg_line}, NULL}},")
			self.emit_line("};")
			self.emit_line(f"char *{scratch} = wvrt_cstr_prepare({descriptors}, {len(converted)});")
			# Materialize the exact const or writable C argument type from the captured descriptor.
			for position, (index, mutable, _, _) in enumerate(converted):
				cast = "(char *)" if mutable else ""
				actuals[index] = f"{cast}{descriptors}[{position}].wvrt_value"
		# Each vector keeps its own detached table and terminated copies until the call completes.
		for index in callee.vectors:
			name = self.value_slot(_C_STRING_ROWS).c
			arg_line = sites[index]
			self.emit_line(f"char *const *{name} = wvrt_cstrs_prepare({values[index]}, -1, {arg_line}, {arg_line});")
			actuals[index] = name
		expression = f"{callee.c_name}({', '.join(actuals)})"
		result_types = (c_type(callee.result),)
		snapshot = callee.snapshot
		# Snapshot return types depend on whether the header returns text or a record pointer.
		if snapshot is not None:
			# The C declaration owns the actual layout of the inspected record.
			if snapshot.field is not None:
				result_types = (f"__typeof__({expression})",)
			else:
				result_types = ("char *", "const char *") if snapshot.release is None else ("char *",)
		prototypes = ", ".join(f"{kind} (*)({formals}): 1" for kind in result_types)
		self.emit_line(f'_Static_assert(_Generic(&({callee.c_name}), {prototypes}, '
			f'default: 0), "Wove C prototype does not match {callee.c_name}");')
		# Snapshot before dropping scratch because borrowed results can alias an argument.
		if snapshot is not None:
			expression = self.native_snapshot(snapshot, expression, site)
		result = self.call_result(callee.result, expression)
		# Only a successful C initializer activates its native object's destructor.
		for index in sorted(callee.initializers):
			self.emit_line(f"wvrt_c_storage_finish_init({values[index]}, {result.c});")
		# Copy back only supplied mutable strings, then release scratch while preserving errno.
		if converted:
			for position, (index, mutable, _, _) in enumerate(converted):
				# Immutable byte views never receive scratch writes.
				if mutable:
					self.emit_line(f"wvrt_cstr_copy_back({values[index]}, {descriptors}[{position}].wvrt_value);")
			self.emit_line(f"wvrt_cstr_drop({scratch});")
		# Drop each detached vector without changing the native call's error state.
		for index in callee.vectors:
			self.emit_line(f"wvrt_cstrs_drop({actuals[index]});")
		return result

	# Copy native text using the runtime's checked field, borrowing, and release operations.
	def native_snapshot(self, snapshot: NativeStringResult, expression: str, site: str) -> str:
		# An extended record reports the live byte extent separately from the nominal array size.
		if snapshot.extent is not None:
			return (f"WVRT_SNAPSHOT_EXTENDED_CSTR_FIELD_AT_SPAN({expression}, "
				f"{snapshot.field}, {snapshot.extent}, {site})")
		# A fixed record field is checked against its original inline storage.
		if snapshot.field is not None:
			return f"WVRT_SNAPSHOT_CSTR_FIELD_AT_SPAN({expression}, {snapshot.field}, {site})"
		release = snapshot.release
		# Match the declared C release function without a cast or a hard-coded symbol.
		if release is None:
			return f"wvrt_snapshot_borrowed_cstr_at_span({expression}, {site})"
		return (f"_Generic(&({release}), void (*)(void *): wvrt_snapshot_cstr_at_span, "
			f"void (*)(char *): wvrt_snapshot_chars_at_span)({expression}, {release}, {site})")

	# Keep right-hand evaluation inside its branch while checking it unconditionally.
	def short_circuit(self, node: Form) -> Value:
		left, right = form(node, arity=2).operands
		first = self.typed(left, BOOL)
		result = self.temporary(BOOL, first.c)
		test = result.c if node.tag == "bool.and" else f"!{result.c}"
		# The left result survives when the right operand is skipped.
		self.emit_line(f"if ({test}) {{")
		# Boolean operands cannot move enclosing owners, so only C temporaries need a scope.
		self.scopes.append({})
		second = self.typed(right, BOOL)
		self.emit_line(f"{result.c} = {second.c};")
		self.scopes.pop()
		self.emit_line("}")
		return result

	# Evaluate one selected arm and keep its result after the branch.
	def conditional_value(self, node: Form) -> Value:
		test, on_true, on_false = form(node, arity=3).operands
		then_operand, = checked_form(on_true, "then", 1).operands
		else_operand, = checked_form(on_false, "else", 1).operands
		condition = self.typed(test, BOOL)
		incoming = self.moved.copy()
		declaration = len(self.lines)
		indent = "    " * len(self.scopes)
		self.emit_line(f"if ({condition.c}) {{")
		self.scopes.append({})
		first = self.expression(then_operand)
		# The driver also selects between exact optional owned array results.
		if not (copyable(first.kind) or isinstance(first.kind, OptionalOwner)):
			raise SourceError(then_operand.line)
		result = self.value_slot(first.kind)
		self.emit_line(f"{result.c} = {first.c};")
		self.scopes.pop()
		then_moved = self.moved
		self.moved = incoming
		self.emit_line("} else {")
		self.scopes.append({})
		second = self.typed(else_operand, result.kind)
		self.emit_line(f"{result.c} = {second.c};")
		self.scopes.pop()
		self.emit_line("}")
		# Either arm can consume an outer owner while producing its result.
		self.moved |= then_moved
		# The result type is known only after lowering the first arm.
		self.lines.insert(declaration, f"{indent}{c_type(result.kind)} {result.c};")
		return result

	# Evaluate field expressions in source order and assemble the named value.
	def make_record(self, node: Form) -> Value:
		# The constructor needs a type name before its field initializers.
		if not node.operands:
			raise SourceError(node.line)
		name, *fields = node.operands
		record = self.records.get(identifier(name))
		# Only a declared record has a usable member layout.
		if not isinstance(record, Record):
			raise SourceError(name.line)
		initializers = {}
		# Resolve each initializer to its C member before evaluating its value.
		for field_node in fields:
			initializer = checked_form(field_node, arity=1)
			field = record.fields.get(initializer.tag)
			# A constructor supplies each declared member exactly once.
			if field is None or initializer.tag in initializers:
				raise SourceError(field_node.line)
			value = self.typed(initializer.operands[0], field.kind)
			initializers[initializer.tag] = f".{field.c_name} = {value.c}"
		# A Wove record supplies every field. C records use C aggregate initialization.
		if not record.native and len(initializers) != len(record.fields):
			raise SourceError(node.line)
		values = ", ".join(initializers.values()) or "0"
		return self.temporary(record, f"({record.c_name}){{{values}}}")

	# Project a field without copying the owner and retain its root binding for loans.
	def project(self, node: Node, *, named: bool = False) -> tuple[Binding | None, Value]:
		# Field paths keep the same root even through nested records.
		if isinstance(node, Form) and node.tag == "struct.get":
			target, name = checked_form(node, "struct.get", 2).operands
			binding, value = self.project(target, named=named)
			record = value.kind
			selection = "."
			# A shared reference projects fields from the original record.
			if isinstance(record, SharedRef):
				record = record.record
				selection = "->"
			# A computed owning temporary must be placed in a binding before projection.
			if not isinstance(record, Record) or (binding is None and owns(record)):
				raise SourceError(target.line)
			member = record.fields.get(identifier(name))
			# A projection keeps the member's exact declared type.
			if member is None:
				raise SourceError(name.line)
			return binding, Value(member.kind, f"{value.c}{selection}{member.c_name}")
		# A borrow or store needs a stable place with a live binding.
		if named or (isinstance(node, Atom) and not node.quoted and IDENTIFIER.fullmatch(node.text)):
			binding = self.binding(node)
			return binding, binding.value
		return None, self.expression(node)

	# Read a non-owning member by value without transferring any containing record.
	def record_field(self, node: Form) -> Value:
		binding, value = self.project(node)
		# An owning field stays in its record unless the record itself is transferred.
		if owns(value.kind):
			raise SourceError(node.line)
		# A read of a place obeys loans held on its root binding.
		if binding is not None:
			self.check_access(binding, False, node.line)
		return self.temporary(value.kind, value.c)

	# Lower scalar fixed arrays with inline storage and ordered element evaluation.
	def initialize_fixed_array(self, node: Form) -> Value:
		kind, elements = fixed_array_initializer(node)
		self.fixed_arrays.add(kind)
		# A repeat evaluates its operand once, even when the result is empty.
		if node.tag == "array.repeat":
			element = self.typed(elements[0], kind.element)
			value = self.temporary(kind, "{0}")
			index = self.value_slot(I32)
			self.emit_line(f"for (int32_t {index.c} = 0; {index.c} < {kind.count}; ++{index.c}) {{")
			self.emit_line(f"    {value.c}.items[{index.c}] = {element.c};")
			self.emit_line("}")
			return value
		items = fixed_array_braces(self.typed(element, kind.element).c for element in elements)
		return self.temporary(kind, items)

	# Use the signed runtime check only with values representable in int64_t.
	def convert_integer(self, value: Value, kind: str | NativeType, line: int) -> Value:
		# Clang knows the exact native type width and signedness from the included header.
		if isinstance(kind, NativeType) or isinstance(value.kind, NativeType) or kind == I64:
			result = self.temporary(kind, "0")
			self.emit_line(f"if (__builtin_add_overflow({value.c}, 0, &{result.c})) {{")
			self.emit_line(f"    wvrt_int_conversion_trap(-1, {line}, {line});")
			self.emit_line("}")
			return result
		low, high = SCALAR_LIMITS[kind]
		site = self.site(line)
		# Every supported nonnegative seed integer fits the wider unsigned type.
		if kind == U64:
			# A negative signed value fails the existing checked conversion.
			if SCALAR_LIMITS[value.kind][0] < 0:
				self.emit_line(f"(void)wvrt_int_convert({value.c}, 0, INT64_MAX, {site});")
			return self.temporary(U64, f"(uint64_t){value.c}")
		operand = value.c
		# Substitute a representable out-of-range value before passing an oversized u64 to C's signed check.
		if value.kind == U64:
			operand = f"({value.c} > UINT64_C({high}) ? INT64_C({high + 1}) : (int64_t){value.c})"
		return self.temporary(kind, f"({C_TYPE[kind]})wvrt_int_convert({operand}, {low}, {high}, {site})")

	# Ask Clang to check the original C object or macro's declared value type.
	def check_native_value(self, value: Value) -> None:
		kind = value.kind
		# A shared native handle must use the pointer type declared by its header.
		if isinstance(kind, NativeRef):
			self.emit_line(f"WVRT_CHECK_C_POINTER_TYPEDEF({c_type(kind)});")
			message = "Wove C borrowed value does not match its declared handle type"
		else:
			message = "Wove C value does not match its declared type"
		self.emit_line(f'_Static_assert(_Generic(({value.c}), {c_type(kind)}: 1, default: 0), "{message}");')

	# Translate one operation, preserving ownership of every array value.
	def expression(self, node: Node) -> Value:
		# A bare name reads a scalar or record before the enclosing operation uses it.
		if isinstance(node, Atom) and not node.quoted and IDENTIFIER.fullmatch(node.text):
			binding = self.binding(node, read=True)
			kind = binding.value.kind
			# Snapshot the current C-owned rows without taking ownership of the original table.
			if binding.snapshot_vector:
				symbol = binding.value.c
				self.emit_line(f"extern char **{symbol};")
				return self.temporary(kind, f"wvrt_snapshot_cstrs_at_span({symbol}, -1, {node.line}, {node.line})")
			# Capture header values before another argument or call can change their native storage.
			if binding.native or isinstance(kind, NativeRef):
				self.check_access(binding, False, node.line)
				self.check_native_value(binding.value)
				return self.temporary(kind, binding.value.c)
			# Array and slice places need an explicit operation.
			if not copyable(kind):
				raise SourceError(node.line)
			self.check_access(binding, False, node.line)
			# Capture the value now even if a later statement writes to its slot.
			if binding.writable:
				return self.temporary(binding.value.kind, binding.value.c)
			return binding.value

		expr = node = value_form(node)
		# Function literals follow the same rules as module initializers.
		if expr.tag in SCALAR_TYPES:
			return read_scalar_literal(expr)

		# Allocate inactive native storage using the provider's exact C layout.
		if expr.tag == "native.alloc":
			operand, = checked_form(expr, arity=1).operands
			owner = self.records.get(identifier(operand))
			# Only a declared storage owner can be allocated by the seed.
			if not isinstance(owner, NativeType) or owner.storage_drop is None:
				raise SourceError(operand.line)
			native, drop = owner.c_name, owner.storage_drop
			self.emit_line(f'_Static_assert(_Generic(&({drop}), __typeof__(({drop})(({native} *)0)) '
				f'(*)({native} *): 1, default: 0), "Wove C storage destructor does not match {native}");')
			return self.temporary(NativeStorage(owner),
				f"wvrt_c_storage_new(sizeof({native}), _Alignof({native}), -1, {expr.line}, {expr.line})")

		# Store embedded bytes in the translation unit rather than reading them at runtime.
		if expr.tag == "embed":
			data = read_embed(expr, self.filename, self.imports)
			value = self.value_slot(_BYTE_VIEW)
			# A function-local embed still has static backing storage.
			for line in static_byte_view(value.c, data):
				self.emit_line(line)
			return value

		# Fixed-size constructors do not allocate a runtime-owned array.
		if expr.tag in ("array.new", "array.repeat"):
			return self.initialize_fixed_array(expr)

		# Binary boolean expressions guard every operation in the right operand.
		if expr.tag in ("bool.and", "bool.or"):
			return self.short_circuit(expr)

		# An expression branch writes one result without evaluating the other arm.
		if expr.tag == "if":
			return self.conditional_value(expr)

		# Negation requires a boolean and keeps its type.
		if expr.tag == "bool.not":
			operand, = form(expr, arity=1).operands
			value = self.typed(operand, BOOL)
			return self.temporary(BOOL, f"!{value.c}")

		# Discard promoted bits above the exact width of the complemented word.
		if expr.tag in ("u32.not", "u64.not"):
			operand, = form(expr, arity=1).operands
			kind = expr.tag.split(".")[0]
			value = self.typed(operand, kind)
			return self.temporary(kind, f"({C_TYPE[kind]})~(uint64_t){value.c}")

		# Match the complete typed alias using only the seed's current integer types.
		destination, separator, origin = expr.tag.partition(".from-")
		typed_conversion = bool(separator and destination in SCALAR_LIMITS and origin in SCALAR_LIMITS)
		if expr.tag == "int.convert" or typed_conversion:
			# Both spellings evaluate the same single operand and use the same range limits.
			if typed_conversion:
				operand, = form(expr, arity=1).operands
				kind = destination
				value = self.typed(operand, origin)
			else:
				target, operand = form(expr, arity=2).operands
				name = identifier(target)
				kind = self.records.get(name, name)
				# Check the destination before evaluating the operand.
				if kind not in SCALAR_LIMITS and not isinstance(kind, NativeType):
					raise SourceError(node.line)
				value = self.expression(operand)
				# Native integer aliases keep their C spelling across the conversion.
				if value.kind not in SCALAR_LIMITS and not isinstance(value.kind, NativeType):
					raise SourceError(node.line)
			# Retain the seed's existing direct widening lowering for this alias.
			if expr.tag == "i32.from-u8":
				return self.temporary(I32, f"(int32_t){value.c}")
			return self.convert_integer(value, kind, node.line)

		# A record construction produces a new value of its declared type.
		if expr.tag == "struct.new":
			return self.make_record(expr)

		# A member access copies a non-owning value.
		if expr.tag == "struct.get":
			return self.record_field(expr)
		# Allocate one runtime array with the source location for traps.
		if expr.tag == "array.alloc":
			element_node, length = form(expr, arity=2).operands
			element = read_owned_element(element_node, self.records)
			size = self.typed(length, I32)
			kind = BufferType(OWNED, element)
			# Byte rows already have a runtime element destructor.
			if element == _OWNED_BYTE_ROW:
				return self.temporary(kind, f"wvrt_array_new_rows_at_span({size.c}, -1, {node.line}, {node.line})")
			# The runtime initializes empty record slots and drops the live ones at scope exit.
			if isinstance(element, Record) and owns(element):
				self.owned_slots.add(element)
				slot = f"{element.c_name}_slot"
				return self.temporary(kind,
					f"wvrt_owned_array_new({slot}, {slot}_drop, {size.c}, -1, {node.line}, {node.line})")
			native = c_type(element)
			site = self.site(node.line)
			return self.temporary(
				kind, f"wvrt_array_new({size.c}, sizeof({native}), _Alignof({native}), {site})"
			)

		# A fallible allocation uses the native layout of a copyable element.
		if expr.tag == "array.try-alloc":
			element_node, length = form(expr, arity=2).operands
			element = read_owned_element(element_node, self.records)
			# Owning elements require an initialized-slot allocator.
			if not copyable(element):
				raise SourceError(element_node.line)
			size = self.typed(length, I32)
			native = c_type(element)
			kind = OptionalOwner(BufferType(OWNED, element))
			return self.temporary(kind, f"wvrt_array_try_new({size.c}, sizeof({native}), _Alignof({native}))")

		# An absent optional is a movable slot with a null pointer.
		if expr.tag == "optional.none":
			payload, = form(expr, arity=1).operands
			return self.temporary(read_optional_owner(payload, self.records), "NULL")

		# Wrap a fresh or transferred owned array without another allocation.
		if expr.tag == "optional.some":
			operand, = form(expr, arity=1).operands
			value = self.expression(operand)
			# A slice or an already optional value cannot supply this payload.
			if not isinstance(value.kind, BufferType) or value.kind.access != OWNED:
				raise SourceError(operand.line)
			return Value(OptionalOwner(value.kind), value.c)

		# Inspect absence without consuming or borrowing the payload.
		if expr.tag == "owner.is-null":
			operand, = form(expr, arity=1).operands
			binding = self.binding(operand)
			# Optional arrays and native pointer owners can represent absence.
			if not isinstance(binding.value.kind, (OptionalOwner, NativePointerOwner)):
				raise SourceError(operand.line)
			self.check_access(binding, False, node.line)
			return self.temporary(BOOL, f"{binding.value.c} == NULL")

		# Move a whole owner or explicitly unwrap and move its present payload.
		if expr.tag == "move":
			return self.move_owner(expr)

		# Read the element count of the live owner or received view.
		if expr.tag == "array.len":
			target, = form(expr, arity=1).operands
			_, _, fields = self.array_access(target, node.line)
			return self.temporary(I32, f"{fields}wvrt_length")

		# Copy an element through either descriptor after checking its index.
		if expr.tag == "array.load":
			target, subscript = form(expr, arity=2).operands
			_, kind, fields = self.array_access(target, node.line)
			element = kind.element
			# A slot with ownership must be loaned or transferred explicitly.
			if owns(element):
				raise SourceError(node.line)
			index = self.typed(subscript, I32)
			# A typed access copies a record value or a shared row descriptor.
			if element != U8:
				return self.temporary(element, self.typed_element(fields, element, index, node.line))
			return self.temporary(
				U8, f"wvrt_slice_load_u8({fields}wvrt_items, {fields}wvrt_length, "
				f"{index.c}, {self.site(node.line)})"
			)

		prefix, _, predicate = expr.tag.partition(".")
		# Decode the exact number of bytes using the runtime's endian-independent path.
		if prefix in BYTE_DECODER_WIDTHS and predicate in ("from-le-bytes", "from-be-bytes"):
			operand, = form(expr, arity=1).operands
			checkpoint = len(self.loans)
			data = self.borrow(operand, _BYTE_VIEW)
			del self.loans[checkpoint:]
			endian = "true" if predicate == "from-be-bytes" else "false"
			width = BYTE_DECODER_WIDTHS[prefix]
			return self.temporary(prefix, f"({c_type(prefix)})wvrt_int_from_bytes("
				f"{data.c}, {width}, {endian}, -1, {node.line}, {node.line})")

		step = STEP_OPS.get(predicate)
		wrapping = WRAPPING_OPS.get(f"{prefix}.{step}" if step is not None else expr.tag)
		# Evaluate exact-type operands in order before performing wrapping arithmetic.
		if wrapping is not None:
			kind, helper = wrapping
			# A unary step uses the existing arithmetic helper with an implicit one.
			if step is not None:
				operand, = form(expr, arity=1).operands
				value = self.typed(operand, kind)
				return self.temporary(kind, f"{helper}({value.c}, 1)")
			left, right = form(expr, arity=2).operands
			first = self.typed(left, kind)
			second = self.typed(right, kind)
			return self.temporary(kind, f"{helper}({first.c}, {second.c})")

		# The driver increments an unsigned 64-bit counter with wrapping arithmetic.
		if expr.tag == "u64.inc":
			operand, = form(expr, arity=1).operands
			value = self.typed(operand, U64)
			return self.temporary(U64, f"{value.c} + UINT64_C(1)")

		zero_comparison = ZERO_PREDICATES.get(predicate)
		# Reuse the same typed operators when a zero operand is implicit.
		if zero_comparison is not None:
			comparison, reverse = zero_comparison
			operation = BINARY_OPS.get(f"{prefix}.{comparison}")
			# A unary alias cannot introduce a type the binary seed does not support.
			if operation is not None:
				kind, result, operator = operation
				operand, = form(expr, arity=1).operands
				value = self.typed(operand, kind)
				# Keep the operand evaluation when unsigned comparison with zero is constant.
				if kind in (U8, U32, U64) and predicate in ("ltz", "not-ltz"):
					truth = "true" if predicate == "not-ltz" else "false"
					return self.temporary(result, f"((void){value.c}, {truth})")
				left, right = ("0", value.c) if reverse else (value.c, "0")
				return self.temporary(result, f"{left} {operator} {right}")

		binary = BINARY_OPS.get(expr.tag)
		# Evaluate both exact-type operands in source order.
		if binary is not None:
			kind, result, operator = binary
			left, right = form(expr, arity=2).operands
			first = self.typed(left, kind)
			second = self.typed(right, kind)
			# Keep the full unsigned product before reducing it to the low word.
			if expr.tag == "u32.mul":
				return self.temporary(U32, f"(uint32_t)((uint64_t){first.c} * {second.c})")
			# Multiply as unsigned bits so signed overflow has Wove wrapping semantics.
			if expr.tag == "i32.mul":
				product = f"(uint32_t)((uint64_t)(uint32_t){first.c} * (uint32_t){second.c})"
				return self.temporary(I32, f"wvrt_i32_from_wrapped_bits({product})")
			# Signed 64-bit arithmetic also wraps through its unsigned representation.
			if result == I64:
				bits = f"(uint64_t){first.c} {operator} (uint64_t){second.c}"
				return self.temporary(I64, f"wvrt_i64_from_wrapped_bits({bits})")
			# Guard both division operations at the original seed source location.
			if expr.tag in ("u32.div", "u32.rem"):
				remainder = "true" if operator == "%" else "false"
				return self.temporary(
					result, f"wvrt_u32_divide({first.c}, {second.c}, {remainder}, {self.site(node.line)})"
				)
			# The runtime handles signed quotient and remainder edge cases.
			if expr.tag in ("i32.div", "i32.rem"):
				remainder = "true" if operator == "%" else "false"
				return self.temporary(I32,
					f"wvrt_i32_divide_at_span({first.c}, {second.c}, {remainder}, -1, {node.line}, {node.line})")
			return self.temporary(result, f"{first.c} {operator} {second.c}")

		shift = SHIFT_OPS.get(predicate)
		# Check the exact signed shift count before passing the unsigned word to C.
		if prefix in (U32, U64) and shift is not None:
			left, right = form(expr, arity=2).operands
			word = self.typed(left, prefix)
			count = self.typed(right, I32)
			# Preserve the location-bearing seed helper for 32-bit operations.
			if prefix == U32:
				return self.temporary(
					U32, f"(uint32_t)((uint64_t){word.c} {shift} "
					f"wvrt_u32_shift_count({count.c}, {self.site(node.line)}))"
				)
			return self.temporary(
				U64, f"{word.c} {shift} wvrt_u64_shift_count_at_span({count.c}, -1, {node.line}, {node.line})"
			)

		# BLAKE2b rotates an unsigned word using the existing runtime operation.
		if expr.tag == "u64.rotr":
			left, right = form(expr, arity=2).operands
			word = self.typed(left, U64)
			count = self.typed(right, I32)
			return self.temporary(
				U64, f"wvrt_u64_rotr_at_span({word.c}, {count.c}, -1, {node.line}, {node.line})"
			)

		# Call lowering manages the lifetime of its argument loans.
		if expr.tag == "call":
			return self.call(expr)

		raise SourceError(node.line)

	# Transfer an owner once and leave its source slot empty on the continuing path.
	def move_owner(self, node: Form) -> Value:
		target, = form(node, arity=1).operands
		unwrap = target if isinstance(target, Form) and target.tag == "optional.unwrap" else None
		# An unwrap exposes the payload of a named optional owner.
		if unwrap is not None:
			target, = checked_form(unwrap, "optional.unwrap", 1).operands
		binding = self.binding(target)
		kind = binding.value.kind
		# Only an owning value can be transferred.
		if not owns(kind):
			raise SourceError(node.line)
		self.check_access(binding, True, node.line)
		storage = binding.value.c
		expression = storage
		# Check presence before clearing the original slot.
		if unwrap is not None:
			# Once present, the payload keeps its exact array type.
			if not isinstance(kind, OptionalOwner):
				raise SourceError(unwrap.line)
			kind = kind.payload
			expression = f"wvrt_require_array_at_span({storage}, -1, {unwrap.line}, {unwrap.line})"
		result = self.temporary(kind, expression)
		self.moved.add(binding)
		empty = f"({c_type(kind)}){{0}}" if isinstance(kind, Record) else "NULL"
		self.emit_line(f"{storage} = {empty};")
		return result

	# Release owned slots from the scopes being exited in reverse declaration order.
	def cleanup(self, scopes: list[dict[str, Binding]]) -> None:
		# Clean up inner scopes before outer ones.
		for scope in reversed(scopes):
			# An earlier declaration outlives a later one in the same scope.
			for binding in reversed(scope.values()):
				# Use the same cleanup as an owning record slot.
				for statement in drop_statements(binding.value.kind, binding.value.c):
					self.emit_line(statement)

	# Evaluate the return value before releasing every scope the return leaves.
	def emit_return(self, node: Form) -> None:
		# A void return leaves the function without a value.
		if self.function.result == VOID:
			form(node, arity=0)
			exit_line = "return;"
		# Evaluate the result before dropping the scopes it can read.
		else:
			operand, = form(node, arity=1).operands
			value = self.typed(operand, self.function.result)
			exit_line = f"return {value.c};"
		self.cleanup(self.scopes)
		self.emit_line(exit_line)

	# Walk statements in order and require each one to follow a continuing path.
	def statements(self, nodes: tuple[Node, ...]) -> bool:
		continues = True
		# A terminating statement still lets the walker check for trailing statements.
		for node in nodes:
			# Point to the first unreachable statement after a terminator.
			if not continues:
				raise SourceError(node.line)
			continues = self.statement(node)
		return continues

	# Enter a scope for an ordinary conditional arm.
	def block(self, node: Form) -> bool:
		self.scopes.append({})
		return self.scoped_statements(node, len(self.loans))

	# Release the current scope's loans and owners on normal fallthrough.
	def scoped_statements(self, node: Form, loan_checkpoint: int) -> bool:
		continues = self.statements(node.operands)
		del self.loans[loan_checkpoint:]
		# A terminating statement has already dropped the locals in this block.
		if continues:
			self.cleanup(self.scopes[-1:])
		scope = self.scopes.pop()
		self.moved.difference_update(scope.values())
		return continues

	# Emit a boolean branch and merge ownership on paths that reach its join.
	def conditional(self, node: Form) -> bool:
		# An if has a condition, a then arm, and at most one else arm.
		if len(node.operands) not in (2, 3):
			raise SourceError(node.line)
		test, then_node, *otherwise = node.operands
		then_arm = checked_form(then_node, "then")
		else_arm = checked_form(otherwise[0], "else") if otherwise else None
		condition = self.typed(test, BOOL)

		# Both arms start with the state after evaluating the condition.
		incoming = self.moved.copy()
		self.emit_line(f"if ({condition.c}) {{")
		then_continues = self.block(then_arm)
		then_moved = self.moved if then_continues else set()
		self.moved = incoming

		# An omitted else reaches the join with the incoming state.
		else_continues = True
		# Emit the explicit else in its own scope from the incoming state.
		if else_arm is not None:
			self.emit_line("} else {")
			else_continues = self.block(else_arm)
		self.emit_line("}")

		# Any continuing arm can make an outer owner unavailable at the join.
		self.moved = then_moved | self.moved if else_continues else then_moved
		return then_continues or else_continues

	# Emit a pre-test loop with one body scope per iteration.
	def loop(self, node: Form) -> bool:
		test, body_node = form(node, arity=2).operands
		body = checked_form(body_node, "body")
		checkpoint = len(self.loans)
		loop = LoopScope(len(self.scopes), self.moved.copy())
		self.loops.append(loop)
		self.emit_line("while (true) {")
		self.scopes.append({})
		condition = self.typed(test, BOOL)
		exit_moved = self.moved.copy()
		# A literal true loop reaches its successor only by an explicit break.
		if condition.c != "true":
			self.emit_line(f"if (!{condition.c}) {{ break; }}")
		continues = self.scoped_statements(body, checkpoint)
		self.finish_loop(node, continues, exit_moved)
		return condition.c != "true" or loop.found_break

	# Check the back edge and expose moves from paths that exit a loop.
	def finish_loop(self, node: Form, continues: bool, exit_moved: set[Binding]) -> None:
		loop = self.loops.pop()
		# Outer owners available at entry must survive every path back to the test.
		if continues and self.moved - loop.incoming:
			raise SourceError(node.line)
		self.moved = exit_moved | loop.break_moved
		self.emit_line("}")

	# Emit a half-open range or a shared array iteration with one fixed input.
	def for_loop(self, node: Form) -> None:
		binder, iterator, body_node = form(node, arity=3).operands
		pattern = checked_form(binder, "let", 0)
		names = tuple(identifier(name) for name in pattern.names)
		body = checked_form(body_node, "body")
		source = checked_form(iterator)
		checkpoint = len(self.loans)
		# The range endpoints evaluate once, before the loop's binding exists.
		if source.tag == "range":
			start_node, end_node = form(source, arity=2).operands
			start = self.expression(start_node)
			# Range iteration needs one name and an exact integer width.
			if len(names) != 1 or start.kind not in (I32, U64):
				raise SourceError(source.line)
			end = self.typed(end_node, start.kind)
			view = None
		# A shared source stays borrowed until every iteration completes.
		else:
			target, = checked_form(source, "borrow-shared", 1).operands
			_, value = self.project(target, named=True)
			# Indexed iteration introduces a separate name for the current row.
			if not isinstance(value.kind, (BufferType, FixedArrayType)) or len(set(names)) != len(names):
				raise SourceError(source.line)
			view = self.borrow(source, BufferType(SHARED, value.kind.element))
			start = Value(I32, "0")
			end = Value(I32, f"{view.c}.wvrt_length")
		index = self.value_slot(start.kind)
		self.emit_line(f"for ({c_type(index.kind)} {index.c} = {start.c}; {index.c} < {end.c}; ++{index.c}) {{")
		loop = LoopScope(len(self.scopes), self.moved.copy())
		self.loops.append(loop)
		scope = {}
		self.scopes.append(scope)
		# A range binds the cursor, and an indexed view binds it before the row.
		if view is None or len(names) == 2:
			scope[names[0]] = Binding(index)
		# Copy a row or borrow an owned byte row without moving it.
		if view is not None:
			fields = view.c + "."
			element = view.kind.element
			# The seed's owned row layout provides a checked borrowed byte descriptor.
			if element == _OWNED_BYTE_ROW:
				item = self.temporary(_BYTE_VIEW,
					f"wvrt_array_borrow_row_at_span({view.c}, {index.c}, -1, {node.line}, {node.line})")
			# Scalar, copyable record and host argv rows are ordinary C values.
			elif copyable(element) or element == _BYTE_VIEW:
				item = self.temporary(element, self.typed_element(fields, element, index, node.line))
			else:
				raise SourceError(source.line)
			self.emit_line(f"(void){item.c};")
			scope[names[-1]] = Binding(item)
		continues = self.scoped_statements(body, checkpoint)
		self.finish_loop(node, continues, loop.incoming)

	# Drop all iteration locals before a break or continue crosses their scopes.
	def loop_exit(self, node: Form) -> None:
		form(node, arity=0)
		# An exit must belong to an enclosing loop.
		if not self.loops:
			raise SourceError(node.line)
		loop = self.loops[-1]
		outer_moved = self.moved.intersection(binding for scope in self.scopes[:loop.depth]
			for binding in scope.values())
		# A break contributes its outer ownership state to the loop successor.
		if node.tag == "break":
			loop.found_break = True
			loop.break_moved.update(outer_moved)
		# A continue must restore owners before returning to the loop header.
		elif outer_moved - loop.incoming:
			raise SourceError(node.line)
		self.cleanup(self.scopes[loop.depth:])
		self.emit_line(f"{node.tag};")

	# Lower a statement and report whether execution can continue past it.
	def statement(self, node: Node) -> bool:
		statement = node = checked_form(node)
		# Branches emit and join their own lexical scopes.
		if statement.tag == "if":
			return self.conditional(statement)

		# A pre-test loop can fall through when its condition becomes false or it breaks.
		if statement.tag == "while":
			return self.loop(statement)

		# An empty or exhausted iterator always has a natural exit.
		if statement.tag == "for":
			self.for_loop(statement)
			return True

		# Loop exits terminate only the current statement path.
		if statement.tag in ("break", "continue"):
			self.loop_exit(statement)
			return False

		# Assignment replaces the value in the named mutable slot.
		if statement.tag == "set":
			target, operand = form(statement, arity=2).operands
			binding = self.find_binding(identifier(target))
			# A moved owner still has a slot that an assignment can refill.
			if binding is None:
				raise SourceError(target.line)
			kind = binding.value.kind
			replace_owner = isinstance(kind, BufferType) and kind.access == OWNED
			copy_value = kind in SCALAR_TYPES or isinstance(kind, Record) and copyable(kind) or binding.native
			# Views and other owning shapes cannot be rebound by the seed.
			if not binding.writable or not (copy_value or replace_owner):
				raise SourceError(node.line)
			self.check_access(binding, True, node.line)
			value = self.typed(operand, kind)
			# A native lvalue must retain its exact type on writes as well as reads.
			if binding.native:
				self.check_native_value(binding.value)
			# The replacement can read or move the old owner before the slot is retired.
			if replace_owner:
				self.emit_line(f"wvrt_array_drop({binding.value.c});")
				self.moved.discard(binding)
			self.emit_line(f"{binding.value.c} = {value.c};")
			return True

		# A return exits the function and every enclosing block.
		if statement.tag == "return":
			self.emit_return(statement)
			return False

		# Append a captured byte owner to the mutable array of rows.
		if statement.tag == "array.push":
			target, operand = form(statement, arity=2).operands
			binding = self.binding(target)
			# The bootstrap driver grows only named arrays of owned byte rows.
			if not binding.writable or binding.value.kind != BufferType(OWNED, _OWNED_BYTE_ROW):
				raise SourceError(target.line)
			self.check_access(binding, True, target.line)
			checkpoint = len(self.loans)
			self.loans.append((binding, MUT))
			# Keep the destination reserved until its replacement row is captured.
			try:
				row = self.typed(operand, _OWNED_BYTE_ROW)
			finally:
				del self.loans[checkpoint:]
			self.emit_line(f"wvrt_array_push_at_span({binding.value.c}, &{row.c}, -1, {node.line}, {node.line});")
			return True

		# Store directly into the backing storage of a mutable owner or view.
		if statement.tag == "array.store":
			target, subscript, operand = form(statement, arity=3).operands
			binding, kind, fields = self.array_access(target, node.line, writable=True)
			element = kind.element
			owning_record = isinstance(element, Record) and owns(element)
			checkpoint = len(self.loans)
			# An owning slot remains reserved through both operands.
			if kind.access == OWNED or owning_record or isinstance(binding.value.kind, FixedArrayType):
				self.loans.append((binding, MUT))
			# Restore the enclosing loan prefix if either operand fails checking.
			try:
				index = self.typed(subscript, I32)
				value = self.typed(operand, element)
			finally:
				del self.loans[checkpoint:]
			# Storing an owning record transfers its fields and retires the previous slot.
			if owning_record:
				self.owned_slots.add(element)
				slot = f"{element.c_name}_slot"
				view = array_view(fields, MUT)
				self.emit_line(
					f"wvrt_owned_array_store({slot}, {slot}_drop, {view}, {index.c}, {value.c}, "
					f"-1, {node.line}, {node.line});")
			# A row store transfers the incoming allocation and drops any previous owner.
			elif element == _OWNED_BYTE_ROW:
				view = array_view(fields, MUT)
				self.emit_line(f"wvrt_array_store_row_at_span({view}, {index.c}, {value.c}, -1, {node.line}, {node.line});")
			# The existing byte path checks the signed index.
			elif element == U8:
				self.emit_line(
					f"wvrt_slice_store_u8({fields}wvrt_items, {fields}wvrt_length, "
					f"{index.c}, {value.c}, {self.site(node.line)});"
				)
			# The checked element index selects one writable record slot.
			else:
				slot = self.typed_element(fields, element, index, node.line, writable=True)
				self.emit_line(f"{slot} = {value.c};")
			return True

		# Calls used as statements complete without a result.
		if statement.tag == "call":
			self.typed(statement, VOID)
			return True

		# Every other statement in the seed introduces a binding.
		if statement.tag not in ("let", "let-mut"):
			raise SourceError(node.line)
		initializer, = form(statement, arity=1).operands
		name, = statement.names
		symbol = identifier(name)
		scope = self.scopes[-1]
		# Each name can be declared once in the current lexical scope.
		if symbol in scope:
			raise SourceError(name.line)
		# A bound shared view keeps its loan until this binding's block exits.
		if statement.tag == "let" and isinstance(initializer, Form) and initializer.tag == "borrow-shared":
			value = self.borrow(initializer, None)
		else:
			value = self.expression(initializer)
		# A binding needs an ordinary value or a view.
		if value.kind == VOID:
			raise SourceError(initializer.line)
		writable = statement.tag == "let-mut"
		# A mutable copyable binding needs its own slot.
		if writable and copyable(value.kind):
			value = self.temporary(value.kind, value.c)
		scope[symbol] = Binding(value, writable=writable)
		# Wove can ignore a local result while retaining its evaluated initializer.
		self.emit_line(f"(void)({value.c});")
		return True

	# Emit the function and require a result from each value-returning exit path.
	def emit(self, retained: tuple[str, ...] = ()) -> list[str]:
		body = self.function.body
		self.lines.append(signature(self.function) + " {")
		# Every named parameter may be unused in the Wove body.
		for index in range(len(self.function.params)):
			self.emit_line(f"(void)wv_value_{index};")
		# Keep loaded provider functions and module bindings private while marking them used.
		for name in retained:
			self.emit_line(f"(void){name};")
		# A path reaching the end of the body may only exit a void function.
		if self.statements(body.operands):
			# A result must be supplied by an explicit return.
			if self.function.result != VOID:
				raise SourceError(body.line)
			self.cleanup(self.scopes)
		self.lines.extend(["}", ""])
		return self.lines


# Select bootstrap declarations against the target of the generated C.
def cfg_matches(node: Node) -> bool:
	# The seed recognizes bootstrap and excludes tests.
	if isinstance(node, Atom):
		predicate = identifier(node, WORD)
		# Keep the existing rejection of unknown flag predicates.
		if predicate not in ("bootstrap", "test"):
			raise SourceError(node.line)
		return predicate == "bootstrap"
	# Negation has exactly one child.
	if node.tag == "not":
		predicate, = form(node, arity=1).operands
		return not cfg_matches(predicate)
	# Evaluate every child so selection does not hide malformed expressions.
	if node.tag in ("all", "any"):
		results = [cfg_matches(child) for child in node.operands]
		return all(results) if node.tag == "all" else any(results)
	operand, = form(node, arity=1).operands
	# Target identity takes a quoted OS or architecture name.
	if node.tag in ("os", "arch"):
		# A bare atom is not a target name.
		if not isinstance(operand, Atom) or not operand.quoted or not IDENTIFIER.fullmatch(operand.text):
			raise SourceError(operand.line)
		os_name, arch, _ = compiler_target()
		return operand.text == (os_name if node.tag == "os" else arch)
	# Deployment bounds are inclusive and match only when Clang reports a deployment.
	if node.tag in ("os_min", "os_max"):
		# Versions are nonnegative decimal atoms in the native selector.
		if not isinstance(operand, Atom) or operand.quoted or not re.fullmatch(r"[0-9]+", operand.text):
			raise SourceError(operand.line)
		digits = operand.text.lstrip("0") or "0"
		# Keep the deployment representable in the same signed domain.
		if len(digits) > 10 or (bound := int(digits)) > SCALAR_LIMITS[I32][1]:
			raise SourceError(operand.line)
		_, _, deployment = compiler_target()
		# A target without deployment metadata cannot satisfy either bound.
		if deployment == "-":
			return False
		version = int(deployment)
		return version >= bound if node.tag == "os_min" else version <= bound
	raise SourceError(node.line)


# Apply the seed's known selection pragmas to an eligible form.
def metadata_enabled(node: Form, *, selectable: bool = False) -> bool:
	enabled = True
	# Other metadata entries and unknown pragma payloads stay opaque to Stage 1.
	for entry in node.metadata:
		# Only pragma entries affect selection.
		if entry.tag != ":pragma":
			continue
		# Validate every known directive, including those after an exclusion.
		for directive in entry.operands:
			pragma = form(directive)
			# Each cfg must permit the bootstrap build.
			if pragma.tag == "cfg":
				predicate, = form(pragma, arity=1).operands
				selected = cfg_matches(predicate)
			# Test declarations are excluded from the bootstrap build.
			elif pragma.tag == "test":
				form(pragma, arity=0)
				selected = False
			# Leave the payload of an unknown directive uninterpreted.
			else:
				continue
			# Module roots, declarations, and declaration groups support selection.
			if not selectable:
				raise SourceError(pragma.line)
			enabled = selected and enabled
	return enabled


# Validate metadata on an ordinary operand or statement.
def checked_form(node: Node, tag: str | None = None, arity: int | None = None) -> Form:
	value = form(node, tag, arity)
	metadata_enabled(value)
	return value


# Classify a use by its bare provider while leaving every other operand opaque.
def use_provider(node: Form) -> str | None:
	# An absent, quoted, or malformed provider goes through ordinary use diagnostics.
	if node.operands:
		provider = node.operands[0]
		# Keep an arbitrary well-formed foreign provider available to the skip rule.
		if isinstance(provider, Atom) and not provider.quoted and WORD.fullmatch(provider.text):
			return provider.text
	return None


# Flatten active groups in source order before declaring any names.
def selected_declarations(nodes: tuple[Node, ...], *, skip_host: bool = False) -> Iterator[Form]:
	# Exclude declarations before opening imported files or validating bodies.
	for node in nodes:
		declaration = form(node)
		# Bootstrap discards skipped host providers before interpreting their pragmas.
		if skip_host:
			# A bridge and every member belong to later compilation stages.
			if declaration.tag == "bridge":
				continue
			# Wove edges and C-header includes both participate in bootstrap selection.
			if declaration.tag == "use" and use_provider(declaration) not in (None, "wove-file", "c-header"):
				continue
		# An inactive declaration or group contributes no definitions.
		if not metadata_enabled(declaration, selectable=True):
			continue
		# A group declares its active children in the enclosing module.
		if declaration.tag == "decls":
			yield from selected_declarations(declaration.operands, skip_host=skip_host)
		# Keep active declarations and their source locations intact.
		else:
			yield declaration


# Add the defining file to any line-only error from local parsing or lowering.
@contextmanager
def source_file(filename: str) -> Iterator[None]:
	# Keep a nested import's diagnostic at its original file.
	try:
		yield
	except SourceError as error:
		# Most syntax and type checks only know the local line.
		if error.filename is None:
			error.filename = filename
		raise


# Collect active local declarations and preserve their emission order.
def read_module(
	source: str, filename: str, record_index: int, function_index: int, value_index: int,
	*, entry: bool = False, imports: dict[str, None] | None = None
) -> Module:
	# Parsing and declaration selection belong to the defining file.
	with source_file(filename):
		module = form(parse(source), "module")
		# An excluded file contributes no declarations or edges to the import graph.
		if not metadata_enabled(module, selectable=True):
			# A build still needs its entry module to be active.
			if entry:
				raise SourceError(module.line)
			return Module(filename, function_index)
		records = {}
		function_declarations = {}
		uses = []
		values = {}
		# Select active declarations before looking at any import's operands or path.
		for node in selected_declarations(module.operands, skip_host=True):
			# Collect imports for the dependency walk.
			if node.tag == "use":
				uses.append(node)
			# Collect nominal record names before resolving any field type.
			elif node.tag == "struct":
				name, record = read_record(node, record_index + len(records))
				# A file has one declaration per record name.
				if name in records:
					raise SourceError(node.line)
				records[name] = record
			# Seed module storage only accepts immutable literal declarations.
			elif node.tag == "let":
				name, value = read_module_value(node, value_index + len(values), filename, imports)
				# A file has one module binding per value name.
				if name in values:
					raise SourceError(node.line)
				values[name] = value
			# Collect function names now and check their signatures after imports bind.
			else:
				name = identifier(form(node, "fn").names[0])
				# A file has one declaration per function name in its function namespace.
				if name in function_declarations:
					raise SourceError(node.line)
				function_declarations[name] = node
		# The root supplies the executable entry point even when a provider defines main.
		if entry and "main" not in function_declarations:
			raise SourceError(module.line)
		return Module(filename, function_index, records, function_declarations, uses, records.copy(),
			values=values, syntax=module)


# Resolve record fields and signatures in the defining module's complete type namespace.
def prepare_functions(module: Module, *, entry: bool) -> None:
	# Report errors at the original field or signature in this file.
	with source_file(module.filename):
		# A field can refer to a local or selectively imported Wove record.
		for record in module.records.values():
			# Keep every resolved field in its original layout slot.
			for name, member in record.fields.items():
				# Built-in scalar fields already carry their resolved type.
				if member.kind in SCALAR_TYPES:
					continue
				# An owned-array field uses the same descriptor as a local owner.
				if isinstance(member.kind, Form):
					element, = checked_form(member.kind, OWNED, 1).operands
					kind = BufferType(OWNED, read_owned_element(element, module.type_bindings))
				# Other named fields must refer to declared records.
				else:
					kind = module.type_bindings.get(member.kind)
					# Unsupported field forms cannot contribute a C layout.
					if not isinstance(kind, Record):
						raise SourceError(member.line)
				record.fields[name] = RecordField(kind, member.c_name, member.line)
		# Number only local definitions, in their original declaration order.
		for index, (name, node) in enumerate(module.function_declarations.items(), module.function_index):
			module.functions[name] = read_function(name, node, index, module.type_bindings, entry_module=entry)
	module.bindings.update(module.functions)
	module.value_bindings.update((name, declaration.binding) for name, declaration in module.values.items())


# Decode the seed's local-file provider and its explicit selection list.
def read_use(node: Form) -> tuple[str, Form]:
	provider, source, only = form(node, "use", 3).operands
	# Stage 1 only opens Wove source files.
	if identifier(provider, WORD) != "wove-file":
		raise SourceError(provider.line)
	# The path is a quoted string, including when it contains escaped characters.
	if not isinstance(source, Atom) or not source.quoted:
		raise SourceError(source.line)
	return source.text, checked_form(only, "only")


# Derive the deduplication key and emitted operand from an active C-header use.
def read_header_include(node: Form, directory: Path) -> tuple[str, str]:
	# A C-header provider needs at least its header operand.
	if len(node.operands) < 2:
		raise SourceError(node.line)
	operand = node.operands[1]
	# The outer Wove quotes protect the C search delimiters and path.
	if not isinstance(operand, Atom) or not operand.quoted:
		raise SourceError(operand.line)
	spelling = operand.text
	opener = spelling[:1]
	# Validate the complete literal before emitting an include directive.
	if opener in ('<', '"'):
		closer = ">" if opener == "<" else '"'
		# Only a matched delimiter pair can enclose a nonempty path.
		if not spelling.endswith(closer):
			raise SourceError(operand.line)
		spelling = spelling[1:-1]
	# Match the native include operand's path alphabet.
	if not HEADER_PATH.fullmatch(spelling):
		raise SourceError(operand.line)
	# Angle brackets keep system search with the original checked path.
	if opener == "<":
		include = f"<{spelling}>"
		return include, include

	candidate = directory / spelling
	key = str(candidate)
	anchored = spelling.startswith(("./", "../", "/"))
	# Local files use their resolved containing directory for comparison.
	try:
		# Keep the header basename so file aliases can retain their include context.
		if candidate.is_file():
			anchored = True
			key = os.path.join(os.path.realpath(candidate.parent, strict=True), candidate.name)
	# If the path cannot be inspected, keep the original lookup for Clang.
	except OSError:
		pass
	# Anchor local paths without collapsing parent traversal across symlinks.
	if anchored:
		spelling = str(candidate)
	else:
		key = spelling
	# The containing directory can contain spaces but must fit a C header-name token.
	if any(byte in spelling for byte in '"\r\n\0'):
		raise SourceError(operand.line)
	return f'"{key}"', f'"{spelling}"'


# Index supported native declarations by the same header key used to emit the include.
def native_declaration_index(modules: Iterator[tuple[str, Form]]):
	index = {}
	namespaces = {"type": "type", "struct": "type", "fn": "fn",
		"const": "value", "let": "value", "let-mut": "value"}
	# Sources can extend the same header while retaining its original C spelling.
	for filename, module in modules:
		# Only declared bridge members can supply names to a native use.
		for bridge in c_bridges(module):
			# A bridge with an invalid header cannot match an active include.
			try:
				key, _ = read_header_include(bridge.node, Path(filename).resolve().parent)
			except SourceError:
				continue
			scope = (*bridge.enclosing, bridge.node)
			# Keep selection deferred until the seed actually needs this name.
			for declaration in declarations(bridge.node.operands[2:], scope):
				node = declaration.node
				namespace = namespaces.get(node.tag)
				# Other C members cannot provide seed bindings in this subset.
				if namespace is not None and node.names:
					identity = (key, namespace, node.names[0].text)
					index.setdefault(identity, []).append((filename, declaration))
	return index


class NativeDeclarations:
	# Share selected type identities across every use of the same bridge.
	def __init__(self, index):
		self.index = index
		self.types = {}

	# Resolve a selected declaration under its own active module, group, and target cfg.
	def select(self, key: str, namespace: str, name: str) -> tuple[str, Form] | None:
		# A project declaration encountered after supplied providers can extend the view.
		for filename, declaration in reversed(self.index.get((key, namespace, name), ())):
			# Selection errors belong to the provider's source.
			with source_file(filename):
				node = declaration.node
				# Excluded definitions contribute no seed binding.
				if all(metadata_enabled(part, selectable=True) for part in (*declaration.enclosing, node)):
					return filename, node
		return None

	# Lower a selected type through its C declaration and the active target cfg.
	def resolve_type(self, key: str, name: str) -> NamedType | None:
		identity = key, name
		# A native record can refer to a type already created while resolving its fields.
		if identity in self.types:
			return self.types[identity]
		selected = self.select(key, "type", name)
		# Save misses too, since several functions can use a typedef from one header.
		if selected is None:
			self.types[identity] = None
			return None
		filename, node = selected
		# Type and record fields are checked in the defining provider.
		with source_file(filename):
			spelling, fields = read_native_type(node)
			# A typedef takes its representation and pointer-owner marker from the bridge.
			if fields is None:
				owner = next((pragma for pragma in native_pragmas(node) if pragma.tag == "c_owned"), None)
				storage_drop = None
				pointer_drop = None
				# The simple owner forms name the native destructor, with an explicit marker for storage.
				if owner is not None and len(owner.operands) in (1, 2):
					release, *representation = owner.operands
					drop = identifier(form(release, "drop", 1).operands[0])
					# An implicit representation denotes a native pointer owner.
					if not representation:
						pointer_drop = drop
					elif (isinstance(representation[0], Atom) and not representation[0].quoted
						and representation[0].text == "storage"):
						storage_drop = drop
				kind = NativeType(spelling, owner is not None, storage_drop=storage_drop, pointer_drop=pointer_drop)
				self.types[identity] = kind
				return kind
			record = self.types[identity] = Record(spelling, {}, native=True)
			# Only scalar or native named fields are in the seed's record subset.
			for field, operand in fields.items():
				# Keep other field shapes opaque until the seed needs to lower them.
				if not isinstance(operand, Atom) or operand.quoted:
					continue
				field_type = operand.text
				kind = field_type if field_type in SCALAR_TYPES else self.resolve_type(key, field_type)
				# An unsupported member has no seed field access.
				if kind is not None:
					record.fields[field] = RecordField(kind, field)
			return record

	# Distinguish direct borrowed handles from addresses of caller-owned places.
	def reference(self, key: str, node: Form) -> NativeRef | NativePlaceRef:
		operand, = checked_form(node, arity=1).operands
		name = identifier(operand)
		writable = node.tag == "mut-ref"
		# Built-in scalar references use an ordinary typed pointer.
		if name in SCALAR_TYPES:
			return NativePlaceRef(name, writable)
		kind = self.resolve_type(key, name)
		# Caller-owned storage lends the address of its backing C object.
		if isinstance(kind, NativeType) and kind.storage_drop is not None:
			return NativePlaceRef(NativeStorage(kind), writable)
		# A borrowed pointer handle already carries the pointer ABI in its declared type.
		if isinstance(kind, NativeType) and (kind.pointer_drop is not None or kind.c_name.endswith("*")):
			return NativeRef(kind, writable)
		# Native layouts and integer typedefs lend the address of the actual object.
		if isinstance(kind, (Record, NativeType)):
			return NativePlaceRef(kind, writable)
		raise SourceError(operand.line)

	# Resolve copyable scalar operands using their exact native typedef identity.
	def scalar(self, key: str, node: Node) -> str | NativeType:
		name = identifier(node)
		# Built-in scalars keep the ordinary seed representation.
		if name in SCALAR_TYPES:
			return name
		kind = self.resolve_type(key, name)
		# Owning and pointer values require a separate bridge contract.
		if isinstance(kind, NativeType) and not kind.owned and not kind.c_name.endswith("*"):
			return kind
		raise SourceError(node.line)

	# A bare native pointer owner is transferred into or out of a C call.
	def call_value_type(self, key: str, node: Node) -> Type:
		name = identifier(node)
		# Built-in scalar operands do not resolve through the foreign provider index.
		if name in SCALAR_TYPES:
			return name
		kind = self.resolve_type(key, name)
		# Ordinary scalar forms continue through the same exact type resolver.
		if isinstance(kind, NativeType) and kind.pointer_drop is not None:
			return NativePointerOwner(kind)
		return self.scalar(key, node)

	# Resolve the C signature and adapt only the contracts admitted by the seed.
	def function(self, key: str, node: Form) -> NativeFunction:
		symbol, operands, constants, result_node = read_native_function(node)
		parameters = {}
		# Keep named formal order and distinguish borrowed handles from byte descriptors.
		for name, operand in operands.items():
			# A C handle borrow passes the native value directly.
			if isinstance(operand, Form) and operand.tag in ("shared-ref", "mut-ref"):
				kind = self.reference(key, operand)
			# Byte descriptors need a checked extent or C-string adaptation.
			elif isinstance(operand, Form) and operand.tag in (SHARED, MUT):
				element, = checked_form(operand, operand.tag, 1).operands
				# Only the declared detached vector shape accepts an owned byte row.
				if operand.tag == SHARED and isinstance(element, Form):
					if read_owned_element(element, {}) != _OWNED_BYTE_ROW:
						raise SourceError(element.line)
					kind = _C_STRING_ROWS
				else:
					element_kind = identifier(element)
					# Native byte and descriptor views keep their original element types.
					if element_kind not in (U8, I32):
						raise SourceError(element.line)
					kind = BufferType(operand.tag, element_kind)
			else:
				kind = self.call_value_type(key, operand)
			parameters[name] = kind
		positions = {name: index for index, name in enumerate(parameters)}
		# A hidden constant carries an integer rather than a loan or aggregate.
		if any(parameters[name] not in SCALAR_LIMITS and not isinstance(parameters[name], NativeType)
			for name in constants):
			raise SourceError(node.line)
		extents = {}
		arrays = {}
		strings = set()
		vectors = set()
		null_only = set()
		nullable = set()
		initializers = set()
		snapshot = None
		macro = False
		variadic_start = None
		# Recover adaptation contracts using the names in the original signature.
		for pragma in native_pragmas(node):
			# Selection was applied before the declaration was resolved.
			if pragma.tag == "cfg":
				continue
			# A macro uses its declared scalar inputs but contributes no addressable prototype.
			if pragma.tag == "c_macro":
				form(pragma, arity=0)
				macro = True
				continue
			# A named variadic tail keeps fixed arity and names its first promoted argument.
			if pragma.tag == "c_variadic":
				first, = form(pragma, arity=1).operands
				position = positions.get(identifier(first))
				# C requires a fixed prefix, and this slice admits scalar tail operands.
				if variadic_start is not None or not position or any(
					kind not in SCALAR_TYPES and not isinstance(kind, NativeType)
					for kind in tuple(parameters.values())[position:]
				):
					raise SourceError(pragma.line)
				variadic_start = position
				continue
			# A nullable native reference accepts either its ordinary loan or literal null.
			if pragma.tag == "c_nullable":
				names = tuple(map(identifier, pragma.operands))
				# Only reference formals use this nullable representation in the seed.
				if not names or any(not isinstance(parameters.get(name), (NativeRef, NativePlaceRef)) for name in names):
					raise SourceError(pragma.line)
				nullable.update(names)
				continue
			# An initializer can borrow inactive storage and make it active on success.
			if pragma.tag == "c_init":
				operand, = form(pragma, arity=1).operands
				name = identifier(operand)
				kind = parameters.get(name)
				# Initialization requires a writable storage loan.
				if not isinstance(kind, NativePlaceRef) or not kind.writable or not isinstance(kind.kind, NativeStorage):
					raise SourceError(pragma.line)
				initializers.add(name)
				continue
			# Resolve string and null contracts before lowering any arguments.
			if pragma.tag in ("c_cstr", "c_null"):
				names = tuple(map(identifier, pragma.operands))
				# Both contracts describe byte parameters in this subset.
				if not names or any(parameters.get(name) not in (_BYTE_VIEW, _MUT_BYTE_VIEW) for name in names):
					raise SourceError(pragma.line)
				(strings if pragma.tag == "c_cstr" else null_only).update(names)
				continue
			# Vector operands detach call-scoped C strings from a shared row view.
			if pragma.tag == "c_cstrs":
				names = tuple(map(identifier, pragma.operands))
				# The original array owns one byte buffer for each row.
				if not names or any(parameters.get(name) != _C_STRING_ROWS for name in names):
					raise SourceError(pragma.line)
				vectors.update(names)
				continue
			# A text snapshot returns a nullable byte owner, optionally releasing the native text.
			if pragma.tag == "c_snapshot":
				# One C result carries one snapshot contract.
				if snapshot is not None or not pragma.operands:
					raise SourceError(pragma.line)
				kind, *options = pragma.operands
				# Other snapshot shapes have their own native result layouts.
				if identifier(kind) != "cstr":
					raise SourceError(pragma.line)
				fields = {}
				# Preserve the exact native field or release identifiers for the C checks.
				for option in options:
					field = form(option, arity=1)
					# Unrecognized or repeated options have no seed representation.
					if field.tag not in ("drop", "field", "extent") or field.tag in fields:
						raise SourceError(field.line)
					fields[field.tag] = identifier(field.operands[0])
				# A record snapshot borrows its fields, and an extent must describe one of those fields.
				if ("field" in fields and "drop" in fields) or ("extent" in fields and "field" not in fields):
					raise SourceError(pragma.line)
				snapshot = NativeStringResult(fields.get("drop"), fields.get("field"), fields.get("extent"))
				continue
			# Element extents use a captured integer or a checked literal count.
			if pragma.tag == "c_array":
				buffer, count = form(pragma, arity=2).operands
				name = identifier(buffer)
				# A typed array contract needs a unique non-vector view.
				view = parameters.get(name)
				if not isinstance(view, BufferType) or view == _C_STRING_ROWS or name in arrays:
					raise SourceError(pragma.line)
				# A small decimal literal can be emitted directly as an element count.
				if isinstance(count, Atom) and not count.quoted and re.fullmatch(r"[0-9]{1,10}", count.text):
					width = int(count.text)
					if width > SCALAR_LIMITS[I32][1]:
						raise SourceError(count.line)
					length = str(width)
				else:
					count_name = identifier(count)
					count_kind = parameters.get(count_name)
					if count_kind not in SCALAR_LIMITS and not isinstance(count_kind, NativeType):
						raise SourceError(count.line)
					length = positions[count_name]
				arrays[name] = (positions[name], length)
				continue
			# Byte extents use either one count or a size and count product.
			if pragma.tag != "c_bytes" or len(pragma.operands) not in (2, 3):
				raise SourceError(pragma.line)
			buffer, *factors, count = map(identifier, pragma.operands)
			# The guarded view and all factors retain their exact captured types.
			if parameters.get(buffer) not in (_BYTE_VIEW, _MUT_BYTE_VIEW) or buffer in extents or any(
				parameters.get(name) not in SCALAR_LIMITS and not isinstance(parameters.get(name), NativeType)
				for name in (*factors, count)
			):
				raise SourceError(pragma.line)
			size = positions[factors[0]] if factors else None
			extents[buffer] = (positions[buffer], size, positions[count])
		# A view has exactly one supported representation. Null-only operands still have a string ABI.
		adapted = [*strings, *extents, *arrays, *vectors]
		if not null_only <= strings or len(adapted) != len(set(adapted)) or any(
			isinstance(kind, BufferType) and name not in adapted for name, kind in parameters.items()
		):
			raise SourceError(node.line)
		# Hidden constants supply the fixed native call prefix, before any named variadic operand.
		if variadic_start is not None and any(positions[name] >= variadic_start for name in constants):
			raise SourceError(node.line)
		# Macros in this subset operate on scalar captures without per-call adapters.
		if macro and (snapshot or variadic_start is not None or any(isinstance(kind, (BufferType, NativeRef, NativePlaceRef, NativePointerOwner))
			for kind in parameters.values())):
			raise SourceError(node.line)
		# Copy and optionally release native strings using the runtime's nullable byte owner ABI.
		if snapshot is not None:
			payload, = checked_form(result_node, "optional", 1).operands
			result = read_optional_owner(payload, {})
			# Only byte payloads can own the copied text.
			if result.payload != _OWNED_BYTE_ROW:
				raise SourceError(payload.line)
		else:
			result = VOID if identifier(result_node) == VOID else self.call_value_type(key, result_node)
		# Macro evaluation cannot synthesize a native owner from an unchecked result.
		if macro and isinstance(result, NativePointerOwner):
			raise SourceError(node.line)
		# An initializer's status is the exact i32 result used to activate storage.
		if initializers and (result != I32 or initializers & nullable):
			raise SourceError(node.line)
		return NativeFunction(
			c_name=symbol, params=tuple(parameters.items()), result=result,
			byte_extents=tuple(extents.values()),
			c_strings=tuple(positions[name] for name in parameters if name in strings),
			null_only=frozenset(positions[name] for name in null_only),
			nullable=frozenset(positions[name] for name in nullable),
			initializers=frozenset(positions[name] for name in initializers),
			snapshot=snapshot, macro=macro, variadic_start=variadic_start,
			constants={positions[name]: symbol for name, symbol in constants.items()},
			array_extents=tuple(arrays.values()),
			vectors=tuple(positions[name] for name in parameters if name in vectors),
		)


	# Bind scalar C values and borrowed handles to the actual header expression.
	def value(self, key: str, node: Form) -> Binding:
		operand, = form(node, arity=1).operands
		adaptations = tuple(pragma for pragma in native_pragmas(node) if pragma.tag != "cfg")
		# Vector globals are immutable expressions that produce fresh byte-row owners.
		if adaptations:
			if len(adaptations) != 1 or node.tag != "let":
				raise SourceError(node.line)
			mode, = form(adaptations[0], "c_snapshot", 1).operands
			element, = checked_form(operand, OWNED, 1).operands
			# A borrowed table is not writable through its snapshot binding.
			if identifier(mode) != "cstrs" or read_owned_element(element, {}) != _OWNED_BYTE_ROW:
				raise SourceError(node.line)
			return Binding(Value(_OWNED_STRING_ROWS, identifier(node.names[0])), native=True, snapshot_vector=True)
		writable = node.tag == "let-mut"
		# An immutable handle can be copied without taking ownership of its pointee.
		if isinstance(operand, Form) and operand.tag == "shared-ref":
			kind = self.reference(key, operand)
			# Caller-owned reference globals and writable handle slots have separate contracts.
			if node.tag != "let" or not isinstance(kind, NativeRef):
				raise SourceError(node.line)
		else:
			kind = self.scalar(key, operand)
		return Binding(Value(kind, identifier(node.names[0])), writable=writable, native=True)

	# Bind explicit selections without opening declarations outside the seed's current subset.
	def bind(self, module: Module, use: Form, key: str) -> None:
		# Includes with absent or opaque selectors still pass through to C.
		if len(use.operands) != 3:
			return
		only = use.operands[2]
		# Other selectors stay opaque until the seed lowers their corresponding features.
		if not isinstance(only, Form) or only.tag != "only":
			return
		# Reuse the ordinary namespace and alias spelling.
		for item in only.operands:
			namespace = "fn"
			# Explicit type and value selectors wrap a name or rename.
			if isinstance(item, Form) and item.tag in ("type", "value") and len(item.operands) == 1:
				namespace = item.tag
				item, = item.operands
			original = local = item
			# Renames only affect the importing module's spelling.
			if isinstance(item, Form) and item.tag == "as" and len(item.operands) == 2:
				original, local = item.operands
			# Leave unsupported selectors opaque.
			if not isinstance(original, Atom) or not isinstance(local, Atom):
				continue
			# Explicit type selections already participate in normal Wove signatures.
			if namespace == "type":
				target = self.resolve_type(key, identifier(original))
				bindings, reserved = module.type_bindings, C_TYPE
			else:
				# An opaque include selector does not contribute a function or value binding.
				if original.quoted or not IDENTIFIER.fullmatch(original.text):
					continue
				selected = self.select(key, namespace, original.text)
				# Like Wove imports, a bare selector prefers a function before falling back to a value.
				if selected is None and namespace == "fn":
					namespace = "value"
					selected = self.select(key, namespace, original.text)
				# A bare selection may belong to the header's opaque declaration set.
				if selected is None:
					continue
				_, node = selected
				# Keep other C declaration forms opaque until the seed supports their ABI.
				try:
					target = self.function(key, node) if namespace == "fn" else self.value(key, node)
				except SourceError:
					continue
				bindings, reserved = (module.bindings, module.function_declarations) if namespace == "fn" else (
					module.value_bindings, module.values)
			# An include's unused declarations need not be modeled by the seed.
			if target is not None:
				name = identifier(local)
				# Keep the selected native and Wove names in the same local namespace.
				if name in reserved or name in bindings:
					raise SourceError(local.line)
				bindings[name] = target


# Bind provider declarations directly so aliases retain the defining identity.
def bind_use(only: Form, provider: Module, importer: Module) -> None:
	# Each item selects a definition in its provider, including an immutable value.
	for item in only.operands:
		namespace = item.tag if isinstance(item, Form) and item.tag in ("type", "value") else None
		# Qualified selectors share the same bare-name and alias syntax.
		if namespace is not None:
			item, = checked_form(item, namespace, 1).operands
		# A bare name uses the provider spelling locally.
		if isinstance(item, Atom):
			original = local = item
		# A renamed import carries both the defining and local names.
		else:
			original, local = checked_form(item, "as", 2).operands
		spelling = identifier(original)
		# An explicit type selector always uses the provider's own type definitions.
		if namespace == "type":
			target = provider.records.get(spelling)
			bindings = importer.type_bindings
			reserved = C_TYPE
		# An unqualified selection preserves function precedence at the provider.
		elif namespace is None and spelling in provider.functions:
			target = provider.functions[spelling]
			bindings = importer.bindings
			reserved = importer.function_declarations
		# Value bindings share the selected declaration's existing storage.
		else:
			declaration = provider.values.get(spelling)
			target = declaration.binding if declaration is not None else None
			bindings = importer.value_bindings
			reserved = importer.values
		# Only declarations defined in the provider can be selected.
		if target is None:
			raise SourceError(original.line)
		name = identifier(local)
		# Keep builtins and local declarations distinct from imports in their own namespace.
		if name in bindings or name in reserved:
			raise SourceError(local.line)
		bindings[name] = target


# Read an active provider and distinguish its source errors from failures to open it.
def read_provider(path: Path, filename: str, use: Form) -> str:
	# File and encoding failures have different source origins.
	try:
		return path.read_text(encoding="utf-8")
	except OSError:
		raise SourceError(use.line) from None
	except UnicodeDecodeError as error:
		prefix = error.object[:error.start]
		line = prefix.count(b"\n") + prefix.count(b"\r") - prefix.count(b"\r\n") + 1
		raise SourceError(line, filename) from None


# Identify files across spelling aliases, symbolic links, and hard links.
def file_identity(path: Path) -> tuple[int, int]:
	status = path.stat()
	return status.st_dev, status.st_ino


# Visit active imports, collecting native includes and optional Make prerequisites.
def load_modules(
	root: Module, *, imports: dict[str, None] | None = None, bridge_files: tuple[str, ...] = ()
) -> tuple[list[Module], list[str]]:
	# A file with no uses only needs signature preparation.
	if not root.uses:
		prepare_functions(root, entry=True)
		return [root], [RUNTIME_INCLUDE]
	path = Path(root.filename).resolve()
	# The root text can also be supplied in memory under a prospective filename.
	try:
		root_identity = file_identity(path)
	except FileNotFoundError:
		root_identity = path
	modules = {root_identity: root}
	headers = {RUNTIME_INCLUDE: RUNTIME_INCLUDE}
	active = {root_identity}
	pending = {root_identity: []}
	order = []
	stack = [(root_identity, path, iter(root.uses))]
	record_index = len(root.records)
	function_index = len(root.function_declarations)
	value_index = len(root.values)
	absolute_paths = Path(root.filename).is_absolute()
	# Keep active importers on the stack until their last use has been processed.
	while stack:
		identity, path, uses = stack[-1]
		importer = modules[identity]
		use = next(uses, None)
		# Finish graph discovery in dependency order before resolving signatures.
		if use is None:
			order.append(identity)
			active.remove(identity)
			stack.pop()
			continue
		# An include, edge, or invalid selection is diagnosed in this importer.
		with source_file(importer.filename):
			# An active C header contributes an include and any selected native types.
			if use_provider(use) == "c-header":
				key, include = read_header_include(use, path.parent)
				headers.setdefault(key, include)
				pending[identity].append((use, key))
				continue
			requested, only = read_use(use)
			candidate = path.parent / requested
			# Resolve from the file containing the use and collapse path aliases.
			try:
				target = candidate.resolve(strict=True)
				target_identity = file_identity(target)
			except (OSError, RuntimeError, ValueError):
				raise SourceError(use.line) from None
			# Track the active import and its resolved file before deduplicating modules.
			if imports is not None:
				track_file(imports, candidate, target, absolute_paths)
			# An edge back into the current path is a cycle.
			if target_identity in active:
				raise SourceError(use.line)
			pending[identity].append((only, target_identity))
			# A provider reached again needs no second discovery pass.
			if target_identity in modules:
				continue
			filename = str(target) if absolute_paths else os.path.relpath(target)
			source = read_provider(target, filename, use)
			provider = read_module(source, filename, record_index, function_index, value_index, imports=imports)
			modules[target_identity] = provider
			record_index += len(provider.records)
			function_index += len(provider.function_declarations)
			value_index += len(provider.values)
			active.add(target_identity)
			pending[target_identity] = []
			stack.append((target_identity, target, iter(provider.uses)))

	# Supply compiler inventory declarations before project declarations.
	def declaration_sources() -> Iterator[tuple[str, Form]]:
		# Inventory files contribute their C bridges only.
		for filename in bridge_files:
			with source_file(filename):
				path = Path(filename)
				# The same dependency inventory tracks any supplied bridge source.
				if imports is not None:
					track_file(imports, path, path.resolve(), absolute_paths)
				yield filename, form(parse(path.read_text(encoding="utf-8")), "module")
		# Project bridges come from the same discovered source graph as their users.
		for module in modules.values():
			# Excluded modules contribute no declarations.
			if module.syntax is not None:
				yield module.filename, module.syntax

	native = NativeDeclarations(native_declaration_index(declaration_sources()))
	# The imports of each Wove provider are ready before any importer binds it.
	for identity in order:
		module = modules[identity]
		with source_file(module.filename):
			# Resolve each use in its original source order.
			for selection, provider in pending[identity]:
				# C and Wove providers contribute names from separate declaration indexes.
				if selection.tag == "use":
					native.bind(module, selection, provider)
				else:
					bind_use(selection, modules[provider], module)
			prepare_functions(module, entry=identity == root_identity)
	return list(modules.values()), list(headers.values())


# Emit each Wove record after the records that it contains by value.
def ordered_records(modules: list[Module]) -> Iterator[Record]:
	sources = {record: module.filename for module in modules for record in module.records.values()}
	active = set()
	emitted = set()

	# Follow only named record fields. Scalar members are already defined.
	def visit(record: Record) -> Iterator[Record]:
		active.add(record)
		# A C field by value needs the complete type before the containing definition.
		for member in record.fields.values():
			dependency = member.kind
			# Only Wove definitions in the source graph participate in layout ordering.
			if not isinstance(dependency, Record) or dependency not in sources or dependency in emitted:
				continue
			# A recursive value layout cannot be emitted as a complete C type.
			if dependency in active:
				raise SourceError(member.line, sources[record])
			yield from visit(dependency)
		active.remove(record)
		emitted.add(record)
		yield record

	# Preserve declaration order where no field dependency requires an earlier definition.
	for record in sources:
		# Imported layouts can already be present through a containing record.
		if record not in emitted:
			yield from visit(record)


# Emit a complete C translation unit after the source graph succeeds.
def compile_source(
	source: str, filename: str, *, imports: dict[str, None] | None = None, bridge_files: tuple[str, ...] = ()
) -> str:
	root = read_module(source, filename, 0, 0, 0, entry=True, imports=imports)
	modules, headers = load_modules(root, imports=imports, bridge_files=bridge_files)
	records = list(ordered_records(modules))
	functions = [function for module in modules for function in module.functions.values()]
	values = [value for module in modules for value in module.values.values()]
	retained = (
		*(function.c_name for module in modules[1:] for function in module.functions.values()),
		*(value.binding.value.c for value in values),
	)
	fixed_arrays = {kind for function in functions for _, kind in function.params
		if isinstance(kind, FixedArrayType)}
	fixed_arrays.update(value.binding.value.kind for value in values
		if isinstance(value.binding.value.kind, FixedArrayType))

	bodies = []
	owned_slots = set()
	# Lower bodies before the C preamble so the used inline and owning slot layouts are known.
	for module in modules:
		# Runtime sites and embedded paths belong to the source that defines each function.
		with source_file(module.filename):
			# Emit provider bodies only once, regardless of their local aliases.
			for function in module.functions.values():
				references = retained if function.is_entry else ()
				emitter = FunctionEmitter(function, module, fixed_arrays, owned_slots, imports)
				bodies.extend(emitter.emit(references))
				# Reuse the same borrowed host argument vector as the native compiler.
				if function.is_entry:
					bodies.extend([f"WVRT_HOST_ENTRY({function.c_name})", ""])

	lines = [f"#include {operand}" for operand in headers]
	lines.append("")
	# Give every exact inline shape one ordinary C type before signatures reference it.
	for kind in sorted(fixed_arrays, key=lambda shape: (shape.element, shape.count)):
		lines.append(f"typedef struct {{ {c_type(kind.element)} items[{max(1, kind.count)}]; }} {c_type(kind)};")
	# Separate any generated array types from the record declarations.
	if fixed_arrays:
		lines.append("")
	# Define nested layouts before emitting any functions that use them by value.
	for record in records:
		lines.extend(record_definition(record))
		# An owning slot layout uses the complete record definition.
		if record in owned_slots:
			lines.extend(record_slot_definition(record))
	# Emit each module binding once, ahead of every function body.
	for value in values:
		lines.extend(module_value_definition(value))
	# Separate static storage from function declarations when the file has bindings.
	if values:
		lines.append("")
	lines.extend(signature(function) + ";" for function in functions)
	lines.append("")
	return "\n".join(lines + bodies)
