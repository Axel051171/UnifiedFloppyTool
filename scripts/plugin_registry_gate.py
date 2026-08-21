#!/usr/bin/env python3
"""The plugin registry must be able to hold what the tree defines, and nothing
may go back to identifying a plugin by its container id.

Why this exists (MF-444 / MF-445 / MF-446). Five independent caps each held the
registry far below the 137 plugins in the tree, and every one failed silently:

  1. `uft_register_format_plugin()` rejected duplicates on `plugin->format`.
     131 of 137 plugins declare `.format = UFT_FORMAT_DSK`, so 130 were refused
     with the same error code used for a broken plugin.
  2. `MAX_FORMAT_PLUGINS` was 32, then 128 — against a real count of 137.
  3. Nothing in the tree called `uft_register_all_formats()` at all — every
     consumer had grown around the registry by linking plugin symbols directly.
  4. `all_plugins[]` was one element too small for its own terminator.
  5. Seven defined plugins were listed in no group, SCP and IMG among them.

Because the registry was always empty, `uft_get_format_plugin(disk->format)`
returned NULL everywhere and the ambiguity never surfaced. With registration
working it becomes live: a first match out of 82, used to pick the `close()`
that frees `plugin_data` another plugin allocated.

Four checks, each one a thing that was actually wrong:

  A. MAX_FORMAT_PLUGINS >= number of plugin definitions in src/ (+ headroom),
     counting the DSK_PLUGIN() macro expansions — MF-445's version of this
     check did not, reported 88 for 137, and cleared a capacity of 128
  B. `.name` unique across all plugin definitions — it is the identity the
     registry keys on since MF-444
  C. no new `uft_get_format_plugin(` call sites in src/. The function stays for the
     unambiguous ids, but reaching for it is how the class came back last time;
     `scripts/plugin_lookup_baseline.json` lists the ones that are allowed.
     New code uses uft_disk_plugin(), uft_resolve_format_plugin() or
     uft_get_format_plugin_by_name().
  D. every defined plugin is listed in a g_*_plugins[] group, or
     uft_register_all_formats() cannot reach it at all
  E. a plugin whose .name matches a UFT_FORMAT_<NAME> value declares that
     value. 30 plugins declared UFT_FORMAT_DSK while uft_format_t already
     had UFT_FORMAT_D64, UFT_FORMAT_ADF, UFT_FORMAT_ATR and 27 more — the
     enum was never the problem, the declarations were (MF-450). The cost
     was that uft_convert_file() looked its conversion path up under a
     container class and found nothing.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

BASELINE = "scripts/plugin_lookup_baseline.json"
HEADROOM = 16          # plugins that may be added before the gate fires

SKIP_DIRS = {".git", "build", "proto", ".claude", "release", "debug"}

_PLUGIN_DEF = re.compile(
    r"const\s+uft_format_plugin_t\s+(uft_format_plugin_\w+)\s*=\s*\{(.*?)\n\};", re.S)
_NAME = re.compile(r'\.name\s*=\s*"([^"]*)"')
_MAXDEF = re.compile(r"^\s*#define\s+MAX_FORMAT_PLUGINS\s+(\d+)", re.M)
_LOOKUP = re.compile(r"\buft_get_format_plugin\s*\(")
# 49 plugins exist only as DSK_PLUGIN(idx, suffix, "NAME", ...) expansions in
# src/formats/dsk_generic/. The MF-445 version of this gate matched only
# written-out definitions and so reported 88 where the truth is 137 — it said
# "fits in 128" while nine plugins would have been refused at registration. A
# gate that undercounts is worse than no gate, because it is believed.
_MACRO_DEF = re.compile(
    r'^\s*DSK_PLUGIN\(\s*\d+\s*,\s*(\w+)\s*,\s*"([^"]+)"', re.M)
# every &uft_format_plugin_x inside a g_*_plugins[] table in the registry
_GROUP_TABLE = re.compile(
    r"static const uft_format_plugin_t\*\s+g_\w+_plugins\[\]\s*=\s*\{(.*?)\};", re.S)
_GROUP_REF = re.compile(r"&(uft_format_plugin_\w+)")
_FORMAT_FIELD = re.compile(r"\.format\s*=\s*(UFT_FORMAT_\w+)")
_ENUM_BLOCK = re.compile(r"typedef enum uft_format \{(.*?)\} uft_format_t;", re.S)
_ENUM_NAME = re.compile(r"UFT_FORMAT_(\w+)")

_BLOCK = re.compile(r"/\*.*?\*/", re.S)
_LINE = re.compile(r"//[^\n]*")


def strip_comments(text: str) -> str:
    """Blank comments, keep line count. String literals are left alone — a
    call spelled inside a string is not a call, but neither is it worth the
    false-negative risk of blanking includes (the MF-431 lesson)."""
    def blank(m):
        return "".join(c if c == chr(10) else " " for c in m.group(0))
    return _LINE.sub(blank, _BLOCK.sub(blank, text))


def sources(repo: Path):
    for p in sorted(repo.rglob("*")):
        if not p.is_file() or p.suffix.lower() not in {".c", ".cpp"}:
            continue
        if any(s in p.parts for s in SKIP_DIRS):
            continue
        yield p


def scan(repo: Path):
    plugins: dict[str, list[str]] = {}       # .name -> defining file(s)
    symbols: dict[str, str] = {}             # uft_format_plugin_x -> file
    declared: dict[str, tuple[str, str]] = {}  # .name -> (.format, file)
    lookups: list[tuple[str, int]] = []

    for p in sources(repo):
        rel = str(p.relative_to(repo)).replace("\\", "/")
        try:
            raw = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        clean = strip_comments(raw)

        for m in _PLUGIN_DEF.finditer(clean):
            nm = _NAME.search(m.group(2))
            fm = _FORMAT_FIELD.search(m.group(2))
            plugins.setdefault(nm.group(1) if nm else "<unnamed>", []).append(rel)
            symbols[m.group(1)] = rel
            if nm and fm:
                declared[nm.group(1)] = (fm.group(1), rel)

        for m in _MACRO_DEF.finditer(clean):
            plugins.setdefault(m.group(2), []).append(rel)
            symbols["uft_format_plugin_dsk_" + m.group(1)] = rel

        # The rule is about production code. The function itself lives in the
        # registry, and tests legitimately call it to pin down what the
        # first-match answer IS — test_plugin_identity.c asserts that
        # uft_disk_plugin() and uft_get_format_plugin() disagree, which is the
        # bug written as a comparison. Flagging that would make the gate
        # argue with its own evidence.
        if rel == "src/core/uft_format_plugin.c" or not rel.startswith("src/"):
            continue
        for m in _LOOKUP.finditer(clean):
            if clean[:m.start()].rstrip().endswith(("uft_register", "uft_unregister")):
                continue
            lookups.append((rel, clean[:m.start()].count(chr(10)) + 1))

    src = (repo / "src/core/uft_format_plugin.c").read_text(
        encoding="utf-8", errors="replace")
    m = _MAXDEF.search(src)
    max_plugins = int(m.group(1)) if m else 0

    # what uft_register_all_formats() can actually reach
    reg = (repo / "src/formats/format_registry/uft_format_registry.c").read_text(
        encoding="utf-8", errors="replace")
    listed: set[str] = set()
    for m in _GROUP_TABLE.finditer(strip_comments(reg)):
        listed |= set(_GROUP_REF.findall(m.group(1)))

    enum_values: set[str] = set()
    types = (repo / "include/uft/uft_types.h").read_text(
        encoding="utf-8", errors="replace")
    eb = _ENUM_BLOCK.search(types)
    if eb:
        enum_values = {m.group(1).upper() for m in _ENUM_NAME.finditer(eb.group(1))}

    return plugins, lookups, max_plugins, symbols, listed, declared, enum_values


def check(repo: Path) -> list[str]:
    (plugins, lookups, max_plugins, symbols, listed,
     declared, enum_values) = scan(repo)
    errors: list[str] = []

    total = sum(len(v) for v in plugins.values())
    if max_plugins < total + HEADROOM:
        errors.append(
            f"MAX_FORMAT_PLUGINS is {max_plugins} but src/ defines {total} "
            f"plugins (+{HEADROOM} headroom required). The registry would "
            f"accept the first {max_plugins} and refuse the rest — raise it in "
            f"src/core/uft_format_plugin.c")

    for name, files in sorted(plugins.items()):
        if len(files) > 1:
            errors.append(
                f"plugin name '{name}' is declared in {len(files)} files "
                f"({', '.join(files)}). Since MF-444 the registry keys on "
                f".name; two plugins sharing one means the second is refused")

    # D. Every defined plugin must be listed in a registry group, or
    #    uft_register_all_formats() cannot reach it. MF-446 found seven that
    #    were not — among them SCP, the flux container the whole DeepRead path
    #    reads, and IMG. Invisible for as long as the function had no caller.
    for sym in sorted(set(symbols) - listed):
        errors.append(
            f"{symbols[sym]} defines {sym} but no g_*_plugins[] group in "
            f"src/formats/format_registry/uft_format_registry.c lists it - "
            f"uft_register_all_formats() cannot reach it, so the plugin exists "
            f"and is unreachable")
    for sym in sorted(listed - set(symbols)):
        errors.append(
            f"the registry lists {sym} but nothing in src/ defines it")

    # E. the enum already knows this format's name
    for name, (fmt, rel) in sorted(declared.items()):
        want = "UFT_FORMAT_" + name.upper()
        if name.upper() in enum_values and fmt != want:
            errors.append(
                f"{rel}: plugin '{name}' declares .format = {fmt} but "
                f"uft_format_t has {want}. A container id where a format id "
                f"exists is how uft_convert_file() ended up looking its "
                f"conversion path up under UFT_FORMAT_DSK and finding none "
                f"(MF-450)")

    bl = repo / BASELINE
    known = set()
    if bl.exists():
        known = set(json.loads(bl.read_text(encoding="utf-8")).get("accepted", []))

    seen = set()
    for rel, ln in lookups:
        seen.add(rel)
        if rel in known:
            continue
        errors.append(
            f"{rel}:{ln} calls uft_get_format_plugin() — it returns the FIRST "
            f"plugin with that container id, and 82 of 88 share "
            f"UFT_FORMAT_DSK. Use uft_disk_plugin() when a disk is in hand, "
            f"uft_resolve_format_plugin() for a target, or "
            f"uft_get_format_plugin_by_name() when the plugin is known")

    for rel in sorted(known - seen):
        errors.append(
            f"{rel} no longer calls uft_get_format_plugin() — remove it from "
            f"{BASELINE} so the baseline keeps meaning something")
    return errors


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    (plugins, lookups, max_plugins, symbols, listed,
     declared, enum_values) = scan(repo)

    if "--write-baseline" in sys.argv:
        payload = {
            "_doc": ("Files calling uft_get_format_plugin() as of MF-445. The "
                     "function is correct for a container id carried by exactly "
                     "one plugin; it is a first-match for the other 82. An entry "
                     "here is a KNOWN site, not an endorsed one."),
            "accepted": sorted({rel for rel, _ in lookups}),
        }
        (repo / BASELINE).write_text(
            json.dumps(payload, indent=2, ensure_ascii=False) + "\n",
            encoding="utf-8")
        print(f"wrote {BASELINE}: {len(payload['accepted'])} file(s)")
        return 0

    total = sum(len(v) for v in plugins.values())
    print(f"plugin definitions in src/ : {total} "
          f"({len(symbols)} symbols, {len(listed)} listed in the registry)")
    print(f"MAX_FORMAT_PLUGINS         : {max_plugins}")
    print(f"uft_get_format_plugin() sites outside the registry: {len(lookups)}")
    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
