<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Repository workflow

- Use `master` as the default branch and the base for pull requests.
- Use Conventional Commits for every commit subject and pull request title. Use `type: description` or `type(scope): description`, for example `docs: define repository workflow`.
- After the initial commit, make all changes on a separate branch and submit them through a pull request. Never push directly to `master`.

## Implementation notes

When compiler work exposes a Wove language gap, land the smallest language
slice that supports the intended source before landing the code that uses it.
Use the archived compiler as the behavioral reference for ports. Keep Stage 1
extensions scoped to what the seed needs to build shared compiler source.

Split reusable logic into focused modules. Organize related modules in
directories named for their purpose, with short module filenames inside
each group. For example, keep the Stage 2 scanner in `stage-2/source/scan.wove`.

Write the bare name immediately after `module` and `fn`, including C bridge
functions. Place their metadata prefix directly after that name.

In Wove modules under `stage-2/`, document each function and multiline
control-flow form with a `(:doc "...")` entry at the start of its metadata
prefix. Explain the form's purpose or an important invariant. Attach a
standalone note to the existing form it describes.

In implementation files written in other languages, place a descriptive
comment above each function definition and immediately before each multiline
control-flow block. Use the file's comment syntax.

Purpose notes are optional under `tests/`.

## Copyright headers

- Start Wove source and fixtures with a named module whose first two metadata entries are
  `(:copyright "Copyright (c) 2026 wove contributors")` and
  `(:license "SPDX-License-Identifier: MIT OR Apache-2.0")`, in that order.
- Start other checked-in files with the same notices in their own comment syntax.
- Keep a shebang on the first line when present. Preserve the archived license texts verbatim.
