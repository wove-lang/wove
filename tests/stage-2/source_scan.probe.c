// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

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
#define scan_token wv_seed_fn_9

typedef wv_seed_record_0 Token;

_Static_assert(_Generic(&source_offset, int32_t (*)(wvrt_slice): 1, default: 0), "source offset type");
_Static_assert(_Generic(&source_kind, uint32_t (*)(wvrt_slice): 1, default: 0), "source kind type");
_Static_assert(_Generic(&scan_token, Token (*)(wvrt_slice, int32_t): 1, default: 0), "scan token type");
_Static_assert(_Generic(((Token){0}).wv_field_0, uint32_t: 1, default: 0), "token kind type");
_Static_assert(_Generic(((Token){0}).wv_field_1, int32_t: 1, default: 0), "token start type");
_Static_assert(_Generic(((Token){0}).wv_field_2, int32_t: 1, default: 0), "token end type");

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
static const uint8_t delimiters[] = "a;b\"c(d)";

static const TokenSpan stream_tokens[] = {
	{40, 1, 2}, {1, 2, 4}, {1, 5, 8}, {41, 8, 9}, {0, 9, 9},
};
static const TokenSpan atom_tokens[] = {{1, 0, 3}, {0, 3, 3}};
static const TokenSpan semicolon_tokens[] = {{3, 0, 1}, {0, 1, 1}};
static const TokenSpan delimiter_tokens[] = {
	{1, 0, 1}, {3, 1, 2}, {1, 2, 3}, {3, 3, 4}, {1, 4, 5},
	{40, 5, 6}, {1, 6, 7}, {41, 7, 8}, {0, 8, 8},
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

// Exercise the same cursor progression used by the lexer caller.
static void check_tokens(const char *label, wvrt_slice input, const TokenSpan *tokens, size_t count) {
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

// Exercise complete inputs and checked views into a larger buffer.
int main(void) {
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
	check_token("empty token view", (wvrt_slice){NULL, 0, sizeof(uint8_t)}, 0, (TokenSpan){0, 0, 0});
	check_token("whitespace token view", (wvrt_slice){whitespace, sizeof(whitespace), sizeof(uint8_t)},
		0, (TokenSpan){0, 4, 4});

	wvrt_slice stream_view = wvrt_slice_range_u8(token_parent, sizeof(token_parent) - 1, 1, 10, site);
	check_tokens("token stream subrange", stream_view, stream_tokens, sizeof(stream_tokens) / sizeof(stream_tokens[0]));
	wvrt_slice atom_view = wvrt_slice_range_u8(token_parent, sizeof(token_parent) - 1, 6, 9, site);
	check_tokens("atom at view boundary", atom_view, atom_tokens, sizeof(atom_tokens) / sizeof(atom_tokens[0]));

	check_tokens("lone semicolon", (wvrt_slice){semicolon, sizeof(semicolon) - 1, sizeof(uint8_t)},
		semicolon_tokens, sizeof(semicolon_tokens) / sizeof(semicolon_tokens[0]));
	check_tokens("atom delimiters", (wvrt_slice){delimiters, sizeof(delimiters) - 1, sizeof(uint8_t)},
		delimiter_tokens, sizeof(delimiter_tokens) / sizeof(delimiter_tokens[0]));

	wvrt_slice plain = {without_comments, sizeof(without_comments) - 1, sizeof(uint8_t)};
	for (size_t i = 0; i < sizeof(comment_cases) / sizeof(comment_cases[0]); ++i) {
		check_tokens(comment_cases[i].label, comment_cases[i].input, comment_cases[i].tokens, comment_cases[i].count);
		check_significant_tokens(comment_cases[i].label, comment_cases[i].input, plain);
	}

	wvrt_slice split_opener = wvrt_slice_range_u8(comment_parent, sizeof(comment_parent) - 1, 1, 2, site);
	check_tokens("view splits comment opener", split_opener,
		semicolon_tokens, sizeof(semicolon_tokens) / sizeof(semicolon_tokens[0]));
	wvrt_slice empty_comment = wvrt_slice_range_u8(comment_parent, sizeof(comment_parent) - 1, 1, 3, site);
	check_token("comment opener at EOF", empty_comment, 0, (TokenSpan){59, 0, 2});
	wvrt_slice clipped_comment = wvrt_slice_range_u8(comment_parent, sizeof(comment_parent) - 1, 1, 6, site);
	check_token("view ends inside comment", clipped_comment, 0, (TokenSpan){59, 0, 5});
	return wove_fixture_main();
}
