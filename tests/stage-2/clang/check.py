# Copyright (c) 2026 wove contributors
# SPDX-License-Identifier: MIT OR Apache-2.0

"""Exercise the standalone source writer and index against fixtures."""

from collections import namedtuple
import json
from pathlib import Path
import shlex
import subprocess
import sys


Inspection = namedtuple(
    "Inspection", "error start end count match_start match_end probe_error probe_start probe_end value"
)


# Invoke the seed-built fixture with exact strings and one source offset.
def inspect(probe, path, offset=0, kind="", name="", label=""):
    output = subprocess.check_output([probe, "inspect", path, str(offset), kind, name, label])
    return Inspection(*map(int, output.split()))


# Compile the rendered probe to verify that the selected header operand works.
def check_source(probe, clang, template, fixtures):
    pattern = template.read_bytes()
    size_asserts = """
_Static_assert(wvpr_short_bits == sizeof(short) * 8, "short ABI");
_Static_assert(wvpr_int_bits == sizeof(int) * 8, "int ABI");
_Static_assert(wvpr_long_bits == sizeof(long) * 8, "long ABI");
_Static_assert(wvpr_long_long_bits == sizeof(long long) * 8, "long long ABI");
_Static_assert(wvpr_char_signed == ((char)-1 < 0), "char sign");
_Static_assert(wvpr_long_double_mantissa == __LDBL_MANT_DIG__, "long double ABI");
"""
    cases = (
        ("probe_fixture.h", "<probe_fixture.h>", 1),
        ('"probe_fixture.h"', '"probe_fixture.h"', 1),
        ("<probe_fixture.h>", "<probe_fixture.h>", 1),
        ("wove_fixture_missing_166.h", "<wove_fixture_missing_166.h>", 0),
    )
    # Exercise both include forms and the fallback when the header is absent.
    for name, literal, present in cases:
        extra = size_asserts + f'\n_Static_assert(wvpr_header_present == {present}, "header presence");\n'
        # A present header must actually expose its declaration in the produced C.
        if present:
            extra += '_Static_assert(sizeof(ProbeFixture) == sizeof(int), "included declaration");\n'
        source = subprocess.check_output([probe, "source", template, name, extra])
        assert source == pattern.replace(b"<@.h>", literal.encode()) + extra.encode(), name
        subprocess.run(
            [*clang, "-std=c11", "-Werror", "-I", fixtures, "-x", "c", "-fsyntax-only", "-"],
            input=source,
            check=True,
        )

    # Malformed names must fail before they can become preprocessing operands.
    for literal in ("", "<>", '""', "<bad.h", '"bad.h>', "<a.h>\n#include <b.h>", "bad\tname.h"):
        result = subprocess.run([probe, "source", template, literal, ""], capture_output=True)
        assert result.returncode == 1 and not result.stdout, (literal, result)
    bad_template = subprocess.run(
        [probe, "source", fixtures / "probe_fixture.h", "probe_fixture.h", ""], capture_output=True
    )
    assert bad_template.returncode == 2 and not bad_template.stdout, bad_template


