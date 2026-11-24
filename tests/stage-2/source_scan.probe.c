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

// Match the seed's generated names to the scanner declarations.
#define source_offset wv_seed_fn_4
#define source_kind wv_seed_fn_5
#define scan_token wv_seed_fn_13
#define admit_source wv_seed_fn_17
#define source_diagnostic wv_seed_fn_19
#define source_position wv_seed_fn_20

typedef wv_seed_record_0 Token;

_Static_assert(_Generic(&source_offset, int32_t (*)(wvrt_slice): 1, default: 0), "source offset type");
_Static_assert(_Generic(&source_kind, uint32_t (*)(wvrt_slice): 1, default: 0), "source kind type");
_Static_assert(_Generic(&scan_token, Token (*)(wvrt_slice, int32_t): 1, default: 0), "scan token type");
_Static_assert(_Generic(&admit_source, int32_t (*)(wvrt_slice): 1, default: 0), "source admission type");
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

static const uint8_t comments_lf[] = ";; (\" \n(fn;; ) \"\nx)\n;; tail ";
static const uint8_t comments_crlf[] = ";; (\" \r\n(fn;; ) \"\r\nx)\r\n;; tail ";
static const uint8_t comments_cr[] = ";; (\" \r(fn;; ) \"\rx)\r;; tail ";
static const uint8_t without_comments[] = "(fn x)";
static const uint8_t comment_parent[] = "x;; tail\r\n(";

static const TokenSpan single_line_ending_tokens[] = {
	{59, 0, 6}, {40, 7, 8}, {1, 8, 10}, {59, 10, 16},
	{1, 17, 18}, {41, 18, 19}, {59, 20, 28}, {0, 28, 28},
};
static const TokenSpan crlf_tokens[] = {
	{59, 0, 6}, {40, 8, 9}, {1, 9, 11}, {59, 11, 17},
	{1, 19, 20}, {41, 20, 21}, {59, 23, 31}, {0, 31, 31},
};

