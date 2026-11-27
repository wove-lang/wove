# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

PYTHON ?= python3
CLANG ?= clang
CFLAGS ?= -O2 -Wall -Wextra
BUILD_DIR ?= build
VSCE ?= npx --yes @vscode/vsce@4.1.1
VSCODE_EXTENSION := editors/vscode
VSCODE_PACKAGE := $(BUILD_DIR)/editors/vscode/wove-language.vsix
VSCODE_GRAMMAR := $(VSCODE_EXTENSION)/syntaxes/wove.tmLanguage.json
VSCODE_SOURCES := $(addprefix $(VSCODE_EXTENSION)/,package.json package.json.license \
	language-configuration.json language-configuration.json.license README.md LICENSE)
VSCODE_LICENSES := LICENSE-MIT LICENSE-APACHE

BUILD_VERSION := $(BUILD_DIR)/version.txt
VERSION_GENERATOR := tools/build-info.sh
VERSION_FIXTURES := tests/stage-2/version

STAGE1 := $(BUILD_DIR)/stage-1
STAGE2 := $(BUILD_DIR)/stage-2
BOOTSTRAP_COMPILER := $(STAGE1)/wovec
COMPILER_ROOT := stage-2/wovec.wove
NATIVE_COMPILER := $(STAGE2)/wovec
FINAL_COMPILER := $(BUILD_DIR)/wovec
SELF_HOST_SOURCE := $(BUILD_DIR)/self-host-source
SELF_HOST_ENTRY := $(SELF_HOST_SOURCE)/$(COMPILER_ROOT)
SELF_HOST_VERIFIED := $(STAGE2)/wovec-self-hosted
SELF_HOST_SMOKE := $(BUILD_DIR)/self-host-smoke
SELF_HOST_INPUTS := tools/stage-compiler-inputs.sh
SELF_HOST_FIXTURES := tests/stage-2/self_host
# Track files and directories in the copied trees. The /. spelling avoids the stage-2 goal.
COMPILER_INPUTS := stage-2/. $(sort $(filter-out stage-2,$(shell find stage-2 -print)))
SELF_HOST_TREE_INPUTS := $(COMPILER_INPUTS) $(sort $(shell find runtime -print))
BUILD_CLANG := tools/run-build-clang.sh
HASHING_FIXTURES := tests/stage-2/hashing
HASHING_CASES := $(HASHING_FIXTURES)/cases.tsv
HASHING_PROGRAMS := $(addprefix $(STAGE2)/hashing/,fnv1a base32hex base16)
HASHING_NATIVE_PROGRAMS := $(addprefix $(STAGE2)/hashing/native/,fnv1a base32hex base16)
BLAKE2B_FIXTURES := $(HASHING_FIXTURES)/blake2b
BLAKE2B_PROBE := $(STAGE2)/hashing/blake2b
JSON_FIXTURES := tests/stage-2/json
JSON_CASES := $(JSON_FIXTURES)/cases.tsv
JSON_PROGRAMS := $(addprefix $(STAGE2)/json/,writer writer_errors reader reader_errors utf8 frame)
DIAGNOSTIC_FIXTURES := tests/stage-2/diagnostics
DIAGNOSTIC_PROGRAM := $(STAGE2)/diagnostics/json
CLI_FIXTURES := tests/stage-2/cli
CLI_CASES := $(CLI_FIXTURES)/cases.tsv
CLI_PROGRAMS := $(addprefix $(STAGE2)/cli/,argv argv_errors host_entry jobs options options_errors)
CONFIG_FIXTURES := tests/stage-2/config
CONFIG_PROGRAM := $(STAGE2)/config/project
COMMAND_FIXTURES := $(CLI_FIXTURES)/command
COMMAND_PROGRAM := $(STAGE2)/cli/command
NATIVE_CLI_FIXTURES := $(CLI_FIXTURES)/native
WORKER_FIXTURES := $(NATIVE_CLI_FIXTURES)/workers
WORKER_RUNTIME := $(STAGE2)/workers/runtime.o
WORKER_PROBE := $(STAGE2)/workers/failure
WORKER_QUEUE_PROBE := $(STAGE2)/workers/queue-failure
WORKER_POOL_PROBE := $(STAGE2)/workers/pool
PROCESS_FIXTURES := tests/stage-2/process
OBJECT_JOB_FIXTURES := tests/stage-2/cli/native/object_jobs
STAGED_BUILD_FIXTURES := $(NATIVE_CLI_FIXTURES)/staged_build
PROCESS_CASES := $(PROCESS_FIXTURES)/cases.tsv
PROCESS_DESCRIPTOR_CASES := $(PROCESS_FIXTURES)/descriptors.tsv
PROCESS_INHERITANCE_CASES := $(PROCESS_FIXTURES)/inheritance.tsv
TEST_RUNNER_ROOT := stage-2/test/runner.wove
TEST_RUNNER := $(STAGE2)/test/runner
TEST_RUNTIME := $(STAGE2)/test/libwove_test.a
CLANG_FIXTURES := tests/stage-2/clang
CLANG_TEMPLATE := stage-2/clang/templates/probe_header.tmpl.h
AST_LIVE := $(STAGE2)/ast/live
NUMBERED_HEADERS := $(STAGE2)/numbered-headers
AST_FIXTURES := tests/support/ast
AST_PROBE := $(STAGE2)/ast/probe
SOURCE_SCAN := $(STAGE2)/source_scan
SOURCE_PROBE_ROOT := tests/stage-2/source_scan.wove
SOURCE_PROBE_C := $(dir $(SOURCE_SCAN))source_scan.c
SEED_UTILITY_PROGRAMS := $(HASHING_PROGRAMS) $(JSON_PROGRAMS) $(CLI_PROGRAMS) $(DIAGNOSTIC_PROGRAM)
SEED_UTILITY_C := $(addsuffix .c,$(SEED_UTILITY_PROGRAMS) $(CONFIG_PROGRAM) $(COMMAND_PROGRAM))
NATIVE_FIXTURE_C := $(addsuffix .c,$(TEST_RUNNER) $(AST_LIVE) $(HASHING_NATIVE_PROGRAMS) $(BLAKE2B_PROBE))
SEED_TARGET := stage-1/target.py tools/run-build-clang.sh
SEED := stage-1/wovec.py stage-1/syntax.py stage-1/compiler.py stage-1/make_deps.py $(SEED_TARGET)
SEED_COMPILER = WOVE_BUILD_CLANG="$(CLANG) -std=c11 $(CFLAGS)" $(PYTHON) stage-1/wovec.py
SEED_EMIT := stage-1/emit.py
SEED_EMIT_SOURCES := $(SEED_EMIT) stage-1/inputs.py stage-1/syntax.py stage-1/make_deps.py
NATIVE_FIXTURE_TOOLS := $(SOURCE_SCAN) $(SEED_EMIT_SOURCES)
EMIT_NATIVE_FIXTURE = $(PYTHON) "$(SEED_EMIT)" "$(SOURCE_SCAN)"
RUNTIME := runtime/wove_runtime.c
RUNTIME_HEADER := runtime/wove_runtime.h
OWNERSHIP_TESTS := tests/stage-1
OWNERSHIP_CASES := $(OWNERSHIP_TESTS)/cases.tsv

