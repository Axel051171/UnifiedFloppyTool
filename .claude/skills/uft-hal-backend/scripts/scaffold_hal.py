#!/usr/bin/env python3
"""scaffold_hal.py — Create C HAL backend + header from templates."""

import argparse
import re
import sys
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parent.parent
TEMPLATES = SKILL_DIR / "templates"


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--backend", required=True, help="backend name (lowercase)")
    p.add_argument("--description", required=True)
    p.add_argument("--enum", required=True,
                   help="UFT_HAL_BACKEND_ suffix (e.g. 'XUM1541')")
    p.add_argument("--can-read-flux", default="true",
                   choices=["true", "false"])
    p.add_argument("--can-write-flux", default="false",
                   choices=["true", "false"])
    p.add_argument("--can-read-sector", default="false",
                   choices=["true", "false"])
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args()


def render(tmpl: Path, subs: dict) -> str:
    text = tmpl.read_text()
    for k, v in subs.items():
        text = text.replace(f"@@{k}@@", str(v))
    return text


def main():
    a = parse_args()
    if not (a.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: {a.uft_root} is not UFT root", file=sys.stderr)
        return 1

    subs = {
        "backend": a.backend, "BACKEND": a.backend.upper(),
        "Backend": a.backend.capitalize(),
        "Description": a.description,
        "can_read_flux": a.can_read_flux,
        "can_write_flux": a.can_write_flux,
        "can_read_sector": a.can_read_sector,
    }

    c_dst = a.uft_root / "src" / "hal" / f"uft_{a.backend}.c"
    h_dst = a.uft_root / "src" / "hal" / f"uft_{a.backend}.h"

    for dst, tmpl_name in [(c_dst, "hal_backend.c.tmpl"),
                            (h_dst, "hal_backend.h.tmpl")]:
        if dst.exists():
            print(f"  [skip] {dst.relative_to(a.uft_root)} exists")
            continue
        if a.dry_run:
            print(f"  [dry]  would create {dst.relative_to(a.uft_root)}")
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text(render(TEMPLATES / tmpl_name, subs))
        print(f"  [new]  {dst.relative_to(a.uft_root)}")

    # Patch enum in include/uft/hal/uft_hal_unified.h
    hdr = a.uft_root / "include" / "uft" / "hal" / "uft_hal_unified.h"
    if hdr.exists():
        text = hdr.read_text()
        token = f"UFT_HAL_BACKEND_{a.enum}"
        if token not in text:
            m = re.search(r"(\s*UFT_HAL_BACKEND_UNKNOWN[^,\n]*,?)", text)
            if m:
                new_text = text[:m.start()] + f"\n    {token}," + text[m.start():]
                if a.dry_run:
                    print(f"  [dry]  would add {token} to hal enum")
                else:
                    hdr.write_text(new_text)
                    print(f"  [edit] added {token} to hal enum")

    # Patch .pro
    pro = a.uft_root / "UnifiedFloppyTool.pro"
    if pro.exists():
        text = pro.read_text()
        line_c = f"    src/hal/uft_{a.backend}.c"
        if line_c not in text:
            m = list(re.finditer(r"^(    src/hal/[^\n]+\\)$", text, re.M))
            if m:
                new_text = text[:m[-1].end()] + "\n" + line_c + " \\" + text[m[-1].end():]
                if a.dry_run:
                    print(f"  [dry]  would add to .pro SOURCES")
                else:
                    pro.write_text(new_text)
                    print(f"  [edit] added to .pro SOURCES")

    print()
    print("Next:")
    print(f"  1. Edit src/hal/uft_{a.backend}.c — replace TODOs with real USB/serial code")
    print(f"  2. Add entry to src/hal/uft_hal_unified.c BACKENDS[] array")
    print(f"  3. If Qt provider needed: scaffold_qt_provider.py --backend {a.backend}")
    print(f"  4. gcc -fsyntax-only check, then full build")
    return 0


if __name__ == "__main__":
    sys.exit(main())
