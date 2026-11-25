# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

PYTHON ?= python3
CLANG ?= clang
GCC ?= gcc
GCC_CXX ?= g++
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR ?= build
BUILD_VERSION := $(BUILD_DIR)/version.txt
VERSION_GENERATOR := tools/build-info.sh
VERSION_FIXTURES := tests/stage-2/version

STAGE1 := $(BUILD_DIR)/stage-1
STAGE2 := $(BUILD_DIR)/stage-2
BOOTSTRAP_COMPILER := $(STAGE1)/wovec
COMPILER_ROOT := stage-2/wovec.wove
NATIVE_COMPILER := $(STAGE2)/wovec
NATIVE_DRIVER := stage-2/driver
SOURCE_SCAN_MODULE := stage-2/source/scan.wove
UTF8_MODULE := stage-2/text/utf8.wove
QUOTED_TEXT_MODULE := stage-2/text/quoted.wove
BYTE_TEXT_MODULE := stage-2/text/bytes.wove
BYTE_MATCH_MODULE := stage-2/text/match.wove
VALUE_TEXT_MODULES := $(BYTE_TEXT_MODULE) stage-2/text/decimal.wove
DECIMAL_WRITER := stage-2/text/decimal_write.wove
SOURCE_STRINGS_MODULE := stage-2/source/strings.wove
SOURCE_MODULE_READER := stage-2/source/module.wove stage-2/source/target.wove stage-2/source/functions.wove
SOURCE_TYPE_MODULE := stage-2/source/types.wove
TEST_DISCOVERY_MODULE := stage-2/scalar/tests.wove
SOURCE_CALLERS := stage-2/source/callers.wove
SOURCE_GRAPH_MODULES := stage-2/source/graph.wove stage-2/source/owned.wove stage-2/source/imports.wove stage-2/source/paths.wove stage-2/source/c_headers.wove stage-2/source/c_names.wove stage-2/source/c_formals.wove stage-2/source/c_scope.wove stage-2/source/tree_copy.wove stage-2/source/c_arguments.wove stage-2/source/c_owners.wove stage-2/source/c_records.wove stage-2/source/c_snapshots.wove
C_HEADER_MODULE := stage-2/c/headers.wove
C_IDENTIFIER_MODULE := stage-2/c/identifiers.wove
C_ARGUMENT_MODULE := stage-2/c/arguments.wove
C_EMITTER_SOURCES := $(C_HEADER_MODULE) $(C_IDENTIFIER_MODULE) $(C_ARGUMENT_MODULE) $(addprefix stage-2/,source/assets.wove emit/embeds.wove emit/c.wove emit/arrays.wove emit/bindings.wove emit/runtime.wove emit/c_values.wove emit/c_arguments.wove emit/c_formals.wove emit/c_vectors.wove emit/c_variadic.wove emit/c_extents.wove emit/c_snapshots.wove emit/c_storage.wove emit/owners.wove emit/graph.wove emit/headers.wove emit/names.wove emit/records.wove emit/text.wove \
	scalar/syntax.wove scalar/signatures.wove scalar/flow.wove scalar/loans.wove scalar/host_entry.wove scalar/plan.wove scalar/graph.wove scalar/trees.wove scalar/c_types.wove scalar/records.wove scalar/owners.wove) $(DECIMAL_WRITER)
HASHING_BITS := stage-2/hashing/bits.wove
HASHING_SOURCES := $(addprefix stage-2/hashing/,fnv1a.wove base16.wove base32hex.wove) $(HASHING_BITS)
HASHING_FIXTURES := tests/stage-2/hashing
HASHING_CASES := $(HASHING_FIXTURES)/cases.tsv
HASHING_PROGRAMS := $(addprefix $(STAGE2)/hashing/,fnv1a base32hex base16)
HASHING_NATIVE_PROGRAMS := $(addprefix $(STAGE2)/hashing/native/,base32hex base16)
BASE32HEX_SOURCE := stage-2/hashing/base32hex.wove
BASE16_SOURCES = stage-2/emit/text.wove stage-2/hashing/base16.wove $(C_IDENTIFIER_MODULE) $(SOURCE_TYPE_MODULE) $(BASE32HEX_SOURCE) $(BOOTSTRAP_WOVE_SOURCES)
BLAKE2B_FIXTURES := $(HASHING_FIXTURES)/blake2b
BLAKE2B_PROBE := $(STAGE2)/hashing/blake2b
BLAKE2B_SOURCE := stage-2/hashing/blake2b.wove
JSON_WRITER_SOURCES := stage-2/json/writer.wove stage-2/json/text.wove \
	$(UTF8_MODULE) $(BYTE_TEXT_MODULE) $(DECIMAL_WRITER)
