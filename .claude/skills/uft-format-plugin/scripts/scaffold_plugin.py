#!/usr/bin/env python3
"""
scaffold_plugin.py — Wire a new UFT format plugin across all 6 touchpoints.

Idempotent: re-running with the same args is a no-op.
Supports --dry-run to preview changes without writing.

Usage:
    python3 scaffold_plugin.py \\
        --name trsdos \\
        --description "TRS-80 LDOS/LS-DOS" \\
        --ext jv1 \\
        --min-size 102400 \\
        --max-size 204800 \\
        --sector-size 256 \\
        --spec-status REVERSE_ENGINEERED \\
        [--dry-run]
"""

import argparse
import re
import sys
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parent.parent
TEMPLATES = SKILL_DIR / "templates"


def parse_args():
    p = argparse.ArgumentParser(description=__doc__,
                                formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--name", required=True,
                   help="short name, lowercase (e.g. 'trsdos')")
    p.add_argument("--description", required=True,
                   help='human-readable (e.g. "TRS-80 LDOS")')
    p.add_argument("--ext", required=True,
                   help="file extension without dot (e.g. 'jv1')")
    p.add_argument("--min-size", type=int, default=0)
    p.add_argument("--max-size", type=int, default=2**31 - 1)
    p.add_argument("--sector-size", type=int, default=256)
    p.add_argument("--spec-status", default="UNKNOWN",
                   choices=["OFFICIAL_FULL", "OFFICIAL_PARTIAL",
                            "REVERSE_ENGINEERED", "DERIVED", "UNKNOWN"])
    p.add_argument("--family", default=None,
                   help="formats/ subdir (default = --name)")
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args()


def render_template(tmpl_path: Path, subs: dict) -> str:
    text = tmpl_path.read_text()
    for key, val in subs.items():
        text = text.replace(f"@@{key}@@", str(val))
    unreplaced = re.findall(r"@@\w+@@", text)
    if unreplaced:
        print(f"  [warn] unreplaced placeholders in {tmpl_path.name}: "
              f"{set(unreplaced)}", file=sys.stderr)
    return text


def write_plugin(args, subs, family, dry):
    dst_dir = args.uft_root / "src" / "formats" / family
    dst = dst_dir / f"uft_{args.name}_plugin.c"
    if dst.exists():
        print(f"  [skip] {dst.relative_to(args.uft_root)} already exists")
        return
    if dry:
        print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
        return
    dst_dir.mkdir(parents=True, exist_ok=True)
    dst.write_text(render_template(TEMPLATES / "plugin.c.tmpl", subs))
    print(f"  [new]  {dst.relative_to(args.uft_root)}")


def patch_enum(args, subs, dry):
    hdr = args.uft_root / "include" / "uft" / "uft_format_plugin.h"
    if not hdr.exists():
        print(f"  [warn] {hdr} not found — skip enum patch")
        return
    text = hdr.read_text()
    enum_token = f"UFT_FORMAT_{subs['NAME']}"
    if re.search(rf"\b{enum_token}\b", text):
        print(f"  [skip] {enum_token} already in enum")
        return

    m = re.search(r"(^\s*UFT_FORMAT_UNKNOWN\s*=\s*0x[0-9A-Fa-f]+\s*,?\s*$)",
                  text, re.M)
    if not m:
        print(f"  [warn] couldn't find UFT_FORMAT_UNKNOWN — add {enum_token} "
              f"manually", file=sys.stderr)
        return

    insertion = f"    {enum_token},   /* {args.description} */\n"
    new_text = text[:m.start()] + insertion + text[m.start():]
    if dry:
        print(f"  [dry]  would add {enum_token} to enum")
        return
    hdr.write_text(new_text)
    print(f"  [edit] added {enum_token} to enum")


def patch_registry(args, subs, dry):
    reg = args.uft_root / "src" / "formats" / "uft_format_registry.c"
    if not reg.exists():
        print(f"  [warn] {reg} not found — skip registry patch")
        return
    text = reg.read_text()
    token = f"UFT_FORMAT_{subs['NAME']}"
    if f".format = {token}" in text or f".format         = {token}" in text \
            or f".format={token}" in text:
        print(f"  [skip] registry entry for {token} already present")
        return

    entry = (f"""    {{
        .format         = {token},
        .name           = "{subs['NAME']}",
        .extension      = ".{args.ext}",
        .description    = "{args.description}",
        .mime_type      = "application/x-{args.name}",
        .supports_read  = true,
        .supports_write = true,
        .supports_flux  = false,
        /* .probe       = uft_{args.name}_probe, */  /* TODO: wire probe */
    }},
""")

    m = re.search(r"(\{\s*\.format\s*=\s*UFT_FORMAT_UNKNOWN[^}]*\},?)",
                  text, re.S)
    if not m:
        print(f"  [warn] couldn't find UFT_FORMAT_UNKNOWN sentinel — add "
              f"entry manually", file=sys.stderr)
        return

    new_text = text[:m.start()] + entry + "    " + text[m.start():]
    if dry:
        print(f"  [dry]  would add registry entry for {token}")
        return
    reg.write_text(new_text)
    print(f"  [edit] added registry entry for {token}")