# Build native programs for executable cases. The runner handles compile-only rejections.
TEST_PROGRAMS := $(shell awk '$$1 ~ /^(run|run-argv|tracked|bounds-trap|conversion-trap|range-trap|shift-trap|division-trap)$$/ { \
	name = $$2; sub(/\.wove$$/, "", name); print name; \
	if ($$1 == "tracked") print name ".tracked" \
	}' "$(OWNERSHIP_CASES)")

STAGE1_FIXTURE_C := $(addprefix $(STAGE1)/,$(addsuffix .c,$(sort $(filter-out %.tracked,$(TEST_PROGRAMS)))))
SEED_GRAPH_C := $(SOURCE_PROBE_C) $(SEED_UTILITY_C) $(NATIVE_FIXTURE_C) $(AST_PROBE).c $(STAGE1_FIXTURE_C)

# Add a "# suite: <group>" comment to schedule a TSV fixture in one of these groups.
# Read the comments only for goals that schedule graph or native manifests.
ifneq ($(filter test test-stage-2 test-stage-2-language test-stage-2-interop test-stage-2-native \
	test-stage-2-native-cli test-stage-2-native-interop test-graph/% test-native/%,$(MAKECMDGOALS)),)
SUITE_TAG := \# suite:
SUITE_CASES := $(sort $(shell find tests/stage-2 -type f -name '*.tsv' \
	-exec awk 'index($$0, "$(SUITE_TAG) ") == 1 { print $$3 ":" FILENAME }' {} +))
endif

# Map each marked manifest to its existing public target within the fixture tree.
suite_targets = $(patsubst $(1):$(2)/%,$(3)/%,$(filter $(1):$(2)/%,$(SUITE_CASES)))
GRAPH_LANGUAGE_TESTS := $(call suite_targets,graph-language,tests/stage-2,test-graph)
GRAPH_INTEROP_TESTS := $(call suite_targets,graph-interop,tests/stage-2,test-graph)
GRAPH_TRACKED_TESTS := $(call suite_targets,graph-tracked,tests/stage-2,test-graph)
GRAPH_TESTS := $(GRAPH_LANGUAGE_TESTS) $(GRAPH_INTEROP_TESTS) $(GRAPH_TRACKED_TESTS)
NATIVE_CLI_TESTS := $(call suite_targets,native-cli,$(NATIVE_CLI_FIXTURES),test-native)
NATIVE_INTEROP_TESTS := $(call suite_targets,native-interop,$(NATIVE_CLI_FIXTURES),test-native)

