#!/usr/bin/env python3
"""Declarations that disagree with the real definition, on parameter count.

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
  - only names with exactly one definition in the tree, so an overload-like
    situation cannot produce a false positive
  - variadic declarations are skipped

Two rules.

**A — a hand-written `extern` in a .c/.cpp file disagrees with the definition.**
That is the MF-442 case above.

**B — a HEADER prototype disagrees with the definition (MF-464).** The original
version of this file excluded headers with the reasoning that "header
declarations are the normal mechanism and are checked by the compiler wherever
the header is included". That reasoning has a hole, and MF-464 walked into it:
`uft_cpm_format()` was declared in TWO headers with different signatures —

    include/uft/formats/uft_cpm_diskdef.h    (uft_disk_image_t*,  def)
    include/uft/formats/uft_cpm_diskdefs.h   (uft_disk_image_t**, def, fill)

— two parameters in one, three in the other, and only the three-parameter
version exists (`src/formats/cpm/uft_cpm_diskdefs.c:1420`). The compiler catches
that only if some translation unit includes both headers. None does. So it sat
there, latent, exactly like `c64_sectors_per_track` before MF-459 and
`UFT_SCP_SIGNATURE` in ARCH-2 — waiting for the first caller to include the
wrong one of the two and, in C, call a three-parameter function with two
arguments. The two-parameter declaration is gone; the header says why.

Rule B is restricted to C headers (`.h`, not `.hpp`) and skips any name that a
`.cpp` or `.hpp` also declares, because C++ overloads make arity a legitimate
difference.

**21 further mismatches were already in the tree** when rule B was added. They
are in `scripts/extern_decl_baseline.json` — a baseline entry is a KNOWN
mismatch, not an acceptable one. None of them has a caller yet, which is the
only reason none has bitten. The list is in docs/KNOWN_ISSUES.md (ARCH-21).
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
# `<type> name(params);` — a prototype, in a header (rule B)
_PROTO = re.compile(
    r"^\s*(?:extern\s+)?(?!typedef\b|static\b|return\b)"
    r"[A-Za-z_][\w \t\*]*?\b([a-z_]\w*)\s*\(([^;{]*)\)\s*;", re.M)


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
    protos: list[tuple[str, str, int, int]] = []    # rule B, C headers only
    cxx_names: set[str] = set()                     # possible overloads

    # MF-633: SKIP_DIRS ist eine Aufzaehlung bekannter Faelle und war
    # unvollstaendig — `tools/uft-scout/work/` ist gitignored, enthaelt
    # geklonte Fremd-Repos, und dieser Pruefer meldete daraus drei
    # Befunde (cbm_open mit 3 gegen 5 Parametern). CI sieht sie nie.
    import importlib.util as _ilu
    _spec = _ilu.spec_from_file_location(
        "uft_repo_scope",
        str(Path(__file__).resolve().parent / "repo_scope.py"))
    _mod = _ilu.module_from_spec(_spec)
    _spec.loader.exec_module(_mod)
    _im_baum, _warn = _mod.make_filter(repo)
    if _warn:
        print("  WARNUNG:", _warn)

    for p in repo.rglob("*"):
        if not p.is_file() or p.suffix.lower() not in {".c", ".cpp", ".h", ".hpp"}:
            continue
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        if not _im_baum(p):        # MF-633: nur was CI auch sieht
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

        suffix = p.suffix.lower()
        if suffix in {".c", ".cpp"}:
            for m in _EXTERN.finditer(clean):
                a = arity(m.group(2))
                if a is None:
                    continue
                # Line of the NAME, not of the match: comments are
                # blanked to spaces, so the leading `^\s*` swallows the
                # doc comment above the prototype and m.start() would
                # point a dozen lines too high.
                ln = clean[:m.start(1)].count(chr(10)) + 1
                externs.append((rel, m.group(1), a, ln))

        if suffix in {".cpp", ".hpp"}:
            # C++ may overload; every name it mentions is off-limits for rule B
            for m in _PROTO.finditer(clean):
                cxx_names.add(m.group(1))
        elif suffix == ".h":
            for m in _PROTO.finditer(clean):
                a = arity(m.group(2))
                if a is None:
                    continue
                # Line of the NAME, not of the match: comments are
                # blanked to spaces, so the leading `^\s*` swallows the
                # doc comment above the prototype and m.start() would
                # point a dozen lines too high.
                ln = clean[:m.start(1)].count(chr(10)) + 1
                protos.append((rel, m.group(1), a, ln))

    findings = []
    for src, name, a, ln in externs:
        d = defs.get(name)
        if not d or len(d) != 1:
            continue                      # unknown, or ambiguous: no claim
        dfile, da = d[0]
        if dfile == src or da == a:
            continue
        findings.append({"rule": "A", "name": name, "decl_file": src,
                         "decl_line": ln, "decl_arity": a,
                         "def_file": dfile, "def_arity": da})

    for src, name, a, ln in protos:
        if name in cxx_names:
            continue                      # C++ in play: arity may differ legally
        d = defs.get(name)
        if not d or len(d) != 1:
            continue
        dfile, da = d[0]
        if da == a:
            continue
        findings.append({"rule": "B", "name": name, "decl_file": src,
                         "decl_line": ln, "decl_arity": a,
                         "def_file": dfile, "def_arity": da})
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
        why = ("A local extern is a promise the compiler believes — include "
               "the header instead, or correct the promise"
               if f.get("rule", "A") == "A" else
               "A header prototype is only checked where that header is "
               "included; if no translation unit includes it, nothing "
               "notices. Correct the prototype, or delete it if the function "
               "does not exist")
        errors.append(
            f"{f['decl_file']}:{f['decl_line']} declares {f['name']}() with "
            f"{f['decl_arity']} parameter(s); {f['def_file']} defines it with "
            f"{f['def_arity']}. {why}")
    for key in sorted(known - seen):
        errors.append(f"{key} no longer mismatches — remove it from {BASELINE}")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    found = scan(repo)

    if "--write-baseline" in sys.argv:
        payload = {
            "_doc": ("Declarations whose parameter count disagrees with the "
                     "single definition of that name. Rule A: hand-written "
                     "externs in .c files (MF-442). Rule B: header prototypes "
                     "(MF-464) — 21 of these were already in the tree when the "
                     "rule was added; none has a caller yet, which is the only "
                     "reason they have not bitten. Comparison is on "
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
