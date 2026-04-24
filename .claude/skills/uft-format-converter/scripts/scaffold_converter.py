#!/usr/bin/env python3
"""
scaffold_converter.py — Wire a new UFT format-conversion path.

Creates the 4 touchpoints: converter.c, registry entry, test.c, CMakeLists.

Usage:
    python3 scaffold_converter.py \\
        --src d64 --src-ext d64 --src-enum D64 \\
        --dst g64 --dst-ext g64 --dst-enum G64 \\
        --fidelity SYNTHETIC \\
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
    p.add_argument("--src", required=True, help="source name (lowercase, e.g. 'd64')")
    p.add_argument("--src-ext", required=True, help="source extension (e.g. 'd64')")
    p.add_argument("--src-enum", required=True,
                   help="source UFT_FORMAT_ suffix (e.g. 'D64')")
    p.add_argument("--dst", required=True, help="target name (lowercase)")
    p.add_argument("--dst-ext", required=True, help="target extension")
    p.add_argument("--dst-enum", required=True,
                   help="target UFT_FORMAT_ suffix")
    p.add_argument("--fidelity", default="LOSSY",
                   choices=["LOSSLESS", "LOSSY", "SYNTHETIC"])
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args()


def render_template(tmpl: Path, subs: dict) -> str:
    text = tmpl.read_text()
    for k, v in subs.items():
        text = text.replace(f"@@{k}@@", str(v))
    return text


def write_converter(args, subs, dry):
    dst = args.uft_root / "src" / "convert" / \
          f"uft_convert_{args.src}_to_{args.dst}.c"
    if dst.exists():
        print(f"  [skip] {dst.relative_to(args.uft_root)} exists")
        return
    if dry:
        print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
        return
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text(render_template(TEMPLATES / "converter.c.tmpl", subs))
    print(f"  [new]  {dst.relative_to(args.uft_root)}")


def patch_registry(args, subs, dry):
    reg = args.uft_root / "src" / "convert" / "uft_convert_registry.c"
    if not reg.exists():
        print(f"  [warn] {reg} not found — add entry manually")
        return
    text = reg.read_text()
    marker = f"uft_convert_{args.src}_to_{args.dst}"
    if marker in text:
        print(f"  [skip] registry already has {marker}")
        return

    entry = (f"""    {{
        .src_format  = UFT_FORMAT_{args.src_enum},
        .dst_format  = UFT_FORMAT_{args.dst_enum},
        .convert     = {marker},
        .fidelity    = UFT_FIDELITY_{args.fidelity},
        .description = "{args.src} → {args.dst} ({args.fidelity.lower()})",
    }},
""")

    m = re.search(r"(\{\s*\.src_format\s*=\s*UFT_FORMAT_UNKNOWN[^}]*\},?)",
                  text, re.S)
    if m:
        insertion_point = m.start()
        new_text = text[:insertion_point] + entry + "    " + text[insertion_point:]
    else:
        m = re.search(r"(const\s+uft_convert_path_t\s+\w+\[\]\s*=\s*\{)", text)
        if not m:
            print(f"  [warn] couldn't locate registry table in {reg}")
            return
        insertion_point = m.end()
        new_text = text[:insertion_point] + "\n" + entry + text[insertion_point:]

    if dry:
        print(f"  [dry]  would add {marker} to registry")
        return
    reg.write_text(new_text)
    print(f"  [edit] added {marker} to convert registry")


def patch_qmake(args, dry):
    pro = args.uft_root / "UnifiedFloppyTool.pro"
    if not pro.exists():
        print(f"  [warn] {pro} not found")
        return
    text = pro.read_text()
    line = f"    src/convert/uft_convert_{args.src}_to_{args.dst}.c"
    if line in text:
        print(f"  [skip] .pro already has converter source")
        return
    m = list(re.finditer(r"^(    src/convert/[^\n]+\\)$", text, re.M))
    if m:
        last = m[-1]
        new_text = text[:last.end()] + "\n" + line + " \\" + text[last.end():]
    else:
        print(f"  [warn] couldn't find src/convert/ SOURCES section")
        return
    if dry:
        print(f"  [dry]  would add converter source to .pro")
        return
    pro.write_text(new_text)
    print(f"  [edit] added converter source to .pro")


def write_test(args, subs, dry):
    dst = args.uft_root / "tests" / \
          f"test_convert_{args.src}_to_{args.dst}.c"
    if dst.exists():
        print(f"  [skip] {dst.relative_to(args.uft_root)} exists")
    elif dry:
        print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
    else:
        dst.write_text(render_template(TEMPLATES / "test.c.tmpl", subs))
        print(f"  [new]  {dst.relative_to(args.uft_root)}")

    cml = args.uft_root / "tests" / "CMakeLists.txt"
    if not cml.exists():
        return
    cml_text = cml.read_text()
    target = f"test_convert_{args.src}_to_{args.dst}"
    if target in cml_text:
        return
    entry = f"\nadd_executable({target} {target}.c)\nadd_test(NAME {target} COMMAND {target})\n"
    if dry:
        print(f"  [dry]  would add {target} to CMakeLists.txt")
        return
    cml.write_text(cml_text.rstrip() + entry)
    print(f"  [edit] added {target} to tests/CMakeLists.txt")


def main():
    args = parse_args()
    if not (args.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: {args.uft_root} doesn't look like UFT root",
              file=sys.stderr)
        return 1

    subs = {
        "src": args.src, "SRC": args.src.upper(),
        "dst": args.dst, "DST": args.dst.upper(),
        "src_ext": args.src_ext, "dst_ext": args.dst_ext,
        "fidelity_class": args.fidelity,
        "loss_item_1": "TODO: what's lost",
        "loss_item_2": "TODO: ",
        "loss_item_3": "TODO: ",
        "lossy_audit_warnings": ("/* No loss — LOSSLESS conversion */"
                                  if args.fidelity == "LOSSLESS"
                                  else '/* TODO: emit audit warnings for each lost aspect */'),
    }

    print(f"Scaffolding converter {args.src} → {args.dst} "
          f"(fidelity: {args.fidelity}, dry-run: {args.dry_run})\n")
    write_converter(args, subs, args.dry_run)
    patch_registry(args, subs, args.dry_run)
    patch_qmake(args, args.dry_run)
    write_test(args, subs, args.dry_run)

    print()
    print("Next:")
    print(f"  1. Edit src/convert/uft_convert_{args.src}_to_{args.dst}.c — "
          f"implement the track-by-track transform")
    print(f"  2. Add a fixture to tests/vectors/{args.src}/minimal.{args.src_ext}")
    print(f"  3. qmake && make -j$(nproc)")
    print(f"  4. ./uft convert --from <sample> --to /tmp/out.{args.dst_ext}")
    print(f"  5. ctest --output-on-failure -R test_convert_{args.src}_to_{args.dst}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