.PHONY: all stage-1 stage-2 self-host test test-stage-2 test-stage-2-json test-stage-2-cli test-stage-2-config test-stage-2-clang test-stage-2-diagnostics
.PHONY: test-stage-2-hashing test-stage-2-blake2b test-stage-2-native test-stage-2-f80 test-stage-2-workers test-stage-2-process test-stage-2-version force-build-info
.PHONY: test-stage-2-inventory vscode test-stage-1 test-stage-2-source test-stage-2-language test-stage-2-utilities
.PHONY: test-stage-2-interop test-stage-2-runtime test-stage-2-self-host test-stage-2-array-copy
.PHONY: test-stage-2-native-cli test-stage-2-native-interop test-stage-2-self-host-incremental $(GRAPH_TESTS)
.DELETE_ON_ERROR:
.SECONDARY:

all: $(FINAL_COMPILER)

# Package the editor extension without building the compiler.
vscode: $(VSCODE_PACKAGE)

# Stage only the extension files and both license texts for the packager.
$(VSCODE_PACKAGE): $(VSCODE_SOURCES) $(VSCODE_GRAMMAR) $(VSCODE_GRAMMAR).license $(VSCODE_LICENSES)
	mkdir -p "$(@D)"
	set -eu; \
	package_dir=$$(mktemp -d "$(@D)/package.XXXXXX"); \
	trap 'rm -rf "$$package_dir"' EXIT; \
	mkdir "$$package_dir/syntaxes"; \
	cp $(VSCODE_SOURCES) $(VSCODE_LICENSES) "$$package_dir/"; \
	cp "$(VSCODE_GRAMMAR)" "$(VSCODE_GRAMMAR).license" "$$package_dir/syntaxes/"; \
	(cd "$$package_dir" && $(VSCE) package --no-dependencies --out wove-language.vsix); \
	mv "$$package_dir/wove-language.vsix" "$@"

stage-1: $(BOOTSTRAP_COMPILER)

stage-2: $(NATIVE_COMPILER)

# Compare all native emissions from the same graph and exercise the final executable.
self-host: export WOVE_BUILD_CLANG = $(CLANG) $(CFLAGS)
self-host: $(FINAL_COMPILER) $(SELF_HOST_SOURCE)/.ready
	CLANG="$(abspath $(BUILD_CLANG))" "$(FINAL_COMPILER)" cc "$(SELF_HOST_ENTRY)" -o "$(SELF_HOST_VERIFIED)"
	cmp "$(STAGE2)/.wovec-cache/default/wovec/bin/main.c" "$(BUILD_DIR)/.wovec-cache/default/wovec/bin/main.c"
	cmp "$(STAGE2)/.wovec-cache/default/wovec/bin/main.c" "$(STAGE2)/.wovec-cache/default/wovec-self-hosted/bin/main.c"
	CLANG="$(abspath $(BUILD_CLANG))" "$(FINAL_COMPILER)" bin "$(NATIVE_CLI_FIXTURES)/input.wove" -o "$(SELF_HOST_SMOKE)"
	@status=0; "$(SELF_HOST_SMOKE)" || status=$$?; test "$$status" -eq 42

# Keep the complete local suite and expose independent groups for CI.
test: test-stage-1 test-stage-2
test-stage-2: stage-2 test-stage-2-language test-stage-2-utilities test-stage-2-native-cli \
	test-stage-2-interop test-stage-2-runtime test-stage-2-version
test-stage-2-language: test-stage-2-source $(GRAPH_LANGUAGE_TESTS) $(GRAPH_TRACKED_TESTS)
test-stage-2-utilities: test-stage-2-hashing test-stage-2-json test-stage-2-cli test-stage-2-config \
	test-stage-2-diagnostics test-stage-2-blake2b
test-stage-2-interop: $(GRAPH_INTEROP_TESTS) test-stage-2-clang test-stage-2-native-interop test-stage-2-f80
test-stage-2-runtime: test-stage-2-array-copy test-stage-2-process test-stage-2-workers test-stage-2-inventory test-stage-2-object-jobs test-stage-2-staged-build
test-stage-2-self-host: self-host test-stage-2-version

# Exercise real incremental builds in a disposable source archive.
test-stage-2-self-host-incremental: $(BUILD_VERSION)
	sh "$(SELF_HOST_FIXTURES)/check.sh" "$(MAKE)" "$(abspath $(BUILD_DIR))" "$(BUILD_VERSION)"

