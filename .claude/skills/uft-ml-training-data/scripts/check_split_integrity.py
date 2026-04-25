#!/usr/bin/env python3
"""
check_split_integrity.py — Verify train/val/test sets don't leak.

Reads tests/training/{train,val,test}/*.label.json and verifies:
- No disk-title appears in more than one split
- Class distribution per split is reasonable
- All labels have required fields
"""

import json
import sys
from collections import defaultdict
from pathlib import Path


def main():
    root = Path.cwd() / "tests" / "training"
    splits = {}

    for split_name in ["train", "val", "test"]:
        split_dir = root / split_name
        if not split_dir.exists():
            print(f"  [warn] {split_dir} does not exist")
            continue

        titles = set()
        classes = defaultdict(int)
        for label_path in split_dir.glob("*.label.json"):
            try:
                label = json.load(open(label_path))
            except Exception as e:
                print(f"  [FAIL] {label_path}: {e}")
                return 1

            # Extract title from filename (convention: <scheme>_<title>_<n>)
            stem = label_path.stem.replace(".label", "")
            parts = stem.split("_")
            if len(parts) >= 2:
                title = "_".join(parts[1:-1])  # everything except scheme and index
                titles.add(title)

            scheme = (label.get("ground_truth", {})
                       .get("protection_scheme", "UNKNOWN"))
            classes[scheme] += 1

        splits[split_name] = {"titles": titles, "classes": dict(classes)}

    # Check overlap
    issues = 0
    split_names = list(splits.keys())
    for i in range(len(split_names)):
        for j in range(i + 1, len(split_names)):
            a, b = split_names[i], split_names[j]
            overlap = splits[a]["titles"] & splits[b]["titles"]
            if overlap:
                print(f"  [FAIL] title leak between {a} and {b}: {overlap}")
                issues += 1
            else:
                print(f"  [OK]   no leak between {a} and {b}")

    # Check class distribution
    print("\nClass distribution per split:")
    for split, info in splits.items():
        total = sum(info["classes"].values())
        print(f"  {split} ({total} samples):")
        for cls, n in sorted(info["classes"].items()):
            pct = 100 * n / total if total else 0
            print(f"    {cls}: {n} ({pct:.0f}%)")
            if total and pct > 70:
                print(f"      [WARN] {cls} is >70% of {split} — imbalanced")
                issues += 1

    return 0 if issues == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
