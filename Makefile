# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

PYTHON ?= python3
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR ?= build

STAGE1 := $(BUILD_DIR)/stage-1
STAGE2 := $(BUILD_DIR)/stage-2
BOOTSTRAP_COMPILER := $(STAGE1)/wovec
COMPILER_ROOT := stage-2/wovec.wove
SOURCE_SCAN_MODULE := stage-2/source/scan.wove
UTF8_MODULE := stage-2/text/utf8.wove
QUOTED_TEXT_MODULE := stage-2/text/quoted.wove
BYTE_TEXT_MODULE := stage-2/text/bytes.wove
VALUE_TEXT_MODULES := $(BYTE_TEXT_MODULE) stage-2/text/decimal.wove
DECIMAL_WRITER := stage-2/text/decimal_write.wove
SOURCE_STRINGS_MODULE := stage-2/source/strings.wove
SOURCE_MODULE_READER := stage-2/source/module.wove
SOURCE_CALLERS := stage-2/source/callers.wove
SOURCE_GRAPH_MODULES := stage-2/source/graph.wove stage-2/source/imports.wove stage-2/source/paths.wove
C_EMITTER_SOURCES := $(addprefix stage-2/,emit/c.wove emit/runtime.wove emit/graph.wove emit/names.wove emit/records.wove emit/text.wove \
	scalar/syntax.wove scalar/flow.wove scalar/plan.wove scalar/graph.wove scalar/records.wove) $(DECIMAL_WRITER)
HASHING_SOURCES := $(addprefix stage-2/hashing/,fnv1a.wove base32hex.wove)
HASHING_FIXTURES := tests/stage-2/hashing
HASHING_CASES := $(HASHING_FIXTURES)/cases.tsv
HASHING_PROGRAMS := $(addprefix $(STAGE2)/hashing/,fnv1a base32hex)
JSON_WRITER_SOURCES := stage-2/json/writer.wove stage-2/json/text.wove \
	$(UTF8_MODULE) $(BYTE_TEXT_MODULE) $(DECIMAL_WRITER)
JSON_READER_SOURCES := stage-2/json/reader.wove stage-2/json/lex.wove $(QUOTED_TEXT_MODULE) $(UTF8_MODULE)
JSON_FIXTURES := tests/stage-2/json
JSON_CASES := $(JSON_FIXTURES)/cases.tsv
JSON_WRITER_PROGRAMS := $(addprefix $(STAGE2)/json/,writer writer_errors)
JSON_READER_PROGRAMS := $(addprefix $(STAGE2)/json/,reader reader_errors)
JSON_PROGRAMS := $(JSON_WRITER_PROGRAMS) $(JSON_READER_PROGRAMS) $(STAGE2)/json/utf8
CLI_SOURCES := stage-2/cli/argv.wove stage-2/cli/values.wove $(VALUE_TEXT_MODULES)
CLI_FIXTURES := tests/stage-2/cli
CLI_CASES := $(CLI_FIXTURES)/cases.tsv
CLI_OPTION_PROGRAMS := $(addprefix $(STAGE2)/cli/,options options_errors)
CLI_PROGRAMS := $(addprefix $(STAGE2)/cli/,argv argv_errors) $(CLI_OPTION_PROGRAMS)
CONFIG_FIXTURES := tests/stage-2/config
CONFIG_PROGRAM := $(STAGE2)/config/project
CONFIG_SOURCES := $(addprefix stage-2/config/,compiler.wove defaults.wove project.wove schema.wove values.wove)
COMMAND_FIXTURES := $(CLI_FIXTURES)/command
COMMAND_PROGRAM := $(STAGE2)/cli/command
COMMAND_SOURCES := $(addprefix stage-2/cli/,command.wove command_args.wove command_schema.wove command_values.wove)
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

.PHONY: all stage-1 stage-2 test test-stage-2 test-stage-2-json test-stage-2-cli test-stage-2-config
.DELETE_ON_ERROR:
.SECONDARY:

all: stage-1

stage-1: $(STAGE1)/ownership.transfer

stage-2: $(BOOTSTRAP_COMPILER) $(SOURCE_SCAN)

