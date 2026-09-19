#!/usr/bin/env python3
"""Erzeugt `UFT_GEOM_TABLE_MAX_SECTOR` AUS der Geometrietafel (MF-1255).

── Warum es diesen Generator gibt ───────────────────────────────────────

`dsk_gen_write_track()` fuellt fehlende Sektoren aus einem Puffer:

    uint8_t pad[1024];
    memset(pad, 0xE5, g->sector_size);

Geschrieben wird, was die Tafelzeile sagt. Gemessen ist das heute
sicher — keine der 49 Zeilen geht ueber 1024 Byte (256 x 22, 512 x 20,
1024 x 2, 128 x 5). Aber „heute sicher, weil keine Zeile darueber
liegt" ist die Beschreibung eines Fehlers, der noch nicht passiert ist:
die Sampler-Klasse benutzt ausdruecklich 2048 Byte (CLAUDE.md §MF-1176
nennt „512-/1024-/2048-Byte-Sektoren"), und `P3-427` haelt die
Registrierung der Roland-S-Serie offen.

Der Eigentuemer hat die Bauform vorgegeben, woertlich:

    „_Static_assert(sizeof(pad) >= UFT_GEOM_TABLE_MAX_SECTOR, ...)
     wobei UFT_GEOM_TABLE_MAX_SECTOR aus der Tafel erzeugt wird, nicht
     daneben gepflegt. Dann bricht der Bau in dem Commit, der die Zeile
     hinzufuegt — und nicht die Diskette, die sie spaeter liest."

Eine daneben gepflegte Konstante waere die Doppelhaltung, gegen die K4
steht: wer eine 2048er-Zeile eintraegt, wuerde die Konstante vergessen,
und der Ueberlauf waere zurueck.

── Was der Generator NICHT tut ──────────────────────────────────────────

Er vergroessert den Puffer nicht. Er stellt nur fest, was die Tafel
verlangt; ob `pad` wachsen muss oder die Zeile anders aussehen sollte,
entscheidet der Mensch, der den Bauabbruch liest.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[2]
QUELLE = WURZEL / "src" / "formats" / "dsk_generic" / "uft_dsk_generic.c"
ZIEL = WURZEL / "include" / "uft" / "formats" / "uft_dsk_geom_max_gen.h"

# Eine Tafelzeile: {cyl, heads, spt, sector_size, expected_size} /* NAME */
_ZEILE = re.compile(
    r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}"
    r"\s*/\*\s*(\w+)")


def tafel(quelle: Path) -> list:
    """Die Geometriezeilen als `(cyl, heads, spt, ss, erwartet, name)`.

    Gelesen wird NUR der Block zwischen `dsk_geometries[] = {` und dem
    zugehoerigen `};` — sonst faenden sich auch Klammerpaare aus
    anderen Tafeln derselben Datei.
    """
    t = quelle.read_text(encoding="utf-8", errors="replace")
    anfang = t.find("dsk_geometries[] = {")
    if anfang < 0:
        return []
    ende = t.find("};", anfang)
    if ende < 0:
        return []
    return [(int(a), int(b), int(c), int(d), int(e), n)
            for a, b, c, d, e, n in _ZEILE.findall(t[anfang:ende])]


def max_sektorgroesse(quelle: Path):
    """Die groesste `sector_size` der Tafel, oder `None`."""
    zeilen = tafel(quelle)
    return max((z[3] for z in zeilen), default=None)


def erzeuge(quelle: Path) -> str:
    zeilen = tafel(quelle)
    groesste = max_sektorgroesse(quelle)
    if groesste is None:
        raise SystemExit("Geometrietafel nicht gefunden: %s" % quelle)

    traeger = sorted({z[5] for z in zeilen if z[3] == groesste})
    verteilung = {}
    for z in zeilen:
        verteilung[z[3]] = verteilung.get(z[3], 0) + 1

    z = []
    z.append("/* ====================================================="
             "================")
    z.append(" * GENERATED FILE — DO NOT EDIT BY HAND.")
    z.append(" * Source of truth: src/formats/dsk_generic/"
             "uft_dsk_generic.c (dsk_geometries[])")
    z.append(" * Regenerate with: python scripts/generators/"
             "gen_dsk_geom_max.py")
    z.append(" * Any manual edits will be overwritten on the next "
             "generator run.")
    z.append(" * ==================================================="
             "================== */")
    z.append("#ifndef UFT_DSK_GEOM_MAX_GEN_H")
    z.append("#define UFT_DSK_GEOM_MAX_GEN_H")
    z.append("")
    z.append("/**")
    z.append(" * @file uft_dsk_geom_max_gen.h")
    z.append(" * @brief Die groesste Sektorgroesse der DSK-Geometrietafel "
             "(ERZEUGT).")
    z.append(" *")
    z.append(" * Damit ein Puffer, der eine Tafelzeile fuellt, sich "
             "gegen die Tafel")
    z.append(" * absichern kann — zur UEBERSETZUNGSZEIT, nicht erst an "
             "der Diskette:")
    z.append(" *")
    z.append(" *     _Static_assert(sizeof(pad) >= "
             "UFT_GEOM_TABLE_MAX_SECTOR, \"...\");")
    z.append(" *")
    z.append(" * Gemessen beim Erzeugen: %d Zeilen, Verteilung"
             % len(zeilen))
    for ss in sorted(verteilung):
        z.append(" *   %5d Byte : %2d Zeilen" % (ss, verteilung[ss]))
    z.append(" *")
    z.append(" * Die groesste tragen: %s" % ", ".join(traeger))
    z.append(" */")
    z.append("#define UFT_GEOM_TABLE_MAX_SECTOR %du" % groesste)
    z.append("")
    z.append("#endif /* UFT_DSK_GEOM_MAX_GEN_H */")
    return "\n".join(z) + "\n"


def _selbsttest() -> int:
    """Der Generator muss die Tafel LESEN, nicht raten — und er muss
    merken, wenn eine groessere Zeile dazukommt."""
    import tempfile

    gruen = 0
    rot = 0

    def zusage(b: bool, was: str) -> None:
        nonlocal gruen, rot
        if b:
            gruen += 1
            print("   [ok ] %s" % was)
        else:
            rot += 1
            print("   [ROT] %s" % was)

    probe = (
        "static const dsk_geometry_t dsk_geometries[] = {\n"
        "    {40, 2, 16, 256, 327680}  /* DSK_A */,\n"
        "    {77, 2,  8, 1024, 1261568}  /* DSK_B */,\n"
        "};\n"
        "static const other_t andere[] = { {1,2,3,4096,5} /* X */ };\n")
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "q.c"
        p.write_text(probe, encoding="utf-8")
        zusage(max_sektorgroesse(p) == 1024,
               "liest die groesste Sektorgroesse aus der Tafel (1024)")
        zusage(len(tafel(p)) == 2,
               "GEGENPROBE: eine Tafel DANEBEN wird nicht mitgelesen — "
               "sonst waere die Antwort 4096")

        # ROT-PROBE: eine groessere Zeile muss die Konstante heben.
        p.write_text(probe.replace("{77, 2,  8, 1024, 1261568}",
                                   "{77, 2,  4, 2048, 1261568}"),
                     encoding="utf-8")
        zusage(max_sektorgroesse(p) == 2048,
               "ROT-PROBE: eine 2048er-Zeile hebt die Konstante — genau "
               "der Tag, an dem der Bau brechen soll")

        # Und ohne Tafel gibt es keine erfundene Zahl.
        p.write_text("int x;\n", encoding="utf-8")
        zusage(max_sektorgroesse(p) is None,
               "ohne Tafel keine Zahl — lieber nichts als geraten")

    print("\nSELBSTTEST %d/%d" % (gruen, gruen + rot))
    return 1 if rot else 0


if __name__ == "__main__":
    if "--selbsttest" in sys.argv:
        raise SystemExit(_selbsttest())
    if _selbsttest() != 0:
        print("Selbsttest ROT — es wird nichts geschrieben.")
        raise SystemExit(1)
    print()
    text = erzeuge(QUELLE)
    if "--stdout" in sys.argv:
        print(text)
    else:
        ZIEL.parent.mkdir(parents=True, exist_ok=True)
        ZIEL.write_text(text, encoding="utf-8", newline="\n")
        print("geschrieben: %s (max %s)"
              % (ZIEL.relative_to(WURZEL), max_sektorgroesse(QUELLE)))
    raise SystemExit(0)
