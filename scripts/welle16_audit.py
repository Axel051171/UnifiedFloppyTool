#!/usr/bin/env python3
"""Welle 16 — find banner-free orphan headers excluding hactool conditional-live tree."""
import re
from pathlib import Path
from collections import defaultdict

include_re = re.compile(r'#include\s*[<"]([^"<>]+)[">]')
all_source = (list(Path("src").rglob("*.h")) + list(Path("src").rglob("*.c")) +
              list(Path("src").rglob("*.cpp")) + list(Path("src").rglob("*.cc")) +
              list(Path("src").rglob("*.hpp")) + list(Path("include").rglob("*.h")) +
              list(Path("tests").rglob("*.c")) + list(Path("tests").rglob("*.cpp")))

references = {}
file_text = {}
for src in all_source:
    rel = str(src).replace("\\","/")
    try:
        file_text[rel] = src.read_text(encoding="utf-8", errors="replace")
    except Exception:
        continue
    for m in include_re.finditer(file_text[rel]):
        path = m.group(1)
        base = path.split("/")[-1].split("\\")[-1]
        references.setdefault(base, set()).add(rel)

all_h = list(Path("src").rglob("*.h")) + list(Path("include").rglob("*.h"))
candidates = []
for h in all_h:
    rel = str(h).replace("\\", "/")
    # Skip hactool conditional-live tree
    if rel.startswith("src/switch/hactool/"):
        continue
    base = h.name
    consumers = references.get(base, set()) - {rel}
    if consumers:
        continue
    text = file_text.get(rel, "")
    if any(s in text for s in ["PLANNED FEATURE", "PARTIALLY IMPLEMENTED",
                                "UFT_SKELETON_PLANNED", "UFT_SKELETON_PARTIAL"]):
        continue
    candidates.append(rel)

# Group by directory
groups = defaultdict(list)
for o in candidates:
    parts = o.split("/")
    key = "/".join(parts[:3]) if len(parts) >= 4 else "/".join(parts[:2])
    groups[key].append(o)

print(f"Banner-free orphan headers (exc. hactool): {len(candidates)}")
print()
print("By directory:")
for k in sorted(groups.keys(), key=lambda x: -len(groups[x])):
    print(f"  {len(groups[k]):3d}  {k}/")

with open("welle16_orphans.txt", "w") as f:
    for o in sorted(candidates):
        f.write(o + "\n")
print(f"\nSaved {len(candidates)} candidates to welle16_orphans.txt")
