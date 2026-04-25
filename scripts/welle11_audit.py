#!/usr/bin/env python3
"""Welle 11 — find header twin-pairs where both sides are dead.

Strategy: collect all .h files with no #include consumer. For each, look
for byte-identical or basename-identical twins anywhere in src/include/.
If the only "external" matches in the maybe-orphan check come from twin
files that are themselves orphan, the file is part of a dead-twin-cluster
and safe to delete.
"""
import re
import hashlib
from pathlib import Path
from collections import defaultdict

include_re = re.compile(r'#include\s*[<"]([^"<>]+)[">]')

all_source = (list(Path("src").rglob("*.h")) + list(Path("src").rglob("*.c")) +
              list(Path("src").rglob("*.cpp")) + list(Path("src").rglob("*.cc")) +
              list(Path("src").rglob("*.hpp")) + list(Path("include").rglob("*.h")) +
              list(Path("tests").rglob("*.c")) + list(Path("tests").rglob("*.cpp")))

file_text = {}
file_hash = {}
for p in all_source:
    rel = str(p).replace("\\","/")
    try:
        text = p.read_text(encoding="utf-8", errors="replace")
        file_text[rel] = text
        file_hash[rel] = hashlib.md5(text.encode("utf-8", errors="replace")).hexdigest()
    except Exception:
        pass

# Hash → list of files (find byte-identical twins)
hash_groups = defaultdict(list)
for path, h in file_hash.items():
    if path.endswith(".h"):
        hash_groups[h].append(path)

# Basename → set of files with that basename
basename_groups = defaultdict(list)
for path in file_text.keys():
    if path.endswith(".h"):
        base = path.split("/")[-1]
        basename_groups[base].append(path)

# Build references map
references = {}
for src_path, text in file_text.items():
    for m in include_re.finditer(text):
        path = m.group(1)
        base = path.split("/")[-1].split("\\")[-1]
        references.setdefault(base, set()).add(src_path)

# All headers under src/ or include/
all_h = [p for p in file_text.keys() if p.endswith(".h") and (p.startswith("src/") or p.startswith("include/"))]

# Step 1: nonincluded headers (no #include of basename anywhere except self)
nonincluded = set()
for h in all_h:
    base = h.split("/")[-1]
    consumers = references.get(base, set()) - {h}
    if not consumers:
        nonincluded.add(h)

print(f"Total headers: {len(all_h)}")
print(f"Without #include consumer: {len(nonincluded)}")

# Step 2: For each nonincluded header, find byte-identical twins
identical_twin_clusters = []
seen = set()
for h in sorted(nonincluded):
    if h in seen:
        continue
    cluster = hash_groups.get(file_hash[h], [])
    cluster_orphan = [c for c in cluster if c in nonincluded]
    if len(cluster_orphan) > 1:
        identical_twin_clusters.append(cluster_orphan)
        seen.update(cluster_orphan)

print(f"\nByte-identical twin clusters (all members orphan): {len(identical_twin_clusters)}")
total_in_twins = sum(len(c) for c in identical_twin_clusters)
print(f"Total files in such clusters: {total_in_twins}")
for c in identical_twin_clusters[:20]:
    print(f"  cluster of {len(c)}:")
    for f in c:
        print(f"    {f}")

# Step 3: For each nonincluded header, find basename-twins (different content)
basename_twin_clusters = []
seen2 = set()
for h in sorted(nonincluded):
    if h in seen2:
        continue
    base = h.split("/")[-1]
    siblings = basename_groups.get(base, [])
    sibling_orphans = [s for s in siblings if s in nonincluded and s != h]
    if sibling_orphans:
        cluster = sorted(set([h] + sibling_orphans))
        if cluster not in basename_twin_clusters:
            basename_twin_clusters.append(cluster)
            seen2.update(cluster)

print(f"\nBasename-twin clusters (same name, different content, all members orphan): {len(basename_twin_clusters)}")
for c in basename_twin_clusters[:15]:
    if len(c) > 1:
        print(f"  basename '{c[0].split('/')[-1]}':")
        for f in c:
            print(f"    {f}")

# Save deletion candidates: identical-twin + basename-twin clusters
combined = set()
for cluster in identical_twin_clusters:
    combined.update(cluster)
for cluster in basename_twin_clusters:
    combined.update(cluster)

with open("welle11_orphans.txt", "w") as f:
    for path in sorted(combined):
        f.write(path + "\n")

print(f"\nTotal Welle 11 deletion candidates (identical + basename twins): {len(combined)}")
print(f"Saved to welle11_orphans.txt")
