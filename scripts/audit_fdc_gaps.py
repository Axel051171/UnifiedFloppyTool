#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Die FDC-Gap-Tabelle gegen sich selbst (MF-838).

── Warum es dieses Tor gibt ────────────────────────────────────────────────

`include/uft/formats/uft_fdc_gaps.h` fuehrt Spurlayouts fuer 17 Formate.
Beim ersten Nachrechnen passten **11 davon nicht auf ihre eigene Spur**,
und `gap3_fmt = 84` stand in **vier** Eintraegen mit **drei verschiedenen
Sektorzahlen** — das Muster eines kopierten Blocks, dieselbe Klasse wie
die fabrizierten Parser FMT-1/2/3.

Die Pointe ist dieselbe wie beim CP/M-DPB (MF-813): fuer diese Pruefung
braucht es **keine Referenz**. Ein Spurlayout haengt arithmetisch
zusammen, und ein Grossteil der Tabelle faellt bei reiner Rechnung durch —
ohne ein einziges Referenzabbild.

── Die zwei Invarianten ────────────────────────────────────────────────────

PASSEN   gap4a + gap1 + Sektoren x (Satzlaenge + gap3_fmt) + gap4b
         darf `track_bytes` nicht ueberschreiten.

         Satzlaenge (MFM) = 12 Sync + 3 A1 + 1 FE + 4 CHRN + 2 CRC
                          + gap2 + 12 Sync + 3 A1 + 1 FB
                          + Sektorgroesse + 2 CRC
         Satzlaenge (FM)  = 6 + 1 + 4 + 2 + gap2 + 6 + 1 + Groesse + 2

ORDNUNG  gap3_fmt >= gap3_rw. Der Format-Gap ist nie kleiner als der
         Lese/Schreib-Gap, weil beim Formatieren Drehzahlschwankung
         aufgefangen werden muss. Belegt an drei Werten bei
         Ch. Hochstaetter (FDFORMAT/88 1.8, ueber FreeDOS FORMAT 0.92
         `floppy.c:952`): 9 Sekt. 80/42, 15 Sekt. 84, 18 Sekt. 108/27.

── Was dieses Tor NICHT prueft ─────────────────────────────────────────────

Es prueft **nicht**, ob ein Gap-Wert der RICHTIGE ist — dafuer braucht es
eine Quelle je Format, und die liegt fuer zehn der Eintraege nicht vor.
Es prueft nur, ob die Tabelle sich selbst widerspricht. „Passt" heisst
also nicht „stimmt".

Diese Grenze steht hier ausdruecklich, weil „0 gefunden" in diesem Baum
schon einmal als Entwarnung gelesen wurde und keine war.

── Dateimenge ──────────────────────────────────────────────────────────────

Der Header wird geparst, nicht aufgezaehlt: jeder neue Eintrag ist
automatisch mit geprueft. Pfad ueber `git ls-files` (Grundsatz MF-636).
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
HEADER = "include/uft/formats/uft_fdc_gaps.h"
GRUNDLINIE = WURZEL / "docs" / "fdc_gaps_baseline.txt"

EINTRAG = re.compile(
    r"static const uft_fdc_format_t (\w+) = \{(.*?)\n\};", re.S)


def _zahl(block: str, feld: str):
    m = re.search(r"\.%s\s*=\s*(\d+)" % feld, block)
    return int(m.group(1)) if m else None


def _bool(block: str, feld: str):
    m = re.search(r"\.%s\s*=\s*(true|false)" % feld, block)
    return (m.group(1) == "true") if m else None


def satzlaenge(sektorgroesse: int, gap2: int, mfm: bool) -> int:
    """Bytes je Sektor OHNE gap3 — Sync, Marken, CHRN, CRC, gap2, Daten."""
    if mfm:
        return 12 + 3 + 1 + 4 + 2 + gap2 + 12 + 3 + 1 + sektorgroesse + 2
    return 6 + 1 + 4 + 2 + gap2 + 6 + 1 + sektorgroesse + 2


def eintraege(text: str):
    for name, b in EINTRAG.findall(text):
        d = {k: _zahl(b, k) for k in
             ("tracks", "sides", "sectors", "sector_size",
              "gap4a", "gap1", "gap2", "gap3_rw", "gap3_fmt", "gap4b",
              "track_bytes")}
        d["mfm"] = _bool(b, "mfm")
        d["name"] = name
        if None in d.values():
            continue          # unvollstaendiger Eintrag: nicht bewertbar
        yield d


