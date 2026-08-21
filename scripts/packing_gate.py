#!/usr/bin/env python3
"""Struct packing is decided in one file, and every pack(push) has its pop.

Why this exists (MF-451, ARCH-1). UFT_PACKED, UFT_PACK_BEGIN and UFT_PACK_END
were defined in SEVEN headers — uft_packed.h, uft_compiler.h, uft_platform.h,
uft_common.h, uft_config.h, compat/uft_platform.h and floppy/uft_floppy_device.h
— and they disagreed:

    uft_packed.h            UFT_PACKED = __attribute__((packed))
    uft_compiler.h          UFT_PACKED = empty, packing via pragma
    compat/uft_platform.h   UFT_PACKED = pack(push,1) on MSVC, EMPTY on POSIX
    uft_common.h            UFT_PACKED_BEGIN / _END = EMPTY on GCC

Which one a translation unit saw depended on include order — for macros that
decide the memory layout of structs that get cast directly onto disk bytes.

Two checks, both from things that were actually wrong:

  A. Only include/uft/uft_packed.h defines the packing vocabulary.
  B. UFT_PACK_BEGIN and UFT_PACK_END balance within each file. Four files had a
     stray UFT_PACK_END with no BEGIN, which pops the compiler's pragma stack
     below its base. Three of them got away with it because their structs were
     naturally aligned anyway. The fourth, fat12_bpb_t in
     src/fileops/uft_file_ops_extended.c, did not: unpacked it is 40 bytes
     instead of 36, and `bytes_per_sector` is read from offset 12 instead of
     11 — every field after the OEM name came out of the wrong place.

Check B counts per file, not per scope, so a BEGIN in a header and an END in
the .c that includes it would be missed. That is a real limitation and the
reason the on-disk structs also carry _Static_assert on sizeof and offsetof:
a balance count proves the pragmas pair up, only an assertion proves the layout.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}
OWNER = "include/uft/uft_packed.h"

_DEFINE = re.compile(
    r"^\s*#\s*define\s+(UFT_PACKED|UFT_PACK_BEGIN|UFT_PACK_END|"
    r"UFT_PACKED_BEGIN|UFT_PACKED_END|UFT_PACKED_STRUCT|UFT_PACKED_ATTR)\b", re.M)
_BEGIN = re.compile(r"^\s*UFT_PACK(?:ED)?_BEGIN\s*$", re.M)
_END = re.compile(r"^\s*UFT_PACK(?:ED)?_END\s*$", re.M)

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")


def strip_comments(text: str) -> str:
    def blank(m):
        return "".join(c if c == chr(10) else " " for c in m.group(0))
    return _LINE.sub(blank, _BLOCK.sub(blank, text))


def sources(repo: Path):
    for p in sorted(repo.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp"}:
            continue
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        yield p


def scan(repo: Path):
    definers: list[tuple[str, int, str]] = []
    unbalanced: list[tuple[str, int, int]] = []

    for p in sources(repo):
        rel = str(p.relative_to(repo)).replace("\\", "/")
        clean = strip_comments(p.read_text(encoding="utf-8", errors="replace"))

        if rel != OWNER:
            for m in _DEFINE.finditer(clean):
                definers.append((rel, clean[:m.start()].count(chr(10)) + 1,
                                 m.group(1)))

        b, e = len(_BEGIN.findall(clean)), len(_END.findall(clean))
        if b != e:
            unbalanced.append((rel, b, e))
    return definers, unbalanced


def check(repo: Path) -> list[str]:
    definers, unbalanced = scan(repo)
    errors: list[str] = []

    for rel, line, name in definers:
        errors.append(
            f"{rel}:{line} defines {name}. Packing decides the layout of "
            f"structs cast onto disk bytes, so it comes from {OWNER} alone — "
            f"seven headers defining it differently is how fat12_bpb_t ended "
            f"up unpacked (MF-451). Include the header instead")

    for rel, b, e in unbalanced:
        errors.append(
            f"{rel}: {b} x UFT_PACK_BEGIN against {e} x UFT_PACK_END. An "
            f"unmatched END pops the compiler's pragma stack below its base; "
            f"an unmatched BEGIN leaves everything after it packed")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    definers, unbalanced = scan(repo)
    print(f"packing macros defined outside {OWNER}: {len(definers)}")
    print(f"files with unbalanced UFT_PACK_BEGIN/END : {len(unbalanced)}")
    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
