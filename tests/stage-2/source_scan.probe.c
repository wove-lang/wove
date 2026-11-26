// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define main wove_fixture_main
#include "source_scan.c"
#undef main

// Match the seed's generated names to the fixture's imported declarations.
#define source_offset wv_seed_fn_6
#define source_kind wv_seed_fn_7
#define scan_token wv_seed_fn_14
#define admit_source wv_seed_fn_15
#define source_diagnostic wv_seed_fn_17
#define source_position wv_seed_fn_18
#define check_source_form wv_seed_fn_21
#define build_source_tree wv_seed_fn_26
#define check_source_metadata wv_seed_fn_47
#define render_source_tree wv_seed_fn_49
#define decode_string wv_seed_fn_81
#define form_parts wv_seed_fn_84
#define read_module_for_target wv_seed_fn_99
#define emit_module wv_seed_fn_141
#define resolve_graph_for_target wv_seed_fn_363
#define default_selection wv_seed_fn_100
#define target_os wv_seed_fn_101
#define target_arch wv_seed_fn_102
#define inspect_callers wv_seed_fn_964
#define run_scalar_graph wv_seed_fn_988
#define discover_graph_tests wv_seed_fn_997
#define inspect_graph_expectations wv_seed_fn_976

typedef wv_seed_record_1 Token;
typedef wv_seed_record_2 SourceDiagnostic;
typedef wv_seed_record_3 SourcePosition;
typedef wv_seed_record_4 FormCheck;
typedef wv_seed_record_5 SourceNode;
typedef wv_seed_record_6 RenderResult;
typedef wv_seed_record_12 DecodeResult;
typedef wv_seed_record_14 FormParts;
typedef wv_seed_record_15 SelectedDeclaration;
typedef wv_seed_record_16 ModuleRead;
typedef wv_seed_record_18 SelectionContext;
typedef wv_seed_record_20 EmitResult;
typedef wv_seed_record_82 EmbedAsset;
typedef wv_seed_record_35 GraphModule;
typedef wv_seed_record_36 GraphBinding;
typedef wv_seed_record_37 GraphUse;
typedef wv_seed_record_38 GraphSelector;
typedef wv_seed_record_39 GraphResult;
typedef wv_seed_record_72 CallSite;
typedef wv_seed_record_44 SuppliedFile;
typedef wv_seed_record_86 CallerInspection;
typedef wv_seed_record_91 GraphOutput;
typedef wv_seed_record_93 DiscoveredTest;
typedef wv_seed_record_94 TestDiscovery;
typedef wv_seed_record_87 ExpectationLink;
typedef wv_seed_record_88 ExpectationResult;
typedef wv_seed_record_89 ExpectationQuery;

enum { MODE_NATIVE = 0, MODE_BOOTSTRAP = 1, MODE_TEST = 2 };

_Static_assert(_Generic(&wove_fixture_main, int (*)(int, char **): 1, default: 0), "source probe entry type");
_Static_assert(_Generic(&source_offset, int32_t (*)(wvrt_slice): 1, default: 0), "source offset type");
_Static_assert(_Generic(&source_kind, uint32_t (*)(wvrt_slice): 1, default: 0), "source kind type");
_Static_assert(_Generic(&scan_token, Token (*)(wvrt_slice, int32_t): 1, default: 0), "scan token type");
_Static_assert(_Generic(&admit_source, int32_t (*)(wvrt_slice): 1, default: 0), "source admission type");
_Static_assert(_Generic(&decode_string, DecodeResult (*)(wvrt_slice, Token, wvrt_mut_slice): 1,
	default: 0), "source decoder type");
_Static_assert(_Generic(((Token){0}).wv_field_0, uint32_t: 1, default: 0), "token kind type");
_Static_assert(_Generic(((Token){0}).wv_field_1, int32_t: 1, default: 0), "token start type");
_Static_assert(_Generic(((Token){0}).wv_field_2, int32_t: 1, default: 0), "token end type");
_Static_assert(_Generic(((Token){0}).wv_field_3, uint32_t: 1, default: 0), "token error code type");

typedef struct {
	const char *label;
	const uint8_t *bytes;
	int32_t length;
	int32_t offset;
	uint32_t kind;
} ScanCase;

static const uint8_t whitespace[] = {32, 9, 13, 10};
static const uint8_t opening[] = {9, 32, 40};
static const uint8_t closing[] = {13, 41, 40};
static const uint8_t atom[] = {10, 120, 40};
static const uint8_t zero_byte[] = {0};
static const uint8_t high_byte[] = {255};
static const uint8_t parent[] = {120, 10, 32, 41, 120};

static const ScanCase cases[] = {
	{"empty", whitespace, 0, 0, 0},
	{"whitespace", whitespace, sizeof(whitespace), 4, 0},
	{"opening parenthesis", opening, sizeof(opening), 2, 40},
	{"closing parenthesis", closing, sizeof(closing), 1, 41},
	{"atom", atom, sizeof(atom), 1, 1},
	{"zero byte", zero_byte, sizeof(zero_byte), 0, 1},
	{"high byte", high_byte, sizeof(high_byte), 0, 1},
};

typedef struct {
	uint32_t kind;
	int32_t start;
	int32_t end;
} TokenSpan;

static const uint8_t token_stream[] = " (fn -42)";
static const uint8_t token_parent[] = "x (fn -42)y";
static const uint8_t semicolon[] = ";";
static const uint8_t delimiters[] = "a;b\"c\"d(e)";

static const TokenSpan stream_tokens[] = {
	{40, 1, 2}, {1, 2, 4}, {1, 5, 8}, {41, 8, 9}, {0, 9, 9},
};
static const TokenSpan atom_tokens[] = {{1, 0, 3}, {0, 3, 3}};
static const TokenSpan semicolon_tokens[] = {{3, 0, 1}, {0, 1, 1}};
static const TokenSpan delimiter_tokens[] = {
	{1, 0, 1}, {3, 1, 2}, {1, 2, 3}, {2, 3, 6}, {1, 6, 7},
	{40, 7, 8}, {1, 8, 9}, {41, 9, 10}, {0, 10, 10},
};

static const uint8_t quoted_stream[] = "\"\"(\"(; ;; )\"\"\\\"\\\\\\/\\b\\f\\n\\r\\t\\u0030\\uAbCd5\")x";
static const uint8_t quoted_parent[] = "p \"x\\u0030\"q";
static const uint8_t bad_escape[] = "\"a\\q)";
static const uint8_t bad_hex[] = "\"\\u03G8)";
static const uint8_t short_hex_quote[] = "\"\\u03\")";
static const uint8_t short_hex[] = "\"\\u03";
static const uint8_t string_lf[] = "\"a\n)";
static const uint8_t string_cr[] = "\"a\r)";
static const uint8_t string_tab[] = "\"a\t)";
static const uint8_t unterminated_string[] = "\"xyz";
static const uint8_t trailing_escape[] = "\"xyz\\";
static const uint8_t lone_quote[] = "\"";
static const uint8_t string_nul[] = {'"', 'a', 0, ')'};

static const TokenSpan quoted_tokens[] = {
	{2, 0, 2}, {40, 2, 3}, {2, 3, 12}, {2, 12, 43}, {41, 43, 44}, {1, 44, 45}, {0, 45, 45},
};

static const struct {
	const char *label;
	wvrt_slice input;
	int32_t end;
} string_errors[] = {
	{"non-hex Unicode digit", {bad_hex, sizeof(bad_hex) - 1, sizeof(uint8_t)}, 6},
	{"closing quote within Unicode escape", {short_hex_quote, sizeof(short_hex_quote) - 1, sizeof(uint8_t)}, 6},
	{"Unicode escape ends at EOF", {short_hex, sizeof(short_hex) - 1, sizeof(uint8_t)}, 5},
	{"raw LF in string", {string_lf, sizeof(string_lf) - 1, sizeof(uint8_t)}, 3},
	{"raw CR in string", {string_cr, sizeof(string_cr) - 1, sizeof(uint8_t)}, 3},
	{"raw tab in string", {string_tab, sizeof(string_tab) - 1, sizeof(uint8_t)}, 3},
	{"unterminated string", {unterminated_string, sizeof(unterminated_string) - 1, sizeof(uint8_t)}, 4},
	{"trailing backslash", {trailing_escape, sizeof(trailing_escape) - 1, sizeof(uint8_t)}, 5},
	{"lone quote", {lone_quote, sizeof(lone_quote) - 1, sizeof(uint8_t)}, 1},
	{"raw NUL in string", {string_nul, sizeof(string_nul), sizeof(uint8_t)}, 3},
};

static const TokenSpan recovered_string_tokens[] = {{3, 0, 4}, {41, 4, 5}, {0, 5, 5}};

