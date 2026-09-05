#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 58 — zwei Zeilen einer Geometrietabelle, dieselbe Gesamtgroesse (MF-906).

WAS DAS TOR SIEHT
-----------------
Viele Format-Plugins fuehren eine Tabelle der Gestalt

    static const struct { int tracks, sectors, heads, sector_size;
                          size_t total_size; const char *name; }
    g_xyz_geom[] = {
        { 40, 10, 2, 512, 409600, "H8/H89 DS/DD 400KB" },
        { 80, 10, 1, 512, 409600, "H89 SS/DD 80T 400KB" },   <-- nie erreichbar
        ...
    };

und eine Sonde, die mit `if (size == geom[i].total_size)` durchlaeuft und
beim ERSTEN Treffer zurueckkehrt. Steht dieselbe Gesamtgroesse zweimal,
ist die zweite Zeile toter Code: das Abbild wird still als die erste
Geometrie ausgegeben, obwohl die Groesse die beiden nicht unterscheidet.

Das ist dieselbe Form wie die Guard-Kollision aus MF-881: eine Auswahl,
die stillschweigend entscheidet, wo sie nicht entscheiden kann.

WAS DAS TOR NICHT SIEHT
-----------------------
* Kollisionen ZWISCHEN Dateien. Vier Plugins fuehren `256256` — aber das
  ist der IBM-3740-Standard fuer 8-Zoll SSSD, den `mfm_detect.c` selbst
  so benennt; vier Maschinen haben ihn wirklich benutzt. Zwischen
  Plugins entscheidet die Konfidenz-Rangfolge (MF-729), nicht diese
  Tabelle. Wer das pruefen will, braucht ein anderes Tor.
* Tabellen anderer Gestalt (weniger oder mehr Zahlenfelder, Groesse
  gerechnet statt geschrieben).
* Ob die Sonde ueberhaupt beim ersten Treffer zurueckkehrt — das Tor
  nimmt es an, weil alle 14 gefundenen Tabellen so benutzt werden.
  Eine Sonde, die alle Treffer sammelt, waere ein Fehlalarm; bisher
  gibt es keine.

