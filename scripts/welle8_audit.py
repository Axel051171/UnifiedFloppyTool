#!/usr/bin/env python3
"""Welle 8 audit — find headers with no #include and rank by external-symbol risk."""
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
    rel = str(p).replace("\\", "/")
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

all_h = list(Path("src").rglob("*.h"))
nonincluded = []
for h in all_h:
    base = h.name
    rel = str(h).replace("\\", "/")
    consumers = references.get(base, set()) - {rel}
    if not consumers:
        nonincluded.append(rel)

nonincluded_set = set(nonincluded)
print(f"Headers without #include consumer: {len(nonincluded)}")

results = []
for o in nonincluded:
    text = file_text.get(o, "")
    func_idents = {i for i in decl_re.findall(text) if i not in common}
    if not func_idents:
        continue  # truly orphan, already handled in Welle 7
    ref_sites = defaultdict(list)
    for path, body in file_text.items():
        if path == o or path in nonincluded_set:
            continue
        for s in func_idents:
            if re.search(r'\b' + re.escape(s) + r'\b', body):
                ref_sites[path].append(s)
    if not ref_sites:
        continue
    n_refs = sum(len(v) for v in ref_sites.values())
    results.append((len(ref_sites), n_refs, o, sorted(ref_sites.keys())[:3], sorted({s for vs in ref_sites.values() for s in vs})[:5]))

results.sort()
print(f"Maybe-orphans: {len(results)}\n")
print("=== Top 20 SAFEST (fewest external sites) ===")
for ns, nr, o, sites, syms in results[:20]:
    print(f"  [{ns} sites/{nr} refs] {o}")
    print(f"    sym sample: {syms}")
    for s in sites:
        print(f"    -> {s}")

with open("/tmp/welle8.txt", "w") as f:
    for ns, nr, o, sites, syms in results:
        f.write(f"{ns}|{nr}|{o}|{','.join(sites)}|{','.join(syms)}\n")
