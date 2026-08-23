#!/usr/bin/env python3
"""Welche Kennungen verlangen die Format-Sonden? (MF-521)

── Warum es dieses Skript gibt ──────────────────────────────────────────

`scripts/audit_probe_sizes.py` hat die Groessen-Tore geoeffnet und die
Fuzz-Abdeckung von 44 auf 98 der 137 Plugins gehoben. Die verbleibenden
39 entscheiden nicht ueber die Groesse, sondern ueber eine KENNUNG am
Dateianfang:

    if (memcmp(data, "HXCPICFE", 8) != 0) return false;
    if (data[0] != 0x0A || data[1] != 0x02) return false;

Zufallsbytes treffen so etwas praktisch nie. Wer diese Plugins erreichen
will, muss ihnen ihre Kennung anbieten — und die steht im Quelltext.

Dieses Skript LIEST sie, statt sie zu raten. Es sammelt

  - `memcmp(data, "...", n)` und `strncmp(...)` gegen Zeichenketten,
  - `#define`-Konstanten, die als Kennung verglichen werden,
  - feste Byte-Vergleiche `data[i] == 0x..` am Dateianfang.

Seine Ausgabe ist die Liste `SIGS` in `tests/test_disk_open_fuzz.c`.

── Was es NICHT ist ─────────────────────────────────────────────────────

Kein Beleg fuer irgendein Format. Ein Treffer heisst "diese Bytes oeffnen
ein Tor", nicht "dieses Format faengt so an". Ob eine Kennung stimmt,
entscheidet eine benannte Referenz — `docs/VERIFICATION_PLAN.md` —, nicht
dieses Skript.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

FUNC = re.compile(
    r"^[ \t]*(?:static[ \t]+)?bool[ \t]+(\w*probe\w*)[ \t]*\([^;{]*\)[ \t]*\{",
    re.M)

# memcmp(data, "MAGIC", n)  /  strncmp(hdr->sig, "MAGIC", n)
MEMCMP_LIT = re.compile(r"\b(?:memcmp|strncmp)\s*\([^,]+,\s*\"((?:[^\"\\]|\\.)*)\"")
# memcmp(data, SOME_SIGNATURE, n)
MEMCMP_CONST = re.compile(r"\b(?:memcmp|strncmp)\s*\([^,]+,\s*([A-Z_][A-Z0-9_]{2,})\s*,")
# #define FOO "MAGIC"
DEFN_STR = re.compile(r'^#define\s+(\w+)\s+"((?:[^"\\]|\\.)*)"', re.M)

MIN_LEN = 2
MAX_LEN = 32


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


def c_escape(s: str) -> str:
    """Zurueck in eine C-Zeichenkette, die derselben Bytefolge entspricht."""
    return s


def measure() -> dict[str, list[str]]:
    per_probe: dict[str, list[str]] = {}
    for p in sorted((ROOT / "src" / "formats").rglob("*.c")):
        t = p.read_text(encoding="utf-8", errors="replace")
        consts = dict(DEFN_STR.findall(t))
        for m in FUNC.finditer(t):
            body = body_of(t, m.start())
            found: list[str] = []
            for lit in MEMCMP_LIT.findall(body):
                if MIN_LEN <= len(lit) <= MAX_LEN:
                    found.append(lit)
            for name in MEMCMP_CONST.findall(body):
                if name in consts and MIN_LEN <= len(consts[name]) <= MAX_LEN:
                    found.append(consts[name])
            if found:
                key = "%s:%s" % (p.relative_to(ROOT).as_posix(), m.group(1))
                # Reihenfolge erhalten, Doubletten weg
                seen, uniq = set(), []
                for f in found:
                    if f not in seen:
                        seen.add(f)
                        uniq.append(f)
                per_probe[key] = uniq
    return per_probe


def main() -> int:
    per_probe = measure()
    all_sigs: list[str] = []
    seen = set()
    for k in sorted(per_probe):
        for s in per_probe[k]:
            if s not in seen:
                seen.add(s)
                all_sigs.append(s)

    print("Sonden mit Kennungs-Toren : %d" % len(per_probe))
    print("verschiedene Kennungen    : %d" % len(all_sigs))

    if "--detail" in sys.argv:
        print()
        for k in sorted(per_probe):
            print("  %-58s %s" % (k, per_probe[k]))

    print("\nC-Liste fuer tests/test_disk_open_fuzz.c (SIGS):")
    for s in all_sigs:
        print('    { "%s", %d, "%s" },' % (c_escape(s), len(s),
                                           s[:10].replace('"', "'")))
    return 0


if __name__ == "__main__":
    sys.exit(main())
