#!/usr/bin/env python3
"""suchraster.py — die Suchfragen aus dem BAUM ableiten (MF-718).

    python tools/uft-scout/scripts/suchraster.py            # Fragen zeigen
    python tools/uft-scout/scripts/suchraster.py --json     # maschinenlesbar
    python tools/uft-scout/scripts/suchraster.py --selbsttest

── Warum es diese Datei gibt ────────────────────────────────────────────

`config.json` traegt einen Schluessel `github_suchen` und darueber den
Kommentar „Suchraster gespeist aus UFTs T3-Liste und offenen Plaenen".
Gespeist wurde nichts: die Liste ist **von Hand gepflegt**. Sie kannte
beim Nachmessen (MF-718) Formate, die laengst gehoben sind, und keine der
seither offenen Fragen.

Das ist in diesem Baum das **dreizehnte** belegte Vorkommen derselben
Fehlerklasse — eine Aufzaehlung bekannter Faelle, die still veraltet
(MF-567/578/598/633/651/652/668/671/678/703/708/710). Sie ist hier
besonders teuer, weil sie nicht nur eine Zahl verfaelscht, sondern
**bestimmt, wonach ueberhaupt gesucht wird**. Was nicht im Raster steht,
wird nie gefunden — und niemand merkt es, weil ein Suchlauf immer
irgendetwas zurueckgibt.

── Woraus die Fragen jetzt kommen ───────────────────────────────────────

Jede Frage haengt an einer der vier Release-Kennzahlen (CLAUDE.md §Regel
9) und nennt sie mit. Was keine Kennzahl bewegt, wird nicht gesucht.

    Quelle                              Frage-Art          Kennzahl
    docs/VERIFICATION_TIERS.md (T3)     Format-Referenz    T3 runter
    docs/ORACLES.md (vorgemerkt)        Oracle-Kandidat    T3 runter
    src/core/uft_roundtrip.c            Wandler-Referenz   Pfade rauf
    docs/OPEN_ITEMS.md (status: offen)  gezielte Frage     je Eintrag

Driftet der Baum, driftet das Raster mit — ohne dass jemand eine Liste
pflegt. Das ist der ganze Zweck.

── Was diese Datei NICHT tut ────────────────────────────────────────────

Sie sucht nicht. Sie erzeugt nur die Fragen; das Suchen bleibt
`scout.py`, das Bewerten Stufe 2/3. Und sie erfindet keine Formatnamen:
was sie nennt, steht so im Baum.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import sys

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.normpath(os.path.join(HIER, "..", "..", ".."))


def _lies(rel: str) -> str:
    p = os.path.join(WURZEL, rel)
    try:
        with open(p, encoding="utf-8", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


# ── Quelle 1: ungeprüfte Formate ────────────────────────────────────────

def t3_formate(text: str | None = None) -> list[str]:
    """Formatnamen, die in der Tier-Tabelle auf T3 stehen."""
    t = text if text is not None else _lies("docs/VERIFICATION_TIERS.md")
    aus = []
    for zeile in t.splitlines():
        m = re.match(r"\|\s*`([^`]+)`\s*\|\s*\*\*T3\*\*\s*\|", zeile)
        if m:
            aus.append(m.group(1))
    return aus


# ── Quelle 2: Oracles, die vorgemerkt aber nicht registriert sind ───────

def offene_oracles(text: str | None = None) -> list[str]:
    """Werkzeuge aus dem Abschnitt „Vorgemerkt, noch nicht registriert"."""
    t = text if text is not None else _lies("docs/ORACLES.md")
    i = t.find("## Vorgemerkt, noch nicht registriert")
    if i < 0:
        return []
    rest = t[i:]
    j = rest.find("\n## ", 4)
    if j > 0:
        rest = rest[:j]
    aus = []
    for zeile in rest.splitlines():
        if not zeile.startswith("|"):
            continue
        m = re.match(r"\|\s*\**`?([A-Za-z0-9_.+-]+)`?", zeile)
        if m and m.group(1) not in ("Werkzeug", "---"):
            aus.append(m.group(1))
    return aus


# ── Quelle 3: Wandlungspaare ohne Eintrag ───────────────────────────────

def wandler_luecken(text: str | None = None) -> list[str]:
    """Formate, die in der Rundlauf-Matrix gar nicht vorkommen."""
    t = text if text is not None else _lies("src/core/uft_roundtrip.c")
    genannt = {m.lower() for m in re.findall(r"UFT_FORMAT_([A-Z0-9_]+)", t)}
    return sorted(genannt)


# ── Quelle 4: offene Punkte ─────────────────────────────────────────────