static const struct {
	const char *label;
	wvrt_slice input;
	const TokenSpan *tokens;
	size_t count;
} comment_cases[] = {
	{"LF comments", {comments_lf, sizeof(comments_lf) - 1, sizeof(uint8_t)},
		single_line_ending_tokens, sizeof(single_line_ending_tokens) / sizeof(single_line_ending_tokens[0])},
	{"CRLF comments", {comments_crlf, sizeof(comments_crlf) - 1, sizeof(uint8_t)},
		crlf_tokens, sizeof(crlf_tokens) / sizeof(crlf_tokens[0])},
	{"CR comments", {comments_cr, sizeof(comments_cr) - 1, sizeof(uint8_t)},
		single_line_ending_tokens, sizeof(single_line_ending_tokens) / sizeof(single_line_ending_tokens[0])},
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
	{"invalid byte in a comment", {'(', ')', ';', ';', 0xff}, 5, 4},
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

static const uint8_t unicode_comment[] = {
	';', ';', 'a', 0xe2, 0x80, 0xa8, 'b', 0xe2, 0x80, 0xa9, 'c', 0xc2, 0x85, 'd', '\r', '\n', 'x',
};
static const TokenSpan unicode_comment_tokens[] = {{59, 0, 14}, {1, 16, 17}, {0, 17, 17}};

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

// Compare kinds and spellings after omitting the raw comment tokens.
static void check_significant_tokens(const char *label, wvrt_slice raw, wvrt_slice plain) {
	const uint8_t *raw_bytes = raw.wvrt_items;
	const uint8_t *plain_bytes = plain.wvrt_items;
	int32_t raw_cursor = 0;
	int32_t plain_cursor = 0;
	while (true) {
		const TokenSpan actual = read_token(raw, raw_cursor);
		raw_cursor = actual.end;
		if (actual.kind == 59) {
			continue;
		}

		const TokenSpan expected = read_token(plain, plain_cursor);
		plain_cursor = expected.end;
		int32_t width = actual.end - actual.start;
		if (actual.kind != expected.kind || width != expected.end - expected.start
			|| memcmp(raw_bytes + actual.start, plain_bytes + expected.start, (size_t)width) != 0) {
			fprintf(stderr, "%s: significant token differs at byte %" PRId32 "\n", label, actual.start);
			exit(EXIT_FAILURE);
		}
		if (actual.kind == 0) {
			return;
		}
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

// Emit numeric kinds, spans, and coordinates while leaving the source bytes in the input.
static void write_token(wvrt_slice parent, int32_t view_start, Token token) {
	const wv_seed_record_2 position = source_position(parent, view_start, token.wv_field_1);
	printf("%" PRIu32 "\t%" PRId32 "\t%" PRId32 "\t%" PRIu32 "\t%" PRIu32 "\t%" PRIu32 "\n",
		token.wv_field_0, token.wv_field_1, token.wv_field_2, token.wv_field_3,
		position.wv_field_0, position.wv_field_1);
}

// Check UTF-8 once, then advance by each token's exclusive end.
static int dump_tokens(wvrt_slice parent, int32_t start, int32_t end) {
	const wvrt_location site = {__FILE__, __LINE__, 1};
	wvrt_slice input = wvrt_slice_range_u8(parent.wvrt_items, parent.wvrt_length, start, end, site);
	const wv_seed_record_1 diagnostic = source_diagnostic(input);
	if (diagnostic.wv_field_0 != 0) {
		write_token(parent, start, (Token){3, diagnostic.wv_field_1, diagnostic.wv_field_2, diagnostic.wv_field_0});
		return EXIT_FAILURE;
	}

	int32_t cursor = 0;
	for (;;) {
		const Token token = scan_token(input, cursor);
		write_token(parent, start, token);
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

// Supply a whole file or one checked view to the same seed-built executable.
static int token_stream_mode(int argc, char **argv) {
	if ((argc != 3 && argc != 5) || strcmp(argv[1], "--tokens") != 0) {
		fputs("usage: source_scan --tokens FILE [START END]\n", stderr);
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
	if (argc == 5 && (!read_offset(argv[3], &start) || !read_offset(argv[4], &end) || start > end || end > length)) {
		fputs("invalid source view\n", stderr);
		free(bytes);
		return EXIT_FAILURE;
	}
	int status = dump_tokens((wvrt_slice){bytes, length, sizeof(uint8_t)}, start, end);
	free(bytes);
	return status;
}

// Exercise complete inputs and checked views into a larger buffer.
int main(int argc, char **argv) {
	if (argc > 1) {
		return token_stream_mode(argc, argv);
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

	wvrt_slice plain = admit_view("significant token reference",
		(wvrt_slice){without_comments, sizeof(without_comments) - 1, sizeof(uint8_t)});
	for (size_t i = 0; i < sizeof(comment_cases) / sizeof(comment_cases[0]); ++i) {
		check_tokens(comment_cases[i].label, comment_cases[i].input, comment_cases[i].tokens, comment_cases[i].count);
		check_significant_tokens(comment_cases[i].label, comment_cases[i].input, plain);
	}

	wvrt_slice split_opener = wvrt_slice_range_u8(comment_parent, sizeof(comment_parent) - 1, 1, 2, site);
	check_tokens("view splits comment opener", split_opener,
		semicolon_tokens, sizeof(semicolon_tokens) / sizeof(semicolon_tokens[0]));
	wvrt_slice empty_comment = wvrt_slice_range_u8(comment_parent, sizeof(comment_parent) - 1, 1, 3, site);
	check_tokens("comment opener at EOF", empty_comment, &(TokenSpan){59, 0, 2}, 1);
	wvrt_slice clipped_comment = wvrt_slice_range_u8(comment_parent, sizeof(comment_parent) - 1, 1, 6, site);
	check_tokens("view ends inside comment", clipped_comment, &(TokenSpan){59, 0, 5}, 1);
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
	wvrt_slice unicode_in_comment = {unicode_comment, sizeof(unicode_comment), sizeof(uint8_t)};
	check_tokens("Unicode separators inside a comment", unicode_in_comment,
		unicode_comment_tokens, sizeof(unicode_comment_tokens) / sizeof(unicode_comment_tokens[0]));
	return wove_fixture_main();
}
