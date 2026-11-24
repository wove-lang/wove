# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Emit C11 for one Stage 1 source file."""

from pathlib import Path
import sys

from compiler import compile_source
from syntax import SourceError


def main() -> int:
    """Use stdout for generated C and report source failures by line."""
    if len(sys.argv) != 2:
        print("usage: wovec.py SOURCE", file=sys.stderr)
        return 1
    filename = sys.argv[1]
    try:
        output = compile_source(Path(filename).read_text(encoding="utf-8"), filename)
    except SourceError as error:
        print(f"{filename}:{error.line}", file=sys.stderr)
        return 1
    except (OSError, UnicodeError) as error:
        print(error, file=sys.stderr)
        return 1
    sys.stdout.write(output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
