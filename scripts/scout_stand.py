#!/usr/bin/env python3
"""Was hat der Scout gesichtet, und was ist daraus geworden? (MF-675)

    python scripts/scout_stand.py [--offen]

── Warum es dieses Skript gibt ──────────────────────────────────────────

Auf die Frage „hat der Scout alle Listen abgearbeitet" liess sich aus
dem Baum **keine Antwort** geben, und das war der eigentliche Befund:

  * Die eingereichten Repo-Listen standen nur im Gespraechsverlauf.
    Nirgendwo im Baum stand, was beauftragt war — also konnte niemand
    pruefen, ob etwas fehlt.
  * Gemessen werden konnte nur, was ANGEKOMMEN ist: geklonte Repos,
    Messdateien, Gutachten. Das beantwortet „was wurde getan", nicht
    „wurde alles getan".

Der Unterschied ist derselbe wie zwischen einem gruenen Test und einem
Test, der scheitern kann. „26 Gutachten" klingt nach Vollstaendigkeit
und ist eine Zahl ohne Nenner.

Dieses Skript liefert den Nenner, soweit er messbar ist, und sagt
ausdruecklich, wo er fehlt. Es zaehlt NICHT aus einer gepflegten Liste,
sondern aus dem, was auf der Platte liegt — dieselbe Regel wie
`repo_scope.py`: Verzeichnisse fragen, nicht Aufzaehlungen pflegen.

── Was es NICHT sagen kann ──────────────────────────────────────────────

Ob ein eingereichtes Repo nie geklont wurde. Dafuer muesste der Auftrag
im Baum stehen. Wer eine Liste uebergibt, traegt sie nach
`tools/uft-scout/data/auftraege.json` ein — dann ist die Frage beim
naechsten Mal beantwortbar.
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
SCOUT = WURZEL / "tools" / "uft-scout"
WORK = SCOUT / "work"
OUT = SCOUT / "out"
AUFTRAEGE = SCOUT / "data" / "auftraege.json"
NEGATIVE = SCOUT / "data" / "known_negatives.json"
LISTE = WURZEL / "docs" / "OPEN_ITEMS.md"


def gemessene() -> list[str]:
    return sorted(p.name[: -len(".messung.json")]
                  for p in WORK.glob("*.messung.json"))


def gutachten() -> dict[str, str]:
    """Name -> Volltext."""
    return {p.name[: -len(".gutachten.md")]:
            p.read_text(encoding="utf-8", errors="replace")
            for p in OUT.glob("*.gutachten.md")}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--offen", action="store_true",
                    help="nur zeigen, was noch aussteht")
    a = ap.parse_args()

    if not WORK.is_dir():
        print("FEHLER: tools/uft-scout/work/ fehlt — nichts zu messen.")
        return 2

    mess = gemessene()
    gut = gutachten()
    texte = "\n".join(gut.values())

    # Ein Repo gilt als begutachtet, wenn sein Name in IRGENDEINEM
    # Gutachten vorkommt — mehrere Gutachten buendeln Repos (etwa
    # `cbm_erzeuger` fuer fuenf), darum trifft ein Namensvergleich
    # Datei-zu-Datei daneben.
    ohne_gutachten = [n for n in mess if n not in texte]

    liste = LISTE.read_text(encoding="utf-8", errors="replace") \
        if LISTE.is_file() else ""
    ohne_entscheidung = sorted(
        n for n in gut
        if n not in liste and not re.search(re.escape(n), liste, re.I))

    neg = 0
    if NEGATIVE.is_file():
        try:
            neg = len(json.loads(NEGATIVE.read_text(encoding="utf-8"))
                      .get("eintraege", {}))
        except (ValueError, OSError):
            neg = -1

    auftraege = None
    if AUFTRAEGE.is_file():
        try:
            auftraege = json.loads(AUFTRAEGE.read_text(encoding="utf-8"))
        except (ValueError, OSError):
            auftraege = None

    if not a.offen:
        print("Scout-Stand")
        print("=" * 60)
        print(f"  geklont und gemessen   : {len(mess)}")
        print(f"  Gutachten geliefert    : {len(gut)}")
        print(f"  davon ohne Gutachten   : {len(ohne_gutachten)}")
        print(f"  frueher schon bewertet : "
              f"{neg if neg >= 0 else 'known_negatives.json unlesbar'}")

    if auftraege is None:
        print("\n  KEIN NENNER: tools/uft-scout/data/auftraege.json fehlt.")
        print("  Damit ist \"alles abgearbeitet?\" NICHT beantwortbar — es")
        print("  laesst sich nur zaehlen, was angekommen ist, nicht was")
        print("  beauftragt war. Wer eine Liste uebergibt, traegt sie dort")
        print("  ein.")
    else:
        offen = [e for e in auftraege.get("eintraege", [])
                 if e.get("name") not in texte]
        print(f"\n  beauftragt             : "
              f"{len(auftraege.get('eintraege', []))}")
        print(f"  davon noch offen       : {len(offen)}")
        for e in offen:
            print(f"      {e.get('name')}  {e.get('url', '')}")

    if ohne_gutachten:
        print("\n  gemessen, aber ohne Gutachten:")
        for n in ohne_gutachten:
            print(f"      {n}")

    if ohne_entscheidung:
        print("\n  Gutachten ohne Spur in OPEN_ITEMS.md:")
        for n in ohne_entscheidung:
            print(f"      {n}")
        print("  (Hinweis: grober Namensvergleich. Ein Gutachten kann unter")
        print("   anderem Namen aufgenommen sein — `settings_triage` etwa")
        print("   als SET-1. Ein Treffer hier ist eine Frage, kein Urteil.)")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
