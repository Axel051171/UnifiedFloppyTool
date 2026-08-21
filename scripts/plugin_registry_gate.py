#!/usr/bin/env python3
"""The plugin registry must be able to hold what the tree defines, and nothing
may go back to identifying a plugin by its container id.

Why this exists (MF-444 / MF-445). Three independent gates each capped the
registry far below the 88 plugins in the tree, and every one of them failed
silently:

  1. `uft_register_format_plugin()` rejected duplicates on `plugin->format`.
     82 of 88 plugins declare `.format = UFT_FORMAT_DSK`, so 81 were refused
     with the same error code used for a broken plugin.
  2. `MAX_FORMAT_PLUGINS` was 32.
  3. Nothing in the tree called `uft_register_all_formats()` at all — every
     consumer had grown around the registry by linking plugin symbols directly.

Because the registry was always empty, `uft_get_format_plugin(disk->format)`
returned NULL everywhere and the ambiguity never surfaced. With registration
working it becomes live: a first match out of 82, used to pick the `close()`
that frees `plugin_data` another plugin allocated.

Three checks, each one a thing that was actually wrong:

  A. MAX_FORMAT_PLUGINS >= number of plugin definitions in src/ (+ headroom)
  B. `.name` unique across all plugin definitions — it is the identity the
     registry keys on since MF-444
  C. no new `uft_get_format_plugin(` call sites in src/. The function stays for the
     unambiguous ids, but reaching for it is how the class came back last time;
     `scripts/plugin_lookup_baseline.json` lists the ones that are allowed.
     New code uses uft_disk_plugin(), uft_resolve_format_plugin() or
     uft_get_format_plugin_by_name().
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
    plugins: dict[str, list[str]] = {}
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
            plugins.setdefault(nm.group(1) if nm else "<unnamed>", []).append(rel)

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
    return plugins, lookups, max_plugins


def check(repo: Path) -> list[str]:
    plugins, lookups, max_plugins = scan(repo)
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
    plugins, lookups, max_plugins = scan(repo)

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
    print(f"plugin definitions in src/ : {total}")
    print(f"MAX_FORMAT_PLUGINS         : {max_plugins}")
    print(f"uft_get_format_plugin() sites outside the registry: {len(lookups)}")
    errs = check(repo)
    for e in errs:
        print(f"  {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