JSON_READER_SOURCES := stage-2/json/reader.wove stage-2/json/lex.wove $(QUOTED_TEXT_MODULE) $(UTF8_MODULE)
JSON_FRAME_MODULE := stage-2/json/frame.wove
JSON_FIXTURES := tests/stage-2/json
JSON_CASES := $(JSON_FIXTURES)/cases.tsv
JSON_WRITER_PROGRAMS := $(addprefix $(STAGE2)/json/,writer writer_errors)
JSON_READER_PROGRAMS := $(addprefix $(STAGE2)/json/,reader reader_errors)
JSON_PROGRAMS := $(JSON_WRITER_PROGRAMS) $(JSON_READER_PROGRAMS) $(STAGE2)/json/utf8 $(STAGE2)/json/frame
DIAGNOSTIC_FIXTURES := tests/stage-2/diagnostics
DIAGNOSTIC_PROGRAM := $(STAGE2)/diagnostics/json
DIAGNOSTIC_SOURCES := stage-2/diagnostics/record.wove stage-2/diagnostics/json.wove
CLI_SOURCES := stage-2/cli/argv.wove stage-2/cli/values.wove $(VALUE_TEXT_MODULES)
CLI_FIXTURES := tests/stage-2/cli
CLI_CASES := $(CLI_FIXTURES)/cases.tsv
CLI_OPTION_PROGRAMS := $(addprefix $(STAGE2)/cli/,options options_errors)
CLI_PROGRAMS := $(addprefix $(STAGE2)/cli/,argv argv_errors host_entry jobs) $(CLI_OPTION_PROGRAMS)
JOB_BUDGET_MODULE := $(NATIVE_DRIVER)/jobs.wove
CONFIG_FIXTURES := tests/stage-2/config
CONFIG_PROGRAM := $(STAGE2)/config/project
CONFIG_SOURCES := $(addprefix stage-2/config/,compiler.wove defaults.wove project.wove schema.wove values.wove)
COMMAND_FIXTURES := $(CLI_FIXTURES)/command
COMMAND_PROGRAM := $(STAGE2)/cli/command
COMMAND_SOURCES := $(addprefix stage-2/cli/command/,resolve.wove args.wove schema.wove values.wove)
NATIVE_CLI_SOURCES := $(addprefix $(NATIVE_DRIVER)/,command.wove source.wove diagnostics.wove assets.wove version.wove bundled.wove graph.wove output.wove publication.wove platform.wove files.wove paths.wove disk.wove disk_files.wove bin/arguments.wove bin/build.wove)
BUNDLED_C_PROVIDERS := $(addprefix stage-2/c-stdlib/,stdlib.wove stdio.wove unistd.wove spawn.wove fcntl.wove errno.wove sys_stat.wove sys_wait.wove)
NATIVE_BUILD_ASSETS := runtime/wove_runtime.c runtime/wove_runtime.h
NATIVE_STDIO := stage-2/io/stdio.wove
DRIVER_C_PROVIDERS := $(addprefix stage-2/c-stdlib/,stdlib.wove stdio.wove sys_stat.wove fcntl.wove unistd.wove errno.wove)
NATIVE_WOVE_SOURCES = $(sort $(GRAPH_COMPILER_SOURCES) \
	$(CLI_SOURCES) $(COMMAND_SOURCES) $(CONFIG_SOURCES) $(JSON_READER_SOURCES) $(JSON_WRITER_SOURCES) \
	$(DIAGNOSTIC_SOURCES) $(NATIVE_CLI_SOURCES) $(NATIVE_STDIO) $(PROCESS_LAUNCHER_SOURCES) $(DRIVER_C_PROVIDERS))
NATIVE_CLI_FIXTURES := $(CLI_FIXTURES)/native
PROCESS_MODULES := stage-2/io/process.wove stage-2/io/capture/descriptors.wove stage-2/io/capture/child.wove
PROCESS_FIXTURES := tests/stage-2/process
PROCESS_CASES := $(PROCESS_FIXTURES)/cases.tsv
PROCESS_DESCRIPTOR_CASES := $(PROCESS_FIXTURES)/descriptors.tsv
PROCESS_INHERITANCE_CASES := $(PROCESS_FIXTURES)/inheritance.tsv
PROCESS_LAUNCHER_SOURCES := $(PROCESS_MODULES) stage-2/text/copy.wove \
	$(addprefix stage-2/c-stdlib/,errno.wove fcntl.wove spawn.wove stdlib.wove sys_stat.wove sys_wait.wove unistd.wove)
