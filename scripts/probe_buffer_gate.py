#!/usr/bin/env python3
"""No probe may need more bytes than UFT_PROBE_BUFFER_SIZE hands it.

Why this exists (MF-449 / ARCH-15). There were three implementations of "probe
a file", with three different buffer sizes:

    uft_probe_file_format()          4096 bytes
    uft_find_format_plugin_for_file() 4096 bytes, own loop, extension fallback
    uft_smart_open()                65536 bytes

So the same file came back as two different formats depending on which door the
caller used — the identification was a property of the buffer size, not of the
file. And 4096 was simply too small for the tree: jv3_probe() opens with

    if (size < JV3_HEADER_SIZE || ...) return false;      /* 8960 */

meaning JV3 could never match through uft_probe_file_format(), and therefore
never through uft_disk_open() either. That is a format the tool claims to read
and could not identify.

The buffer is one constant now. This checks it stays big enough: every
`size < X` guard at the top of a probe function is compared against it. The
scan is deliberately shallow — a probe that computes its requirement at runtime
is not caught, and that limitation is stated rather than papered over.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}

_CONST = re.compile(r"^\s*#define\s+([A-Za-z_]\w*)\s+(0[xX][0-9A-Fa-f]+|\d+)\s*$", re.M)
_PROBE = re.compile(r"^(?:static\s+)?bool\s+(\w*probe\w*)\s*\([^;{]*\)\s*\{", re.M)
_NEED = re.compile(r"\bsize\s*<\s*([A-Za-z_]\w*|0[xX][0-9A-Fa-f]+|\d+)")
_BUFSZ = re.compile(r"^\s*#define\s+UFT_PROBE_BUFFER_SIZE\s+(\d+)u?\s*$", re.M)

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")


def strip_comments(text: str) -> str:
    def blank(m):
        return "".join(c if c == chr(10) else " " for c in m.group(0))
    return _LINE.sub(blank, _BLOCK.sub(blank, text))


def value_of(tok: str, consts: dict[str, int]):
    if tok.isdigit():
        return int(tok)
    if tok[:2].lower() == "0x":
        try:
            return int(tok, 16)
        except ValueError:
            return None
    return consts.get(tok)


def scan(repo: Path):
    hdr = (repo / "include/uft/uft_format_plugin.h").read_text(
        encoding="utf-8", errors="replace")
    m = _BUFSZ.search(hdr)
    buffer_size = int(m.group(1)) if m else 0

    needs: list[tuple[str, str, int]] = []      # file, probe, bytes
    for p in sorted((repo / "src").rglob("*.c")):
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        raw = p.read_text(encoding="utf-8", errors="replace")
        clean = strip_comments(raw)
        consts = {k: int(v, 0) for k, v in _CONST.findall(clean)}
        rel = str(p.relative_to(repo)).replace("\\", "/")

        for pm in _PROBE.finditer(clean):
            body = clean[pm.end(): pm.end() + 4000]
            stop = body.find(chr(10) + "}")
            if stop > 0:
                body = body[:stop]
            biggest = 0
            for nm in _NEED.finditer(body):
                v = value_of(nm.group(1), consts)
                if v and v > biggest:
                    biggest = v
            if biggest:
                needs.append((rel, pm.group(1), biggest))
    return buffer_size, needs


def check(repo: Path) -> list[str]:
    buffer_size, needs = scan(repo)
    errors: list[str] = []

    if buffer_size == 0:
        return ["UFT_PROBE_BUFFER_SIZE not found in "
                "include/uft/uft_format_plugin.h — the probe buffer has no "
                "single definition again (ARCH-15)"]

    for rel, fn, n in needs:
        if n > buffer_size:
            errors.append(
                f"{rel}: {fn}() returns false below {n} bytes, but probes are "
                f"handed at most UFT_PROBE_BUFFER_SIZE = {buffer_size}. This "
                f"format can never be identified through uft_probe_file_format() "
                f"or uft_disk_open() — raise the constant or lower the "
                f"requirement")

    # A literal buffer size is how the 4096/65536 split happened.
    src = (repo / "src/core/uft_format_plugin.c").read_text(
        encoding="utf-8", errors="replace")
    literal = re.findall(r"probe_size\s*=\s*\(\s*\(?size_t\)?\s*fs\s*<\s*(\d+)", src)
    for lit in literal:
        errors.append(
            f"src/core/uft_format_plugin.c sizes a probe buffer with the "
            f"literal {lit} instead of UFT_PROBE_BUFFER_SIZE — that is how the "
            f"4096/65536 split happened (ARCH-15)")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    buffer_size, needs = scan(repo)
    needs.sort(key=lambda x: -x[2])

    print(f"UFT_PROBE_BUFFER_SIZE      : {buffer_size}")
    print(f"probes with a size floor   : {len(needs)}")
    for rel, fn, n in needs[:5]:
        flag = "  <-- TOO BIG" if n > buffer_size else ""
        print(f"  {n:8d}  {fn}(){flag}")
    if len(needs) > 5:
        print(f"  ... {len(needs) - 5} more, all smaller")

    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
