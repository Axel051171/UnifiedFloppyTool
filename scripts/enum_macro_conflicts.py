#!/usr/bin/env python3
"""Names that are an enum constant in one place and a macro in another.

Why this exists (MF-427, MF-431): `include/uft/uft_track.h` carried

    /* Define local constants only if uft_types.h not included */
    #ifndef UFT_ENC_UNKNOWN
    #define UFT_ENC_GCR_CBM 3
    ...
    #endif

`UFT_ENC_UNKNOWN` is an enum constant in uft_types.h. Enum constants do not
exist for the preprocessor, so the guard could never fire and the fallback was
always active — with numbers that disagreed with the enum. `UFT_ENC_GCR_CBM`
meant 9 in every format plugin and 3 in the 43 translation units that also
pulled in uft_track.h. Every comparison across that line missed, silently, with
no compiler diagnostic of any kind. It surfaced only because a G71 track came
back with an encoding that could not be named.

That is not a one-off, it is a shape: a name defined twice in two different
language mechanisms, where the one the preprocessor cannot see loses without
saying so. This finds the shape.

  LIVE    some translation unit reaches both the enum and the macro. Whatever
          the values, the meaning of the name in that TU depends on include
          order. Hard failure — these get fixed, not baselined.
  LATENT  the two definitions exist but no TU sees both today. Recorded in
          scripts/enum_macro_baseline.json so a NEW one fails the gate and a
          resolved one has to be removed from the list.

Values are resolved as far as plain C allows without a compiler: integer
literals, references to another enum constant or object-like macro, implicit
enum increment. Anything else counts as UNRESOLVED and is not reported — a
guess here would be worse than a gap.

Include closures follow `#include` unconditionally, ignoring `#if` branches.
That over-approximates which TUs see what, which is the safe direction: it can
name a TU a real compile would not produce, but it cannot hide a live case.

Run standalone for the full report:
    python scripts/enum_macro_conflicts.py
"""
from __future__ import annotations

import collections
import json
import re
import sys
from pathlib import Path

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug",
             "graphify-out"}
HDR_EXT = {".h", ".hpp"}
SRC_EXT = {".c", ".cpp"}
ALL_EXT = HDR_EXT | SRC_EXT

BASELINE = "scripts/enum_macro_baseline.json"

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")
_STR = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
_INC = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.M)
_DEF = re.compile(r"^\s*#\s*define\s+([A-Za-z_]\w*)(?!\()[ \t]*([^\n]*)", re.M)
_INT = re.compile(r"^[-+]?(?:0[xX][0-9a-fA-F]+|\d+)[uUlL]*$")
_IDENT = re.compile(r"^[A-Za-z_]\w*$")


def _blank(m: re.Match) -> str:
    return "".join(c if c == "\n" else " " for c in m.group(0))


def strip_comments(text: str) -> str:
    """Comments out, string literals kept — #include paths ARE literals."""
    return _LINE.sub(_blank, _BLOCK.sub(_blank, text))


def strip_all(text: str) -> str:
    """Comments and string literals out, for scanning declarations."""
    return _STR.sub(_blank, strip_comments(text))


def _as_int(tok: str):
    tok = tok.strip()
    if not _INT.match(tok):
        return None
    return int(re.sub(r"[uUlL]+$", "", tok), 0)


_SCOPE_CACHE = {}


def _scope(repo: Path):
    """Was CI sieht — git statt Verzeichnisnamen (MF-633).

    SKIP_DIRS bleibt als zweites Sieb: es faengt auch die Faelle, in denen
    git nicht befragbar ist. Es ist aber nicht mehr die einzige Grenze,
    und genau das war der Fehler — `tools/uft-scout/work/` ist gitignored
    und stand in keiner Liste, also meldete dieser Pruefer Befunde aus
    geklonten Fremd-Repos."""
    key = str(repo.resolve())
    if key not in _SCOPE_CACHE:
        import importlib.util
        spec = importlib.util.spec_from_file_location(
            "uft_repo_scope",
            str(Path(__file__).resolve().parent / "repo_scope.py"))
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        pred, warn = mod.make_filter(repo)
        if warn:
            print("  WARNUNG:", warn)
        _SCOPE_CACHE[key] = pred
    return _SCOPE_CACHE[key]


