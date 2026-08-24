#!/usr/bin/env python3
"""Eine Schranke auf X, indiziert wird X + Konstante (MF-563)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

ASan hat in der CI gemeldet:

    heap-buffer-overflow, READ of size 1
    dmk_read_track   src/formats/dmk/uft_dmk.c:121
    29 Byte rechts von einem 6047-Byte-Bereich

Die Stelle lautete:

    uint16_t idam_off = (ptr & 0x3FFF) - DMK_IDAM_SIZE;
    if (idam_off >= p->track_len - 20) continue;
    uint8_t* idam = &tbuf[DMK_IDAM_SIZE + idam_off];
    if (idam[0] != 0xFE) continue;                    /* <- hier */

**Die Schranke prueft `idam_off`, indiziert wird mit
`DMK_IDAM_SIZE + idam_off`.** Der geprueste Wert und der benutzte Index
sind um 128 verschieden — die Pruefung ist also um 128 zu kurz.

Nachgerechnet an der ASan-Meldung: `track_len` 6047, gelesen bei
6076 = 128 + 5948. Die Schranke fragte `5948 >= 6027`, das ist falsch,
also lief es durch.

Das ist eine eigene Bauart, und die bestehenden Tore fangen sie nicht:

  * `audit_negative_index.py` sucht FEHLENDE untere Schranken. Hier ist
    eine da, sie ist nur auf der falschen Groesse.
  * `audit_unbounded_alloc.py` sucht Werte OHNE Schranke. Hier gibt es
    eine.

Eine Pruefung, die die falsche Zahl prueft, sieht aus wie eine Pruefung.
Das ist dieselbe Familie wie die Tautologien aus MF-552 (`crc_valid`,
Dateigroesse): der Code TUT etwas, das nach Sorgfalt aussieht, und deckt
nichts.

── Was gesucht wird ─────────────────────────────────────────────────────

In `src/formats/**`: eine Schranke `if (V >= …) continue|return|break`,
und innerhalb der naechsten sechs Zeilen ein Index `[K + V]` oder
`[V + K]`, in dem V mit etwas anderem verrechnet wird.

── Rotbeweis ────────────────────────────────────────────────────────────

Gegen den Stand vor der Reparatur (`git show HEAD:…/uft_dmk.c`) meldet
dieses Muster **zwei** Stellen — Leser und Schreiber, dieselbe Rechnung
zweimal. Gegen den reparierten Stand meldet es **null**. Ein Tor, das
gegen den bekannten Fehler nicht feuert, waere keines.

── Grenzen ──────────────────────────────────────────────────────────────

Textuell und auf sechs Zeilen Abstand begrenzt. Nicht gefunden werden
Faelle, in denen zwischen Schranke und Zugriff mehr steht, oder in denen
der Versatz nicht als `K + V` dasteht, sondern in einer eigenen Variablen
zwischengelagert wird.

Es faengt die Form, die vorkam.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

BOUND = re.compile(
    r"if\s*\(\s*(\w+)\s*(?:>=|>)\s*[^)]*\)\s*(?:continue|return|break)")

LOOKAHEAD = 6

BASELINE: dict[str, str] = {}


def _strip_comments(text: str) -> str:
    def keep(m):
        return "\n" * m.group(0).count("\n")
    text = re.sub(r"/\*.*?\*/", keep, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    d = repo / "src" / "formats"
    if not d.exists():
        return errors

    for p in sorted(d.rglob("*.c")):
        try:
            raw = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        lines = _strip_comments(raw).splitlines()
        rel = p.relative_to(repo).as_posix()

        for i, ln in enumerate(lines):
            m = BOUND.search(ln)
            if not m:
                continue
            v = re.escape(m.group(1))
            idx_pat = re.compile(
                r"\[\s*([A-Za-z_]\w*)\s*\+\s*" + v + r"\s*\]"
                r"|\[\s*" + v + r"\s*\+\s*([A-Za-z_]\w*)\s*\]")
            for k in range(i + 1, min(i + 1 + LOOKAHEAD, len(lines))):
                hit = idx_pat.search(lines[k])
                if not hit:
                    continue
                other = hit.group(1) or hit.group(2)
                key = f"{rel}:{k + 1}:{m.group(1)}"
                if key in BASELINE:
                    break
                errors.append(
                    f"{rel}:{k + 1}: die Schranke in Zeile {i + 1} prueft "
                    f"`{m.group(1)}`, indiziert wird mit "
                    f"`{other} + {m.group(1)}`. Geprueft und benutzt sind "
                    f"nicht dasselbe — die Schranke ist um `{other}` zu "
                    f"kurz. Vorbild MF-563: ASan fand genau das in "
                    f"dmk_read_track (128 Byte Versatz, 29 Byte hinter "
                    f"einem 6047-Byte-Puffer).")
                break

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Schranke auf der falschen Groesse (root={repo}):")
    print(f"  begruendete Ausnahmen : {len(BASELINE)}")
    print(f"  Befunde               : {len(errs)}")
    for e in errs:
        print(f"    {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
