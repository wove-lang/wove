<!--
Copyright (c) 2026 wove contributors
SPDX-License-Identifier: MIT OR Apache-2.0
-->

# Wove Language

Syntax highlighting and delimiter pairing for canonical Wove S-expressions in
Visual Studio Code. The extension selects Wove mode for `.wove` files.

## Build and install

With Node.js 22 or later and npm available, run `make vscode` from the
repository root. In VS Code, choose **Extensions: Install from VSIX...** and
select `build/editors/vscode/wove-language.vsix`.

## Editing Wove

The grammar highlights form tags, names, metadata, types, and literals.
Double-quoted strings use JSON escapes. Raw backtick strings can span lines and
use a doubled backtick for an embedded backtick. Parentheses, double quotes,
single quotes, and backticks pair while typing and can surround a selection.

See the [Wove language reference](https://github.com/wove-lang/wove/blob/master/LANGUAGE.md)
for the current syntax.
