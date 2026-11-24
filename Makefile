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
	name = $$2; sub(/\.wove$$/, "", name); print name; \
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
	$(PYTHON) tests/stage-2/check_streams.py "$(SOURCE_SCAN)" \
		tests/stage-2/lexer_layouts.tsv tests/stage-2/lexer_errors.tsv tests/stage-2/source_forms.tsv \
		tests/stage-2/source_trees.tsv tests/stage-2/source_metadata.tsv tests/stage-2/source_renderings.tsv \
		stage-2/*.wove tests/stage-1/ownership.transfer.wove tests/stage-1/cfg.select.wove

test: stage-1 test-stage-2 $(addprefix $(STAGE1)/,$(TEST_PROGRAMS))
	sh "$(OWNERSHIP_TESTS)/check.sh" "$(PYTHON)" "$(STAGE1)" "$(OWNERSHIP_CASES)"

# Translate the canonical Stage 2 scanner directly with the Python seed.
$(STAGE2)/source_scan.c: stage-2/source_scan.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

# Supply source slices and check full-width results in the fixture executable.
$(SOURCE_SCAN): tests/stage-2/source_scan.probe.c $(STAGE2)/source_scan.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(STAGE2)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

$(STAGE1)/runtime.o: $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -c "$<" -o "$@"

$(STAGE1)/%.fixture.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -c "$<" -o "$@"

$(STAGE1)/%: $(STAGE1)/%.fixture.o $(STAGE1)/runtime.o
	$(CLANG) $(CFLAGS) $^ -o "$@"

# Inspect the full scalar results in the generated translation unit.
$(STAGE1)/scalar.round_trip $(STAGE1)/scalar.convert $(STAGE1)/scalar.wrapping $(STAGE1)/scalar.compare: $(STAGE1)/%: \
	$(OWNERSHIP_TESTS)/%.probe.c $(STAGE1)/%.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(STAGE1)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/runtime.tracked.o: $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -Dmalloc=tracked_malloc -Dcalloc=tracked_calloc \
		-Dfree=tracked_free -c "$<" -o "$@"

$(STAGE1)/%.tracked.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -Dmain=wove_test_main -c "$<" -o "$@"

$(STAGE1)/%.tracked: $(OWNERSHIP_TESTS)/allocations.c $(STAGE1)/%.tracked.o $(STAGE1)/runtime.tracked.o
	$(CLANG) -std=c11 $(CFLAGS) $^ -o "$@"
