#!/usr/bin/env python3
"""Welle 12 — pinpoint orphan headers whose decl-matches are ALL in dead headers.

After Welle 10/11 cleared truly-orphan and twin-clusters, the remaining
maybe-orphans (no #include consumer + decl matches elsewhere) typically
fall into two classes:

  Class A: decl matches are actually false positives (comment text, common
           identifier collisions). Header is dead.

  Class B: decl matches are in OTHER orphan headers (i.e., a dead-cluster
           that the twin-detector missed because content/basename differ).

Class A and B are both deletion-safe. We surface them by checking whether
ALL decl-match files are themselves in the no-#include-consumer set.
"""
import re
from pathlib import Path
from collections import defaultdict

include_re = re.compile(r'#include\s*[<"]([^"<>]+)[">]')
decl_re = re.compile(r'\b([A-Za-z_][A-Za-z0-9_]{3,})\s*\(', re.MULTILINE)
common = {'if','while','for','switch','sizeof','return','case','do','else','goto',
          'main','static','const','void','char','int','long','short','unsigned',
          'typedef','struct','enum','union','extern','inline'}

all_source = (list(Path("src").rglob("*.h")) + list(Path("src").rglob("*.c")) +
              list(Path("src").rglob("*.cpp")) + list(Path("src").rglob("*.cc")) +
              list(Path("src").rglob("*.hpp")) + list(Path("include").rglob("*.h")) +
              list(Path("tests").rglob("*.c")) + list(Path("tests").rglob("*.cpp")))

file_text = {}
for p in all_source:
    rel = str(p).replace("\\","/")
    try:
        file_text[rel] = p.read_text(encoding="utf-8", errors="replace")
    except Exception:
        pass

references = {}
for src_path, text in file_text.items():
    for m in include_re.finditer(text):
        path = m.group(1)
        base = path.split("/")[-1].split("\\")[-1]
        references.setdefault(base, set()).add(src_path)

all_h = [p for p in file_text if p.endswith(".h") and (p.startswith("src/") or p.startswith("include/"))]

nonincluded = set()
for h in all_h:
    base = h.split("/")[-1]
    consumers = references.get(base, set()) - {h}
    if not consumers:
        nonincluded.add(h)

print(f"Non-included headers: {len(nonincluded)}")

# For each nonincluded, find where its decls are referenced
results = []
for o in sorted(nonincluded):
    text = file_text.get(o, "")
    func_idents = {i for i in decl_re.findall(text) if i not in common}
    if not func_idents:
        continue  # already handled in welle 7/10
    ref_sites = set()
    for path, body in file_text.items():
        if path == o:
            continue
        for s in func_idents:
            if re.search(r'\b' + re.escape(s) + r'\b', body):
                ref_sites.add(path)
                break
    if not ref_sites:
        continue  # already handled in welle 7/10 (truly orphan)
    # Check if ALL ref sites are themselves orphan
    all_orphan = all(site in nonincluded for site in ref_sites)
    results.append((o, len(ref_sites), all_orphan, sorted(ref_sites)[:3]))

# Class B: all matches in orphan headers (safe)
safe = [(o, n, sites) for o, n, all_orphan, sites in results if all_orphan]
risky = [(o, n, sites) for o, n, all_orphan, sites in results if not all_orphan]

print(f"\nMaybe-orphan with ALL refs in other orphan headers (Class B - SAFE): {len(safe)}")
for o, n, sites in sorted(safe)[:30]:
    print(f"  [{n} sites] {o}")
    for s in sites[:2]:
        print(f"    -> {s}")

print(f"\nMaybe-orphan with refs in LIVE files (Class C - RISKY): {len(risky)}")

with open("welle12_safe.txt", "w") as f:
    for o, _, _ in safe:
        f.write(o + "\n")

with open("welle12_risky.txt", "w") as f:
    for o, _, sites in risky:
        f.write(f"{o}|{','.join(sites)}\n")

print(f"\nSaved welle12_safe.txt ({len(safe)} files) + welle12_risky.txt ({len(risky)} files)")
