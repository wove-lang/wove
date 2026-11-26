// Copyright (c) 2026 wove contributors
// SPDX-License-Identifier: MIT OR Apache-2.0

#include <stdio.h>
#include <stdlib.h>

#undef main
int wove_test_main(int argc, char **argv);

typedef struct {
	void *descriptor;
	void *backing;
} Allocation;

static Allocation allocations[4];
static size_t descriptor_count;
static size_t backing_count;
static size_t drop_count;

// The intact Grid drops scratch before cells. After a field move, the separate
// cells local drops first and the Grid releases only its remaining scratch.
static const size_t drop_order[] = {1, 0, 2, 3};

// Fail at the first unexpected allocator operation.
static _Noreturn void fail(const char *message) {
	fputs(message, stderr);
	fputc('\n', stderr);
	exit(EXIT_FAILURE);
}

// Register one descriptor before its backing allocation.
void *tracked_malloc(size_t size) {
	if (descriptor_count == 4 || descriptor_count != backing_count) { fail("unexpected array descriptor"); }
	Allocation *item = &allocations[descriptor_count++];
	item->descriptor = malloc(size);
	return item->descriptor;
}

// The cells field has three bytes and the scratch field has one.
void *tracked_calloc(size_t count, size_t size) {
	if (backing_count + 1 != descriptor_count || count != (backing_count % 2 == 0 ? 3U : 1U) || size != 1) {
		fail("unexpected array backing");
	}
	Allocation *item = &allocations[backing_count++];
	item->backing = calloc(count, size);
	return item->backing;
}

// Require each live backing block to drop before its own descriptor.
void tracked_free(void *pointer) {
	if (pointer == NULL) { return; }
	if (drop_count == 4) { fail("duplicate array drop"); }
	Allocation *item = &allocations[drop_order[drop_count]];
	// The drop sequence identifies the owner even if malloc reuses an address.
	if (pointer == item->backing) {
		item->backing = NULL;
	} else if (pointer == item->descriptor && item->backing == NULL) {
		item->descriptor = NULL;
		++drop_count;
	} else {
		fail("unexpected array cleanup order");
	}
	free(pointer);
}

// Check that the two executions released every allocation.
int main(int argc, char **argv) {
	int status = wove_test_main(argc, argv);
	if (descriptor_count != 4 || backing_count != 4 || drop_count != 4) { fail("missing array cleanup"); }
	return status;
}
