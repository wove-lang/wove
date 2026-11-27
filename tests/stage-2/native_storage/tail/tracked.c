/* Copyright (c) 2026 wove contributors
 * SPDX-License-Identifier: MIT OR Apache-2.0
 */

#include <stdio.h>
#include <stdlib.h>

#undef main
int wove_test_main(int argc, char **argv);

struct test_allocation {
	void *descriptor;
	void *backing;
};

enum { RECORD_COUNT = 3 };
static struct test_allocation allocations[RECORD_COUNT];
static size_t created;
static size_t registered;
static size_t released;

/* Stop at the first unexpected allocator operation. */
static _Noreturn void fail(const char *message)
{
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

/* Each native owner first allocates one Wove descriptor. */
void *tracked_malloc(size_t size)
{
	if (created == RECORD_COUNT || registered != created) {
		fail("unexpected C storage descriptor");
	}
	struct test_allocation *allocation = &allocations[created++];
	allocation->descriptor = malloc(size);
	return allocation->descriptor;
}

/* This extended-alignment fixture expects aligned_alloc for every backing block. */
void *tracked_calloc(size_t count, size_t size)
{
	(void)count;
	(void)size;
	fail("unaligned C backing allocation");
}

/* The native initializer identifies the aligned block lent by the owner. */
void test_register_tail(void *data)
{
	if (registered + 1 != created || data == NULL) {
		fail("unexpected C backing block");
	}
	allocations[registered++].backing = data;
}

/* Each owner releases its backing block before releasing its descriptor. */
void tracked_free(void *pointer)
{
	if (pointer == NULL) {
		return;
	}
	if (released == registered) {
		fail("unexpected C storage free");
	}
	struct test_allocation *allocation = &allocations[released];
	if (pointer == allocation->backing) {
		allocation->backing = NULL;
	} else if (pointer == allocation->descriptor && allocation->backing == NULL) {
		allocation->descriptor = NULL;
		++released;
	} else {
		fail("duplicate C storage free or unexpected cleanup order");
	}
	free(pointer);
}

/* Both failed initializations and the successful retry release one owner apiece. */
int main(int argc, char **argv)
{
	int status = wove_test_main(argc, argv);
	if (created != RECORD_COUNT || registered != RECORD_COUNT || released != RECORD_COUNT) {
		fail("missing C storage cleanup");
	}
	return status;
}
