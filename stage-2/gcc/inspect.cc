// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include "gcc-plugin.h"
#include "plugin-version.h"
#include "tree.h"
#include "wide-int-print.h"

int plugin_is_GPL_compatible;

namespace {
bool first;
bool open;

// Escape text before writing a JSON declaration key or type spelling.
void json_text(const char *text) {
	putchar('"');
	// GCC identifiers may contain multibyte characters. Preserve their UTF-8 bytes.
	for (const unsigned char *byte = reinterpret_cast<const unsigned char *>(text); *byte; ++byte) {
		if (*byte == '"' || *byte == '\\') putchar('\\');
		if (*byte < 32) printf("\\u%04x", *byte);
		else putchar(*byte);
	}
	putchar('"');
}

// Extract a tag or declaration name from GCC's identifier and typedef nodes.
const char *name_of(tree node) {
	if (node && DECL_P(node)) node = DECL_NAME(node);
	return node && TREE_CODE(node) == IDENTIFIER_NODE ? IDENTIFIER_POINTER(node) : nullptr;
}

// Use target type identities to distinguish ranks even when their widths agree.
const char *scalar_spelling(tree type) {
	type = TYPE_MAIN_VARIANT(type);
	if (type == void_type_node) return "void";
	if (type == boolean_type_node) return "_Bool";
	if (type == char_type_node) return "char";
	if (type == signed_char_type_node) return "signed char";
	if (type == unsigned_char_type_node) return "unsigned char";
	if (type == short_integer_type_node) return "short";
	if (type == short_unsigned_type_node) return "unsigned short";
	if (type == integer_type_node) return "int";
	if (type == unsigned_type_node) return "unsigned int";
	if (type == long_integer_type_node) return "long";
	if (type == long_unsigned_type_node) return "unsigned long";
	if (type == long_long_integer_type_node) return "long long";
	if (type == long_long_unsigned_type_node) return "unsigned long long";
	if (type == float_type_node) return "float";
	if (type == double_type_node) return "double";
	if (type == long_double_type_node) return "long double";
	if (TREE_CODE(type) == INTEGER_TYPE && TYPE_PRECISION(type) == 128)
		return TYPE_UNSIGNED(type) ? "unsigned __int128" : "__int128";
	return nullptr;
}

// Write the type forms consumed by Wove's scalar and opaque-pointer readers.
void type_spelling(tree type, unsigned depth = 0) {
	// Stop at invalid GCC tree nodes and bound traversal of nested types.
	if (!type || type == error_mark_node || depth == 64) {
		fputs("<unsupported C type>", stdout);
		return;
	}
	if (TYPE_READONLY(type)) fputs("const ", stdout);
	if (TYPE_VOLATILE(type)) fputs("volatile ", stdout);
	// Spell shapes that the scalar importer must reject, even inside typedefs.
	switch (TREE_CODE(type)) {
	case POINTER_TYPE:
		type_spelling(TREE_TYPE(type), depth + 1);
		fputs(" *", stdout);
		return;
	case ARRAY_TYPE:
		type_spelling(TREE_TYPE(type), depth + 1);
		fputs("[]", stdout);
		return;
	case FUNCTION_TYPE:
		fputs("function ()", stdout);
		return;
	case RECORD_TYPE:
	case UNION_TYPE:
	case ENUMERAL_TYPE: {
		const char *kind = TREE_CODE(type) == RECORD_TYPE ? "struct" :
			(TREE_CODE(type) == UNION_TYPE ? "union" : "enum");
		const char *name = name_of(TYPE_NAME(type));
		printf("%s %s", kind, name ? name : "<anonymous>");
		return;
	}
	default:
		break;
	}
	const char *scalar = scalar_spelling(type);
	const char *name = name_of(TYPE_NAME(TYPE_MAIN_VARIANT(type)));
	fputs(scalar ? scalar : (name ? name : "<unsupported C type>"), stdout);
}

// Names and keywords in these spellings come from C identifiers and fixed tokens.
void type_field(tree type) {
	fputs(",\"type\":{\"qualType\":\"", stdout);
	type_spelling(type);
	fputs("\"}", stdout);
}

// Open an item in the translation unit's top-level declaration array.
void declaration(const char *kind, const char *name) {
	if (!first) putchar(',');
	first = false;
	fputs("{\"kind\":", stdout);
	json_text(kind);
	fputs(",\"name\":", stdout);
	json_text(name);
}

// Adapt GCC's prototype list and variadic sentinel to the existing import schema.
void function_type(tree type) {
	tree args = TYPE_ARG_TYPES(type);
	fputs(",\"type\":{\"qualType\":\"", stdout);
	type_spelling(TREE_TYPE(type));
	fputs(args ? " (void)\"},\"inner\":[" : " ()\"},\"inner\":[", stdout);
	bool comma = false;
	tree param = args;
	// C prototypes terminate their fixed list with void. Variadic lists end at null.
	for (; param && TREE_VALUE(param) != void_type_node; param = TREE_CHAIN(param)) {
		if (comma) putchar(',');
		comma = true;
		fputs("{\"kind\":\"ParmVarDecl\"", stdout);
		type_field(TREE_VALUE(param));
		putchar('}');
	}
	putchar(']');
	if (args && !param) fputs(",\"variadic\":true", stdout);
}

// Keep declaration lookup at file scope so parameters and local variables cannot shadow an import.
void finish_decl(void *event, void *) {
	tree node = static_cast<tree>(event);
	if (!node || !DECL_P(node) || !DECL_NAME(node)) return;
	tree context = DECL_CONTEXT(node);
	if (context && TREE_CODE(context) != TRANSLATION_UNIT_DECL) return;
	const char *name = name_of(node);
	// Function bodies are unnecessary. Only the resolved declaration gets serialized.
	switch (TREE_CODE(node)) {
	case FUNCTION_DECL:
		declaration("FunctionDecl", name);
		function_type(TREE_TYPE(node));
		break;
	case VAR_DECL:
		declaration("VarDecl", name);
		type_field(TREE_TYPE(node));
		break;
	case TYPE_DECL:
		declaration("TypedefDecl", name);
		type_field(TREE_TYPE(node));
		break;
	default:
		return;
	}
	putchar('}');
}

// Serialize completed enum probes using GCC's full-width constant arithmetic.
void finish_type(void *event, void *) {
	tree type = static_cast<tree>(event);
	if (!type || TREE_CODE(type) != ENUMERAL_TYPE || !TYPE_VALUES(type)) return;
	const char *name = name_of(TYPE_NAME(type));
	constexpr char prefix[] = "wvpr_";
	if (!name || strncmp(name, prefix, sizeof(prefix) - 1) != 0) return;
	declaration("EnumDecl", name);
	fputs(",\"inner\":[", stdout);
	bool comma = false;
	// Named probe members are read independently of the enum's chosen storage width.
	for (tree entry = TYPE_VALUES(type); entry; entry = TREE_CHAIN(entry)) {
		const char *member = name_of(TREE_PURPOSE(entry));
		tree value = TREE_VALUE(entry);
		if (value && TREE_CODE(value) == CONST_DECL) value = DECL_INITIAL(value);
		if (!member || !value || TREE_CODE(value) != INTEGER_CST) continue;
		if (comma) putchar(',');
		comma = true;
		fputs("{\"name\":", stdout);
		json_text(member);
		fputs(",\"inner\":[{\"value\":\"", stdout);
		print_dec(wi::to_wide(value), stdout, TYPE_UNSIGNED(TREE_TYPE(value)) ? UNSIGNED : SIGNED);
		fputs("\"}]}", stdout);
	}
	fputs("]}", stdout);
}

// Start the document before parsing any headers.
void start_unit(void *, void *) {
	open = true;
	first = true;
	fputs("{\"inner\":[", stdout);
}

// Syntax-only invocations still run the final plugin callback.
void finish(void *, void *) {
	if (open) fputs("]}\n", stdout);
}
}

// Require headers from the installed compiler before using its internal trees.
int plugin_init(plugin_name_args *info, plugin_gcc_version *version) {
	// The callbacks read GCC's internal tree layouts and require an exact build match.
	if (!plugin_default_version_check(version, &gcc_version)) {
		fputs("wovec: GCC plugin headers do not match the running GCC build\n", stderr);
		return 1;
	}
	register_callback(info->base_name, PLUGIN_START_UNIT, start_unit, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_DECL, finish_decl, nullptr);
	// Inline definitions can be the only declaration in a header.
	register_callback(info->base_name, PLUGIN_FINISH_PARSE_FUNCTION, finish_decl, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH_TYPE, finish_type, nullptr);
	register_callback(info->base_name, PLUGIN_FINISH, finish, nullptr);
	return 0;
}