static const uint8_t utf8_atom[] = {
	' ', '(', 0xc2, 0x80, 0xe0, 0xa0, 0x80, 0xed, 0x9f, 0xbf,
	0xee, 0x80, 0x80, 0xf0, 0x90, 0x80, 0x80, 0xf4, 0x8f, 0xbf, 0xbf, ')',
};
static const TokenSpan utf8_atom_tokens[] = {{40, 1, 2}, {1, 2, 21}, {41, 21, 22}, {0, 22, 22}};

static const struct {
	const char *label;
	uint8_t bytes[8];
	int32_t length;
	int32_t offset;
} utf8_errors[] = {
	{"isolated continuation", {0x80}, 1, 0},
	{"truncated two-byte sequence", {'a', 0xc2}, 2, 1},
	{"truncated three-byte sequence", {'a', 0xe2, 0x82}, 3, 1},
	{"truncated four-byte sequence", {'a', 0xf0, 0x90, 0x80}, 4, 1},
	{"malformed first continuation", {'a', 0xc2, 'b', 0xff}, 4, 1},
	{"malformed later continuation", {0xc2, 0x80, 'a', 0xf0, 0x90, 0x80, 'b'}, 7, 3},
	{"overlong two-byte sequence", {0xc0, 0xaf}, 2, 0},
	{"overlong three-byte sequence", {0xe0, 0x9f, 0xbf}, 3, 0},
	{"overlong four-byte sequence", {0xf0, 0x8f, 0xbf, 0xbf}, 4, 0},
	{"surrogate code point", {0xed, 0xa0, 0x80}, 3, 0},
	{"code point above U+10FFFF", {0xf4, 0x90, 0x80, 0x80}, 4, 0},
	{"invalid four-byte lead", {0xf5, 0x80, 0x80, 0x80}, 4, 0},
	{"invalid sequence in a string", {'"', 'a', 0xe2, 0x82, '"'}, 5, 2},
};
static const uint8_t utf8_parent[] = {'x', ' ', 0xe2, 0x82, 0xac, 'y'};

static const struct {
	uint32_t code_point;
	uint8_t bytes[3];
	uint8_t width;
} separators[] = {
	{0x0009, {0x09}, 1},
	{0x000a, {0x0a}, 1},
	{0x000b, {0x0b}, 1},
	{0x000c, {0x0c}, 1},
	{0x000d, {0x0d}, 1},
	{0x001c, {0x1c}, 1},
	{0x001d, {0x1d}, 1},
	{0x001e, {0x1e}, 1},
	{0x001f, {0x1f}, 1},
	{0x0020, {0x20}, 1},
	{0x0085, {0xc2, 0x85}, 2},
	{0x00a0, {0xc2, 0xa0}, 2},
	{0x1680, {0xe1, 0x9a, 0x80}, 3},
	{0x2000, {0xe2, 0x80, 0x80}, 3},
	{0x2001, {0xe2, 0x80, 0x81}, 3},
	{0x2002, {0xe2, 0x80, 0x82}, 3},
	{0x2003, {0xe2, 0x80, 0x83}, 3},
	{0x2004, {0xe2, 0x80, 0x84}, 3},
	{0x2005, {0xe2, 0x80, 0x85}, 3},
	{0x2006, {0xe2, 0x80, 0x86}, 3},
	{0x2007, {0xe2, 0x80, 0x87}, 3},
	{0x2008, {0xe2, 0x80, 0x88}, 3},
	{0x2009, {0xe2, 0x80, 0x89}, 3},
	{0x200a, {0xe2, 0x80, 0x8a}, 3},
	{0x2028, {0xe2, 0x80, 0xa8}, 3},
	{0x2029, {0xe2, 0x80, 0xa9}, 3},
	{0x202f, {0xe2, 0x80, 0xaf}, 3},
	{0x205f, {0xe2, 0x81, 0x9f}, 3},
	{0x3000, {0xe3, 0x80, 0x80}, 3},
};

static const uint8_t nonseparators[] = {
	0xef, 0xbb, 0xbf, 'a', 0xe2, 0x80, 0x8b, 0xe2, 0x80, 0xaa, 'b',
};
static const TokenSpan nonseparator_tokens[] = {{1, 0, 11}, {0, 11, 11}};

// Compare the full offset and kind returned by the compiled Wove scanner.
static void check(const char *label, wvrt_slice input, int32_t offset, uint32_t kind) {
	int32_t actual_offset = source_offset(input);
	uint32_t actual_kind = source_kind(input);
	// Show both return values for a failing source case.
	if (actual_offset != offset || actual_kind != kind) {
		fprintf(stderr, "%s: got offset %" PRId32 " and kind %" PRIu32
			", expected offset %" PRId32 " and kind %" PRIu32 "\n",
			label, actual_offset, actual_kind, offset, kind);
		exit(EXIT_FAILURE);
	}
}

// Check the admission result before the fixture can proceed to tokenization.
static void check_utf8(const char *label, wvrt_slice input, int32_t expected) {
	int32_t offset = admit_source(input);
	if (offset != expected) {
		fprintf(stderr, "%s: UTF-8 admission returned %" PRId32 ", expected %" PRId32 "\n",
			label, offset, expected);
		exit(EXIT_FAILURE);
	}
}

// Pass the original bytes and coordinates through after one successful admission.
static wvrt_slice admit_view(const char *label, wvrt_slice input) {
	check_utf8(label, input, -1);
	return input;
}

// Read the scanner record through the fixture's named fields.
static TokenSpan read_token(wvrt_slice input, int32_t cursor) {
	const Token token = scan_token(input, cursor);
	return (TokenSpan){token.wv_field_0, token.wv_field_1, token.wv_field_2};
}

// Check all three record fields and return the cursor for the next scan.
static int32_t check_token(const char *label, wvrt_slice input, int32_t cursor, TokenSpan expected) {
	const TokenSpan token = read_token(input, cursor);
	if (token.kind != expected.kind || token.start != expected.start || token.end != expected.end) {
		fprintf(stderr, "%s at %" PRId32 ": got token %" PRIu32 " [%" PRId32 ", %" PRId32
			"), expected %" PRIu32 " [%" PRId32 ", %" PRId32 ")\n",
			label, cursor, token.kind, token.start, token.end, expected.kind, expected.start, expected.end);
		exit(EXIT_FAILURE);
	}
	return token.end;
}

// Admit a view before walking its tokens, including a single-token check.
static void check_tokens(const char *label, wvrt_slice input, const TokenSpan *tokens, size_t count) {
	input = admit_view(label, input);
	int32_t cursor = 0;
	for (size_t i = 0; i < count; ++i) {
		cursor = check_token(label, input, cursor, tokens[i]);
	}
}

// Compare the exact byte spans on both sides of every seed separator.
static void check_whitespace(void) {
	for (size_t i = 0; i < sizeof(separators) / sizeof(separators[0]); ++i) {
		char label[32];
		snprintf(label, sizeof(label), "separator U+%04" PRIX32, separators[i].code_point);
		uint8_t bytes[5] = {'a'};
		memcpy(bytes + 1, separators[i].bytes, separators[i].width);
		int32_t right = 1 + separators[i].width;
		int32_t end = right + 1;
		bytes[right] = 'b';
		wvrt_slice input = {bytes, end, sizeof(uint8_t)};
		const TokenSpan tokens[] = {{1, 0, 1}, {1, right, end}, {0, end, end}};
		check_tokens(label, input, tokens, sizeof(tokens) / sizeof(tokens[0]));

		// The same bytes inside quotes follow the string's raw control-byte rule.
		bytes[0] = '"';
		bytes[right] = '"';
		if (separators[i].code_point < 0x20) {
			check_tokens(label, input, &(TokenSpan){3, 0, 2}, 1);
		} else {
			const TokenSpan quoted[] = {{2, 0, end}, {0, end, end}};
			check_tokens(label, input, quoted, sizeof(quoted) / sizeof(quoted[0]));
		}
	}
}

// Map a view-relative result to file coordinates and emit its numeric fields.
static void write_span(wvrt_slice parent, int32_t view_start, uint32_t value, int32_t start, int32_t end,
	uint32_t error) {
	const SourcePosition position = source_position(parent, view_start, start);
	printf("%" PRIu32 "\t%" PRId32 "\t%" PRId32 "\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu32 "\n",
		value, start, end, error, position.wv_field_0, position.wv_field_1);
}

// Report a body diagnostic's related span in the same source and coordinate space.
static void write_related_span(wvrt_slice parent, int32_t view_start, int32_t file, int32_t start, int32_t end) {
	if (start < 0) { return; }
	const SourcePosition position = source_position(parent, view_start, start);
	printf("related\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRIu32 "\t%" PRIu32 "\n",
		file, start, end, position.wv_field_0, position.wv_field_1);
}