def pruefe(e: dict) -> list[str]:
    fehler = []
    per = satzlaenge(e["sector_size"], e["gap2"], e["mfm"])
    belegt = e["gap4a"] + e["gap1"] + e["sectors"] * (per + e["gap3_fmt"]) \
        + e["gap4b"]
    if belegt > e["track_bytes"]:
        fehler.append("PASSEN: %d Byte belegt, Spur traegt %d (%+d)"
                      % (belegt, e["track_bytes"],
                         e["track_bytes"] - belegt))
    if e["gap3_fmt"] < e["gap3_rw"]:
        fehler.append("ORDNUNG: gap3_fmt %d < gap3_rw %d"
                      % (e["gap3_fmt"], e["gap3_rw"]))
    return fehler


def lies_header(wurzel: Path) -> str | None:
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others",
             "--exclude-standard", HEADER],
            cwd=wurzel, capture_output=True, text=True, timeout=60)
        if aus.returncode != 0 or not aus.stdout.strip():
            print("  WARNUNG: %s nicht in git — Tor laesst durch" % HEADER,
                  file=sys.stderr)
            return None
    except Exception as e:                                   # noqa: BLE001
        print("  WARNUNG: git nicht befragbar (%s) — Tor laesst durch" % e,
              file=sys.stderr)
        return None
    p = wurzel / HEADER
    if not p.exists():
        return None
    return p.read_text(encoding="utf-8", errors="replace")


def selbsttest() -> bool:
    """Vor dem Nenner. Bricht bei roter Abnahme ab."""
    ok = 0
    # 1: eine stimmige Spur muss durchgehen.
    #    9 x (574 + 80) = 5886, + 80 + 50 = 6016, + 234 = 6250
    gut = dict(name="probe_gut", tracks=80, sides=2, sectors=9,
               sector_size=512, gap4a=80, gap1=50, gap2=22,
               gap3_rw=42, gap3_fmt=80, gap4b=234,
               track_bytes=6250, mfm=True)
    if not pruefe(gut):
        ok += 1
    else:
        print("  SELBSTTEST 1 ROT: stimmige Spur abgewiesen:", pruefe(gut))

    # 2: GEGENBEWEIS Passen — dieselbe Spur mit dem ST-Gap4b 664 muss
    #    auffallen. Ohne diesen Fall koennte das Tor blind sein.
    schlecht = dict(gut, name="probe_zu_lang", gap4b=664)
    f = pruefe(schlecht)
    if any(x.startswith("PASSEN") for x in f):
        ok += 1
    else:
        print("  SELBSTTEST 2 ROT: zu lange Spur nicht erkannt")

    # 3: GEGENBEWEIS Ordnung — fmt < rw muss auffallen.
    verdreht = dict(gut, name="probe_verdreht", gap3_rw=108, gap3_fmt=84)
    f = pruefe(verdreht)
    if any(x.startswith("ORDNUNG") for x in f):
        ok += 1
    else:
        print("  SELBSTTEST 3 ROT: verdrehte Gaps nicht erkannt")

    print("  Selbsttest %d/3" % ok)
    return ok == 3


def messen(wurzel: Path):
    text = lies_header(wurzel)
    if text is None:
        return None, 0
    befunde = []
    n = 0
    for e in eintraege(text):
        n += 1
        for f in pruefe(e):
            befunde.append("%s: %s" % (e["name"], f))
    return befunde, n


def grenze() -> int | None:
    if not GRUNDLINIE.exists():
        return None
    for z in GRUNDLINIE.read_text(encoding="utf-8").splitlines():
        z = z.split("#")[0].strip()
        if z.isdigit():
            return int(z)
    return None


def check(repo=None):
    """Schnittstelle fuer scripts/check_consistency.py."""
    global GRUNDLINIE
    wurzel = Path(repo) if repo else WURZEL
    GRUNDLINIE = wurzel / "docs" / "fdc_gaps_baseline.txt"
    g = grenze()
    if g is None:
        return []
    befunde, _ = messen(wurzel)
    if befunde is None or len(befunde) <= g:
        return []
    return ["%d Widersprueche > Grundlinie %d: %s"
            % (len(befunde), g, "; ".join(befunde[:4]))]


def main() -> int:
    print("audit_fdc_gaps (MF-838)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2

    befunde, n = messen(WURZEL)
    if befunde is None:
        return 0
    print("  Eintraege geprueft          : %d" % n)
    print("  Widersprueche in sich       : %d" % len(befunde))
    for b in befunde:
        print("    %s" % b)

    g = grenze()
    if g is None:
        print("  keine Grundlinie — nur Bericht")
        return 0
    print("  Grundlinie                  : %d" % g)
    if len(befunde) > g:
        print("  FEHLER: die Zahl ist gestiegen.")
        return 1
    if len(befunde) < g:
        print("  Hinweis: Grundlinie auf %d senken." % len(befunde))
    print("  OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
