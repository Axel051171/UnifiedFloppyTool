#!/usr/bin/env python3
"""Korpus-Abbilder, deren AUFBAU `gw` bestimmt hat (MF-783).

Fünf Formate standen auf **T3** — kein Test, keine Spec-Quelle. Jedes
behauptet eine **Geometrie**, und bis hierher stand jede Behauptung für
sich allein. `gw` 1.23 führt für alle fünf ein eigenes, unabhängiges
Modell derselben Geometrie; stimmen beide überein, ist das die Aussage,
die T1b von T3 trennt.

── Warum der Inhalt strukturiert ist und nicht zufällig ─────────────────

Ein Abbild aus Zufallsbytes belegt die Geometrie nur halb: es zeigt, dass
die richtige **Anzahl** Bytes zurückkommt, nicht dass sie in der
richtigen **Reihenfolge** stehen. Würde `gw` die Sektoren einer Spur
anders nummerieren als UFT, fiele das bei Zufallsdaten trotzdem als
byteidentisch auf — der Strom ginge nur durch dieselbe Permutation hin
und zurück.

Deshalb trägt hier jeder Sektor seine **eigene laufende Nummer** in den
ersten Bytes, gefolgt von einem daraus abgeleiteten Muster. Eine
Umsortierung wird damit sichtbar, und der Test kann Sektor *n* gezielt
prüfen statt nur die Dateigröße.

── Der Weg, reproduzierbar ──────────────────────────────────────────────

    python tests/corpus_manifest/gen_gw_geometry_corpus.py <gw.exe>

Je Format zweimal `gw convert`: Rohabbild → SCP → Rohabbild. Das
Ergebnis ist die Korpus-Datei. Beide Läufe müssen „100 %" melden und der
Rundlauf byteidentisch sein — sonst schreibt das Skript nichts.

── Was das NICHT belegt ─────────────────────────────────────────────────

Ein **Dateisystem**. Der Inhalt ist eigen; belegt ist allein, dass zwei
unabhängige Hände dieselbe Sektoraufteilung annehmen. Wer mehr
hineinliest, überzeichnet — dieselbe Grenze wie bei MF-782.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent.parent
ZIEL = WURZEL / "tests" / "corpus_free"

# (UFT-Plugin, gw-Format, Bytes, Sektorgröße, Sektoren gesamt)
FORMATE = [
    ("micropolis", "micropolis.100tpi.ss", 315392, 256, 1232),
    ("northstar",  "northstar.fm.ss",       89600, 256,  350),
    ("ssd",        "acorn.dfs.ss",         102400, 256,  400),
    ("po",         "apple2.prodos.140",    143360, 256,  560),
    ("trd",        "zx.trdos.ds80",        655360, 256, 2560),
    # Zweite Runde (MF-784). Alle fuenf sind KOPFLOSE Sektordumps —
    # Formate mit Container-Kopf (sap, 2img, vdk, fdi_pc98) oder
    # Archivstruktur (scl) kann gw grundsaetzlich nicht liefern.
    ("pdp",        "dec.rx01",             256256, 128, 2002),
    ("img",        "ibm.720",              737280, 512, 1440),
    ("t1k",        "ibm.1440",            1474560, 512, 2880),
    ("sam",        "ibm.800",              819200, 512, 1600),
    ("jvc",        "coco.decb",            161280, 256,  630),
]


def inhalt(n_bytes: int, sektorgroesse: int) -> bytes:
    """Jeder Sektor traegt seine laufende Nummer und ein daraus
    abgeleitetes Muster — eine Umsortierung wird dadurch sichtbar."""
    d = bytearray(n_bytes)
    for s in range(n_bytes // sektorgroesse):
        off = s * sektorgroesse
        d[off:off + 4] = b"UFT\x00"
        d[off + 4] = s & 0xFF
        d[off + 5] = (s >> 8) & 0xFF
        for i in range(6, sektorgroesse):
            d[off + i] = (s * 7 + i * 31) & 0xFF
    return bytes(d)


def gw(exe: str, fmt: str, ein: Path, aus: Path) -> str:
    r = subprocess.run([exe, "convert", "--format", fmt, str(ein), str(aus)],
                       capture_output=True, text=True)
    text = r.stdout + r.stderr
    for zeile in text.splitlines():
        if "Found" in zeile:
            return zeile.strip()
    return f"(kein Ergebnis, rc={r.returncode})"


def main() -> int:
    if len(sys.argv) < 2:
        print(__doc__)
        print("Aufruf: gen_gw_geometry_corpus.py <pfad/zu/gw.exe>")
        return 2
    exe = sys.argv[1]
    tmp = WURZEL / "build" / "_gwcorpus"
    tmp.mkdir(parents=True, exist_ok=True)

    fehler = 0
    for name, fmt, n, ss, sektoren in FORMATE:
        quelle = tmp / f"{name}_src.img"
        mitte = tmp / f"{name}.scp"
        ziel = ZIEL / f"gw_{name}.img"

        quelle.write_bytes(inhalt(n, ss))
        hin = gw(exe, fmt, quelle, mitte)
        rueck = gw(exe, fmt, mitte, ziel)

        ok = ziel.exists() and ziel.read_bytes() == quelle.read_bytes()
        if not ok:
            fehler += 1
            ziel.unlink(missing_ok=True)
        erwartet = f"Found {sektoren} sectors of {sektoren}"
        passt = erwartet in hin
        if not passt:
            fehler += 1
        print(f"  {'ok  ' if (ok and passt) else 'FAIL'} {name:11s} {fmt:22s} "
              f"{n:7d} B  {hin}")
        if not passt:
            print(f"       erwartet: {erwartet}")
        if not ok:
            print("       Rundlauf NICHT byteidentisch — nichts geschrieben")

    print(f"{'PASS' if fehler == 0 else 'FAIL'} ({fehler} Fehler)")
    return 1 if fehler else 0


if __name__ == "__main__":
    raise SystemExit(main())
