#!/usr/bin/env python3
"""
audit_skeleton.py — Verify SKELETON banner counts match reality.

For each header containing UFT_SKELETON_PLANNED, count actual function
declarations and compare against the banner's claim. Find new call sites
to still-skeleton functions in the rest of the codebase.

Usage:
    audit_skeleton.py [--uft-root <path>]
"""

import argparse
import re
import sys
from pathlib import Path


def parse_args():
    p = argparse.ArgumentParser()
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    return p.parse_args()


def find_skeleton_headers(root: Path):
    """Return list of headers containing UFT_SKELETON_PLANNED."""
    results = []
    for path in (root / "include").rglob("*.h"):
        try:
            text = path.read_text(errors="ignore")
        except Exception:
            continue
        if "UFT_SKELETON_PLANNED" in text:
            results.append(path)
    return results


def count_function_decls(text: str) -> int:
    """
    Count function declarations like:
        uft_error_t uft_ml_decoder_init(uft_ml_decoder_t *m);
    Skip macros, structs, typedefs.
    """
    pattern = re.compile(
        r"^\s*(?!#|/|\*|typedef\b|struct\b|enum\b|union\b)"
        r"[a-zA-Z_][a-zA-Z0-9_*\s]+\s+[a-zA-Z_][a-zA-Z0-9_]*\s*\([^)]*\)\s*;",
        re.MULTILINE,
    )
    return len(pattern.findall(text))


def banner_claim(text: str):
    """Extract (declared_count, unimplemented_count) from banner."""
    m = re.search(
        r"declares\s+(\d+)\s+public\s+functions,\s+of\s+which\s+(\d+)\s+have\s+no",
        text,
    )
    if m:
        return int(m.group(1)), int(m.group(2))
    return None, None


def find_implementations(root: Path, header_name: str):
    """Find .c files that #include this header — likely implementations."""
    impls = []
    for src_dir in [root / "src", root / "tests"]:
        if not src_dir.exists():
            continue
        for path in src_dir.rglob("*.c"):
            try:
                text = path.read_text(errors="ignore")
            except Exception:
                continue
            if f'#include "{header_name}"' in text or \
               f"#include <{header_name}>" in text or \
               header_name.split("/")[-1] in text:
                impls.append(path)
    return impls


def find_callers(root: Path, function_names: list[str]):
    """Find files that call any of the given function names."""
    callers = {fn: [] for fn in function_names}
    for src_dir in [root / "src", root / "tests"]:
        if not src_dir.exists():
            continue
        for path in src_dir.rglob("*.c"):
            try:
                text = path.read_text(errors="ignore")
            except Exception:
                continue
            for fn in function_names:
                if re.search(rf"\b{re.escape(fn)}\s*\(", text):
                    callers[fn].append(path)
    return callers


def main():
    args = parse_args()
    if not (args.uft_root / "include").exists():
        print(f"ERROR: {args.uft_root} doesn't look like UFT root",
              file=sys.stderr)
        return 1

    headers = find_skeleton_headers(args.uft_root)
    if not headers:
        print("No UFT_SKELETON_PLANNED headers found.")
        return 0

    print(f"=== Auditing {len(headers)} skeleton header(s) ===\n")

    issues = 0
    for header in headers:
        rel = header.relative_to(args.uft_root)
        text = header.read_text(errors="ignore")
        actual = count_function_decls(text)
        claimed, unimpl = banner_claim(text)

        print(f"### {rel}")
        print(f"  Banner claims:  {claimed} declared, {unimpl} unimplemented")
        print(f"  Counted decls:  {actual}")

        if claimed is not None and claimed != actual:
            print(f"  [FAIL] banner count out of sync")
            issues += 1
        else:
            print(f"  [OK]   counts match")

        # Extract function names
        fn_names = re.findall(
            r"^\s*(?:[a-zA-Z_][a-zA-Z0-9_*\s]+)\s+"
            r"([a-zA-Z_][a-zA-Z0-9_]*)\s*\([^)]*\)\s*;",
            text, re.MULTILINE,
        )

        # Find which are still unimplemented
        impl_files = find_implementations(args.uft_root,
                                            str(rel).replace("include/", ""))
        impl_text = ""
        for f in impl_files:
            try:
                impl_text += f.read_text(errors="ignore")
            except Exception:
                pass

        unimpl_fns = []
        for fn in fn_names:
            # Look for function DEFINITION (not just declaration)
            if not re.search(
                rf"^\s*(?:[a-zA-Z_][a-zA-Z0-9_*\s]+)\s+{re.escape(fn)}"
                rf"\s*\([^)]*\)\s*\{{", impl_text, re.MULTILINE
            ):
                unimpl_fns.append(fn)

        print(f"  Unimplemented:  {len(unimpl_fns)}")

        if unimpl_fns:
            # Find callers to unimplemented functions OUTSIDE the impl files
            callers = find_callers(args.uft_root, unimpl_fns)
            external_callers = {}
            for fn, paths in callers.items():
                external = [p for p in paths if p not in impl_files]
                if external:
                    external_callers[fn] = external

            if external_callers:
                print(f"  [WARN] external callsites to unimplemented:")
                for fn, paths in external_callers.items():
                    print(f"         {fn}:")
                    for p in paths[:3]:
                        print(f"           {p.relative_to(args.uft_root)}")
                issues += 1
        print()

    if issues == 0:
        print("=== OK: all skeleton headers consistent ===")
        return 0
    print(f"=== {issues} issue(s) found ===")
    return 1


if __name__ == "__main__":
    sys.exit(main())