TEST_RUNNER_ROOT := stage-2/test/runner.wove
TEST_RUNNER_SOURCES := $(sort $(PROCESS_LAUNCHER_SOURCES) $(NATIVE_STDIO) $(DECIMAL_WRITER) \
	$(BYTE_TEXT_MODULE) stage-2/c-stdlib/stdio.wove)
TEST_RUNNER := $(STAGE2)/test/runner
TEST_RUNTIME := $(STAGE2)/test/libwove_test.a
CLANG_FIXTURES := tests/stage-2/clang
CLANG_SOURCES := $(C_HEADER_MODULE) $(addprefix stage-2/clang/,source.wove fields.wove index.wove values.wove)
CLANG_TEMPLATE := stage-2/clang/templates/probe_header.tmpl.h
AST_FIXTURES := tests/support/ast
AST_PROBE := $(STAGE2)/ast/probe
GCC_FIXTURES := tests/stage-2/gcc
SOURCE_SCAN := $(STAGE2)/source_scan
SOURCE_PROBE_ROOT := tests/stage-2/source_scan.wove
SOURCE_PROBE_C := $(dir $(SOURCE_SCAN))source_scan.c
BOOTSTRAP_WOVE_SOURCES := $(SOURCE_SCAN_MODULE) $(UTF8_MODULE) $(QUOTED_TEXT_MODULE) $(BYTE_MATCH_MODULE)
GRAPH_COMPILER_SOURCES := $(BOOTSTRAP_WOVE_SOURCES) $(SOURCE_STRINGS_MODULE) $(SOURCE_MODULE_READER) $(VALUE_TEXT_MODULES) \
	$(SOURCE_GRAPH_MODULES) $(SOURCE_TYPE_MODULE) $(SOURCE_CALLERS) $(TEST_DISCOVERY_MODULE) $(C_EMITTER_SOURCES) stage-2/emit/tests.wove $(HASHING_SOURCES)
SEED := stage-1/wovec.py stage-1/syntax.py stage-1/compiler.py
RUNTIME := runtime/wove_runtime.c
RUNTIME_HEADER := runtime/wove_runtime.h
OWNERSHIP_TESTS := tests/stage-1
OWNERSHIP_CASES := $(OWNERSHIP_TESTS)/cases.tsv

# Build native programs for executable cases. The runner handles compile-only rejections.
TEST_PROGRAMS := $(shell awk '$$1 ~ /^(run|tracked|bounds-trap|conversion-trap|range-trap|shift-trap)$$/ { \
	name = $$2; sub(/\.wove$$/, "", name); print name; \
	if ($$1 == "tracked") print name ".tracked" \
	}' "$(OWNERSHIP_CASES)")

.PHONY: all stage-1 stage-2 test test-stage-2 test-stage-2-json test-stage-2-cli test-stage-2-config test-stage-2-clang test-stage-2-diagnostics
.PHONY: test-stage-2-gcc test-stage-2-gcc-live test-stage-2-hashing test-stage-2-blake2b test-stage-2-native test-stage-2-process test-stage-2-version force-build-info
.PHONY: test-stage-2-inventory
.DELETE_ON_ERROR:
.SECONDARY:

all: stage-1

stage-1: $(STAGE1)/ownership.transfer

stage-2: $(BOOTSTRAP_COMPILER) $(SOURCE_SCAN) $(NATIVE_COMPILER)

