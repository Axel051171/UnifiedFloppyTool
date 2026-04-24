#!/usr/bin/env python3
"""scaffold_protection.py — Scaffold a new protection detector."""

import argparse
import re
import sys
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parent.parent
TEMPLATES = SKILL_DIR / "templates"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--scheme", required=True, help="scheme name (lowercase)")
    p.add_argument("--description", required=True)
    p.add_argument("--platforms", default="C64,AMIGA",
                   help="comma-separated: C64,AMIGA,ATARI_ST,APPLE_II,PC,DOS")
    p.add_argument("--first-seen", default="TODO: game (year)")
    p.add_argument("--reference", default="TODO: spec URL or book citation")
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()

    if not (args.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: not UFT root", file=sys.stderr)
        return 1

    platform_flags = " | ".join(
        f"UFT_PLATFORM_{p.strip().upper()}"
        for p in args.platforms.split(","))

    subs = {
        "scheme": args.scheme, "SCHEME": args.scheme.upper(),
        "Description": args.description,
        "platforms": args.platforms,
        "platform_flags": platform_flags,
        "first_seen": args.first_seen,
        "reference": args.reference,
    }

    # 1. Detector file
    dst = args.uft_root / "src" / "protection" / f"uft_{args.scheme}.c"
    if not dst.exists():
        text = (TEMPLATES / "detector.c.tmpl").read_text()
        for k, v in subs.items():
            text = text.replace(f"@@{k}@@", str(v))
        if args.dry_run:
            print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            dst.write_text(text)
            print(f"  [new]  {dst.relative_to(args.uft_root)}")

    # 2. Test file
    dst = args.uft_root / "tests" / f"test_{args.scheme}_protection.c"
    if not dst.exists():
        text = (TEMPLATES / "test.c.tmpl").read_text()
        for k, v in subs.items():
            text = text.replace(f"@@{k}@@", str(v))
        if args.dry_run:
            print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
        else:
            dst.write_text(text)
            print(f"  [new]  {dst.relative_to(args.uft_root)}")

    # 3. Patch enum
    hdr = args.uft_root / "include" / "uft" / "protection" / "uft_protection_types.h"
    if hdr.exists():
        text = hdr.read_text()
        token = f"UFT_PROT_{subs['SCHEME']}"
        if token not in text:
            m = re.search(r"^\s*UFT_PROT_UNKNOWN[^,\n]*,?", text, re.M)
            if m:
                entry = f"    {token},   /* {args.description} — {args.first_seen} */\n"
                new_text = text[:m.start()] + entry + text[m.start():]
                if args.dry_run:
                    print(f"  [dry]  would add {token} to protection enum")
                else:
                    hdr.write_text(new_text)
                    print(f"  [edit] added {token} to enum")

    # 4. Patch .pro
    pro = args.uft_root / "UnifiedFloppyTool.pro"
    if pro.exists():
        text = pro.read_text()
        line = f"    src/protection/uft_{args.scheme}.c"
        if line not in text:
            m = list(re.finditer(r"^(    src/protection/[^\n]+\\)$", text, re.M))
            if m:
                new_text = text[:m[-1].end()] + "\n" + line + " \\" + text[m[-1].end():]
                if args.dry_run:
                    print(f"  [dry]  would add to .pro SOURCES")
                else:
                    pro.write_text(new_text)
                    print(f"  [edit] added to .pro SOURCES")

    print()
    print("Next:")
    print(f"  1. Edit src/protection/uft_{args.scheme}.c — replace signature pattern + detection logic")
    print(f"  2. Add detector to src/protection/uft_protection_detect.c DETECTORS[] (specific first)")
    print(f"  3. Document in docs/protection_schemes.md")
    print(f"  4. qmake && make -j$(nproc) && ctest --output-on-failure -R {args.scheme}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