// Check UTF-8 once, then advance by each token's exclusive end.
static int dump_tokens(wvrt_slice parent, wvrt_slice input, int32_t start) {
	const SourceDiagnostic diagnostic = source_diagnostic(input);
	if (diagnostic.wv_field_0 != 0) {
		write_span(parent, start, 3, diagnostic.wv_field_1, diagnostic.wv_field_2, diagnostic.wv_field_0);
		return EXIT_FAILURE;
	}

	int32_t cursor = 0;
	for (;;) {
		const Token token = scan_token(input, cursor);
		write_span(parent, start, token.wv_field_0, token.wv_field_1, token.wv_field_2, token.wv_field_3);
		if (token.wv_field_0 == 0) {
			return EXIT_SUCCESS;
		}
		if (token.wv_field_3 != 0) {
			return EXIT_FAILURE;
		}
		if (token.wv_field_2 <= cursor) {
			fputs("token scan did not advance\n", stderr);
			return EXIT_FAILURE;
		}
		cursor = token.wv_field_2;
	}
}

// Emit one form check result with its root or error span.
static int dump_form(wvrt_slice parent, wvrt_slice input, int32_t start) {
	const FormCheck result = check_source_form(input);
	write_span(parent, start, result.wv_field_0, result.wv_field_1, result.wv_field_2, result.wv_field_3);
	return result.wv_field_3 == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Emit a completed node's links with the position of its first byte in the parent.
static void write_source_node(wvrt_slice parent, int32_t view_start, uint32_t index, SourceNode node) {
	const SourcePosition position = source_position(parent, view_start, node.wv_field_1);
	printf("%" PRIu32 "\t%" PRIu32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32
		"\t%" PRIu32 "\t%" PRIu32 "\n", index, node.wv_field_0, node.wv_field_1, node.wv_field_2,
		node.wv_field_3, node.wv_field_4, node.wv_field_5, position.wv_field_0, position.wv_field_1);
}

// Build into the caller's buffer and verify that all unused storage retains its sentinel.
static int dump_tree(wvrt_slice parent, wvrt_slice input, int32_t start, int32_t capacity, bool metadata) {
	const FormCheck checked = check_source_form(input);
	if (capacity < 0) {
		capacity = (int32_t)checked.wv_field_0;
	}
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *storage = wvrt_array_new(capacity, sizeof(SourceNode), _Alignof(SourceNode), site);
	unsigned char *bytes = storage->wvrt_items;
	size_t size = (size_t)capacity * sizeof(SourceNode);
	memset(bytes, 0xa5, size);
	const wvrt_mut_slice nodes = {bytes, capacity, sizeof(SourceNode)};
	const FormCheck built = build_source_tree(input, checked, nodes);
	uint32_t used = built.wv_field_3 == 0 ? built.wv_field_0 : 0;
	if (used > (uint32_t)capacity) {
		fputs("source tree returned more rows than fit in storage\n", stderr);
		wvrt_array_drop(storage);
		return EXIT_FAILURE;
	}
	for (size_t offset = (size_t)used * sizeof(SourceNode); offset < size; ++offset) {
		if (bytes[offset] != 0xa5) {
			fputs("source tree modified unused storage\n", stderr);
			wvrt_array_drop(storage);
			return EXIT_FAILURE;
		}
	}

	const wvrt_slice shared_nodes = {bytes, capacity, sizeof(SourceNode)};
	const FormCheck result = metadata ? check_source_metadata(input, built, shared_nodes) : built;
	write_span(parent, start, result.wv_field_0, result.wv_field_1, result.wv_field_2, result.wv_field_3);
	const SourceNode *rows = storage->wvrt_items;
	if (result.wv_field_3 == 0) {
		for (uint32_t index = 0; index < used; ++index) {
			write_source_node(parent, start, index, rows[index]);
		}
	}
	wvrt_array_drop(storage);
	return result.wv_field_3 == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Allocate an interior writable view with one sentinel byte at each edge.
static uint8_t *guarded_output(int32_t capacity) {
	const size_t size = (size_t)capacity + 2;
	uint8_t *storage = malloc(size);
	if (storage == NULL) {
		fputs("could not allocate output buffer\n", stderr);
		return NULL;
	}
	memset(storage, 0xa5, size);
	return storage;
}

// Check the reported count, both guards, and every unwritten byte.
static bool output_intact(const uint8_t *storage, int32_t capacity, int32_t written) {
	if (written < 0 || written > capacity || storage[0] != 0xa5) {
		fputs("write exceeded the output view\n", stderr);
		return false;
	}
	// Start after the written prefix and include the right guard byte.
	for (size_t offset = (size_t)written + 1; offset < (size_t)capacity + 2; ++offset) {
		if (storage[offset] != 0xa5) {
			fputs("unused output bytes were modified\n", stderr);
			return false;
		}
	}
	return true;
}

// Render into an interior slice and check the guards before reporting bytes.
static int dump_render(wvrt_slice input, wvrt_slice nodes, int32_t capacity) {
	if (capacity < 0) {
		capacity = input.wvrt_length > INT32_MAX / 2 ? INT32_MAX : input.wvrt_length * 2;
	}
	uint8_t *storage = guarded_output(capacity);
	if (storage == NULL) {
		return EXIT_FAILURE;
	}
	const wvrt_mut_slice output = {storage + 1, capacity, sizeof(uint8_t)};
	const RenderResult result = render_source_tree(input, nodes, output);
	const int32_t written = result.wv_field_0;
	if (!output_intact(storage, capacity, written)) {
		free(storage);
		return EXIT_FAILURE;
	}
	printf("%" PRId32 "\t%" PRIu32 "\n", written, result.wv_field_1);
	const size_t emitted = fwrite(storage + 1, 1, (size_t)written, stdout);
	free(storage);
	return result.wv_field_1 == 0 && emitted == (size_t)written ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Check the input and decode its first token into a guarded output view.
static int dump_decode(wvrt_slice parent, wvrt_slice input, int32_t start, int32_t capacity) {
	const SourceDiagnostic diagnostic = source_diagnostic(input);
	if (diagnostic.wv_field_0 != 0) {
		write_span(parent, start, 0, diagnostic.wv_field_1, diagnostic.wv_field_2, diagnostic.wv_field_0);
		return EXIT_FAILURE;
	}
	const Token token = scan_token(input, 0);
	if (token.wv_field_3 != 0) {
		write_span(parent, start, 0, token.wv_field_1, token.wv_field_2, token.wv_field_3);
		return EXIT_FAILURE;
	}
	if (token.wv_field_0 != 2) {
		fputs("expected a string token\n", stderr);
		return EXIT_FAILURE;
	}
	if (capacity < 0) {
		capacity = input.wvrt_length;
	}
	uint8_t *storage = guarded_output(capacity);
	if (storage == NULL) {
		return EXIT_FAILURE;
	}
	const wvrt_mut_slice output = {storage + 1, capacity, sizeof(uint8_t)};
	const DecodeResult result = decode_string(input, token, output);
	const int32_t written = result.wv_field_0;
	if (!output_intact(storage, capacity, written)) {
		free(storage);
		return EXIT_FAILURE;
	}
	write_span(parent, start, (uint32_t)written, result.wv_field_1, result.wv_field_2, result.wv_field_3);
	const size_t emitted = fwrite(storage + 1, 1, (size_t)written, stdout);
	free(storage);
	return result.wv_field_3 == 0 && emitted == (size_t)written ? EXIT_SUCCESS : EXIT_FAILURE;
}

// Build and validate a source tree for consumers that require checked rows.
static wvrt_array *checked_source_tree(wvrt_slice input, FormCheck *result) {
	const FormCheck checked = check_source_form(input);
	if (checked.wv_field_3 != 0) {
		*result = checked;
		return NULL;
	}
	const int32_t rows = (int32_t)checked.wv_field_0;
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *storage = wvrt_array_new(rows, sizeof(SourceNode), _Alignof(SourceNode), site);
	const wvrt_mut_slice writable = {storage->wvrt_items, rows, sizeof(SourceNode)};
	const wvrt_slice nodes = {storage->wvrt_items, rows, sizeof(SourceNode)};
	const FormCheck built = build_source_tree(input, checked, writable);
	*result = check_source_metadata(input, built, nodes);
	return storage;
}

// Build and validate the original tree before supplying it to the renderer.
static int render_view(wvrt_slice input, int32_t capacity) {
	FormCheck checked;
	wvrt_array *storage = checked_source_tree(input, &checked);
	int status = EXIT_FAILURE;
	// A malformed source keeps its original diagnostic.
	if (checked.wv_field_3 != 0) {
		printf("0\t%" PRIu32 "\n", checked.wv_field_3);
	} else {
		const wvrt_slice nodes = {storage->wvrt_items, storage->wvrt_length, sizeof(SourceNode)};
		status = dump_render(input, nodes, capacity);
	}
	wvrt_array_drop(storage);
	return status;
}

// Emit a form's index and the starting indices of its checked child views.
static void write_form_parts(wvrt_slice input, wvrt_slice nodes, int32_t index) {
	const SourceNode *rows = nodes.wvrt_items;
	const FormParts parts = form_parts(input, nodes, rows[index]);
	printf("%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\t%" PRId32 "\n",
		index, parts.wv_field_0, parts.wv_field_1, parts.wv_field_2, parts.wv_field_3);
}

// Read one module and report the original row indices of its active declarations.
static int dump_module(wvrt_slice parent, wvrt_slice input, int32_t start, int32_t capacity, SelectionContext context) {
	FormCheck checked;
	wvrt_array *storage = checked_source_tree(input, &checked);
	const wvrt_slice nodes = {
		storage != NULL ? storage->wvrt_items : NULL,
		storage != NULL ? storage->wvrt_length : 0,
		sizeof(SourceNode),
	};
	if (capacity < 0) {
		capacity = nodes.wvrt_length;
	}
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *selected = wvrt_array_new(capacity, sizeof(SelectedDeclaration), _Alignof(SelectedDeclaration), site);
	const size_t size = (size_t)capacity * sizeof(SelectedDeclaration);
	unsigned char *bytes = selected->wvrt_items;
	memset(bytes, 0xa5, size);
	const wvrt_mut_slice output = {bytes, capacity, sizeof(SelectedDeclaration)};
	const ModuleRead result = read_module_for_target(input, nodes, checked, context, output);
	const int32_t count = result.wv_field_1;
	int status = EXIT_FAILURE;
	// Report rows only when the count and untouched destination tail are valid.
	if (count < 0 || count > capacity) {
		fputs("module reader exceeded the output view\n", stderr);
	} else {
		size_t offset = (size_t)count * sizeof(SelectedDeclaration);
		while (offset < size && bytes[offset] == 0xa5) {
			++offset;
		}
		if (offset != size) {
			fputs("module reader modified unused output rows\n", stderr);
		} else {
			write_span(parent, start, (uint32_t)count, result.wv_field_3, result.wv_field_4, result.wv_field_5);
			printf("%" PRId32 "\t%d\n", result.wv_field_0, result.wv_field_2);
			if (result.wv_field_0 >= 0) {
				const SourceNode *rows = nodes.wvrt_items;
				const SelectedDeclaration *declarations = selected->wvrt_items;
				write_form_parts(input, nodes, rows[0].wv_field_4);
				for (int32_t index = 0; index < count; ++index) {
					write_form_parts(input, nodes, declarations[index].wv_field_0);
				}
			}
			status = result.wv_field_5 == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
		}
	}
	wvrt_array_drop(selected);
	wvrt_array_drop(storage);
	return status;
}

typedef struct {
	wvrt_array *storage;
	wvrt_array *selected;
	wvrt_slice nodes;
	wvrt_slice active;
	ModuleRead module;
} CheckedModule;

// Prepare the same checked source and declaration views for compilation and inspection.
static CheckedModule checked_module(wvrt_slice input, SelectionContext context) {
	FormCheck checked;
	wvrt_array *storage = checked_source_tree(input, &checked);
	const wvrt_slice nodes = {
		storage != NULL ? storage->wvrt_items : NULL,
		storage != NULL ? storage->wvrt_length : 0,
		sizeof(SourceNode),
	};
	const wvrt_location site = {__FILE__, __LINE__, 1};
	const int32_t rows = nodes.wvrt_length;
	wvrt_array *selected = wvrt_array_new(rows, sizeof(SelectedDeclaration), _Alignof(SelectedDeclaration), site);
	const wvrt_mut_slice declarations = {selected->wvrt_items, rows, sizeof(SelectedDeclaration)};
	const ModuleRead module = read_module_for_target(input, nodes, checked, context, declarations);
	const wvrt_slice active = {selected->wvrt_items, module.wv_field_1, sizeof(SelectedDeclaration)};
	return (CheckedModule){storage, selected, nodes, active, module};
}

// Retire the arrays that own the checked module's borrowed views.
static void drop_checked_module(CheckedModule source) {
	wvrt_array_drop(source.selected);
	wvrt_array_drop(source.storage);
}

// Compile a checked module into a guarded C output view and report its result span.
static int dump_c(wvrt_slice parent, wvrt_slice input, int32_t start, int32_t capacity, SelectionContext context) {
	const CheckedModule source = checked_module(input, context);
	// Allow ordinary local helper modules without a fixture-supplied byte limit.
	if (capacity < 0) {
		capacity = 4096;
	}
	uint8_t *bytes = guarded_output(capacity);
	int status = EXIT_FAILURE;
	// Check both guards and the unwritten tail before exposing emitted bytes.
	if (bytes != NULL) {
		const wvrt_mut_slice output = {bytes + 1, capacity, sizeof(uint8_t)};
		const EmitResult result = emit_module(input, source.nodes, source.module, source.active, 0, start,
			(wvrt_mut_slice){NULL, 0, sizeof(EmbedAsset)}, false, output);
		const int32_t count = result.wv_field_0;
		if (output_intact(bytes, capacity, count)) {
			write_span(parent, start, (uint32_t)count, result.wv_field_1, result.wv_field_2, result.wv_field_3);
			write_related_span(parent, start, 0, result.wv_field_4, result.wv_field_5);
			const size_t emitted = fwrite(bytes + 1, 1, (size_t)count, stdout);
			status = result.wv_field_3 == 0 && emitted == (size_t)count ? EXIT_SUCCESS : EXIT_FAILURE;
		}
		free(bytes);
	}
	drop_checked_module(source);
	return status;
}

typedef struct {
	wvrt_slice bytes;
	wvrt_slice nodes;
	const SuppliedFile *files;
} CallerSources;

// Recover a name from its checked declaration row. A graph endpoint selects its own source.
static void write_function_ref(const char *tag, CallerSources source, int32_t file, int32_t declaration) {
	wvrt_array *storage = NULL;
	// The graph supplies canonical file rows plus original local declaration indices.
	if (source.files != NULL) {
		const SuppliedFile input = source.files[file];
		const wvrt_location site = {__FILE__, __LINE__, 1};
		source.bytes = wvrt_slice_range_u8(source.bytes.wvrt_items, source.bytes.wvrt_length,
			input.wv_field_3, input.wv_field_4, site);
		FormCheck checked;
		storage = checked_source_tree(source.bytes, &checked);
		source.nodes = (wvrt_slice){storage->wvrt_items, storage->wvrt_length, sizeof(SourceNode)};
	}
	const SourceNode *rows = source.nodes.wvrt_items;
	const FormParts parts = form_parts(source.bytes, source.nodes, rows[declaration]);
	const SourceNode name = rows[parts.wv_field_3];
	const int32_t start = name.wv_field_1;
	const int width = (int)(name.wv_field_2 - start);
	printf("(%s %" PRId32 " %" PRId32 " %.*s)", tag, file, declaration,
		width, (const char *)source.bytes.wvrt_items + start);
	wvrt_array_drop(storage);
}

// Render a checked callers list as a metadata prefix on the inspected declaration.
static void write_callers(CallerSources source, int32_t file, int32_t declaration, int32_t count,
		const CallSite *rows) {
	fputs("(declaration\n\t(:callers", stdout);
	// Render every call row in source order, with original declaration identities and offsets.
	for (int32_t i = 0; i < count; ++i) {
		const CallSite site = rows[i];
		fputs("\n\t\t(call-site ", stdout);
		write_function_ref("caller", source, site.wv_field_0, site.wv_field_1);
		fputc(' ', stdout);
		write_function_ref("callee", source, site.wv_field_2, site.wv_field_3);
		printf(" (span %" PRId32 " %" PRId32 " %" PRId32 "))", site.wv_field_0, site.wv_field_4, site.wv_field_5);
	}
	fputs(")\n\t", stdout);
	write_function_ref("identity", source, file, declaration);
	fputs(")\n", stdout);
}

// Inspect a selected name and check that a short result preserves the supplied rows.
static int dump_callers(wvrt_slice parent, wvrt_slice input, int32_t start, const char *name, int32_t capacity,
	bool entry_module, SelectionContext context) {
	const CheckedModule source = checked_module(input, context);
	if (capacity < 0) {
		capacity = source.nodes.wvrt_length;
	}
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *storage = wvrt_array_new(capacity, sizeof(CallSite), _Alignof(CallSite), site);
	const size_t size = (size_t)capacity * sizeof(CallSite);
	unsigned char *bytes = storage->wvrt_items;
	memset(bytes, 0xa5, size);
	const wvrt_mut_slice output = {storage->wvrt_items, capacity, sizeof(CallSite)};
	const wvrt_slice target = {name, (int32_t)strlen(name), sizeof(uint8_t)};
	const CallerInspection result = inspect_callers(input, source.nodes, source.module, source.active, 0, start,
		entry_module, target, output);

	int status = EXIT_FAILURE;
	const int32_t count = result.wv_field_1;
	// A failed inspection and every unused result row must retain the sentinel bytes.
	if (count < 0 || count > capacity) {
		fputs("caller inspection exceeded the output view\n", stderr);
	} else {
		size_t offset = (size_t)count * sizeof(CallSite);
		while (offset < size && bytes[offset] == 0xa5) {
			++offset;
		}
		if (offset != size) {
			fputs("caller inspection modified unused output rows\n", stderr);
		} else {
			write_span(parent, start, (uint32_t)count, result.wv_field_2, result.wv_field_3, result.wv_field_4);
			write_related_span(parent, start, result.wv_field_5, result.wv_field_6, result.wv_field_7);
			if (result.wv_field_4 == 0) {
				write_callers((CallerSources){input, source.nodes, NULL}, result.wv_field_5,
					result.wv_field_0, count, storage->wvrt_items);
				status = EXIT_SUCCESS;
			}
		}
	}
	wvrt_array_drop(storage);
	drop_checked_module(source);
	return status;
}

// Load the bytes unchanged so newline and UTF-8 offsets survive into the probe.
static uint8_t *read_source(const char *filename, int32_t *length) {
	FILE *input = fopen(filename, "rb");
	if (input == NULL) {
		return NULL;
	}
	long size = fseek(input, 0, SEEK_END) == 0 ? ftell(input) : -1;
	if (size < 0 || size > INT32_MAX || fseek(input, 0, SEEK_SET) != 0) {
		fclose(input);
		return NULL;
	}
	uint8_t *bytes = malloc(size == 0 ? 1 : (size_t)size);
	if (bytes != NULL && fread(bytes, 1, (size_t)size, input) != (size_t)size) {
		free(bytes);
		bytes = NULL;
	}
	fclose(input);
	*length = (int32_t)size;
	return bytes;
}

// Keep file subrange arguments in the scanner's signed byte-offset domain.
static bool read_offset(const char *text, int32_t *offset) {
	char *rest;
	errno = 0;
	long value = strtol(text, &rest, 10);
	if (errno != 0 || rest == text || *rest != '\0' || value < 0 || value > INT32_MAX) {
		return false;
	}
	*offset = (int32_t)value;
	return true;
}

// Append a fixture byte span after checking that it fits the shared offset domain.
static bool append_graph_bytes(uint8_t **arena, int32_t *length, const void *bytes, size_t width) {
	if (width > (size_t)(INT32_MAX - *length)) {
		return false;
	}
	if (width == 0) {
		return true;
	}
	uint8_t *grown = realloc(*arena, (size_t)*length + width);
	if (grown == NULL) {
		return false;
	}
	memcpy(grown + *length, bytes, width);
	*arena = grown;
	*length += (int32_t)width;
	return true;
}

// Pack virtual path rows and raw fixture files into the graph's immutable arena.
static bool read_graph_files(char **args, SuppliedFile *files, int32_t count, uint8_t **arena, int32_t *length) {
	// Alias rows carry a path without a second copy of the source.
	for (int32_t i = 0; i < count; ++i, args += 3) {
		const char *path = args[1];
		int32_t identity;
		if (!read_offset(args[0], &identity) || identity >= count || path[0] != '/' || path[1] == '\0') {
			return false;
		}
		const int32_t path_start = *length;
		if (!append_graph_bytes(arena, length, path, strlen(path))) {
			return false;
		}
		const int32_t source_start = *length;
		if (i == identity) {
			int32_t width;
			uint8_t *bytes = read_source(args[2], &width);
			const bool added = bytes != NULL && append_graph_bytes(arena, length, bytes, (size_t)width);
			free(bytes);
			if (!added) {
				return false;
			}
		}
		files[i] = (SuppliedFile){identity, path_start, source_start, source_start, *length, false};
	}
	// Every path points directly to its canonical file row.
	for (int32_t i = 0; i < count; ++i) {
		const int32_t identity = files[i].wv_field_0;
		if (files[identity].wv_field_0 != identity) {
			return false;
		}
	}
	return true;
}

// Append raw asset payloads without exposing their paths as graph modules.
static bool read_graph_assets(char **args, SuppliedFile *assets, int32_t count, uint8_t **arena, int32_t *length) {
	// Keep each supplied virtual path and its exact byte extent in the shared arena.
	for (int32_t i = 0; i < count; ++i, args += 3) {
		const char *path = args[1];
		const int32_t start = *length;
		// An asset row has no source-module identity and cannot be an alias row.
		if (strcmp(args[0], "--asset") != 0 || path[0] != '/' || path[1] == '\0' ||
				!append_graph_bytes(arena, length, path, strlen(path))) {
			return false;
		}
		const int32_t payload = *length;
		int32_t width;
		uint8_t *bytes = read_source(args[2], &width);
		const bool added = bytes != NULL && append_graph_bytes(arena, length, bytes, (size_t)width);
		free(bytes);
		if (!added) { return false; }
		assets[i] = (SuppliedFile){i, start, payload, payload, *length, false};
	}
	return true;
}

// Expose reached module rows and binding origins with spellings from their owning files.
static void write_graph(const uint8_t *arena, const SuppliedFile *files, int32_t count,
		const GraphModule *modules, const GraphBinding *bindings, GraphResult result) {
	printf("ok %" PRId32 " %" PRId32 "\n", result.wv_field_0, result.wv_field_1);
	// Emit each initialized identity row once, in table order.
	for (int32_t i = 0; i < count; ++i) {
		const GraphModule row = modules[i];
		if (row.wv_field_0 != 0) {
			printf("module %" PRId32 " %" PRId32 " %" PRId32 " %d %" PRIu32 "\n",
				i, row.wv_field_1, row.wv_field_2, row.wv_field_6, row.wv_field_0);
		}
	}
	// Report the original name span and the target declaration's exact node index.
	for (int32_t i = 0; i < result.wv_field_1; ++i) {
		const GraphBinding row = bindings[i];
		const int32_t owner = row.wv_field_0;
		const int32_t start = row.wv_field_1;
		const int32_t end = row.wv_field_2;
		const char *name = (const char *)arena + files[owner].wv_field_3 + start;
		const char *kind = row.wv_field_5 == 0 ? "binding" : "type-binding";
		if (row.wv_field_5 == 2) { kind = "value-binding"; }
		// Preserve the separate namespace when exposing C header bindings.
		if (row.wv_field_6 == 8) {
			kind = row.wv_field_5 == 0 ? "c-binding" : "c-type-binding";
		}
		printf("%s %" PRId32 " %.*s %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
			kind, owner, (int)(end - start), name, row.wv_field_3, row.wv_field_4, start, end);
		// The declared header, native symbol, and signature all use defining-file offsets.
		if (row.wv_field_6 == 8) {
			printf("c-target %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
				row.wv_field_7, row.wv_field_8, row.wv_field_9, row.wv_field_10, row.wv_field_11, row.wv_field_12);
			// A pointer contract keeps the defining type and destructor, even for an aliased function.
			if (row.wv_field_13 > 0 && row.wv_field_14 >= 0) {
				printf("c-owner %" PRId32 " %" PRId32 " %" PRId32 "\n",
					row.wv_field_13, row.wv_field_14, row.wv_field_15);
			} else if (row.wv_field_13 > 0) {
				printf("c-integer %" PRId32 " %" PRId32 " %" PRId32 "\n",
					row.wv_field_13, row.wv_field_16, row.wv_field_17);
			}
		}
	}
}

// Report a graph diagnostic in the original file's own byte and coordinate space.
static void write_graph_error(wvrt_slice arena, const SuppliedFile *files, uint32_t error,
		int32_t file, int32_t start, int32_t end, int32_t related_start, int32_t related_end) {
	// Path row failures can occur before there is a source file for the diagnostic.
	if (file < 0) {
		printf("error %" PRIu32 " -1 0 0 0 0\n", error);
		return;
	}
	const SuppliedFile row = files[file];
	const wvrt_location site = {__FILE__, __LINE__, 1};
	const wvrt_slice source = wvrt_slice_range_u8(arena.wvrt_items, arena.wvrt_length, row.wv_field_3, row.wv_field_4, site);
	const SourcePosition position = source_position(source, 0, start);
	printf("error %" PRIu32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRIu32 " %" PRIu32 "\n",
		error, file, start, end, position.wv_field_0, position.wv_field_1);
	write_related_span(source, 0, file, related_start, related_end);
}

// Compile or inspect a resolved graph and check that the chosen destination remains bounded.
static int finish_graph(wvrt_slice arena, wvrt_slice files, wvrt_slice assets, wvrt_slice modules, wvrt_slice bindings,
		GraphResult resolved, int32_t root, int32_t query, const char *name, int32_t capacity, bool private_helpers) {
	const bool emit = query < 0;
	const int32_t byte_capacity = emit ? capacity : 0;
	const int32_t site_capacity = emit ? 0 : capacity;
	uint8_t *bytes = guarded_output(byte_capacity);
	if (bytes == NULL) {
		return EXIT_FAILURE;
	}
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *storage = wvrt_array_new(site_capacity, sizeof(CallSite), _Alignof(CallSite), site);
	unsigned char *sites = storage->wvrt_items;
	const size_t site_bytes = (size_t)site_capacity * sizeof(CallSite);
	memset(sites, 0xa5, site_bytes);
	const GraphOutput result = run_scalar_graph(arena, files, modules, bindings, resolved, root, query,
		(wvrt_slice){name, (int32_t)strlen(name), sizeof(uint8_t)}, assets, private_helpers,
		(wvrt_mut_slice){bytes + 1, byte_capacity, sizeof(uint8_t)},
		(wvrt_mut_slice){sites, site_capacity, sizeof(CallSite)});
	const int32_t written = result.wv_field_0;
	const int32_t byte_count = emit ? written : 0;
	const int32_t site_count = emit ? 0 : written;
	int status = EXIT_FAILURE;
	// Inspect the output canaries before publishing either a result or its diagnostic.
	if (!output_intact(bytes, byte_capacity, byte_count) || site_count < 0 || site_count > site_capacity) {
		fputs("scalar graph result exceeded its output view\n", stderr);
	} else {
		size_t offset = (size_t)site_count * sizeof(CallSite);
		while (offset < site_bytes && sites[offset] == 0xa5) {
			++offset;
		}
		if (offset != site_bytes) {
			fputs("scalar graph modified unused caller rows\n", stderr);
		} else if (result.wv_field_5 != 0) {
			write_graph_error(arena, files.wvrt_items, result.wv_field_5, result.wv_field_2,
				result.wv_field_3, result.wv_field_4, result.wv_field_6, result.wv_field_7);
		} else {
			printf("ok %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 "\n",
				written, result.wv_field_2, result.wv_field_3, result.wv_field_4);
			if (emit) {
				status = fwrite(bytes + 1, 1, (size_t)written, stdout) == (size_t)written ? EXIT_SUCCESS : EXIT_FAILURE;
			} else {
				write_callers((CallerSources){arena, {0}, files.wvrt_items}, result.wv_field_2,
					result.wv_field_1, written, storage->wvrt_items);
				status = EXIT_SUCCESS;
			}
		}
	}
	wvrt_array_drop(storage);
	free(bytes);
	return status;
}

// Expose inventory rows with canonical file paths, original spelling, and declaration spans.
static int finish_tests(wvrt_slice arena, wvrt_slice supplied, wvrt_slice modules, GraphResult graph,
		int32_t capacity) {
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *storage = wvrt_array_new(capacity, sizeof(DiscoveredTest), _Alignof(DiscoveredTest), site);
	const TestDiscovery result = discover_graph_tests(arena, supplied, modules, graph,
		(wvrt_mut_slice){storage->wvrt_items, capacity, sizeof(DiscoveredTest)});
	const SuppliedFile *files = supplied.wvrt_items;
	const uint8_t *bytes = arena.wvrt_items;
	int status = EXIT_FAILURE;
	// Keep malformed signatures and capacity failures in their original file coordinates.
	if (result.wv_field_4 != 0) {
		write_graph_error(arena, files, result.wv_field_4, result.wv_field_1, result.wv_field_2, result.wv_field_3, -1, -1);
	} else {
		printf("ok %" PRId32 "\n", result.wv_field_0);
		const DiscoveredTest *rows = storage->wvrt_items;
		// The remaining path column retains spaces in the original virtual path.
		for (int32_t index = 0; index < result.wv_field_0; ++index) {
			const DiscoveredTest test = rows[index];
			const SuppliedFile file = files[test.wv_field_0];
			const char *name = (const char *)bytes + file.wv_field_3 + test.wv_field_2;
			const char *path = (const char *)bytes + file.wv_field_1;
			printf("test %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %.*s %.*s\n",
				test.wv_field_0, test.wv_field_1, test.wv_field_4, test.wv_field_5,
				(int)(test.wv_field_3 - test.wv_field_2), name, (int)(file.wv_field_2 - file.wv_field_1), path);
		}
		status = EXIT_SUCCESS;
	}
	wvrt_array_drop(storage);
	return status;
}

// Expose each original expectation link with its attachment and token coordinates.
static int finish_expectations(wvrt_slice arena, wvrt_slice supplied, wvrt_slice modules, GraphResult graph,
		ExpectationQuery query, const char *key, int32_t capacity) {
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *storage = wvrt_array_new(capacity, sizeof(ExpectationLink), _Alignof(ExpectationLink), site);
	const ExpectationResult result = inspect_graph_expectations(arena, supplied, modules, graph, query,
		(wvrt_slice){key, (int32_t)strlen(key), sizeof(uint8_t)},
		(wvrt_mut_slice){storage->wvrt_items, capacity, sizeof(ExpectationLink)});
	const SuppliedFile *files = supplied.wvrt_items;
	const uint8_t *bytes = arena.wvrt_items;
	int fixture_status = EXIT_FAILURE;
	// Print diagnostics in the defining file's original coordinate space.
	if (result.wv_field_4 != 0) {
		write_graph_error(arena, files, result.wv_field_4, result.wv_field_1, result.wv_field_2, result.wv_field_3, -1, -1);
	} else {
		printf("ok %" PRId32 "\n", result.wv_field_0);
		const ExpectationLink *links = storage->wvrt_items;
		// Keep the original token delimiters and any escapes in the displayed link.
		for (int32_t index = 0; index < result.wv_field_0; ++index) {
			const ExpectationLink link = links[index];
			const uint8_t *source = bytes + files[link.wv_field_0].wv_field_3;
			printf("expectation %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32 " %" PRId32
					" %" PRId32 " %" PRId32 " %s %.*s ",
				link.wv_field_0, link.wv_field_1, link.wv_field_2, link.wv_field_3, link.wv_field_4, link.wv_field_5,
				link.wv_field_6, link.wv_field_7, link.wv_field_8 == 1 ? "issue" : "test",
				(int)(link.wv_field_3 - link.wv_field_2), (const char *)source + link.wv_field_2);
			fwrite(source + link.wv_field_6, 1, (size_t)(link.wv_field_7 - link.wv_field_6), stdout);
			putchar('\n');
		}
		fixture_status = ferror(stdout) ? EXIT_FAILURE : EXIT_SUCCESS;
	}
	wvrt_array_drop(storage);
	return fixture_status;
}

// Resolve a supplied graph before exposing bindings, compiled C, or inspection rows.
static int graph_mode(int argc, char **argv, SelectionContext context) {
	const bool private_helpers = strcmp(argv[1], "--emit-private-graph") == 0;
	const bool emit = private_helpers || strcmp(argv[1], "--emit-graph") == 0;
	const bool inspect = strcmp(argv[1], "--graph-callers") == 0;
	const bool tests = strcmp(argv[1], "--graph-tests") == 0;
	const bool expect_name = strcmp(argv[1], "--graph-expect") == 0;
	const bool expect_link = strcmp(argv[1], "--graph-expect-link") == 0;
	const bool expectations = expect_name || expect_link;
	const int first_file = (inspect || expectations) ? 7 : (emit || tests) ? 5 : 4;
	int32_t entry;
	int32_t capacity;
	int32_t output = 0;
	int32_t query = -1;
	// The fixture supplies graph scratch capacity and an independent result limit for each consumer.
	if (argc < first_file + 3 || (argc - first_file) % 3 != 0 ||
			!read_offset(argv[2], &entry) || !read_offset(argv[3], &capacity) ||
			((emit || inspect || tests || expectations) && !read_offset(argv[first_file - 1], &output)) ||
			((inspect || expect_name) && !read_offset(argv[4], &query)) ||
			(expect_link && strcmp(argv[4], "issue") != 0 && strcmp(argv[4], "test") != 0)) {
		fputs("usage: source_scan --graph ENTRY SCRATCH (IDENTITY PATH FILE)...\n"
			"       source_scan --emit-graph ENTRY SCRATCH CAPACITY (IDENTITY PATH FILE)... [--asset PATH FILE]...\n"
			"       source_scan --emit-private-graph ENTRY SCRATCH CAPACITY (IDENTITY PATH FILE)... [--asset PATH FILE]...\n"
			"       source_scan --graph-callers ENTRY SCRATCH FILE NAME CAPACITY (IDENTITY PATH FILE)...\n"
			"       source_scan --graph-tests ENTRY SCRATCH CAPACITY (IDENTITY PATH FILE)...\n"
			"       source_scan --graph-expect ENTRY SCRATCH FILE NAME CAPACITY (IDENTITY PATH FILE)...\n"
			"       source_scan --graph-expect-link ENTRY SCRATCH {issue|test} LINK CAPACITY (IDENTITY PATH FILE)...\n", stderr);
		return EXIT_FAILURE;
	}
	int first_asset = first_file;
	// Source rows precede any raw asset rows so their canonical identities stay stable.
	while (first_asset < argc && strcmp(argv[first_asset], "--asset") != 0) { first_asset += 3; }
	const int32_t count = (int32_t)((first_asset - first_file) / 3);
	const int32_t asset_count = (int32_t)((argc - first_asset) / 3);
	if (count == 0 || (!emit && asset_count != 0)) { return EXIT_FAILURE; }
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_array *file_storage = wvrt_array_new(count, sizeof(SuppliedFile), _Alignof(SuppliedFile), site);
	SuppliedFile *files = file_storage->wvrt_items;
	wvrt_array *asset_storage = wvrt_array_new(asset_count, sizeof(SuppliedFile), _Alignof(SuppliedFile), site);
	uint8_t *bytes = NULL;
	int32_t length = 0;
	// Read original bytes once, then give Wove the supplied virtual identities and views.
	if (!read_graph_files(argv + first_file, files, count, &bytes, &length) ||
			!read_graph_assets(argv + first_asset, asset_storage->wvrt_items, asset_count, &bytes, &length)) {
		fputs("could not load supplied graph\n", stderr);
		free(bytes);
		wvrt_array_drop(asset_storage);
		wvrt_array_drop(file_storage);
		return EXIT_FAILURE;
	}
	const wvrt_slice arena = {bytes, length, sizeof(uint8_t)};
	const wvrt_slice supplied = {files, count, sizeof(SuppliedFile)};
	// Test discovery accepts spare module storage beyond the supplied file table.
	const int32_t module_capacity = count + (tests ? 1 : 0);
	wvrt_array *modules = wvrt_array_new(module_capacity, sizeof(GraphModule), _Alignof(GraphModule), site);
	wvrt_array *bindings = wvrt_array_new(capacity, sizeof(GraphBinding), _Alignof(GraphBinding), site);
	wvrt_array *uses = wvrt_array_new(capacity, sizeof(GraphUse), _Alignof(GraphUse), site);
	wvrt_array *selectors = wvrt_array_new(capacity, sizeof(GraphSelector), _Alignof(GraphSelector), site);
	const GraphResult result = resolve_graph_for_target(arena, supplied, entry, context,
		(wvrt_mut_slice){modules->wvrt_items, module_capacity, sizeof(GraphModule)},
		(wvrt_mut_slice){bindings->wvrt_items, capacity, sizeof(GraphBinding)},
		(wvrt_mut_slice){uses->wvrt_items, capacity, sizeof(GraphUse)},
		(wvrt_mut_slice){selectors->wvrt_items, capacity, sizeof(GraphSelector)});
	int status = EXIT_FAILURE;
	// Every consumer requires successful import selection and canonical binding resolution.
	if (result.wv_field_5 != 0) {
		write_graph_error(arena, files, result.wv_field_5, result.wv_field_2, result.wv_field_3, result.wv_field_4, -1, -1);
	} else if (tests) {
		status = finish_tests(arena, supplied,
			(wvrt_slice){modules->wvrt_items, module_capacity, sizeof(GraphModule)}, result, output);
	// Keep expectation inspection independent of scalar lowering and its call-site tables.
	} else if (expectations) {
		const uint32_t kind = expect_name ? 0 : strcmp(argv[4], "issue") == 0 ? 1 : 2;
		status = finish_expectations(arena, supplied,
			(wvrt_slice){modules->wvrt_items, count, sizeof(GraphModule)}, result,
			(ExpectationQuery){kind, query}, argv[5], output);
	} else if (emit || inspect) {
		status = finish_graph(arena, supplied, (wvrt_slice){asset_storage->wvrt_items, asset_count, sizeof(SuppliedFile)},
			(wvrt_slice){modules->wvrt_items, count, sizeof(GraphModule)},
			(wvrt_slice){bindings->wvrt_items, result.wv_field_1, sizeof(GraphBinding)}, result, files[entry].wv_field_0,
			query, inspect ? argv[5] : "", output, private_helpers);
	} else {
		write_graph(bytes, files, count, modules->wvrt_items, bindings->wvrt_items, result);
		status = EXIT_SUCCESS;
	}
	wvrt_array_drop(selectors);
	wvrt_array_drop(uses);
	wvrt_array_drop(bindings);
	wvrt_array_drop(modules);
	wvrt_array_drop(asset_storage);
	wvrt_array_drop(file_storage);
	free(bytes);
	return status;
}

// Supply a whole file or one checked view to the same seed-built executable.
static int source_mode(int argc, char **argv, SelectionContext context) {
	const bool tokens = strcmp(argv[1], "--tokens") == 0;
	const bool form = strcmp(argv[1], "--form") == 0;
	const bool metadata = strcmp(argv[1], "--metadata") == 0;
	const bool tree = metadata || strcmp(argv[1], "--tree") == 0;
	const bool render = strcmp(argv[1], "--render") == 0;
	const bool decode = strcmp(argv[1], "--decode") == 0;
	const bool module = strcmp(argv[1], "--module") == 0;
	const bool emit = strcmp(argv[1], "--emit-c") == 0;
	const bool callers_root = strcmp(argv[1], "--callers-root") == 0;
	const bool callers = callers_root || strcmp(argv[1], "--callers") == 0;
	const bool takes_capacity = tree || render || decode || module || emit || callers;
	const int bounds = callers ? 4 : 3;
	if ((argc != bounds && argc != bounds + 2 && !(takes_capacity && argc == bounds + 3)) ||
			(!tokens && !form && !takes_capacity)) {
		fputs("usage: source_scan [--tokens | --form | --tree | --metadata | --render | --decode | --module | --emit-c]"
			" FILE [START END [CAPACITY]]\n"
			"       source_scan [--callers | --callers-root] FILE NAME [START END [CAPACITY]]\n", stderr);
		return EXIT_FAILURE;
	}

	int32_t length;
	uint8_t *bytes = read_source(argv[2], &length);
	if (bytes == NULL) {
		fputs("could not load source bytes\n", stderr);
		return EXIT_FAILURE;
	}
	int32_t start = 0;
	int32_t end = length;
	if (argc >= bounds + 2 && (!read_offset(argv[bounds], &start) || !read_offset(argv[bounds + 1], &end) ||
			start > end || end > length)) {
		fputs("invalid source view\n", stderr);
		free(bytes);
		return EXIT_FAILURE;
	}
	int32_t capacity = -1;
	if (argc == bounds + 3 && !read_offset(argv[bounds + 2], &capacity)) {
		fputs("invalid view capacity\n", stderr);
		free(bytes);
		return EXIT_FAILURE;
	}
	const wvrt_slice parent = {bytes, length, sizeof(uint8_t)};
	const wvrt_location site = {__FILE__, __LINE__, 1};
	const wvrt_slice view = wvrt_slice_range_u8(bytes, length, start, end, site);
	int status;
	if (tree) {
		status = dump_tree(parent, view, start, capacity, metadata);
	} else if (render) {
		status = render_view(view, capacity);
	} else if (decode) {
		status = dump_decode(parent, view, start, capacity);
	} else if (module) {
		status = dump_module(parent, view, start, capacity, context);
	} else if (emit) {
		status = dump_c(parent, view, start, capacity, context);
	} else if (callers) {
		status = dump_callers(parent, view, start, argv[3], capacity, callers_root, context);
	} else {
		status = tokens ? dump_tokens(parent, view, start) : dump_form(parent, view, start);
	}
	free(bytes);
	return status;
}

// Read fixture target names through the same classifiers used by Wove cfg.
static bool probe_target(char **words, SelectionContext *context) {
	const wvrt_slice os = {words[0], (int32_t)strlen(words[0]), sizeof(uint8_t)};
	const wvrt_slice arch = {words[1], (int32_t)strlen(words[1]), sizeof(uint8_t)};
	context->wv_field_1 = target_os(os);
	context->wv_field_2 = target_arch(arch);
	context->wv_field_3 = -1;
	return (context->wv_field_1 != 0 || strcmp(words[0], "-") == 0) &&
		(context->wv_field_2 != 0 || strcmp(words[1], "-") == 0) &&
		(strcmp(words[2], "-") == 0 || read_offset(words[2], &context->wv_field_3));
}

// Read the mode and optional supplied target before dispatching one probe command.
static int probe_command(int argc, char **argv) {
	SelectionContext context = default_selection(MODE_BOOTSTRAP);
	// Either prefix can precede the other without resetting the selected context.
	while (argc > 1) {
		// A mode prefix needs a recognized value and a command.
		if (strcmp(argv[1], "--mode") == 0) {
			// Validate the mode before advancing to the next prefix.
			if (argc < 4 || (strcmp(argv[2], "bootstrap") != 0 && strcmp(argv[2], "native") != 0 &&
					strcmp(argv[2], "test") != 0)) {
				fputs("usage: source_scan --mode {bootstrap|native|test} COMMAND ...\n", stderr);
				return EXIT_FAILURE;
			}
			context.wv_field_0 = strcmp(argv[2], "test") == 0 ? MODE_TEST :
				(strcmp(argv[2], "native") == 0 ? MODE_NATIVE : MODE_BOOTSTRAP);
			argc -= 2;
			argv += 2;
			continue;
		}
		// A target prefix supplies canonical names and an encoded deployment version.
		if (strcmp(argv[1], "--target") == 0) {
			// Require a command after all three target words.
			if (argc < 6 || !probe_target(argv + 2, &context)) {
				fputs("usage: source_scan --target OS ARCH DEPLOYMENT COMMAND ...\n", stderr);
				return EXIT_FAILURE;
			}
			argc -= 4;
			argv += 4;
			continue;
		}
		break;
	}
	// Test emission needs the supplied graph for inventory and defining-file paths.
	if (context.wv_field_0 == MODE_TEST && strcmp(argv[1], "--emit-c") == 0) {
		fputs("test mode emits native tests with --emit-graph\n", stderr);
		return EXIT_FAILURE;
	}
	const bool graph = strcmp(argv[1], "--graph") == 0 || strcmp(argv[1], "--emit-graph") == 0 ||
		strcmp(argv[1], "--emit-private-graph") == 0 ||
		strcmp(argv[1], "--graph-callers") == 0 || strcmp(argv[1], "--graph-tests") == 0 ||
		strcmp(argv[1], "--graph-expect") == 0 || strcmp(argv[1], "--graph-expect-link") == 0;
	return graph ? graph_mode(argc, argv, context) : source_mode(argc, argv, context);
}

// Exercise complete inputs and checked views into a larger buffer.
int main(int argc, char **argv) {
	if (argc > 1) { return probe_command(argc, argv); }
	const int decimal_status = wove_fixture_main(argc, argv);
	// The seed-built entry checks both decimal passes before the source fixtures.
	if (decimal_status != 0) {
		fprintf(stderr, "decimal emission fixture check %d failed\n", decimal_status);
		return EXIT_FAILURE;
	}
	// Each complete input supplies its own logical byte length.
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		const ScanCase *test = &cases[i];
		wvrt_slice input = {test->bytes, test->length, sizeof(uint8_t)};
		check(test->label, input, test->offset, test->kind);
	}

	wvrt_location site = {__FILE__, __LINE__, 1};
	check("subrange", wvrt_slice_range_u8(parent, sizeof(parent), 1, 4, site), 2, 41);
	check("subrange end", wvrt_slice_range_u8(parent, sizeof(parent), 1, 3, site), 2, 0);

	wvrt_slice stream = {token_stream, sizeof(token_stream) - 1, sizeof(uint8_t)};
	check_tokens("token stream", stream, stream_tokens, sizeof(stream_tokens) / sizeof(stream_tokens[0]));
	check_token("cursor inside atom", stream, 6, (TokenSpan){1, 6, 8});
	check_tokens("empty token view", (wvrt_slice){NULL, 0, sizeof(uint8_t)}, &(TokenSpan){0, 0, 0}, 1);
	check_tokens("whitespace token view", (wvrt_slice){whitespace, sizeof(whitespace), sizeof(uint8_t)},
		&(TokenSpan){0, 4, 4}, 1);

	wvrt_slice stream_view = wvrt_slice_range_u8(token_parent, sizeof(token_parent) - 1, 1, 10, site);
	check_tokens("token stream subrange", stream_view, stream_tokens, sizeof(stream_tokens) / sizeof(stream_tokens[0]));
	wvrt_slice atom_view = wvrt_slice_range_u8(token_parent, sizeof(token_parent) - 1, 6, 9, site);
	check_tokens("atom at view boundary", atom_view, atom_tokens, sizeof(atom_tokens) / sizeof(atom_tokens[0]));

	check_tokens("lone semicolon", (wvrt_slice){semicolon, sizeof(semicolon) - 1, sizeof(uint8_t)},
		semicolon_tokens, sizeof(semicolon_tokens) / sizeof(semicolon_tokens[0]));
	check_tokens("atom delimiters", (wvrt_slice){delimiters, sizeof(delimiters) - 1, sizeof(uint8_t)},
		delimiter_tokens, sizeof(delimiter_tokens) / sizeof(delimiter_tokens[0]));

	wvrt_slice quoted = {quoted_stream, sizeof(quoted_stream) - 1, sizeof(uint8_t)};
	check_tokens("quoted token stream", quoted, quoted_tokens, sizeof(quoted_tokens) / sizeof(quoted_tokens[0]));
	for (size_t i = 0; i < sizeof(string_errors) / sizeof(string_errors[0]); ++i) {
		check_tokens(string_errors[i].label, string_errors[i].input, &(TokenSpan){3, 0, string_errors[i].end}, 1);
	}
	wvrt_slice recovery = {bad_escape, sizeof(bad_escape) - 1, sizeof(uint8_t)};
	check_tokens("resume after invalid escape", recovery,
		recovered_string_tokens, sizeof(recovered_string_tokens) / sizeof(recovered_string_tokens[0]));

	wvrt_slice quoted_view = wvrt_slice_range_u8(quoted_parent, sizeof(quoted_parent) - 1, 1, 11, site);
	check_tokens("quoted subrange", quoted_view, &(TokenSpan){2, 1, 10}, 1);
	wvrt_slice clipped_hex = wvrt_slice_range_u8(quoted_parent, sizeof(quoted_parent) - 1, 1, 8, site);
	check_tokens("subrange ends inside Unicode escape", clipped_hex, &(TokenSpan){3, 1, 7}, 1);
	wvrt_slice clipped_quote = wvrt_slice_range_u8(quoted_parent, sizeof(quoted_parent) - 1, 1, 10, site);
	check_tokens("closing quote outside subrange", clipped_quote, &(TokenSpan){3, 1, 9}, 1);

	wvrt_slice multibyte = {utf8_atom, sizeof(utf8_atom), sizeof(uint8_t)};
	check_tokens("multibyte atom", multibyte, utf8_atom_tokens, sizeof(utf8_atom_tokens) / sizeof(utf8_atom_tokens[0]));
	for (size_t i = 0; i < sizeof(utf8_errors) / sizeof(utf8_errors[0]); ++i) {
		wvrt_slice invalid = {utf8_errors[i].bytes, utf8_errors[i].length, sizeof(uint8_t)};
		check_utf8(utf8_errors[i].label, invalid, utf8_errors[i].offset);
	}
	wvrt_slice clipped_utf8 = wvrt_slice_range_u8(utf8_parent, sizeof(utf8_parent), 1, 4, site);
	check_utf8("subrange truncates a scalar", clipped_utf8, 1);
	wvrt_slice starts_in_utf8 = wvrt_slice_range_u8(utf8_parent, sizeof(utf8_parent), 3, 6, site);
	check_utf8("subrange starts at a continuation", starts_in_utf8, 0);
	check_whitespace();
	wvrt_slice contiguous_atom = {nonseparators, sizeof(nonseparators), sizeof(uint8_t)};
	check_tokens("BOM and adjacent non-whitespace", contiguous_atom,
		nonseparator_tokens, sizeof(nonseparator_tokens) / sizeof(nonseparator_tokens[0]));
	return EXIT_SUCCESS;
}
