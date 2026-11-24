<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Repository workflow

- Use `master` as the default branch and the base for pull requests.
- Use Conventional Commits for every commit subject and pull request title. Use `type: description` or `type(scope): description`, for example `docs: define repository workflow`.
- After the initial commit, make all changes on a separate branch and submit them through a pull request. Never push directly to `master`.

## Code comments

Add a descriptive comment above each function definition and immediately
before each multiline control-flow block. Describe its purpose or an
important invariant. Use the file's comment syntax, including `;;` in Wove.

## Copyright headers

- Start each checked-in file with the Wove copyright and SPDX notice shown above, using the file's comment syntax.
- Use `;;` for `.wove` sources and fixtures. Apply the same notice to other test files in their own comment syntax.
- Keep a shebang on the first line when present. Preserve the archived license texts verbatim.
