#!/usr/bin/env python3
"""Welche Dateigroessen verlangen die Format-Sonden? (MF-518)

── Warum es dieses Skript gibt ──────────────────────────────────────────

`tests/test_disk_open_fuzz.c` erreichte anfangs nur 44 der 137
registrierten Plugins. Der Grund ist keine Schwaeche des Fuzzers, sondern
die Bauart der Sonden: die meisten entscheiden ueber die DATEIGROESSE.

    static bool xyz_probe(const uint8_t *d, size_t s, size_t file_size,
                          int *conf) {
        if (file_size != 819200) return false;
        ...

An so einem Tor kommt eine Zufallsdatei beliebiger Laenge nie vorbei. Wer
diese Plugins erreichen will, muss ihnen genau die Groessen anbieten, die
sie verlangen.

Dieses Skript LIEST diese Groessen aus den Sonden, statt sie zu raten.
Seine Ausgabe ist die Liste `GATE_SIZES` in test_disk_open_fuzz.c.

── Wie gemessen wird ────────────────────────────────────────────────────

Fuer jede Funktion, deren Name `probe` enthaelt und die `bool`
zurueckgibt, werden alle Vergleiche gegen `file_size`, `size`, `fs` oder
`len` eingesammelt — sowohl Zahlenliterale als auch `#define`-Konstanten
derselben Datei. Beruecksichtigt wird, was zwischen 1 KB und 20 MB liegt;
darunter sind es Feldlaengen, darueber keine Diskettenabbilder.

Das ist bewusst grob: es soll Eingaben ERZEUGEN, nicht Formate belegen.
Ein Treffer heisst "diese Groesse oeffnet ein Tor", nicht "dieses Format
hat diese Groesse".
"""
from __future__ import annotations

import collections
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

FUNC = re.compile(
    r"^[ \t]*(?:static[ \t]+)?bool[ \t]+(\w*probe\w*)[ \t]*\([^;{]*\)[ \t]*\{",
    re.M)
CMP = re.compile(r"\b(?:file_size|size|fs|len)\s*(?:==|!=|>=|<=|>|<)\s*"
                 r"(\d{4,9}|0[xX][0-9a-fA-F]+)")
DEFN = re.compile(r"^#define\s+(\w+)\s+\(?(\d{4,9})\)?\s*(?:/\*|//|$)", re.M)
NAMED = re.compile(r"\b(?:file_size|size|fs|len)\s*(?:==|!=)\s*([A-Z_][A-Z0-9_]{3,})")

MIN_SIZE = 1024
MAX_SIZE = 20_000_000


def body_of(text: str, start: int) -> str:
    depth, i = 0, text.index("{", start)
    j = i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    return text[i:]


def measure() -> tuple[collections.Counter, dict]:
    sizes: collections.Counter = collections.Counter()
    per_probe: dict[str, list[int]] = {}
    for p in sorted((ROOT / "src" / "formats").rglob("*.c")):
        t = p.read_text(encoding="utf-8", errors="replace")
        consts = {k: int(v) for k, v in DEFN.findall(t)}
        for m in FUNC.finditer(t):
            body = body_of(t, m.start())
            found: set[int] = set()
            for v in CMP.findall(body):
                n = int(v, 16) if v.lower().startswith("0x") else int(v)
                if MIN_SIZE <= n <= MAX_SIZE:
                    found.add(n)
            for name in NAMED.findall(body):
                if name in consts and MIN_SIZE <= consts[name] <= MAX_SIZE:
                    found.add(consts[name])
            if found:
                key = "%s:%s" % (p.relative_to(ROOT).as_posix(), m.group(1))
                per_probe[key] = sorted(found)
                for n in found:
                    sizes[n] += 1
    return sizes, per_probe


def main() -> int:
    sizes, per_probe = measure()
    print("Sonden mit Groessen-Toren : %d" % len(per_probe))
    print("verschiedene Groessen     : %d" % len(sizes))

    if "--detail" in sys.argv:
        print()
        for k in sorted(per_probe):
            print("  %-60s %s" % (k, per_probe[k]))

    print("\nC-Liste fuer tests/test_disk_open_fuzz.c (GATE_SIZES):")
    vals = sorted(sizes)
    for i in range(0, len(vals), 6):
        print("        " + " ".join("%d," % v for v in vals[i:i + 6]))

    print("\nAm haeufigsten verlangt:")
    for n, c in sizes.most_common(8):
        print("  %9d  von %d Sonden" % (n, c))
    return 0


if __name__ == "__main__":
    sys.exit(main())
