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

    Rohabbild --Hand 1--> Container --Hand 2--> Rohabbild

Ist der Rundlauf nicht byteidentisch, wird **nichts geschrieben**. Damit
kann kein Eintrag entstehen, der nur auf einer Hand steht.

── Welche Hand SCHREIBT, ist nicht beliebig (MF-806) ────────────────────

MF-785 hat gemessen, für welche Formate die beiden Werkzeuge **dieselbe
Hand wie UFT** sind (fünfte Frage): SAMdisk für `cqm`, `do`, `fdi`,
`hfe`, `msa`, `st`, `td0`, `udi`; hxcfe für `dim_atari`, `hfe`,
`hxcstream`.

Für `msa` heißt das: UFTs Leser ist gegen `src/samdisk/msa.cpp`
geschrieben. Ließe man SAMdisk die Datei **erzeugen**, bestätigte der
Test nur, wovon er abgeleitet ist — dieselbe Lage, die MF-780 bei
`openMSX`/`msx_disk` erkannt und verworfen hat. Also **muss hxcfe
schreiben**, und SAMdisk liest zurück.

Deshalb trägt jeder Eintrag seinen Erzeuger, und der Dateiname im Korpus
nennt ihn: `hxcfe_msa.msa`, `samdisk_edsk.dsk`.

── Zwei gemessene Eigenheiten der Werkzeuge ─────────────────────────────

* **hxcfe schreibt 84 Zylinder**, auch wenn das Layout 80 nennt — sein
  Plattenmodell hat 84 Spuren, die überzähligen bleiben leer. Eine
  80-Zylinder-Quelle kann den Rundlauf deshalb gar nicht schließen; die
  Quelle wird zu 84 erzeugt.
* **SAMdisk 4.0 schreibt roh nur als `.raw`.** `.img` weist es mit
  „unknown output file type" ab, `.st` mit „ST is not supported for
  output". Gemessen, nicht vermutet.

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

# (UFT-Plugin, Endung, Erzeuger, Zyl, Koepfe, Sektoren, Sektorgroesse,
#  erster Sektor, hxcfe-Layout)
#
# ERZEUGER ist die Hand, die den Container SCHREIBT, und sie darf nicht
# die sein, aus der UFTs Leser stammt (fuenfte Frage, MF-785). Die
# jeweils andere liest zurueck.
#
#   edsk  UFTs Leser hat keine der beiden als Quelle -> SAMdisk schreibt
#   msa   UFTs Leser stammt aus src/samdisk/msa.cpp  -> hxcfe MUSS
#         schreiben, sonst bestaetigt der Test nur, wovon er abgeleitet
#         ist
FORMATE = [
    ("edsk", "dsk", "samdisk", 80, 2, 9, 512, 1, None),
    ("msa",  "msa", "hxcfe",   84, 2, 9, 512, 1, "ATARIST_DD_720KB"),
    # MF-810: dieselbe Geometrie, aber KOMPRIMIERBARER Inhalt. Die
    # Zeile darueber laeuft nie durch die RLE-Kette, weil das
    # Markenmuster nicht komprimiert (774 490 B fuer 774 144 B
    # Nutzdaten — MSA legt eine Spur unkomprimiert ab, wenn
    # Kompression sie groesser macht). Gemessen wird die Kette erst
    # hier: 15 484 B statt 774 490.
    ("msa_rle", "msa", "hxcfe", 84, 2, 9, 512, 1, "ATARIST_DD_720KB"),
]


HXC_KONV = {"msa": "ATARIST_MSA", "msa_rle": "ATARIST_MSA"}


def inhalt(n_bytes: int, sektorgroesse: int, komprimierbar: bool = False) -> bytes:
    """Jeder Sektor traegt seine laufende Nummer — eine Umsortierung wird
    dadurch sichtbar. Gleiche Bauart wie gen_gw_geometry_corpus.py.

    `komprimierbar` fuellt den Rest mit konstantem 0xE5 statt mit einem
    Muster. Nur so laeuft ein Container mit RLE (MSA) ueberhaupt durch
    seine Kompressionskette — MF-810."""
    d = bytearray([0xE5]) * n_bytes if komprimierbar else bytearray(n_bytes)
    for s in range(n_bytes // sektorgroesse):
        off = s * sektorgroesse
        d[off:off + 4] = b"UFT\x00"
        d[off + 4] = s & 0xFF
        d[off + 5] = (s >> 8) & 0xFF
        if not komprimierbar:
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
    for name, ext, erzeuger, z, k, spt, ss, basis, layout in FORMATE:
        n = z * k * spt * ss
        quelle = tmp / f"{name}_src.img"
        quelle.write_bytes(inhalt(n, ss, name.endswith("_rle")))
        behaelter = tmp / f"{name}.{ext}"
        zurueck = tmp / (f"{name}_zurueck.img" if erzeuger == "samdisk"
                         else f"{name}_zurueck.raw")
        ziel = ZIEL / f"{erzeuger}_{name}.{ext}"
        geraten = False

        if erzeuger == "samdisk":
            # Hand 1: SAMdisk schreibt, mit EXPLIZITER Geometrie.
            r1 = subprocess.run(
                [samdisk, "copy", str(quelle), str(behaelter),
                 f"-c{z}", f"-s{spt}", f"-z{groessencode(ss)}", f"-b{basis}",
                 "-f"],
                capture_output=True, text=True)
            geraten = "guessed from file size" in (r1.stdout + r1.stderr)
            # Hand 2: hxcfe liest zurueck.
            subprocess.run([hxcfe, f"-finput:{behaelter}",
                            "-conv:RAW_LOADER", f"-foutput:{zurueck}"],
                           capture_output=True, text=True)
        else:
            # Hand 1: hxcfe schreibt, mit benanntem Layout.
            subprocess.run([hxcfe, f"-finput:{quelle}",
                            f"-uselayout:{layout}",
                            f"-conv:{HXC_KONV[name]}",
                            f"-foutput:{behaelter}"],
                           capture_output=True, text=True)
            # Hand 2: SAMdisk liest zurueck. `.raw` ist die einzige rohe
            # Ausgabeform, die SAMdisk 4.0 kennt — `.img` und `.st` weist
            # es ab („unknown output file type" / „ST is not supported
            # for output"), gemessen.
            subprocess.run([samdisk, "copy", str(behaelter), str(zurueck),
                            "-f"], capture_output=True, text=True)

        rund = zurueck.exists() and zurueck.read_bytes() == quelle.read_bytes()
        if rund and not geraten:
            ziel.write_bytes(behaelter.read_bytes())
        else:
            fehler += 1
            ziel.unlink(missing_ok=True)

        print(f"  {'ok  ' if (rund and not geraten) else 'FAIL'} {name:6s} "
              f"{erzeuger:8s} "
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
