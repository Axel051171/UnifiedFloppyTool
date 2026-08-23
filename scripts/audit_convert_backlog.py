#!/usr/bin/env python3
"""Was blockiert die ungeprueften Wandlungspfade? (MF-533)

── Warum es dieses Skript gibt ──────────────────────────────────────────

Die Wandlungstabelle fuehrt 44 Paare. Das Preflight-Tor (MF-263/UFT-A01)
bietet nur an, was in `src/core/uft_roundtrip.c` einen Eintrag hat; alles
andere wird als UNGEPRUEFT abgewiesen. Nach MF-526/527/532 stehen 31 auf
UNGEPRUEFT.

"31 offen" ist aber keine Arbeitsanweisung. Die Frage, die zaehlt, lautet:
**woran haengt jedes einzelne?** Es gibt genau drei Gruende, und sie
verlangen voellig Verschiedenes:

  KEIN WANDLER   Der Dispatcher hat keinen Zweig fuer das Paar. Ein
                 Eintrag waere eine Zusage auf nichts — der Zweig
                 `src_class == SECTOR && dst_class == SECTOR` gibt
                 dafuer ausdruecklich UFT_ERR_NOT_IMPLEMENTED zurueck
                 (UFT-A08: "a D64 with a .img extension is not a
                 successful IMG conversion"). Hier fehlt Code.

  KEIN KORPUS    Der Wandler existiert, aber es liegt keine Datei des
                 Quellformats unter tests/corpus_free/. Ohne Eingabe kein
                 Beweis. Hier fehlt Material — das ist Beschaffung, nicht
                 Programmierung.

  MESSBAR        Wandler da, Korpusdatei da. Diese Paare koennen HEUTE
                 belegt werden, ohne irgendetwas zu beschaffen.

Die dritte Gruppe ist die einzige, an der ohne den Eigentuemer
weitergearbeitet werden kann. Dieses Skript sagt, wie gross sie ist.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

TABLES = ROOT / "src/formats/uft_format_convert_tables.c"
DISPATCH = ROOT / "src/formats/uft_format_convert_dispatch.c"
ROUNDTRIP = ROOT / "src/core/uft_roundtrip.c"
CORPUS = ROOT / "tests/corpus_free"

# Endung -> Formatname der Tabelle. Nur was wirklich im Korpus liegt.
EXT_TO_FMT = {
    ".d64": "D64", ".d71": "D71", ".d81": "D81", ".d67": "D67",
    ".d80": "D80", ".d82": "D82", ".g64": "G64", ".g71": "G71",
    ".adf": "ADF", ".hfe": "HFE", ".atr": "ATR", ".xfd": "XFD",
    ".scp": "SCP", ".img": "IMG", ".imd": "IMD", ".td0": "TD0",
    ".stx": "STX", ".ipf": "IPF", ".woz": "WOZ", ".nib": "NIB",
    ".dsk": "DSK", ".nbz": "NBZ",
}


def load_table():
    t = TABLES.read_text(encoding="utf-8")
    return [(m.group(1), m.group(2)) for m in re.finditer(
        r"\.source = UFT_FORMAT_([A-Z0-9_]+), \.target = UFT_FORMAT_([A-Z0-9_]+)", t)
        if m.group(1) != "UNKNOWN"]


def load_status():
    t = ROUNDTRIP.read_text(encoding="utf-8")
    return {(m.group(1), m.group(2)): m.group(3) for m in re.finditer(
        r"\{\s*UFT_FORMAT_([A-Z0-9_]+),\s*UFT_FORMAT_([A-Z0-9_]+),\s*UFT_RT_([A-Z_]+)", t)}


def has_converter(src: str, dst: str, dispatch: str) -> bool:
    """Gibt es im Dispatcher einen Zweig fuer dieses Paar?

    Die Bedingungen sind teils zusammengesetzt
    (`(a || b) && (c || d)`), deshalb wird nicht auf ein Muster
    gematcht, sondern geprueft, ob beide Formatnamen in derselben
    if-Bedingung vorkommen. Identitaet (src == dst) hat einen eigenen
    Zweig weiter unten und zaehlt immer als vorhanden.
    """
    if src == dst:
        return True
    for m in re.finditer(r"if\s*\((.*?)\)\s*\{", dispatch, re.S):
        cond = m.group(1)
        if len(cond) > 600:
            continue
        if ("UFT_FORMAT_%s" % src) in cond and ("UFT_FORMAT_%s" % dst) in cond:
            if "src_format" in cond and "dst_format" in cond:
                return True
    return False


def corpus_formats() -> set:
    out = set()
    if not CORPUS.is_dir():
        return out
    for f in CORPUS.iterdir():
        fmt = EXT_TO_FMT.get(f.suffix.lower())
        if fmt:
            out.add(fmt)
    return out


def main() -> int:
    table = load_table()
    status = load_status()
    dispatch = DISPATCH.read_text(encoding="utf-8")
    have = corpus_formats()

    untested = [p for p in table if p not in status]
    groups = {"MESSBAR": [], "KEIN KORPUS": [], "KEIN WANDLER": []}
    for src, dst in untested:
        if not has_converter(src, dst, dispatch):
            groups["KEIN WANDLER"].append((src, dst))
        elif src not in have:
            groups["KEIN KORPUS"].append((src, dst))
        else:
            groups["MESSBAR"].append((src, dst))

    print("Wandlungstabelle          : %d Paare" % len(table))
    print("mit Rundlauf-Eintrag      : %d" % (len(table) - len(untested)))
    print("UNGEPRUEFT                : %d" % len(untested))
    print("Korpus-Quellformate       : %s"
          % (", ".join(sorted(have)) if have else "(keine)"))
    print()
    for k in ("MESSBAR", "KEIN KORPUS", "KEIN WANDLER"):
        v = groups[k]
        print("  %-14s %2d   %s" % (k, len(v),
              ", ".join("%s->%s" % p for p in v) if v else "—"))

    if groups["MESSBAR"]:
        print("\nDiese Paare koennen HEUTE belegt werden — Wandler und")
        print("Korpusdatei sind beide da. Alles andere braucht Code oder")
        print("Material.")
    else:
        print("\nKein Paar ist heute belegbar: was einen Wandler hat, hat")
        print("keine Korpusdatei, und was eine Korpusdatei hat, hat keinen")
        print("Wandler. Der Rest ist Beschaffung oder Implementierung.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