def offene_punkte(text: str | None = None) -> list[tuple[str, str]]:
    """(Kennung, Überschrift) je Abschnitt mit `status: offen`."""
    t = text if text is not None else _lies("docs/OPEN_ITEMS.md")
    aus = []
    zeilen = t.splitlines()
    for n, z in enumerate(zeilen):
        if not z.startswith("## "):
            continue
        # Die Statusmarke steht in den naechsten drei Zeilen.
        marke = " ".join(zeilen[n + 1:n + 4])
        if "status: offen" not in marke:
            continue
        titel = z[3:].strip()
        m = re.match(r"([A-Z]+-[0-9]+)", titel)
        aus.append((m.group(1) if m else titel[:12], titel))
    return aus


# ── Die Fragen ──────────────────────────────────────────────────────────

def raster() -> list[dict]:
    fragen: list[dict] = []

    for f in t3_formate():
        fragen.append({
            "frage": f"{f} disk image format",
            "grund": f"`{f}` steht auf T3 — eine fremde Referenz-"
                     f"Implementierung oder Spec hebt es",
            "kennzahl": "T3 runter",
            "quelle": "docs/VERIFICATION_TIERS.md",
        })

    for w in offene_oracles():
        fragen.append({
            "frage": f"{w} command line",
            "grund": f"`{w}` ist als Oracle vorgemerkt, aber nicht "
                     f"registriert — es fehlt der Bau- oder Laufbeweis",
            "kennzahl": "T3 runter",
            "quelle": "docs/ORACLES.md",
        })

    for k, titel in offene_punkte():
        fragen.append({
            "frage": titel,
            "grund": f"offener Punkt {k}",
            "kennzahl": "je Eintrag",
            "quelle": "docs/OPEN_ITEMS.md",
        })

    return fragen


# ── Selbsttest: vor dem Nenner, nicht danach ────────────────────────────

def selbsttest() -> int:
    """Jede Ableitung an einer gepflanzten Vorlage, deren Antwort
    feststeht. Ein Zaehler, der sich selbst bestaetigt, ist keiner
    (MF-693, MF-710)."""
    fehler = []

    t = ("| `alpha` | **T3** | x | — |\n"
         "| `beta` | **T2** | x | — |\n"
         "| `gamma` | **T3** | x | — |\n")
    got = t3_formate(t)
    if got != ["alpha", "gamma"]:
        fehler.append(f"t3_formate: {got!r} statt ['alpha', 'gamma']")

    o = ("## Vorgemerkt, noch nicht registriert\n"
         "| Werkzeug | Stand | offen |\n"
         "|---|---|---|\n"
         "| `nibconv`, `nibscan` | gebaut | Eintrag |\n"
         "| **fdc_bitstream** | extern | bauen |\n"
         "\n## Was ausdruecklich kein Oracle ist\n"
         "| `nichtgesucht` | Grund |\n")
    got = offene_oracles(o)
    if got != ["nibconv", "fdc_bitstream"]:
        fehler.append(f"offene_oracles: {got!r}")

    p = ("## AAA-1 — erster Punkt (MF-1)\n"
         "<!-- status: offen -->\n"
         "Text\n"
         "## BBB-2 — zweiter (MF-2)\n"
         "<!-- status: erledigt(MF-2) -->\n"
         "Text\n"
         "## CCC-3 — dritter (MF-3)\n"
         "<!-- status: wartet-eigentuemer(2026-01-01) -->\n")
    got = [k for k, _ in offene_punkte(p)]
    if got != ["AAA-1"]:
        fehler.append(f"offene_punkte: {got!r} statt ['AAA-1']")

    # Und die Gegenprobe: leere Eingaben duerfen nichts erfinden.
    for name, fn in (("t3_formate", t3_formate),
                     ("offene_oracles", offene_oracles),
                     ("offene_punkte", offene_punkte)):
        if fn(""):
            fehler.append(f"{name}: erfindet Eintraege aus leerem Text")

    print(f"Selbsttest: {4 - len(fehler)}/4")
    for f in fehler:
        print("  FAIL " + f)
    return 1 if fehler else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--selbsttest", action="store_true")
    a = ap.parse_args()

    if a.selbsttest:
        return selbsttest()

    fragen = raster()
    if a.json:
        json.dump({"_erzeugt_von": "tools/uft-scout/scripts/suchraster.py",
                   "_kommentar": "Abgeleitet, nicht gepflegt (MF-718).",
                   "fragen": fragen}, sys.stdout,
                  ensure_ascii=False, indent=1)
        print()
        return 0

    if not fragen:
        print("Keine Fragen ableitbar — stehen die Quelldateien am Platz?")
        return 1
    nach: dict[str, list[dict]] = {}
    for f in fragen:
        nach.setdefault(f["quelle"], []).append(f)
    print(f"{len(fragen)} Suchfragen, abgeleitet aus {len(nach)} Quellen\n")
    for q, fs in nach.items():
        print(f"  {q}  ({len(fs)})")
        for f in fs[:6]:
            print(f"     {f['frage'][:66]}")
        if len(fs) > 6:
            print(f"     … und {len(fs) - 6} weitere")
        print()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
