#!/usr/bin/env python3
"""
bump_version.py — Atomically bump UFT version across all 6 sync-required files.

Files patched:
    UnifiedFloppyTool.pro
    CLAUDE.md
    CHANGELOG.md                (adds section header stub)
    docs/USER_MANUAL.md
    include/uft/uft_version.h
    README.md

Usage:
    bump_version.py --version 4.2.0
    bump_version.py --version 4.2.0 --dry-run
"""

import argparse
import re
import sys
from datetime import date
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--version", required=True,
                   help="new version (e.g. 4.2.0)")
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    return p.parse_args()


def validate_version(v: str):
    if not re.match(r"^\d+\.\d+\.\d+$", v):
        print(f"ERROR: version must be X.Y.Z (got '{v}')", file=sys.stderr)
        return False
    return True


def patch_file(path: Path, regex: str, replacement: str, dry: bool) -> bool:
    if not path.exists():
        print(f"  [warn] {path} does not exist — skipping")
        return False
    text = path.read_text()
    new_text, n = re.subn(regex, replacement, text, flags=re.MULTILINE)
    if n == 0:
        print(f"  [warn] {path}: no match for pattern — manual fix needed")
        return False
    if dry:
        print(f"  [dry]  would patch {path.name} ({n} occurrences)")
        return True
    path.write_text(new_text)
    print(f"  [edit] {path.name} ({n} occurrences)")
    return True


def prepend_changelog_section(path: Path, version: str, dry: bool):
    if not path.exists():
        print(f"  [warn] {path} does not exist")
        return
    text = path.read_text()
    today = date.today().isoformat()
    section = (f"## [{version}] - {today}\n\n"
               f"### Added\n- TODO\n\n"
               f"### Changed\n- TODO\n\n"
               f"### Fixed\n- TODO\n\n"
               f"### Removed\n- TODO\n\n")
    if f"## [{version}]" in text:
        print(f"  [skip] CHANGELOG already has entry for {version}")
        return
    # Insert after the first heading (typically "# Changelog")
    m = re.search(r"^#\s+.+?\n\n?", text, re.M)
    if m:
        new_text = text[:m.end()] + section + text[m.end():]
    else:
        new_text = section + text
    if dry:
        print(f"  [dry]  would prepend section to CHANGELOG.md")
        return
    path.write_text(new_text)
    print(f"  [edit] prepended {version} section to CHANGELOG.md")


def main():
    args = parse_args()
    if not validate_version(args.version):
        return 1
    if not (args.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: {args.uft_root} is not UFT root", file=sys.stderr)
        return 1

    v = args.version
    major, minor, patch = v.split(".")
    print(f"Bumping version to {v} (dry-run: {args.dry_run})\n")

    # 1. .pro file
    patch_file(args.uft_root / "UnifiedFloppyTool.pro",
                r"^VERSION\s*=\s*\d+\.\d+\.\d+",
                f"VERSION = {v}", args.dry_run)

    # 2. CLAUDE.md
    patch_file(args.uft_root / "CLAUDE.md",
                r"(?<=version\s)\d+\.\d+\.\d+",
                v, args.dry_run)

    # 3. CHANGELOG.md — prepend section
    prepend_changelog_section(args.uft_root / "CHANGELOG.md",
                                v, args.dry_run)

    # 4. USER_MANUAL.md
    patch_file(args.uft_root / "docs" / "USER_MANUAL.md",
                r"(?<=UFT\s)v?\d+\.\d+\.\d+",
                f"v{v}", args.dry_run)

    # 5. uft_version.h
    ver_h = args.uft_root / "include" / "uft" / "uft_version.h"
    if ver_h.exists():
        text = ver_h.read_text()
        patched = text
        patched = re.sub(r"#define\s+UFT_VERSION_MAJOR\s+\d+",
                          f"#define UFT_VERSION_MAJOR {major}", patched)
        patched = re.sub(r"#define\s+UFT_VERSION_MINOR\s+\d+",
                          f"#define UFT_VERSION_MINOR {minor}", patched)
        patched = re.sub(r"#define\s+UFT_VERSION_PATCH\s+\d+",
                          f"#define UFT_VERSION_PATCH {patch}", patched)
        patched = re.sub(r'#define\s+UFT_VERSION_STRING\s+"[^"]*"',
                          f'#define UFT_VERSION_STRING "{v}"', patched)
        if patched != text:
            if args.dry_run:
                print(f"  [dry]  would patch uft_version.h")
            else:
                ver_h.write_text(patched)
                print(f"  [edit] uft_version.h")
        else:
            print(f"  [warn] uft_version.h: no version macros to patch")

    # 6. README.md
    patch_file(args.uft_root / "README.md",
                r"(?<=UFT\s)v?\d+\.\d+\.\d+",
                f"v{v}", args.dry_run)

    print()
    print("Next:")
    print(f"  1. Fill in CHANGELOG.md section {v} (replace TODOs)")
    print(f"  2. git diff — review all changes")
    print(f"  3. bash scripts/pre_release_check.sh")
    print(f"  4. git commit -am 'Release v{v}'")
    print(f"  5. git tag -s v{v} -m 'UFT v{v}'")
    print(f"  6. git push && git push --tags")
    return 0


if __name__ == "__main__":
    sys.exit(main())