# Validate the combined capture by framing each Clang translation unit in turn.
def check_capture(probe, path):
    data = path.read_bytes()
    # The capture keeps kind, name, inner and value from Darwin arm64 Clang AST output.
    captured_facts = dict(
        wvpr_char_signed=1,
        wvpr_short_bits=16,
        wvpr_int_bits=32,
        wvpr_long_bits=64,
        wvpr_long_long_bits=64,
        wvpr_long_double_mantissa=53,
        wvpr_has_float16=1,
        wvpr_has_float128=0,
    )
    offset = 0
    # The original header declaration is available only in the first document.
    for present, count in ((1, 3), (0, 1)):
        result = inspect(probe, path, offset, "TypedefDecl", "ProbeFixture", "wvpr_header_present")
        assert (result.error, result.start, result.count) == (0, offset, count), result
        assert (result.probe_error, result.value) == (0, present), result
        assert json.loads(data[result.probe_start:result.probe_end]) == str(present), result
        assert json.loads(data[result.start:result.end])["kind"] == "TranslationUnitDecl", result
        # Missing headers still report the ABI, with no declaration from the absent header.
        if present:
            node = json.loads(data[result.match_start:result.match_end])
            assert (node["kind"], node["name"]) == ("TypedefDecl", "ProbeFixture"), node
        else:
            assert (result.match_start, result.match_end) == (-1, -1), result
        # Read each ABI constant from the same document's retained spans.
        for label, value in captured_facts.items():
            fact = inspect(probe, path, offset, label=label)
            assert (fact.error, fact.probe_error, fact.value) == (0, 0, value), (label, fact)
            assert json.loads(data[fact.probe_start:fact.probe_end]) == str(value), (label, fact)
        offset = result.end

    eof = inspect(probe, path, offset)
    assert (eof.error, eof.start, eof.end, eof.count) == (0, len(data), len(data), 0), eof


# Check exact keys, first-array selection, and original error coordinates.
def check_index(probe, fixtures):
    path = fixtures / "index.json"
    data = path.read_bytes()
    typedef = inspect(probe, path, kind="TypedefDecl", name="duplicate", label="wvpr_shared")
    assert (typedef.error, typedef.count, typedef.probe_error, typedef.value) == (0, 9, 0, 7), typedef
    # The last typedef wins even though another node kind has the same name.
    assert json.loads(data[typedef.match_start:typedef.match_end])["type"]["qualType"] == "long", typedef
    same_name = inspect(probe, path, kind="EnumDecl", name="duplicate", label="wvpr_named")
    assert json.loads(data[same_name.match_start:same_name.match_end])["kind"] == "EnumDecl", same_name
    assert (same_name.probe_error, same_name.value) == (0, 65535), same_name
    escaped = inspect(probe, path, kind="TypedefDecl", name="escaped_name")
    assert json.loads(data[escaped.match_start:escaped.match_end])["name"] == "escaped_name", escaped
    anonymous = inspect(probe, path, kind="TypedefDecl")
    assert json.loads(data[anonymous.match_start:anonymous.match_end]) == {"kind": "TypedefDecl"}, anonymous
    # Exact lengths and kinds are required, even for decoded keys.
    for kind, name in (("TypedefDecl", "nested_only"), ("TypedefDecl", "prefix"),
                       ("TypedefDecl", "missing"), ("FunctionDecl", "duplicate")):
        absent = inspect(probe, path, kind=kind, name=name)
        assert (absent.error, absent.match_start, absent.match_end) == (0, -1, -1), absent
    absent_probe = inspect(probe, path, label="wvpr_missing")
    assert absent_probe.probe_error == 17, absent_probe
    incomplete_probe = inspect(probe, path, label="unselected")
    assert incomplete_probe.probe_error == 18, incomplete_probe

    shadowed = inspect(probe, fixtures / "shadowed.json", kind="TypedefDecl", name="later")
    assert (shadowed.error, shadowed.count, shadowed.match_start) == (0, 0, -1), shadowed
    malformed_path = fixtures / "malformed.jsons"
    first = inspect(probe, malformed_path)
    assert (first.error, first.end, first.count) == (0, len(b'{"inner":[]}'), 0), first
    malformed = inspect(probe, malformed_path, first.end)
    at = malformed_path.read_bytes().index(b":}") + 1
    assert (malformed.error, malformed.start, malformed.end) == (2, at, at + 1), malformed


# Use the supplied test compiler only to check the generated C fixture.
def main():
    executable, clang, template = sys.argv[1:]
    fixtures = Path(__file__).resolve().parent / "fixtures"
    check_source(executable, shlex.split(clang), Path(template), fixtures)
    check_capture(executable, fixtures / "capture.jsons")
    check_index(executable, fixtures)


if __name__ == "__main__":
    main()