# Preserve quoting in compiler settings until the runner splits them into arguments.
test-stage-2-source: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-source: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-source: export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
test-stage-2-source: export WOVE_TEST_TRACKED_RUNTIME = $(STAGE1)/runtime.tracked.o
test-stage-2-source: $(SOURCE_SCAN) $(STAGE1)/runtime.tracked.o $(NUMBERED_HEADERS)
	"$(SOURCE_SCAN)"
	$(PYTHON) tests/stage-2/check_units.py "$(SOURCE_SCAN)" tests/stage-2/numbered_units/cases.jsonc "$(NUMBERED_HEADERS)"
	$(PYTHON) tests/stage-2/check_streams.py "$(SOURCE_SCAN)" \
		tests/stage-2/lexer_layouts.tsv tests/stage-2/lexer_errors.tsv tests/stage-2/source_forms.tsv \
		tests/stage-2/source_trees.tsv tests/stage-2/source_metadata.tsv tests/stage-2/source_renderings.tsv \
		tests/stage-2/source_decodings.tsv tests/stage-2/source_modules.tsv tests/stage-2/source_compilations.tsv \
		tests/stage-2/source_callers.tsv \
		$(filter %.wove,$(COMPILER_INPUTS)) \
		"$(SOURCE_PROBE_ROOT)" tests/stage-1/ownership.transfer.wove tests/stage-1/cfg.select.wove

# Give each isolated graph manifest a Make target so checks can run side by side.
$(GRAPH_TESTS): export WOVE_TEST_CLANG = $(CLANG)
$(GRAPH_TESTS): export WOVE_TEST_CFLAGS = $(CFLAGS)
$(GRAPH_TESTS): test-graph/%: tests/stage-2/% $(SOURCE_SCAN)
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" "$<"

$(GRAPH_LANGUAGE_TESTS) $(GRAPH_INTEROP_TESTS): export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
$(GRAPH_LANGUAGE_TESTS) $(GRAPH_INTEROP_TESTS): $(STAGE1)/runtime.o
$(GRAPH_TRACKED_TESTS): export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.tracked.o
$(GRAPH_TRACKED_TESTS): $(STAGE1)/runtime.tracked.o

# Exercise the same byte and ID encoders in both stages and the native word digest.
test-stage-2-hashing: $(HASHING_PROGRAMS) $(HASHING_NATIVE_PROGRAMS)
	sh tests/stage-1/check.sh "$(PYTHON)" "$(STAGE2)/hashing" "$(HASHING_CASES)"
	"$(STAGE2)/hashing/native/fnv1a"
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

# Keep the aggregate entry point for installed-compiler and worker checks.
test-stage-2-native: test-stage-2-native-cli test-stage-2-native-interop test-stage-2-f80 test-stage-2-workers

test-stage-2-native-cli: $(NATIVE_CLI_TESTS) test-native-discovery test-native-debug test-native-parallel
test-stage-2-native-interop: $(NATIVE_INTEROP_TESTS) test-native-numeric test-native-library test-native-emission test-native-macro-modes test-native-fcntl-optional test-native-limits test-native-cpio test-native-tar test-native-utsname test-native-poll test-native-times test-native-resource

NATIVE_TESTS := $(NATIVE_CLI_TESTS) $(NATIVE_INTEROP_TESTS) \
	test-native-discovery test-native-debug test-native-parallel test-native-numeric test-native-library test-native-emission test-native-macro-modes test-native-fcntl-optional test-native-limits test-native-cpio test-native-tar test-native-utsname test-native-poll test-native-times test-native-resource
.PHONY: $(NATIVE_TESTS)

$(NATIVE_TESTS): export WOVE_TEST_CLANG = $(CLANG)
$(NATIVE_TESTS): export WOVE_TEST_CFLAGS = $(CFLAGS)
$(NATIVE_TESTS): export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
$(NATIVE_TESTS): export WOVE_BUILD_CLANG = $(CLANG)
$(NATIVE_TESTS): $(NATIVE_COMPILER) $(STAGE1)/runtime.o

# Each manifest stages the compiler and its inputs in its own temporary directory.
$(NATIVE_CLI_TESTS) $(NATIVE_INTEROP_TESTS): test-native/%: $(NATIVE_CLI_FIXTURES)/%
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/$*"

# Exercise installed opt-in bin through the real compiler and a recorded Clang.
test-native-parallel:
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/parallel/check.py" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/parallel/cases.jsonc"

test-native-discovery:
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/discovery/check.py" "$(BOOTSTRAP_COMPILER)" "$(NATIVE_COMPILER)"

test-native-debug:
	sh "$(NATIVE_CLI_FIXTURES)/debug/check.sh" "$(PYTHON)" "$(abspath $(NATIVE_COMPILER))" "$(NATIVE_CLI_FIXTURES)/debug/cases.tsv"

test-native-numeric:
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/half/check.py" "$(NATIVE_COMPILER)"
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/quad/check.py" "$(NATIVE_COMPILER)"
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/int128/check.py" "$(NATIVE_COMPILER)"
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/int256/check.py" "$(NATIVE_COMPILER)" runtime/probes/wide_integer.c

test-native-library: $(SOURCE_SCAN)
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/library/check.py" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/library/cases.jsonc"
	$(PYTHON) tests/stage-2/check_library_units.py "$(SOURCE_SCAN)" "$(NATIVE_COMPILER)" tests/stage-2/numbered_units/library.jsonc

test-native-emission:
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/emission/check.py" "$(BOOTSTRAP_COMPILER)" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/emission/cases.tsv"