def patch_qmake(args, subs, family, dry):
    pro = args.uft_root / "UnifiedFloppyTool.pro"
    if not pro.exists():
        print(f"  [warn] {pro} not found — skip qmake patch")
        return
    text = pro.read_text()
    src_line = f"    src/formats/{family}/uft_{args.name}_plugin.c"
    if src_line in text:
        print(f"  [skip] qmake SOURCES already has {args.name}_plugin.c")
        return

    matches = list(re.finditer(r"^(    src/formats/[^\n]+\\)$", text, re.M))
    if not matches:
        print(f"  [warn] couldn't find src/formats/ SOURCES section in .pro",
              file=sys.stderr)
        return
    last = matches[-1]
    insert_at = last.end()
    new_text = text[:insert_at] + "\n" + src_line + " \\" + text[insert_at:]
    if dry:
        print(f"  [dry]  would add {src_line.strip()} to .pro")
        return
    pro.write_text(new_text)
    print(f"  [edit] appended to UnifiedFloppyTool.pro SOURCES")


def write_test(args, subs, dry):
    dst = args.uft_root / "tests" / f"test_{args.name}_plugin.c"
    if dst.exists():
        print(f"  [skip] {dst.relative_to(args.uft_root)} exists")
    elif dry:
        print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
    else:
        dst.write_text(render_template(TEMPLATES / "test.c.tmpl", subs))
        print(f"  [new]  {dst.relative_to(args.uft_root)}")

    cml = args.uft_root / "tests" / "CMakeLists.txt"
    if not cml.exists():
        print(f"  [warn] {cml} not found — skip CMake patch")
        return
    cml_text = cml.read_text()
    target = f"test_{args.name}_plugin"
    if target in cml_text:
        print(f"  [skip] tests/CMakeLists.txt already has {target}")
        return
    entry = (f"\nadd_executable({target} {target}.c)\n"
             f"add_test(NAME {target} COMMAND {target})\n")
    if dry:
        print(f"  [dry]  would add {target} to tests/CMakeLists.txt")
        return
    cml.write_text(cml_text.rstrip() + entry)
    print(f"  [edit] added {target} to tests/CMakeLists.txt")


def main():
    args = parse_args()
    if not (args.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: {args.uft_root} doesn't look like UFT root "
              f"(no UnifiedFloppyTool.pro)", file=sys.stderr)
        return 1

    if not re.match(r"^[a-z][a-z0-9_]*$", args.name):
        print(f"ERROR: --name must be lowercase identifier (got '{args.name}')",
              file=sys.stderr)
        return 1

    family = args.family or args.name

    subs = {
        "NAME":              args.name.upper(),
        "name":              args.name,
        "Description":       args.description,
        "ext":                args.ext,
        "min_size":           args.min_size,
        "max_size":           args.max_size,
        "sector_size":        args.sector_size,
        "spec_status":        args.spec_status,
        "format_summary":    f"{args.description} — {args.sector_size}B sectors",
        "spec_source":       "TODO: cite primary spec source",
        "sample_provenance": "TODO: describe test sample provenance",
    }

    print(f"Scaffolding plugin '{args.name}' (UFT_FORMAT_{subs['NAME']})")
    print(f"  family: {family}   dry-run: {args.dry_run}\n")

    write_plugin(args, subs, family, args.dry_run)
    patch_enum(args, subs, args.dry_run)
    patch_registry(args, subs, args.dry_run)
    patch_qmake(args, subs, family, args.dry_run)
    write_test(args, subs, args.dry_run)

    print()
    print("Next:")
    print(f"  1. Edit src/formats/{family}/uft_{args.name}_plugin.c — "
          f"replace TODOs with real probe/read logic")
    print(f"  2. Edit tests/test_{args.name}_plugin.c — add format-specific "
          f"assertions")
    print(f"  3. Wire probe function in uft_format_registry.c "
          f"(search for 'TODO: wire probe')")
    print(f"  4. qmake && make -j$(nproc)")
    print(f"  5. cd tests && cmake . && make && ctest --output-on-failure")
    return 0


if __name__ == "__main__":
    sys.exit(main())
