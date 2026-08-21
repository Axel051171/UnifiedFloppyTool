#!/usr/bin/env python3
"""Hand-written `extern` declarations that disagree with the real definition.

Why this exists (MF-442): `src/formats/uft_v3_bridge.c` declared

    extern bool scp_detect_protection(const struct scp_disk*, char*, size_t);

while the definition in `src/formats/scp/uft_scp_parser_v3.c` takes FOUR
parameters, the fourth a `float *confidence` the callee dereferences straight
after its NULL check. The bridge includes no header for that parser — the v3
parsers have none, their types live inside the .c files — so nothing could
catch it. At runtime the callee would have written through whatever the fourth
argument slot happened to hold: an arbitrary-address write.

A local `extern` is a promise the compiler believes. This checks the promises.

Comparison is on ARITY, not on spelling. Types are written differently across
files for the same thing (`size_t` vs `unsigned long`, a typedef vs the
underlying struct), and flagging those would drown the real finding. A
parameter-count mismatch cannot be a matter of style.

Deliberately narrow, so the result stays worth reading:
  - only `extern` declarations written inside .c/.cpp files, which is where
    hand-written promises live; header declarations are the normal mechanism
    and are checked by the compiler wherever the header is included
  - only names with exactly one definition in the tree, so an overload-like
    situation cannot produce a false positive
  - variadic declarations are skipped
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}
BASELINE = "scripts/extern_decl_baseline.json"

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")
_STR = re.compile(r'"(?:\\.|[^"\\])*"')

# `extern <type> name(params);` on one or more lines, inside a .c file
_EXTERN = re.compile(
    r"^\s*extern\s+[A-Za-z_][\w \t\*]*?\b([a-z_]\w*)\s*\(([^;{]*)\)\s*;", re.M)
# `<type> name(params) {` at column 0 — a definition
_DEFN = re.compile(
    r"^(?!static\b|typedef\b)[A-Za-z_][\w \t\*]*?\b([a-z_]\w*)\s*\(([^;{]*)\)\s*\{",
    re.M)


def strip(text: str) -> str:
    def blank(m):
        return "".join(c if c == chr(10) else " " for c in m.group(0))
    return _STR.sub(blank, _LINE.sub(blank, _BLOCK.sub(blank, text)))


def arity(params: str):
    """Parameter count, or None when it cannot be decided honestly."""
    p = " ".join(params.split())
    if not p or p == "void":
        return 0
    if "..." in p:
        return None                      # variadic: arity is not fixed
    depth, count = 0, 1
    for ch in p:
        if ch in "([":
            depth += 1
        elif ch in ")]":
            depth -= 1
        elif ch == "," and depth == 0:
            count += 1
    return count


def scan(repo: Path):
    defs: dict[str, list[tuple[str, int]]] = {}
    externs: list[tuple[str, str, int, int]] = []   # file, name, arity, line

    for p in repo.rglob("*"):
        if not p.is_file() or p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp"}:
            continue
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        rel = str(p.relative_to(repo)).replace("\\", "/")
        try:
            clean = strip(p.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue

        for m in _DEFN.finditer(clean):
            a = arity(m.group(2))
            if a is not None:
                defs.setdefault(m.group(1), []).append((rel, a))

        if p.suffix.lower() in {".c", ".cpp"}:
            for m in _EXTERN.finditer(clean):
                a = arity(m.group(2))
                if a is None:
                    continue
                ln = clean[:m.start()].count(chr(10)) + 1
                externs.append((rel, m.group(1), a, ln))

    findings = []
    for src, name, a, ln in externs:
        d = defs.get(name)
        if not d or len(d) != 1:
            continue                      # unknown, or ambiguous: no claim
        dfile, da = d[0]
        if dfile == src or da == a:
            continue
        findings.append({"name": name, "decl_file": src, "decl_line": ln,
                         "decl_arity": a, "def_file": dfile, "def_arity": da})
    return findings


def check(repo: Path) -> list[str]:
    found = scan(repo)
    bl = repo / BASELINE
    known = set()
    if bl.exists():
        known = set(json.loads(bl.read_text(encoding="utf-8")).get("accepted", []))

    errors = []
    seen = set()
    for f in found:
        key = f"{f['decl_file']}:{f['name']}"
        seen.add(key)
        if key in known:
            continue
        errors.append(
            f"{f['decl_file']}:{f['decl_line']} declares {f['name']}() with "
            f"{f['decl_arity']} parameter(s); {f['def_file']} defines it with "
            f"{f['def_arity']}. A local extern is a promise the compiler "
            f"believes — include the header instead, or correct the promise")
    for key in sorted(known - seen):
        errors.append(f"{key} no longer mismatches — remove it from {BASELINE}")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    found = scan(repo)

    if "--write-baseline" in sys.argv:
        payload = {
            "_doc": ("Hand-written extern declarations in .c files whose "
                     "parameter count disagrees with the single definition of "
                     "that name, as they stood after MF-442. Comparison is on "
                     "arity only — type spelling varies legitimately across "
                     "files. An entry here is a KNOWN mismatch, not an "
                     "acceptable one; the fix is to include the header."),
            "accepted": sorted({f"{f['decl_file']}:{f['name']}" for f in found}),
            "detail": found,
        }
        (repo / BASELINE).write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8")
        print(f"wrote {BASELINE}: {len(payload['accepted'])} entries")
        return 0

    print(f"extern-vs-definition arity mismatches: {len(found)}")
    for f in found:
        print(f"  {f['name']}()")
        print(f"      declared {f['decl_arity']}x  {f['decl_file']}:{f['decl_line']}")
        print(f"      defined  {f['def_arity']}x  {f['def_file']}")
    return 1 if found else 0


if __name__ == "__main__":
    raise SystemExit(main())