test-native-macro-modes:
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/check_macro_modes.py" "$(NATIVE_COMPILER)" "$(NATIVE_CLI_FIXTURES)/macro-modes.tsv"

test-native-fcntl-optional:
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/fcntl/check_optional.py" "$(NATIVE_COMPILER)"

test-native-limits:
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/limits/check.py" "$(NATIVE_COMPILER)" runtime

test-native-cpio test-native-tar: test-native-%:
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/check_native_header.py" "$(NATIVE_COMPILER)" runtime "$(NATIVE_CLI_FIXTURES)/$*" ir

test-native-utsname test-native-poll test-native-times test-native-resource: test-native-%:
	CLANG="$(abspath $(BUILD_CLANG))" $(PYTHON) "$(NATIVE_CLI_FIXTURES)/check_native_header.py" "$(NATIVE_COMPILER)" runtime "$(NATIVE_CLI_FIXTURES)/$*" run

# Check f80 admission and run the native cases when the selected tool supports extended precision.
test-stage-2-f80: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-f80: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-f80: export WOVE_BUILD_CLANG = $(CLANG)
test-stage-2-f80: $(NATIVE_COMPILER)
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/extended/check.py" "$(NATIVE_COMPILER)"

# Run the Wove transfer fixtures and the host probe for thread API failures.
test-stage-2-workers: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-workers: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-workers: export WOVE_BUILD_CLANG = $(CLANG)
test-stage-2-workers: export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
test-stage-2-workers: export WOVE_TEST_WORKER_RUNTIME = $(WORKER_RUNTIME)
test-stage-2-workers: $(NATIVE_COMPILER) $(SOURCE_SCAN) $(WORKER_PROBE) $(WORKER_QUEUE_PROBE) $(WORKER_POOL_PROBE) $(NUMBERED_HEADERS)
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" tests/stage-2/worker_records/cases.tsv
	WOVE_TEST_RUNTIME="$(WORKER_RUNTIME)" $(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" tests/stage-2/worker_records/ledger.tsv
	$(PYTHON) tests/stage-2/check_units.py "$(SOURCE_SCAN)" tests/stage-2/numbered_units/workers.jsonc "$(NUMBERED_HEADERS)"
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" "$(NATIVE_COMPILER)" "$(WORKER_FIXTURES)/cases.tsv"
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" --host "$(WORKER_PROBE)" "$(WORKER_FIXTURES)/failure.tsv"
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" --host "$(WORKER_QUEUE_PROBE)" "$(WORKER_FIXTURES)/queue-failure.tsv"
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" --host "$(WORKER_POOL_PROBE)" "$(WORKER_FIXTURES)/pool.tsv"

# Intercept thread creation and join in the runtime object used by the worker probe.
$(WORKER_RUNTIME): $(RUNTIME) $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -pthread -Dpthread_create=fixture_worker_create \
		-Dpthread_join=fixture_worker_join -Dmalloc=fixture_worker_malloc -Dfree=fixture_worker_free \
		-Dcalloc=fixture_worker_calloc -c "$<" -o "$@"

# The probe can still call the original thread APIs when a case allows them.
$(WORKER_PROBE) $(WORKER_QUEUE_PROBE) $(WORKER_POOL_PROBE): $(STAGE2)/workers/%: $(WORKER_FIXTURES)/%.c $(WORKER_RUNTIME) $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -pthread -I runtime $(filter-out %.h,$^) -o "$@"

# Check generated metadata and version invalidation in an isolated build.
test-stage-2-version: $(NATIVE_COMPILER) $(BUILD_VERSION)
	+sh "$(VERSION_FIXTURES)/check.sh" "$(MAKE)" "$(abspath $(BUILD_DIR))" "$(NATIVE_COMPILER)" "$(BUILD_VERSION)" \
		"$(VERSION_FIXTURES)/cases.tsv"

# Exercise the project loader with the checked-in config and standalone cases.
test-stage-2-config: $(CONFIG_PROGRAM)
	$(PYTHON) "$(CONFIG_FIXTURES)/check.py" "$(CONFIG_PROGRAM)" "$(CONFIG_FIXTURES)/cases.tsv"

# Run the generated inline-test entry against the separate Wove runner archive.
test-stage-2-inventory: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-inventory: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-inventory: export WOVE_TEST_RUNTIME = $(TEST_RUNTIME)
test-stage-2-inventory: $(SOURCE_SCAN) $(TEST_RUNTIME) $(NUMBERED_HEADERS)
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" tests/stage-2/test_execution.tsv
	$(PYTHON) tests/stage-2/check_units.py "$(SOURCE_SCAN)" tests/stage-2/numbered_units/test_dispatch.jsonc "$(NUMBERED_HEADERS)"

# Keep generated runner helpers local to their translation unit.
$(TEST_RUNNER).c: $(TEST_RUNNER_ROOT) $(NATIVE_FIXTURE_TOOLS)
	$(call emit_seed_graph,$(EMIT_NATIVE_FIXTURE) --private-helpers)

# Expose only the host argument entry under the test-specific C name.
$(TEST_RUNNER).o: $(TEST_RUNNER).c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -D_XOPEN_SOURCE=700 -D_DARWIN_C_SOURCE -D_GNU_SOURCE -Dmain=wvrt_test_parent -I runtime -c "$<" -o "$@"

$(STAGE2)/test/wove_test.o: stage-2/test/wove_test.c stage-2/test/wove_test.h
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -c "$<" -o "$@"

$(TEST_RUNTIME): $(STAGE2)/test/wove_test.o $(TEST_RUNNER).o $(STAGE1)/runtime.o
	$(AR) rcs "$@" $^

# Check runtime array copy directly with a C fixture.
test-stage-2-array-copy: $(STAGE2)/array-copy-runtime
	"$(STAGE2)/array-copy-runtime"

# Run the Wove launcher and interleave unrelated launches during descriptor creation.
test-stage-2-process: export WOVE_TEST_CLANG = $(CLANG)
test-stage-2-process: export WOVE_TEST_CFLAGS = $(CFLAGS)
test-stage-2-process: export WOVE_TEST_RUNTIME = $(STAGE1)/runtime.o
test-stage-2-process: $(SOURCE_SCAN)
	$(PYTHON) tests/stage-2/check_graphs.py "$(SOURCE_SCAN)" "$(PROCESS_CASES)" "$(PROCESS_DESCRIPTOR_CASES)" "$(PROCESS_INHERITANCE_CASES)"

# Exercise prepared native jobs without connecting the runner to compiler CLI scheduling.
.PHONY: test-stage-2-object-jobs
test-stage-2-object-jobs: $(STAGE2)/object-jobs
	$(PYTHON) "$(NATIVE_CLI_FIXTURES)/check.py" --host "$<" "$(OBJECT_JOB_FIXTURES)/cases.tsv"

$(STAGE2)/object-jobs: export WOVE_BUILD_CLANG = $(CLANG) $(CFLAGS)
$(STAGE2)/object-jobs: $(addprefix $(OBJECT_JOB_FIXTURES)/,main.wove tool.py scenarios.jsonc) $(NATIVE_COMPILER)
	CLANG="$(abspath $(BUILD_CLANG))" "$(NATIVE_COMPILER)" bin "$<" -o "$@"

# Exercise object compilation and linking from an already prepared private directory.
.PHONY: test-stage-2-staged-build
test-stage-2-staged-build: export WOVE_BUILD_CLANG = $(CLANG) $(CFLAGS)
test-stage-2-staged-build: $(STAGE2)/staged-build
	$(PYTHON) "$(STAGED_BUILD_FIXTURES)/check.py" "$<" "$(abspath $(BUILD_CLANG))" "$(STAGED_BUILD_FIXTURES)/cases.jsonc"

$(STAGE2)/staged-build: export WOVE_BUILD_CLANG = $(CLANG) $(CFLAGS)
$(STAGE2)/staged-build: $(STAGED_BUILD_FIXTURES)/main.wove $(NATIVE_COMPILER)
	CLANG="$(abspath $(BUILD_CLANG))" "$(NATIVE_COMPILER)" bin "$<" -o "$@"

# Check generated C, archived JSON, and live tool invocation.
test-stage-2-clang: $(AST_PROBE) $(AST_LIVE) $(CLANG_TEMPLATE)
	sh "$(AST_FIXTURES)/check.sh" "$(AST_PROBE)" "$(CLANG_TEMPLATE)" "$(CLANG_FIXTURES)/cases.tsv" $(CLANG)
	sh "$(CLANG_FIXTURES)/check-live.sh" "$(AST_LIVE)" "$(CLANG_FIXTURES)/live.tsv" $(CLANG)

# Build the live Clang fixture with its declared probe template embed.
$(AST_LIVE).c: $(AST_FIXTURES)/live.wove $(NATIVE_FIXTURE_TOOLS)
	$(call emit_seed_graph,$(EMIT_NATIVE_FIXTURE))

$(AST_LIVE): $(AST_LIVE).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -D_XOPEN_SOURCE=700 -D_DARWIN_C_SOURCE -D_GNU_SOURCE -I runtime $^ -o "$@"

# Check incremental hashes and field framing against independent digest vectors.
test-stage-2-blake2b: $(BLAKE2B_PROBE)
	sh "$(BLAKE2B_FIXTURES)/check.sh" "$(BLAKE2B_PROBE)" "$(BLAKE2B_FIXTURES)/cases.tsv"

# Exercise the seed's programs, diagnostics, and emitted headers independently.
test-stage-1: stage-1 $(STAGE1)/runtime.cstr_cleanup $(addprefix $(STAGE1)/,$(TEST_PROGRAMS))
	"$(STAGE1)/runtime.cstr_cleanup"
	sh "$(OWNERSHIP_TESTS)/check.sh" "$(PYTHON)" "$(STAGE1)" "$(OWNERSHIP_CASES)"
	$(PYTHON) "$(OWNERSHIP_TESTS)/check_strings.py" "$(OWNERSHIP_TESTS)/strings.tsv"
	sh "$(OWNERSHIP_TESTS)/check_includes.sh" "$(STAGE1)/imports.c_headers.c" "$(OWNERSHIP_TESTS)/imports/c_headers/includes.txt"

# Build the compiler from its ordinary entry with the seed-built Wove emitter.
$(STAGE1)/wovec.c: export WOVE_BUILD_CLANG = $(CLANG) -std=c11 $(CFLAGS)
$(STAGE1)/wovec.c: $(SOURCE_SCAN) $(SELF_HOST_SOURCE)/.ready stage-1/build.py $(SEED_EMIT_SOURCES) stage-1/target.py $(BUILD_CLANG) $(BUILD_VERSION) force-build-info
	$(call build_versioned,mkdir -p "$(@D)"; c_output="$@.tmp.$$$$"; \
		trap 'rm -f "$$c_output"' 0; WOVE_BUILD_DIR="$(abspath $(BUILD_DIR))" $(PYTHON) stage-1/build.py > "$$c_output"; \
		mv "$$c_output" "$@")

# Publish the generated pair after the seed has resolved and emitted its graph.
define emit_seed_graph
mkdir -p "$(@D)"
set -eu; \
c_output="$@.tmp.$$$$"; deps="$@.d.tmp.$$$$"; \
trap 'rm -f "$$c_output" "$$deps"' 0; \
$(1) --make-deps "$$deps" "$<" > "$$c_output"; \
mv "$$deps" "$@.d"; \
mv "$$c_output" "$@"
endef

$(SOURCE_PROBE_C): $(SOURCE_PROBE_ROOT) $(SEED)
	$(call emit_seed_graph,$(SEED_COMPILER))

$(SEED_UTILITY_C): $(STAGE2)/%.c: tests/stage-2/%.wove $(SEED)
	$(call emit_seed_graph,$(SEED_COMPILER))

# Include the resolved inputs. A missing sidecar forces regeneration, and its timestamp is irrelevant.
.PHONY: force-seed-deps
define seed_graph_dependencies
$(1): $(if $(wildcard $(1).d),,force-seed-deps)
wove_seed_target := $(1)
-include $(wildcard $(1).d)
endef
$(foreach unit,$(SEED_GRAPH_C),$(eval $(call seed_graph_dependencies,$(unit))))

# Link the first compiler with the ordinary runtime.
$(BOOTSTRAP_COMPILER): $(STAGE1)/wovec.c $(RUNTIME) $(RUNTIME_HEADER)
	@printf 'Building %s\n' "$@"
	$(CLANG) -std=c11 $(CFLAGS) -D_XOPEN_SOURCE=700 -D_DARWIN_C_SOURCE -D_GNU_SOURCE -I runtime \
		-include "$(RUNTIME)" "$<" -o "$@"

# Compile the C harness with its generated fixture and the shared runtime.
$(SOURCE_SCAN): tests/stage-2/source_scan.probe.c $(SOURCE_PROBE_C) $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

$(NUMBERED_HEADERS): export WOVE_BUILD_CLANG = $(CLANG) $(CFLAGS)
# All of this fixture's imports are in the tree already tracked by the native compiler.
$(NUMBERED_HEADERS): tests/stage-2/numbered_units/bridges/check.wove $(NATIVE_COMPILER) $(BUILD_CLANG)
	CLANG="$(abspath $(BUILD_CLANG))" "$(NATIVE_COMPILER)" bin "$<" -o "$@"

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

# Keep the relative version and runtime embeds inside the selected build directory.
$(SELF_HOST_SOURCE)/.ready: $(SELF_HOST_INPUTS) $(COMPILER_ROOT) $(SELF_HOST_TREE_INPUTS) $(BUILD_VERSION) force-build-info
	$(call build_versioned,sh "$(SELF_HOST_INPUTS)" "$(@D)" "$(BUILD_VERSION)")

# Preserve the configured Make command and forward each flag as one native CLI option.
define build_with_wovec
	$(call build_versioned,printf 'Building %s\n' "$@"; set -- $(CFLAGS); count=$$#; \
		for flag do set -- "$$@" --cflag "$$flag"; done; shift "$$count"; \
		CLANG="$(abspath $(BUILD_CLANG))" "$<" bin "$(SELF_HOST_ENTRY)" -o "$@" "$$@")
endef

$(NATIVE_COMPILER) $(FINAL_COMPILER): export WOVE_BUILD_CLANG = $(CLANG)
$(NATIVE_COMPILER): $(BOOTSTRAP_COMPILER) $(SELF_HOST_SOURCE)/.ready $(BUILD_CLANG) wovec.jsonc $(BUILD_VERSION) force-build-info
	$(build_with_wovec)

$(FINAL_COMPILER): $(NATIVE_COMPILER) $(SELF_HOST_SOURCE)/.ready $(BUILD_CLANG) wovec.jsonc $(BUILD_VERSION) force-build-info
	$(build_with_wovec)

# Link the standalone seed utilities against the same runtime.
$(SEED_UTILITY_PROGRAMS): %: %.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$<" "$(STAGE1)/runtime.o" -o "$@"

# Emit the native hash fixtures from their ordinary entry modules.
$(addsuffix .c,$(HASHING_NATIVE_PROGRAMS)): $(STAGE2)/hashing/native/%.c: $(HASHING_FIXTURES)/%.wove $(NATIVE_FIXTURE_TOOLS)
	$(call emit_seed_graph,$(EMIT_NATIVE_FIXTURE))

$(BLAKE2B_PROBE).c: $(BLAKE2B_FIXTURES)/probe.wove $(NATIVE_FIXTURE_TOOLS)
	$(call emit_seed_graph,$(EMIT_NATIVE_FIXTURE))

$(HASHING_NATIVE_PROGRAMS): %: %.c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$<" "$(STAGE1)/runtime.o" -o "$@"

$(BLAKE2B_PROBE): $(BLAKE2B_FIXTURES)/probe.probe.c $(BLAKE2B_PROBE).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Build the project fixture with file input supplied by the probe.
$(CONFIG_PROGRAM): $(CONFIG_FIXTURES)/project.probe.c $(CONFIG_PROGRAM).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Supply an argv arena and config bytes to the standalone command fixture.
$(COMMAND_PROGRAM): $(CLI_FIXTURES)/command.probe.c $(COMMAND_PROGRAM).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Discover the standalone probe's imports with the Python seed.
$(AST_PROBE).c: $(AST_FIXTURES)/probe.wove $(SEED)
	$(call emit_seed_graph,$(SEED_COMPILER))

$(AST_PROBE): $(AST_FIXTURES)/probe.probe.c $(AST_PROBE).c $(STAGE1)/runtime.o
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -I "$(@D)" "$<" "$(STAGE1)/runtime.o" -o "$@"

# Manifest-driven Stage 1 fixtures use the same successful-build sidecar rule.
$(STAGE1_FIXTURE_C): $(STAGE1)/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	$(call emit_seed_graph,$(SEED_COMPILER))

$(STAGE1)/%.c: $(OWNERSHIP_TESTS)/%.wove $(SEED)
	mkdir -p "$(@D)"
	$(SEED_COMPILER) "$<" > "$@"

# The seed reports Wove imports. Keep the project C include inputs explicit.
C_HEADER_PROJECT_HEADERS := $(addprefix $(OWNERSHIP_TESTS)/imports/c_headers/,project.h link lookup.h tree/lookup.h tree/leaf/value.h)
$(STAGE1)/imports.c_headers.c: $(C_HEADER_PROJECT_HEADERS)

# Force the compiler to report missing inputs that Make can skip under .SECONDARY.
REQUIRED_BUILD_SOURCES := $(COMPILER_ROOT) $(SOURCE_PROBE_ROOT) \
	$(TEST_RUNNER_ROOT) $(AST_FIXTURES)/live.wove $(AST_FIXTURES)/probe.wove $(BLAKE2B_FIXTURES)/probe.wove \
	$(patsubst $(STAGE2)/hashing/native/%,$(HASHING_FIXTURES)/%.wove,$(HASHING_NATIVE_PROGRAMS)) \
	$(patsubst $(STAGE2)/%.c,tests/stage-2/%.wove,$(SEED_UTILITY_C)) \
	$(patsubst $(STAGE1)/%.c,$(OWNERSHIP_TESTS)/%.wove,$(STAGE1_FIXTURE_C)) $(C_HEADER_PROJECT_HEADERS)
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

# Exercise rejected descriptor contracts directly at the copy ABI.
$(STAGE2)/array-copy-runtime: tests/stage-2/array_copy/runtime.c $(STAGE1)/runtime.o $(RUNTIME_HEADER)
	mkdir -p "$(@D)"
	$(CLANG) -std=c11 $(CFLAGS) -I runtime "$<" "$(STAGE1)/runtime.o" -o "$@"

# Simulate an allocator that clobbers errno when native call scratch is released.
$(STAGE1)/runtime.cstr_cleanup: tests/support/cstr_cleanup.c $(STAGE1)/runtime.tracked.o $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime $(filter-out %.h,$^) -o "$@"

$(STAGE1)/%.tracked.o: $(STAGE1)/%.c $(RUNTIME_HEADER)
	$(CLANG) -std=c11 $(CFLAGS) -I runtime -Dmain=wove_test_main -c "$<" -o "$@"

# Relink tracked fixtures when the shared allocator probe changes.
$(STAGE1)/%.tracked: $(OWNERSHIP_TESTS)/allocations.c $(STAGE1)/%.tracked.o $(STAGE1)/runtime.tracked.o tests/support/array_tracking.h
	$(CLANG) -std=c11 $(CFLAGS) $(filter-out %.h,$^) -o "$@"