# Preserve quoting in compiler settings until the runner splits them into arguments.
test-stage-2: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2: export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
test-stage-2: export WOVE_TEST_TRACKED_RUNTIME = $(STAGE1)/runtime.tracked.o
test-stage-2: stage-2 $(STAGE1)/runtime.tracked.o test-stage-2-hashing test-stage-2-json test-stage-2-cli test-stage-2-config test-stage-2-clang test-stage-2-diagnostics test-stage-2-gcc test-stage-2-blake2b test-stage-2-native test-stage-2-process test-stage-2-version test-stage-2-inventory
	"$(BOOTSTRAP_COMPILER)"
	"$(SOURCE_SCAN)"
	$(PYTHON) tests/stage-2/check_streams.py "$(SOURCE_SCAN)" \
		tests/stage-2/lexer_layouts.tsv tests/stage-2/lexer_errors.tsv tests/stage-2/source_forms.tsv \
		tests/stage-2/source_trees.tsv tests/stage-2/source_metadata.tsv tests/stage-2/source_renderings.tsv \
		tests/stage-2/source_decodings.tsv tests/stage-2/source_modules.tsv tests/stage-2/source_compilations.tsv \
		tests/stage-2/source_callers.tsv \
		stage-2/*.wove stage-2/source/*.wove stage-2/scalar/*.wove stage-2/emit/*.wove stage-2/c-stdlib/*.wove $(HASHING_SOURCES) $(BLAKE2B_SOURCE) \
		$(UTF8_MODULE) $(QUOTED_TEXT_MODULE) $(BYTE_MATCH_MODULE) $(VALUE_TEXT_MODULES) $(DECIMAL_WRITER) $(CONFIG_SOURCES) $(COMMAND_SOURCES) $(JOB_BUDGET_MODULE) \
		$(JSON_FRAME_MODULE) $(C_IDENTIFIER_MODULE) $(C_ARGUMENT_MODULE) $(CLANG_SOURCES) $(DIAGNOSTIC_SOURCES) $(NATIVE_CLI_SOURCES) $(NATIVE_STDIO) $(PROCESS_MODULES) \
		"$(SOURCE_PROBE_ROOT)" tests/stage-1/ownership.transfer.wove tests/stage-1/cfg.select.wove
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" \
		tests/stage-2/source_graphs.tsv tests/stage-2/scalar_graphs.tsv tests/stage-2/embed/cases.tsv tests/stage-2/test_discovery.tsv \
		tests/stage-2/native_alias/cases.tsv tests/stage-2/native_records/cases.tsv tests/stage-2/native_values/cases.tsv \
		tests/stage-2/native_macros/cases.tsv tests/stage-2/native_variadic/cases.tsv tests/stage-2/native_arrays/cases.tsv \
		tests/stage-2/native_scalar_refs/cases.tsv tests/stage-2/native_storage/cases.tsv tests/stage-2/native_rows/cases.tsv tests/stage-2/native_vectors/cases.tsv tests/stage-2/native_streams/cases.tsv tests/stage-2/native_paths/cases.tsv \
		tests/stage-2/graph/targets/cases.tsv

# Exercise the same byte and ID encoders in both stages and the native word digest.
test-stage-2-hashing: $(HASHING_PROGRAMS) $(HASHING_NATIVE_PROGRAMS)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/hashing" "$(HASHING_CASES)"
	"$(STAGE2)/hashing/native/base16"
	"$(STAGE2)/hashing/native/base32hex"

# Run the standalone JSON utilities with the same manifest runner as the seed.
test-stage-2-json: $(JSON_PROGRAMS)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/json" "$(JSON_CASES)"

# Check the standalone diagnostic serializer and JSON reader together.
test-stage-2-diagnostics: $(DIAGNOSTIC_PROGRAM)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/diagnostics" "$(DIAGNOSTIC_FIXTURES)/cases.tsv"

# Run the standalone typed argv fixtures with the seed manifest runner.
test-stage-2-cli: $(CLI_PROGRAMS) $(COMMAND_PROGRAM)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/cli" "$(CLI_CASES)"
	$(PYTHON) "$(COMMAND_FIXTURES)/check.py" "$(COMMAND_PROGRAM)" "$(COMMAND_FIXTURES)/cases.tsv"

# Run native CLI cases in isolated work directories with the configured C toolchain.
test-stage-2-native: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-native: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-native: $(NATIVE_COMPILER)
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/cases.tsv"
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/bin.tsv"

# Check generated metadata and version invalidation in an isolated build.
test-stage-2-version: $(NATIVE_COMPILER) $(BUILD_VERSION)
	+sh "$(VERSION_FIXTURES)/check.sh" "$(MAKE)" "$(abspath $(BUILD_DIR))" "$(NATIVE_COMPILER)" "$(BUILD_VERSION)" \
		"$(VERSION_FIXTURES)/cases.tsv" "$(abspath $(STAGE1))" "$(abspath $(SOURCE_SCAN))"

# Exercise the project loader with the checked-in config and standalone cases.
test-stage-2-config: $(CONFIG_PROGRAM)
	$(PYTHON) "$(CONFIG_FIXTURES)/check.py" "$(CONFIG_PROGRAM)" "$(CONFIG_FIXTURES)/cases.tsv"

# Run the generated inline-test entry against the separate Wove runner archive.
test-stage-2-inventory: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-inventory: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-inventory: export WOVE_TEST_RUNTIME = $(TEST_RUNTIME)
test-stage-2-inventory: $(SOURCE_SCAN) $(TEST_RUNTIME)
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" tests/stage-2/test_execution.tsv

# Keep generated runner helpers local to their translation unit.
$(TEST_RUNNER).c: $(TEST_RUNNER_ROOT) $(TEST_RUNNER_SOURCES) $(SOURCE_SCAN) $(NATIVE_DRIVER)/bootstrap.py | $(BUILD_VERSION)
	mkdir -p "$(@D)"
	$(PYTHON) "$(NATIVE_DRIVER)/bootstrap.py" "$(SOURCE_SCAN)" "$@" "$(BUILD_VERSION)" --private-helpers $(filter %.wove,$^)

# Expose only the host argument entry under the test-specific C name.
$(TEST_RUNNER).o: $(TEST_RUNNER).c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -D_XOPEN_SOURCE=700 -D_DARWIN_C_SOURCE -D_GNU_SOURCE -Dmain=wvrt_test_parent -I runtime -c "$<" -o "$@"

$(STAGE2)/test/wove_test.o: stage-2/test/wove_test.c stage-2/test/wove_test.h
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -c "$<" -o "$@"

$(TEST_RUNTIME): $(STAGE2)/test/wove_test.o $(TEST_RUNNER).o $(STAGE1)/runtime.o
	$(AR) rcs "$@" $^

# Run the Wove launcher and interleave unrelated launches during descriptor creation.
test-stage-2-process: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-process: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-process: export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
test-stage-2-process: $(SOURCE_SCAN)
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" "$(PROCESS_CASES)" "$(PROCESS_DESCRIPTOR_CASES)" "$(PROCESS_INHERITANCE_CASES)"

# Check generated C input and captured JSON separately from native command launching.
test-stage-2-clang: $(AST_PROBE) $(CLANG_TEMPLATE)
	sh "$(AST_FIXTURES)/check.sh" "$(AST_PROBE)" "$(CLANG_TEMPLATE)" "$(CLANG_FIXTURES)/cases.tsv" $(CLANG)

# Check an archived-format GCC capture through the shared AST lookup fixture.
test-stage-2-gcc: $(AST_PROBE)
	sh "$(AST_FIXTURES)/check.sh" "$(AST_PROBE)" "$(CLANG_TEMPLATE)" "$(GCC_FIXTURES)/cases.tsv"

# Build and load the plugin only on hosts with matching GNU GCC plugin headers.
test-stage-2-gcc-live: $(AST_PROBE) $(CLANG_TEMPLATE)
	sh "$(GCC_FIXTURES)/check-live.sh" "$(AST_PROBE)" "$(CLANG_TEMPLATE)" stage-2/gcc/inspect.cc "$(GCC)" "$(GCC_CXX)"

# Check incremental hashes and field framing against independent digest vectors.
test-stage-2-blake2b: $(BLAKE2B_PROBE)
	sh "$(BLAKE2B_FIXTURES)/check.sh" "$(BLAKE2B_PROBE)" "$(BLAKE2B_FIXTURES)/cases.tsv"

test: stage-1 test-stage-2 $(STAGE1)/runtime.cstr_cleanup $(addprefix $(STAGE1)/,$(TEST_PROGRAMS))
	"$(STAGE1)/runtime.cstr_cleanup"
	sh "$(OWNERSHIP_TESTS)/check.sh" "$(PYTHON)" "$(STAGE1)" "$(OWNERSHIP_CASES)"
	$(PYTHON) "$(OWNERSHIP_TESTS)/check_strings.py" "$(OWNERSHIP_TESTS)/strings.tsv"
	sh "$(OWNERSHIP_TESTS)/check_includes.sh" "$(STAGE1)/imports.c_headers.c" "$(OWNERSHIP_TESTS)/imports/c_headers/includes.txt"

# Track the imports active in each seed root.
$(STAGE1)/wovec.c: $(COMPILER_ROOT) $(BOOTSTRAP_WOVE_SOURCES) $(SEED)
$(SOURCE_PROBE_C): $(SOURCE_PROBE_ROOT) $(GRAPH_COMPILER_SOURCES) $(SEED)

# Replace the generated C only after the seed succeeds.
$(STAGE1)/wovec.c $(SOURCE_PROBE_C):
	mkdir -p "$(@D)"
	set -eu; \
	c_output="$@.tmp.$$$$"; \
	trap 'rm -f "$$c_output"' 0; \
	$(PYTHON) stage-1/wovec.py "$<" > "$$c_output"; \
	mv "$$c_output" "$@"

# Link the seed-built compiler entry with the shared runtime.
$(BOOTSTRAP_COMPILER): $(STAGE1)/wovec.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime $^ -o "$@"

# Compile the C harness with its generated fixture and the shared runtime.
$(SOURCE_SCAN): tests/stage-2/source_scan.probe.c $(SOURCE_PROBE_C) $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Check the current Git commit or explicit override on each native build.
force-build-info:

$(BUILD_VERSION): $(VERSION_GENERATOR) force-build-info
	@sh "$(VERSION_GENERATOR)" "$@"

# Compare version bytes because older Make releases ignore subsecond mtime changes.
define build_versioned
	@set -eu; \
	if [ ! -f "$@" ] || [ -n "$(filter-out force-build-info $(BUILD_VERSION),$?)" ] || \
		! cmp -s "$(BUILD_VERSION)" "$@.version"; then \
		$(1); \
		cp "$(BUILD_VERSION)" "$@.version"; \
	fi
endef

# Ask the seed-built Stage 2 graph emitter to compile the root with its embedded assets.
$(STAGE2)/wovec.c: $(COMPILER_ROOT) $(NATIVE_WOVE_SOURCES) $(BUNDLED_C_PROVIDERS) $(NATIVE_BUILD_ASSETS) $(SOURCE_SCAN) $(NATIVE_DRIVER)/bootstrap.py $(BUILD_VERSION) force-build-info
	$(call build_versioned,mkdir -p "$(@D)"; $(PYTHON) "$(NATIVE_DRIVER)/bootstrap.py" "$(SOURCE_SCAN)" "$@" "$(BUILD_VERSION)" "$(COMPILER_ROOT)" $(NATIVE_WOVE_SOURCES) --asset $(BUNDLED_C_PROVIDERS) $(NATIVE_BUILD_ASSETS))

# Link the emitted native compiler with the shared runtime.
$(NATIVE_COMPILER): $(STAGE2)/wovec.c $(STAGE1)/runtime.o $(BUILD_VERSION) force-build-info
	$(call build_versioned,$(CLANG) -std=c11 $(CFLAGS) -D_XOPEN_SOURCE=700 -D_DARWIN_C_SOURCE -D_GNU_SOURCE -I runtime $(filter-out $(BUILD_VERSION) force-build-info,$^) -o "$@")

# Build utility-only Wove programs with the ordinary Python seed and runtime.
$(HASHING_PROGRAMS): $(STAGE2)/hashing/%: $(HASHING_FIXTURES)/%.wove $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Track the lookup modules reached by the seed-built hashing fixtures.
$(STAGE2)/hashing/fnv1a: stage-2/hashing/fnv1a.wove $(HASHING_BITS)
$(STAGE2)/hashing/base32hex: $(BASE32HEX_SOURCE)
$(STAGE2)/hashing/base16: $(BASE16_SOURCES)

# Use the supplied native graph to exercise the shared encoders and fixed-array digest.
$(addsuffix .c,$(HASHING_NATIVE_PROGRAMS)): $(STAGE2)/hashing/native/%.c: $(HASHING_FIXTURES)/%.wove
$(STAGE2)/hashing/native/base32hex.c: $(BASE32HEX_SOURCE) $(BYTE_TEXT_MODULE)
$(STAGE2)/hashing/native/base16.c: $(BASE16_SOURCES)
$(BLAKE2B_PROBE).c: $(BLAKE2B_FIXTURES)/probe.wove $(BLAKE2B_SOURCE) $(BASE32HEX_SOURCE) $(BYTE_TEXT_MODULE)

$(addsuffix .c,$(HASHING_NATIVE_PROGRAMS) $(BLAKE2B_PROBE)): $(SOURCE_SCAN) $(NATIVE_DRIVER)/bootstrap.py | $(BUILD_VERSION)
	mkdir -p "$(@D)"
	$(PYTHON) "$(NATIVE_DRIVER)/bootstrap.py" "$(SOURCE_SCAN)" "$@" "$(BUILD_VERSION)" $(filter %.wove,$^)

$(HASHING_NATIVE_PROGRAMS): %: %.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$<" "$(STAGE1)/runtime.o" -o "$@"

$(BLAKE2B_PROBE): $(BLAKE2B_FIXTURES)/probe.probe.c $(BLAKE2B_PROBE).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Build JSON fixtures separately from the Stage 2 compiler entry.
$(JSON_PROGRAMS): $(STAGE2)/json/%: $(JSON_FIXTURES)/%.wove $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Track each standalone JSON fixture's shared text inputs.
$(JSON_WRITER_PROGRAMS): $(JSON_WRITER_SOURCES)
$(JSON_READER_PROGRAMS): $(JSON_READER_SOURCES)
$(STAGE2)/json/frame: $(JSON_FRAME_MODULE) $(JSON_READER_SOURCES)
$(STAGE2)/json/utf8: $(UTF8_MODULE)

# Build the serializer fixture without importing it into the CLI.
$(DIAGNOSTIC_PROGRAM): $(DIAGNOSTIC_FIXTURES)/json.wove $(DIAGNOSTIC_SOURCES) $(JSON_WRITER_SOURCES) \
	$(JSON_READER_SOURCES) $(VALUE_TEXT_MODULES) $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Build each argv fixture with its own test entry and the seed.
$(CLI_PROGRAMS): $(STAGE2)/cli/%: $(CLI_FIXTURES)/%.wove $(CLI_FIXTURES)/fixture_data.wove $(CLI_SOURCES) $(SEED) $(STAGE1)/runtime.o
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@.c"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$@.c" "$(STAGE1)/runtime.o" -o "$@"

# Only the compiler-options fixtures load and validate JSONC configuration.
$(CLI_OPTION_PROGRAMS): stage-2/cli/options.wove stage-2/config/compiler.wove $(JSON_READER_SOURCES)

$(STAGE2)/cli/jobs: $(JOB_BUDGET_MODULE)

# Build the project fixture with file input supplied by the probe.
$(CONFIG_PROGRAM).c: $(CONFIG_FIXTURES)/project.wove $(CONFIG_SOURCES) $(JSON_READER_SOURCES) $(VALUE_TEXT_MODULES) $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

$(CONFIG_PROGRAM): $(CONFIG_FIXTURES)/project.probe.c $(CONFIG_PROGRAM).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Supply an argv arena and config bytes to the standalone command fixture.
$(COMMAND_PROGRAM).c: $(CLI_FIXTURES)/command.wove $(COMMAND_SOURCES) $(CLI_SOURCES) $(CONFIG_SOURCES) $(JOB_BUDGET_MODULE) \
	$(JSON_READER_SOURCES) $(SOURCE_SCAN_MODULE) $(BYTE_MATCH_MODULE) $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

$(COMMAND_PROGRAM): $(CLI_FIXTURES)/command.probe.c $(COMMAND_PROGRAM).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Bring only the JSON and byte helpers into the standalone probe fixture.
$(AST_PROBE).c: $(AST_FIXTURES)/probe.wove $(CLANG_SOURCES) $(JSON_FRAME_MODULE) $(JSON_READER_SOURCES) $(VALUE_TEXT_MODULES) $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

$(AST_PROBE): $(AST_FIXTURES)/probe.probe.c $(AST_PROBE).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	mkdir -p "$(@D)"
	$(PYTHON) stage-1/wovec.py "$<" > "$@"

# Keep required providers in the build graph even when a source file is missing.
ROUND_TRIP_PROVIDERS := $(addprefix $(OWNERSHIP_TESTS)/imports/,left.wove right.wove nested/shared.wove)
RUNTIME_LOCATION_PROVIDER := $(OWNERSHIP_TESTS)/slice.load_negative.wove
C_HEADER_IMPORT_SOURCES := $(addprefix $(OWNERSHIP_TESTS)/,imports.c_headers.wove imports/c_headers/provider.wove)
C_HEADER_PROJECT_HEADERS := $(addprefix $(OWNERSHIP_TESTS)/imports/c_headers/,project.h link lookup.h tree/lookup.h tree/leaf/value.h)
MODULE_BINDINGS_PROVIDER := $(OWNERSHIP_TESTS)/modules/bindings.wove
MODULE_IMPORTS_PROVIDER := $(OWNERSHIP_TESTS)/modules/import_values.wove
MODULE_SELECT_SOURCES := $(addprefix $(OWNERSHIP_TESTS)/,module.select.wove modules/bootstrap.wove \
	modules/full.wove modules/testing.wove imports/nested/shared.wove)
TYPE_IMPORT_SOURCES := $(addprefix $(OWNERSHIP_TESTS)/,types.round_trip.wove types/base.wove types/forward.wove)
SOURCE_TREE_IMPORT := $(OWNERSHIP_TESTS)/types.source_tree.wove
$(STAGE1)/imports.round_trip.c: $(ROUND_TRIP_PROVIDERS)
$(STAGE1)/imports.host_declarations.c: $(OWNERSHIP_TESTS)/imports/nested/shared.wove
$(STAGE1)/imports.c_headers.c: $(C_HEADER_IMPORT_SOURCES) $(C_HEADER_PROJECT_HEADERS)
$(STAGE1)/imports.runtime_location.c: $(RUNTIME_LOCATION_PROVIDER)
$(STAGE1)/module.select.c: $(MODULE_SELECT_SOURCES)
$(STAGE1)/module.bindings.c: $(MODULE_BINDINGS_PROVIDER)
$(STAGE1)/module.imports.c: $(MODULE_IMPORTS_PROVIDER)
$(STAGE1)/types.round_trip.c: $(TYPE_IMPORT_SOURCES)
$(STAGE1)/types.source_tree.c: $(SOURCE_TREE_IMPORT) $(SOURCE_SCAN_MODULE) $(UTF8_MODULE) $(QUOTED_TEXT_MODULE) $(BYTE_MATCH_MODULE)

# Force the compiler to report missing inputs that Make can skip under .SECONDARY.
REQUIRED_BUILD_SOURCES := $(COMPILER_ROOT) $(SOURCE_PROBE_ROOT) $(NATIVE_WOVE_SOURCES) $(GRAPH_COMPILER_SOURCES) $(BUNDLED_C_PROVIDERS) $(NATIVE_BUILD_ASSETS) \
	$(JOB_BUDGET_MODULE) $(CLI_FIXTURES)/jobs.wove \
	$(ROUND_TRIP_PROVIDERS) $(RUNTIME_LOCATION_PROVIDER) $(MODULE_SELECT_SOURCES) $(MODULE_BINDINGS_PROVIDER) \
	$(MODULE_IMPORTS_PROVIDER) $(addprefix $(OWNERSHIP_TESTS)/,module.bindings.wove module.table_bounds.wove module.imports.wove) \
	$(TYPE_IMPORT_SOURCES) $(SOURCE_TREE_IMPORT) $(C_HEADER_IMPORT_SOURCES) $(C_HEADER_PROJECT_HEADERS)
.PHONY: $(filter-out $(wildcard $(REQUIRED_BUILD_SOURCES)),$(REQUIRED_BUILD_SOURCES))

$(STAGE1)/runtime.o: $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -c "$<" -o "$@"

$(STAGE1)/%.fixture.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -c "$<" -o "$@"

$(STAGE1)/%: $(STAGE1)/%.fixture.o $(STAGE1)/runtime.o
	$(CLANG) $(CFLAGS) $^ -o "$@"

# Inspect full scalar results and static storage in the generated translation unit.
$(addprefix $(STAGE1)/,scalar.round_trip scalar.convert scalar.wrapping scalar.compare module.bindings): $(STAGE1)/%: \
	$(OWNERSHIP_TESTS)/%.probe.c $(STAGE1)/%.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(STAGE1)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(STAGE1)/runtime.tracked.o: $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -Dmalloc=tracked_malloc -Dcalloc=tracked_calloc \
		-Dfree=tracked_free -c "$<" -o "$@"

# Simulate an allocator that clobbers errno when native call scratch is released.
$(STAGE1)/runtime.cstr_cleanup: tests/support/cstr_cleanup.c $(STAGE1)/runtime.tracked.o $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime $(filter-out %.h,$^) -o "$@"

$(STAGE1)/%.tracked.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -Dmain=wove_test_main -c "$<" -o "$@"

# Relink tracked fixtures when the shared allocator probe changes.
$(STAGE1)/%.tracked: $(OWNERSHIP_TESTS)/allocations.c $(STAGE1)/%.tracked.o $(STAGE1)/runtime.tracked.o tests/support/array_tracking.h
	$(CLANG) -std=c11 $(CFLAGS) $(filter-out %.h,$^) -o "$@"
