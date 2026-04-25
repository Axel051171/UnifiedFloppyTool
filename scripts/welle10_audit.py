#!/usr/bin/env python3
"""Welle 10 — find truly orphan headers in include/ tree."""
import re
import sys
from pathlib import Path

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

all_h = list(Path("include").rglob("*.h"))
nonincluded = []
for h in all_h:
    base = h.name
    rel = str(h).replace("\\", "/")
    consumers = references.get(base, set()) - {rel}
    if not consumers:
        nonincluded.append(rel)

nonincluded_set = set(nonincluded)
truly = []
for o in nonincluded:
    text = file_text.get(o, "")
    func_idents = {i for i in decl_re.findall(text) if i not in common}
    if not func_idents:
        truly.append((o, 0))
        continue
    used_outside = False
    for path, body in file_text.items():
        if path == o or path in nonincluded_set:
            continue
        for s in func_idents:
            if re.search(r'\b' + re.escape(s) + r'\b', body):
                used_outside = True
                break
        if used_outside:
            break
    if not used_outside:
        truly.append((o, len(func_idents)))

truly.sort()
out = Path("welle10_orphans.txt")
with out.open("w") as f:
    for o, n in truly:
        f.write(f"{o}|{n}\n")
print(f"include/ headers without #include consumer: {len(nonincluded)}")
print(f"include/ truly orphan: {len(truly)}")
print(f"Saved to {out}")
