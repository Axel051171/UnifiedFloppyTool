#!/usr/bin/env python3
"""Generate the authoritative registered-format list from the code.

Registry introspection (Format-Erweiterung goal, Phase 0): scans src/formats/
for every `const uft_format_plugin_t uft_format_plugin_<name> = { ... };` and
pulls .name / .extensions / .format from the struct body, plus whether the file
uses the UFT_REGISTER_FORMAT_PLUGIN auto-register macro. This replaces the
hand-maintained FORMAT_GROUPS.md table (which drifted to a stale 130).

Usage:
  python scripts/gen_format_list.py            # summary + counts
  python scripts/gen_format_list.py --md        # emit a markdown table
  python scripts/gen_format_list.py --check FILE # diff a doc's count vs reality
"""
from __future__ import annotations
import argparse
import re
import sys
from pathlib import Path

PLUGIN_RE = re.compile(
    r"const\s+uft_format_plugin_t\s+uft_format_plugin_(\w+)\s*=\s*\{", re.S)


def field(body: str, name: str) -> str | None:
    m = re.search(rf"\.{name}\s*=\s*\"([^\"]*)\"", body)
    return m.group(1) if m else None


def scan(root: Path) -> list[dict]:
    plugins: list[dict] = []
    for f in sorted((root / "src" / "formats").rglob("*.c")):
        text = f.read_text(encoding="utf-8", errors="replace")
        has_macro = "UFT_REGISTER_FORMAT_PLUGIN" in text
        for m in PLUGIN_RE.finditer(text):
            sym = m.group(1)
            # grab a slice of the struct body for field extraction
            body = text[m.end():m.end() + 1200]
            plugins.append({
                "symbol": sym,
                "name": field(body, "name") or sym.upper(),
                "ext": field(body, "extensions") or "",
                "desc": field(body, "description") or "",
                "auto": has_macro,
                "file": str(f.relative_to(root)).replace("\\", "/"),
            })
    # de-dup by symbol (a file could match twice defensively)
    seen, out = set(), []
    for p in plugins:
        if p["symbol"] in seen:
            continue
        seen.add(p["symbol"])
        out.append(p)
    return out


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--root", default=".")
    ap.add_argument("--md", action="store_true", help="emit markdown table")
    ap.add_argument("--check", metavar="DOC",
                    help="report the plugin count claimed in DOC vs reality")
    args = ap.parse_args()
    root = Path(args.root).resolve()

    plugins = scan(root)
    auto = sum(1 for p in plugins if p["auto"])
    manual = len(plugins) - auto

    if args.md:
        print("| Format | Ext | Registrierung | Datei |")
        print("|---|---|---|---|")
        for p in plugins:
            reg = "auto" if p["auto"] else "manuell"
            print(f"| **{p['name']}** | {p['ext']} | {reg} | `{p['file']}` |")
        return 0

    if args.check:
        doc = Path(args.check).read_text(encoding="utf-8", errors="replace")
        claims = sorted(set(int(n) for n in re.findall(r"\b(\d{2,3})\s*(?:registrierte[rn]?\s*)?Plugin", doc)))
        print(f"{args.check}: claims {claims or 'none'}; reality {len(plugins)}")
        return 0 if len(plugins) in claims else 1

    print(f"Registered format plugins (code-derived): {len(plugins)}")
    print(f"  auto-registered (UFT_REGISTER_FORMAT_PLUGIN) : {auto}")
    print(f"  manually registered                          : {manual}")
    if manual:
        print("  manual:", ", ".join(p["symbol"] for p in plugins if not p["auto"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