# Preserve quoting in compiler settings until the runner splits them into arguments.
test-stage-2: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2: export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
test-stage-2: export WOVE_TEST_TRACKED_RUNTIME = $(STAGE1)/runtime.tracked.o
test-stage-2: stage-2 $(STAGE1)/runtime.tracked.o $(HASHING_PROGRAMS) test-stage-2-json test-stage-2-cli test-stage-2-config
	"$(BOOTSTRAP_COMPILER)"
	"$(SOURCE_SCAN)"
	$(PYTHON) tests/stage-2/check_streams.py "$(SOURCE_SCAN)" \
		tests/stage-2/lexer_layouts.tsv tests/stage-2/lexer_errors.tsv tests/stage-2/source_forms.tsv \
		tests/stage-2/source_trees.tsv tests/stage-2/source_metadata.tsv tests/stage-2/source_renderings.tsv \
		tests/stage-2/source_decodings.tsv tests/stage-2/source_modules.tsv tests/stage-2/source_compilations.tsv \
		tests/stage-2/source_callers.tsv \
		stage-2/*.wove stage-2/source/*.wove stage-2/scalar/*.wove stage-2/emit/*.wove $(HASHING_SOURCES) \
		$(UTF8_MODULE) $(QUOTED_TEXT_MODULE) $(VALUE_TEXT_MODULES) $(DECIMAL_WRITER) $(CONFIG_SOURCES) $(COMMAND_SOURCES) \
		tests/stage-1/ownership.transfer.wove tests/stage-1/cfg.select.wove
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" \
		tests/stage-2/source_graphs.tsv tests/stage-2/scalar_graphs.tsv
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/hashing" "$(HASHING_CASES)"

# Run the standalone JSON utilities with the same manifest runner as the seed.
test-stage-2-json: $(JSON_PROGRAMS)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/json" "$(JSON_CASES)"

# Run the standalone typed argv fixtures with the seed manifest runner.
test-stage-2-cli: $(CLI_PROGRAMS) $(COMMAND_PROGRAM)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/cli" "$(CLI_CASES)"
	$(PYTHON) "$(COMMAND_FIXTURES)/check.py" "$(COMMAND_PROGRAM)" "$(COMMAND_FIXTURES)/cases.tsv"

# Exercise the project loader with the checked-in config and standalone cases.
test-stage-2-config: $(CONFIG_PROGRAM)
	$(PYTHON) "$(CONFIG_FIXTURES)/check.py" "$(CONFIG_PROGRAM)" "$(CONFIG_FIXTURES)/cases.tsv"

test: stage-1 test-stage-2 $(addprefix $(STAGE1)/,$(TEST_PROGRAMS))
	sh "$(OWNERSHIP_TESTS)/check.sh" "$(PYTHON)" "$(STAGE1)" "$(OWNERSHIP_CASES)"
	$(PYTHON) "$(OWNERSHIP_TESTS)/check_strings.py" "$(OWNERSHIP_TESTS)/strings.tsv"

# Translate the compiler root and regenerate C when any source module changes.
$(STAGE1)/wovec.c: $(COMPILER_ROOT) $(SOURCE_SCAN_MODULE) $(UTF8_MODULE) $(QUOTED_TEXT_MODULE) \
	$(SOURCE_STRINGS_MODULE) $(SOURCE_MODULE_READER) $(VALUE_TEXT_MODULES) \
	$(SOURCE_GRAPH_MODULES) $(SOURCE_CALLERS) $(C_EMITTER_SOURCES) $(HASHING_SOURCES) $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

# Link the seed-built compiler entry with the shared runtime.
$(BOOTSTRAP_COMPILER): $(STAGE1)/wovec.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime $^ -o "$@"

# Give the scanner fixture access to the same generated scanner functions.
$(SOURCE_SCAN): tests/stage-2/source_scan.probe.c $(STAGE1)/wovec.c $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(STAGE1)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Build utility-only Wove programs with the ordinary Python seed and runtime.
$(HASHING_PROGRAMS): $(STAGE2)/hashing/%: $(HASHING_FIXTURES)/%.wove $(HASHING_SOURCES) $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Build JSON fixtures separately from the Stage 2 compiler entry.
$(JSON_PROGRAMS): $(STAGE2)/json/%: $(JSON_FIXTURES)/%.wove $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Track each standalone JSON fixture's shared text inputs.
$(JSON_WRITER_PROGRAMS): $(JSON_WRITER_SOURCES)
$(JSON_READER_PROGRAMS): $(JSON_READER_SOURCES)
$(STAGE2)/json/utf8: $(UTF8_MODULE)

# Build each argv fixture with its own test entry and the seed.
$(CLI_PROGRAMS): $(STAGE2)/cli/%: $(CLI_FIXTURES)/%.wove $(CLI_FIXTURES)/fixture_data.wove $(CLI_SOURCES) $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Only the compiler-options fixtures load and validate JSONC configuration.
$(CLI_OPTION_PROGRAMS): stage-2/cli/options.wove stage-2/config/compiler.wove $(JSON_READER_SOURCES)

# Build the project fixture with file input supplied by the probe.
$(CONFIG_PROGRAM).c: $(CONFIG_FIXTURES)/project.wove $(CONFIG_SOURCES) $(JSON_READER_SOURCES) $(VALUE_TEXT_MODULES) $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

$(CONFIG_PROGRAM): $(CONFIG_FIXTURES)/project.probe.c $(CONFIG_PROGRAM).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Supply an argv arena and config bytes to the standalone command fixture.
$(COMMAND_PROGRAM).c: $(CLI_FIXTURES)/command.wove $(COMMAND_SOURCES) $(CLI_SOURCES) $(CONFIG_SOURCES) $(JSON_READER_SOURCES) $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

$(COMMAND_PROGRAM): $(CLI_FIXTURES)/command.probe.c $(COMMAND_PROGRAM).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

# Keep required providers in the build graph even when a source file is missing.
ROUND_TRIP_PROVIDERS := $(addprefix $(OWNERSHIP_TESTS)/imports/,left.wove right.wove nested/shared.wove)
RUNTIME_LOCATION_PROVIDER := $(OWNERSHIP_TESTS)/slice.load_negative.wove
MODULE_SELECT_SOURCES := $(addprefix $(OWNERSHIP_TESTS)/,module.select.wove modules/bootstrap.wove \
	modules/full.wove modules/testing.wove imports/nested/shared.wove)
TYPE_IMPORT_SOURCES := $(addprefix $(OWNERSHIP_TESTS)/,types.round_trip.wove types/base.wove types/forward.wove)
SOURCE_TREE_IMPORT := $(OWNERSHIP_TESTS)/types.source_tree.wove
$(STAGE1)/imports.round_trip.c: $(ROUND_TRIP_PROVIDERS)
$(STAGE1)/imports.host_declarations.c: $(OWNERSHIP_TESTS)/imports/nested/shared.wove
$(STAGE1)/imports.runtime_location.c: $(RUNTIME_LOCATION_PROVIDER)
$(STAGE1)/module.select.c: $(MODULE_SELECT_SOURCES)
$(STAGE1)/types.round_trip.c: $(TYPE_IMPORT_SOURCES)
$(STAGE1)/types.source_tree.c: $(SOURCE_TREE_IMPORT) $(SOURCE_SCAN_MODULE) $(UTF8_MODULE) $(QUOTED_TEXT_MODULE)

# Force the seed to report missing Wove inputs that Make can skip under .SECONDARY.
REQUIRED_WOVE_SOURCES := $(COMPILER_ROOT) $(SOURCE_SCAN_MODULE) $(UTF8_MODULE) $(QUOTED_TEXT_MODULE) \
	$(SOURCE_STRINGS_MODULE) $(SOURCE_MODULE_READER) $(VALUE_TEXT_MODULES) \
	$(SOURCE_GRAPH_MODULES) $(SOURCE_CALLERS) $(C_EMITTER_SOURCES) $(HASHING_SOURCES) $(ROUND_TRIP_PROVIDERS) \
	$(RUNTIME_LOCATION_PROVIDER) $(MODULE_SELECT_SOURCES) $(TYPE_IMPORT_SOURCES) $(SOURCE_TREE_IMPORT)
.PHONY: $(filter-out $(wildcard $(REQUIRED_WOVE_SOURCES)),$(REQUIRED_WOVE_SOURCES))

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