GRUNDLINIE
----------
Der erste Lauf fand DREI Kollisionen, alle in Dateien, die
`docs/orphan_baseline.txt` als verwaist fuehrt — kein Benutzer trifft
sie heute. Sie sind BENANNT eingefroren, nicht stillschweigend: wird
eine dieser Dateien verdrahtet, muss die Kollision vorher aufgeloest
werden.
"""
from __future__ import annotations

import io
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

# Eine Tabellenzeile: { z1, z2, z3, z4, GROESSE, "Name" }
ZEILE = re.compile(
    r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*"
    r"\"((?:[^\"\\]|\\.)*)\"")
TABELLE = re.compile(r"\}\s*(\w+)\s*\[\s*\]\s*=\s*\{")

# Benannt eingefroren (MF-906). Jede Zeile: Datei, Tabelle, Groesse.
# Alle drei liegen in verwaisten Dateien (docs/orphan_baseline.txt).
GRUNDLINIE = {
    ("src/formats/industrial/uft_heathkit.c", "g_heathkit_geom", 409600),
    ("src/formats/japanese_ext/uft_hitachi_s1.c", "g_hitachi_geom", 655360),
    ("src/formats/soviet/uft_bk0010.c", "g_bk_geom", 409600),
}


def quellen(wurzel: Path) -> list[str]:
    """Die Dateien, die CI sehen wuerde — aus git, nicht aus einer Liste."""
    try:
        from repo_scope import repo_files
    except ImportError:
        return []
    alle = repo_files(wurzel)
    if alle is None:
        return []
    out = []
    for p in alle:
        try:
            rel = p.relative_to(wurzel).as_posix()
        except ValueError:
            continue
        if rel.startswith("src/") and rel.endswith(".c"):
            out.append(rel)
    return sorted(out)


def kollisionen_in(text: str):
    """@return Liste (tabellenname, groesse, [namen])."""
    treffer = []
    for m in TABELLE.finditer(text):
        name = m.group(1)
        rest = text[m.end():]
        ende = rest.find("};")
        if ende < 0:
            continue
        zeilen = ZEILE.findall(rest[:ende])
        if len(zeilen) < 2:
            continue
        gesehen: dict[int, list[str]] = {}
        for z in zeilen:
            groesse = int(z[4])
            if groesse == 0:          # Abschlusszeile
                continue
            gesehen.setdefault(groesse, []).append(z[5])
        for groesse, namen in sorted(gesehen.items()):
            if len(namen) > 1:
                treffer.append((name, groesse, namen))
    return treffer


def selbsttest() -> bool:
    """Vor dem Nenner: findet das Tor, was es finden soll — und nur das?

    Regel aus `tools/uft-innendienst`: ein Tor, dessen Selbsttest nicht
    laeuft, ist eine Behauptung. Eine Erstfassung dort meldete
    "Selbsttest 3/3" und lieferte gemessen 0/3.
    """
    faelle = [
        # (Text, erwartete Zahl Kollisionen, Beschreibung)
        ('} g_a[] = {\n'
         '{ 40, 10, 2, 512, 409600, "A" },\n'
         '{ 80, 10, 1, 512, 409600, "B" },\n'
         '{ 0,0,0,0,0, NULL }\n};', 1, "zwei Zeilen, dieselbe Groesse"),
        ('} g_b[] = {\n'
         '{ 40, 10, 1, 256, 102400, "A" },\n'
         '{ 40, 10, 1, 512, 204800, "B" },\n'
         '{ 0,0,0,0,0, NULL }\n};', 0, "verschiedene Groessen"),
        ('} g_c[] = {\n'
         '{ 1, 1, 1, 1, 0, "A" },\n'
         '{ 2, 2, 2, 2, 0, "B" },\n'
         '{ 0,0,0,0,0, NULL }\n};', 0, "Nullgroessen zaehlen nicht"),
        ('} g_d[] = {\n'
         '{ 40, 10, 2, 512, 409600, "A" },\n'
         '};', 0, "eine Zeile allein"),
        ('} g_e[] = {\n'
         '{ 1, 1, 1, 1, 700, "A" },\n'
         '{ 2, 2, 2, 2, 700, "B" },\n'
         '{ 3, 3, 3, 3, 700, "C" },\n'
         '{ 0,0,0,0,0, NULL }\n};', 1, "drei Zeilen, EINE Kollision"),
    ]
    ok = 0
    for text, erwartet, was in faelle:
        ist = len(kollisionen_in(text))
        if ist == erwartet:
            ok += 1
        else:
            print("  SELBSTTEST FEHLER (%s): erwartet %d, gemessen %d"
                  % (was, erwartet, ist))
    print("  Selbsttest %d/%d" % (ok, len(faelle)))
    return ok == len(faelle)


def main() -> int:
    if "--selbsttest" in sys.argv:
        return 0 if selbsttest() else 1

    if not selbsttest():
        print("FAIL: Selbsttest rot — das Tor misst nicht, was es soll.")
        return 1

    dateien = quellen(WURZEL)
    if not dateien:
        print("  git nicht befragbar — Tor laesst durch und sagt es "
              "(Grundsatz MF-636).")
        return 0

    gefunden = set()
    ausgabe = []
    tabellen = 0
    for rel in dateien:
        try:
            text = io.open(WURZEL / rel, encoding="utf-8",
                           errors="replace").read()
        except OSError:
            continue
        for m in TABELLE.finditer(text):
            rest = text[m.end():]
            if rest.find("};") >= 0 and len(ZEILE.findall(
                    rest[:rest.find("};")])) >= 2:
                tabellen += 1
        for name, groesse, namen in kollisionen_in(text):
            gefunden.add((rel, name, groesse))
            ausgabe.append("  %s  %s\n      %d Byte -> %s"
                           % (rel, name, groesse, " | ".join(namen)))

    neu = gefunden - GRUNDLINIE
    erledigt = GRUNDLINIE - gefunden

    print("  Geometrietabellen dieser Gestalt : %d" % tabellen)
    print("  Kollisionen                      : %d" % len(gefunden))
    print("  Grundlinie                       : %d" % len(GRUNDLINIE))
    print("  NEU                              : %d" % len(neu))
    for z in ausgabe:
        print(z)

    if erledigt:
        print("\n  Grundlinien-Eintraege erledigt (bitte aus GRUNDLINIE "
              "streichen):")
        for e in sorted(erledigt):
            print("    %s %s %d" % e)

    if neu:
        print("\nFAIL: neue Geometrie-Kollision(en):")
        for n in sorted(neu):
            print("    %s  %s  %d Byte" % n)
        print("\n  Zwei Zeilen derselben Tabelle mit gleicher Gesamtgroesse:")
        print("  die zweite ist toter Code, und die Sonde entscheidet still,")
        print("  wo die Groesse nicht entscheidet. Entweder die Zeilen an")
        print("  einem weiteren Merkmal trennen, oder die Doppeldeutigkeit")
        print("  melden statt die erste Zeile zu nehmen.")
        return 1

    print("  OK")
    return 0


def check(repo) -> list:
    """Anbindung an `check_consistency.py`.

    @return Liste der NEUEN Kollisionen (Grundlinie abgezogen). Leere
            Liste heisst: nichts Neues, die drei benannten Altfaelle
            liegen weiter in verwaisten Dateien.
    """
    global WURZEL
    alt, WURZEL = WURZEL, Path(repo)
    try:
        if not selbsttest_still():
            return ["audit_geometrie_kollision: Selbsttest rot"]
        gefunden = set()
        for rel in quellen(Path(repo)):
            try:
                text = io.open(Path(repo) / rel, encoding="utf-8",
                               errors="replace").read()
            except OSError:
                continue
            for name, groesse, namen in kollisionen_in(text):
                gefunden.add((rel, name, groesse))
        return ["%s: %s fuehrt %d Byte zweimal - die zweite Zeile ist "
                "toter Code" % (rel, name, groesse)
                for rel, name, groesse in sorted(gefunden - GRUNDLINIE)]
    finally:
        WURZEL = alt


def selbsttest_still() -> bool:
    """Wie `selbsttest()`, aber ohne Ausgabe (fuer die Anbindung)."""
    import contextlib
    with contextlib.redirect_stdout(io.StringIO()):
        return selbsttest()


if __name__ == "__main__":
    sys.exit(main())
