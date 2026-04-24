#!/usr/bin/env python3
"""scaffold_benchmark.py — Create a UFT micro-benchmark from template."""

import argparse
import sys
from pathlib import Path

SKILL_DIR = Path(__file__).resolve().parent.parent
TEMPLATES = SKILL_DIR / "templates"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("--function", required=True,
                   help="name of the function under test (e.g. flux_find_sync)")
    p.add_argument("--workload-size", type=int, default=500000)
    p.add_argument("--unit", default="transitions",
                   help="what the workload size measures")
    p.add_argument("--uft-root", type=Path, default=Path.cwd())
    p.add_argument("--dry-run", action="store_true")
    args = p.parse_args()

    if not (args.uft_root / "UnifiedFloppyTool.pro").exists():
        print(f"ERROR: {args.uft_root} is not UFT root", file=sys.stderr)
        return 1

    dst_dir = args.uft_root / "tests" / "benchmarks"
    dst = dst_dir / f"bench_{args.function}.c"
    if dst.exists():
        print(f"  [skip] {dst.relative_to(args.uft_root)} exists")
        return 0

    tmpl = (TEMPLATES / "bench.c.tmpl").read_text()
    tmpl = tmpl.replace("@@function@@", args.function)
    tmpl = tmpl.replace("@@workload_size@@", str(args.workload_size))
    tmpl = tmpl.replace("@@unit@@", args.unit)

    if args.dry_run:
        print(f"  [dry]  would create {dst.relative_to(args.uft_root)}")
        return 0
    dst_dir.mkdir(parents=True, exist_ok=True)
    dst.write_text(tmpl)
    print(f"  [new]  {dst.relative_to(args.uft_root)}")

    print()
    print("Next:")
    print(f"  1. Edit {dst.relative_to(args.uft_root)} — replace TODO with real call")
    print(f"  2. Add to tests/CMakeLists.txt (inside 'if(UFT_ENABLE_BENCHMARKS)')")
    print(f"  3. Compile with -O3 -DNDEBUG for realistic numbers")
    print(f"  4. Run 3× to verify stability (±10% expected)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
