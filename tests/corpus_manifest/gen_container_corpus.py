#!/usr/bin/env python3
"""Korpus-Abbilder für Formate MIT Container-Kopf (MF-796).

Das Rezept aus `gen_gw_geometry_corpus.py` reicht hier nicht: `gw` kann
nur kopflose Sektordumps schreiben. Container mit Kopf beherrschen
**SAMdisk 4.0** und **hxcfe 2.16.13**.

── Was MF-785 falsch geschlossen hatte ──────────────────────────────────

Dort standen fünf Kandidaten, alle fielen durch, und die Ursache wurde
in SAMdisks Warnung *„input format guessed from file size"* gesehen.
Gemessen (MF-794) trägt das nicht: mit **expliziter** Geometrie kommt
für `sad` eine Datei heraus, die mit der geratenen **byteidentisch**
ist. Das Raten war folgenlos.

Falsch war die **Erwartung**. Ein Container legt den Inhalt nach seinem
eigenen Format ab — SAD zum Beispiel **kopf-dur** (alle Zylinder von
Seite 0, dann alle von Seite 1). Der Test verglich gegen eine lineare
Anordnung und meldete „umsortiert", wo das Zielformat einfach anders
ordnet. Der echte Fehler lag bei UFT (MF-794).

── Die Zwei-Hände-Prüfung, eingebaut ────────────────────────────────────

Ein von SAMdisk erzeugter Container belegt allein noch nichts: es wäre
EINE Hand. Dieses Skript schreibt eine Datei deshalb nur, wenn eine
**zweite, unabhängige** Hand sie zurücklesen kann und dabei genau das
Rohabbild wieder herausgibt, aus dem sie entstanden ist:

    Rohabbild --SAMdisk--> Container --hxcfe--> Rohabbild

Ist der Rundlauf nicht byteidentisch, wird **nichts geschrieben**. Damit
kann kein Eintrag entstehen, der nur auf einer Hand steht.

MF-785 hat vorher geprüft, für welche Formate die beiden Werkzeuge
DIESELBE Hand wie UFT sind (fünfte Frage): SAMdisk für `cqm`, `do`,
`fdi`, `hfe`, `msa`, `st`, `td0`, `udi`; hxcfe für `dim_atari`, `hfe`,
`hxcstream`. Keines der Formate unten steht auf einer der beiden Listen.

── Was das NICHT belegt ─────────────────────────────────────────────────

Ein **Dateisystem**. Der Inhalt ist eigen; belegt ist die
Sektoraufteilung und ihre Reihenfolge, mehr nicht.

── Der Weg, reproduzierbar ──────────────────────────────────────────────

    python tests/corpus_manifest/gen_container_corpus.py <SAMdisk.exe> <hxcfe.exe>
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent.parent
ZIEL = WURZEL / "tests" / "corpus_free"

# (UFT-Plugin, Dateiendung, Zylinder, Koepfe, Sektoren, Sektorgroesse,
#  erster Sektor)
FORMATE = [
    ("edsk", "dsk", 80, 2, 9, 512, 1),
]


def inhalt(n_bytes: int, sektorgroesse: int) -> bytes:
    """Jeder Sektor traegt seine laufende Nummer — eine Umsortierung wird
    dadurch sichtbar. Gleiche Bauart wie gen_gw_geometry_corpus.py."""
    d = bytearray(n_bytes)
    for s in range(n_bytes // sektorgroesse):
        off = s * sektorgroesse
        d[off:off + 4] = b"UFT\x00"
        d[off + 4] = s & 0xFF
        d[off + 5] = (s >> 8) & 0xFF
        for i in range(6, sektorgroesse):
            d[off + i] = (s * 7 + i * 31) & 0xFF
    return bytes(d)


def groessencode(ss: int) -> int:
    """SAMdisks -z ist der IBM-Groessencode: 128 << N."""
    n, v = 0, 128
    while v < ss:
        v <<= 1
        n += 1
    return n


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        print("Aufruf: gen_container_corpus.py <SAMdisk.exe> <hxcfe.exe>")
        return 2
    samdisk, hxcfe = sys.argv[1], sys.argv[2]
    tmp = WURZEL / "build" / "_containerkorpus"
    tmp.mkdir(parents=True, exist_ok=True)

    fehler = 0
    for name, ext, z, k, s, ss, basis in FORMATE:
        n = z * k * s * ss
        quelle = tmp / f"{name}_src.img"
        quelle.write_bytes(inhalt(n, ss))
        behaelter = tmp / f"{name}.{ext}"
        zurueck = tmp / f"{name}_zurueck.img"
        ziel = ZIEL / f"samdisk_{name}.{ext}"

        # Hand 1: SAMdisk schreibt, mit EXPLIZITER Geometrie.
        befehl = [samdisk, "copy", str(quelle), str(behaelter),
                  f"-c{z}", f"-s{s}", f"-z{groessencode(ss)}", f"-b{basis}",
                  "-f"]
        r1 = subprocess.run(befehl, capture_output=True, text=True)
        geraten = "guessed from file size" in (r1.stdout + r1.stderr)

        # Hand 2: hxcfe liest zurueck.
        subprocess.run([hxcfe, f"-finput:{behaelter}", "-conv:RAW_LOADER",
                        f"-foutput:{zurueck}"], capture_output=True, text=True)

        rund = zurueck.exists() and zurueck.read_bytes() == quelle.read_bytes()
        if rund and not geraten:
            ziel.write_bytes(behaelter.read_bytes())
        else:
            fehler += 1
            ziel.unlink(missing_ok=True)

        print(f"  {'ok  ' if (rund and not geraten) else 'FAIL'} {name:6s} "
              f"{behaelter.stat().st_size if behaelter.exists() else 0:8d} B "
              f"Container, Rundlauf {'byteidentisch' if rund else 'NICHT gleich'}"
              f"{'' if not geraten else ', GERATENE Geometrie'}")
        if not rund:
            print("       nichts geschrieben — ein Container auf EINER Hand "
                  "belegt nichts")

    print(f"{'PASS' if fehler == 0 else 'FAIL'} ({fehler} Fehler)")
    return 1 if fehler else 0


if __name__ == "__main__":
    raise SystemExit(main())
