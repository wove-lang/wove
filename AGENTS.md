<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Repository workflow

- Use `master` as the default branch and the base for pull requests.
- Use Conventional Commits for every commit subject and pull request title. Use `type: description` or `type(scope): description`, for example `docs: define repository workflow`.
- After the initial commit, make all changes on a separate branch and submit them through a pull request. Never push directly to `master`.

## Code comments

Document each implementation function's purpose or an important invariant.
For Wove functions under `stage-2/`, put a `:doc` entry first in the function's
`with-meta` list. For other implementation code, place a descriptive comment
above the function definition.

Add a descriptive comment immediately before each multiline control-flow block.
Use the file's comment syntax, including `;;` in Wove.

Purpose comments are optional under `tests/`.

## Copyright headers

- Start each checked-in file with the Wove copyright and SPDX notice shown above, using the file's comment syntax.
- Use `;;` for `.wove` sources and fixtures. Apply the same notice to other test files in their own comment syntax.
- Keep a shebang on the first line when present. Preserve the archived license texts verbatim.