def _walk(repo: Path):
    im_baum = _scope(repo)
    for p in repo.rglob("*"):
        if p.is_file() and p.suffix.lower() in ALL_EXT \
                and not any(d in p.parts for d in SKIP_DIRS) \
                and im_baum(p):
            yield p


def collect(repo: Path):
    """-> (enum_values, enum_files, macro_values, macro_files), keyed by name."""
    ev: dict[str, set] = collections.defaultdict(set)
    ef: dict[str, set] = collections.defaultdict(set)
    mv: dict[str, set] = collections.defaultdict(set)
    mf: dict[str, set] = collections.defaultdict(set)

    for p in _walk(repo):
        rel = str(p.relative_to(repo)).replace("\\", "/")
        try:
            clean = strip_all(p.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue

        for m in re.finditer(r"\benum\b[^{;]*\{", clean):
            depth, i, n = 1, m.end(), len(clean)
            while i < n and depth:
                if clean[i] == "{":
                    depth += 1
                elif clean[i] == "}":
                    depth -= 1
                i += 1
            counter = 0
            for part in clean[m.end():i - 1].split(","):
                part = part.strip()
                if not part:
                    continue
                if "=" in part:
                    name, _, expr = part.partition("=")
                    name, expr = name.strip(), expr.strip()
                    val = _as_int(expr)
                    if val is None and _IDENT.match(expr):
                        val = ("ref", expr)
                else:
                    name, val = part, counter
                if not _IDENT.match(name):
                    continue
                ev[name].add(val)
                ef[name].add(rel)
                counter = val + 1 if isinstance(val, int) else counter + 1

        for m in _DEF.finditer(clean):
            name, body = m.group(1), m.group(2).strip()
            val = _as_int(body)
            if val is None:
                val = ("ref", body) if _IDENT.match(body) else ("raw", body[:40])
            mv[name].add(val)
            mf[name].add(rel)
    return ev, ef, mv, mf


def _resolve(v, ev, mv, depth=0):
    if isinstance(v, int):
        return v
    if depth > 6 or not isinstance(v, tuple) or v[0] != "ref":
        return None
    got = set()
    for s in ev.get(v[1], set()) | mv.get(v[1], set()):
        r = _resolve(s, ev, mv, depth + 1)
        if r is not None:
            got.add(r)
    return got.pop() if len(got) == 1 else None


def _index(repo: Path) -> dict[str, Path]:
    idx: dict[str, Path] = {}
    for p in _walk(repo):
        if p.suffix.lower() not in HDR_EXT:
            continue
        parts = str(p.relative_to(repo)).replace("\\", "/").split("/")
        for i in range(len(parts)):
            idx.setdefault("/".join(parts[i:]), p)
    return idx


def _closure(path: Path, repo: Path, idx, cache) -> set[str]:
    key = str(path)
    if key in cache:
        return cache[key]
    cache[key] = set()                                  # cycle guard
    out: set[str] = set()
    try:
        clean = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    except OSError:
        cache[key] = out
        return out
    for m in _INC.finditer(clean):
        want = m.group(1).replace("\\", "/")
        tgt = idx.get(want)
        if tgt is None:
            local = path.parent / want
            tgt = local if local.exists() else None
        if tgt is None:
            continue
        try:
            rel = str(tgt.resolve().relative_to(repo)).replace("\\", "/")
        except ValueError:
            continue
        if rel in out:
            continue
        out.add(rel)
        out |= _closure(tgt, repo, idx, cache)
    cache[key] = out
    return out


def analyse(repo: Path) -> tuple[dict, dict]:
    """-> (live, latent); each maps name -> dict with values and locations."""
    ev, ef, mv, mf = collect(repo)
    diverging = {}
    for name in set(ev) & set(mv):
        e = {_resolve(v, ev, mv) for v in ev[name]} - {None}
        m = {_resolve(v, ev, mv) for v in mv[name]} - {None}
        if e and m and e != m:
            diverging[name] = (sorted(e), sorted(m))
    if not diverging:
        return {}, {}

    idx = _index(repo)
    cache: dict[str, set[str]] = {}
    closures = {}
    for p in _walk(repo):
        if p.suffix.lower() not in SRC_EXT:
            continue
        rel = str(p.relative_to(repo)).replace("\\", "/")
        closures[rel] = _closure(p, repo, idx, cache) | {rel}

    live, latent = {}, {}
    for name, (e, m) in diverging.items():
        hits = sorted(tu for tu, cl in closures.items()
                      if (cl & ef[name]) and (cl & mf[name]))
        rec = {"enum": e, "macro": m, "enum_files": sorted(ef[name]),
               "macro_files": sorted(mf[name]), "tus": hits}
        (live if hits else latent)[name] = rec
    return live, latent


def check(repo: Path) -> list[str]:
    """Gate entry point, called from check_consistency.py."""
    live, latent = analyse(repo)
    bl_path = repo / BASELINE
    if not bl_path.exists():
        return [f"{BASELINE} missing — run "
                f"python scripts/enum_macro_conflicts.py --write-baseline"]
    known = set(json.loads(bl_path.read_text(encoding="utf-8")).get("accepted", []))

    errors = []
    for name in sorted(live):
        rec = live[name]
        errors.append(
            f"{name} is an enum constant ({rec['enum']}) AND a macro "
            f"({rec['macro']}), and {len(rec['tus'])} translation unit(s) see "
            f"both — e.g. {rec['tus'][0]}. Include order decides what the name "
            f"means. This one is not baselinable; give one of the two its own "
            f"name.")
    for name in sorted(set(latent) - known):
        rec = latent[name]
        errors.append(
            f"{name} is an enum constant ({rec['enum']}) in "
            f"{rec['enum_files'][0]} AND a macro ({rec['macro']}) in "
            f"{rec['macro_files'][0]}. No translation unit sees both today — "
            f"rename one, or add the name to {BASELINE} with a reason.")
    for name in sorted(known - set(latent) - set(live)):
        errors.append(
            f"{name} no longer collides between enum and macro — remove it "
            f"from {BASELINE} so the baseline keeps meaning something")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    live, latent = analyse(repo)

    if "--write-baseline" in sys.argv:
        payload = {
            "_doc": (
                "Names that exist as BOTH an enum constant and a macro, with "
                "different values, as they stood after the sweep of 2026-08-19 "
                "(KNOWN_ISSUES ARCH-5). Every entry here is LATENT: no "
                "translation unit reaches both definitions. A live one is never "
                "baselined — scripts/enum_macro_conflicts.py fails on it "
                "outright, because there the meaning of the name depends on "
                "include order. Mechanism and history: MF-427, MF-431."),
            "accepted": sorted(latent),
            "detail": {k: {"enum": v["enum"], "macro": v["macro"],
                           "enum_files": v["enum_files"][:2],
                           "macro_files": v["macro_files"][:2]}
                       for k, v in sorted(latent.items())},
        }
        (repo / BASELINE).write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8")
        print(f"wrote {BASELINE}: {len(latent)} latent entries")
        return 0

    print(f"LIVE   {len(live)}")
    for name, rec in sorted(live.items(), key=lambda kv: -len(kv[1]["tus"])):
        print(f"  {name}: enum {rec['enum']} vs macro {rec['macro']}  "
              f"({len(rec['tus'])} TUs)")
        print(f"      enum  {', '.join(rec['enum_files'][:2])}")
        print(f"      macro {', '.join(rec['macro_files'][:2])}")
        print(f"      e.g.  {', '.join(rec['tus'][:3])}")
    print(f"\nLATENT {len(latent)}")
    print("  " + ", ".join(sorted(latent)))
    return 1 if live else 0


if __name__ == "__main__":
    raise SystemExit(main())
