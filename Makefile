# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

PYTHON ?= python3
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR ?= build

STAGE1 := $(BUILD_DIR)/stage-1
STAGE2 := $(BUILD_DIR)/stage-2
SOURCE_SCAN := $(STAGE2)/source_scan
SEED := stage-1/wovec.py stage-1/syntax.py stage-1/compiler.py
RUNTIME := runtime/wove_runtime.c
RUNTIME_HEADER := runtime/wove_runtime.h
OWNERSHIP_TESTS := tests/stage-1
OWNERSHIP_CASES := $(OWNERSHIP_TESTS)/cases.tsv

# Build native programs for executable cases. The runner handles compile-only rejections.
TEST_PROGRAMS := $(shell awk '$$1 ~ /^(run|tracked|bounds-trap|conversion-trap|range-trap)$$/ { \
	name = $$2; sub(/\.wove$$/, "", name); \
	if ($$4 == "bootstrap") name = "bootstrap/" name; \
	print name; \
	if ($$1 == "tracked") print name ".tracked" \
	}' "$(OWNERSHIP_CASES)")

.PHONY: all stage-1 stage-2 test test-stage-2
.DELETE_ON_ERROR:
.SECONDARY:

all: stage-1

stage-1: $(STAGE1)/ownership.transfer

stage-2: $(SOURCE_SCAN)

test-stage-2: stage-2
	"$(SOURCE_SCAN)"

test: stage-1 test-stage-2 $(addprefix $(STAGE1)/,$(TEST_PROGRAMS))
	sh "$(OWNERSHIP_TESTS)/check.sh" "$(PYTHON)" "$(STAGE1)" "$(OWNERSHIP_CASES)"

# Translate the Stage 2 scanner with the bootstrap profile of the Python seed.
$(STAGE2)/source_scan.c: stage-2/source_scan.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py --bootstrap "$<" > "$@"

# Supply source slices and check full-width results in the fixture executable.
$(SOURCE_SCAN): tests/stage-2/source_scan.probe.c $(STAGE2)/source_scan.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(STAGE2)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

# Keep both profiles buildable from the same fixture at the same time.
$(STAGE1)/bootstrap/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py --bootstrap "$<" > "$@"

$(STAGE1)/runtime.o: $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -c "$<" -o "$@"

$(STAGE1)/%.fixture.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -c "$<" -o "$@"

# Inspect the full scalar results in the generated translation unit.
$(STAGE1)/%: $(OWNERSHIP_TESTS)/%.probe.c $(STAGE1)/%.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Find the same source-side probe when building a bootstrap case.
$(STAGE1)/bootstrap/%: $(OWNERSHIP_TESTS)/%.probe.c $(STAGE1)/bootstrap/%.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/%: $(STAGE1)/%.fixture.o $(STAGE1)/runtime.o
	$(CLANG) $(CFLAGS) $^ -o "$@"

$(STAGE1)/runtime.tracked.o: $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -Dmalloc=tracked_malloc -Dcalloc=tracked_calloc \
		-Dfree=tracked_free -c "$<" -o "$@"

$(STAGE1)/%.tracked.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -Dmain=wove_test_main -c "$<" -o "$@"

$(STAGE1)/%.tracked: $(OWNERSHIP_TESTS)/allocations.c $(STAGE1)/%.tracked.o $(STAGE1)/runtime.tracked.o
	$(CLANG) -std=c11 $(CFLAGS) $^ -o "$@"
