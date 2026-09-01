#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Der Quarantaene-Stand — abgeleitet aus den Tabellen, nicht gezaehlt
(MF-742)

`docs/QUARANTINE.md` trug bis heute eine **von Hand geschriebene**
Prosazeile:

    > **Stand 2026-08-31: 1 vollzogen, 5 vorgemerkt, 0 aufgeloest.**

`scripts/gen_stand.py:139` las genau diese Zeile per Regex und machte
daraus eine der **Release-Kennzahlen** (Herkunft, Befund-Stufe). Eine
gepflegte Zahl, die eine Kennzahl speist — das ist das Muster, das
dieser Baum inzwischen **fuenfzehnmal** bezahlt hat
(MF-567/578/598/633/651/652/668/671/678/703/708/710/718/735/738).

**Und sie war falsch.** Gemessen an den Tabellen:

    vollzogen     1   stimmt
    vorgemerkt    5   stimmt
    aufgeloest    0   FALSCH — es sind **2**

Der Abschnitt „Rehabilitiert (Weg 1)" fuehrt `uft_gcr_ops.c` und
`uft_d64_g64.c`. Beide sind aufgeloest, beide standen als 0 da. Die
Zahl unterberichtete **erledigte Arbeit** — die Richtung, in der ein
Fehler am laengsten unbemerkt bleibt, weil niemand nachfragt, warum
etwas noch offen ist.

── Woran die Zaehlung haengt ────────────────────────────────────────────

An der Ueberschrift des Abschnitts, nicht an einer Dateiliste:

    ## Vollzogen                    -> `### `pfad``-Ueberschriften
    ## Vorgemerkt …                 -> `| `pfad` |`-Tabellenzeilen
    ## Rehabilitiert (Weg 1) …      -> `| `pfad` |`-Tabellenzeilen

Abschnitte, die ausdruecklich NICHT zaehlen, weil sie erklaeren statt
zu fuehren: „Was **nicht** auf dieser Liste steht, und warum" und
„Offene Vorbedingung".

Aufruf:
    python scripts/quarantine_stand.py              # Bericht
    python scripts/quarantine_stand.py --selbsttest
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
ZIEL = "docs/QUARANTINE.md"

# Abschnitt -> Rubrik. Wer eine Rubrik ergaenzt, traegt sie hier ein;
# ein UNBEKANNTER Abschnitt mit Eintraegen wird gemeldet, nicht
# stillschweigend verworfen — sonst waere die Ableitung wieder eine
# Aufzaehlung.
RUBRIK = (
    (re.compile(r"^Vollzogen\b", re.I), "vollzogen"),
    (re.compile(r"^Vorgemerkt\b", re.I), "vorgemerkt"),
    (re.compile(r"^Rehabilitiert\b", re.I), "aufgeloest"),
)

# Abschnitte, die erklaeren statt zu fuehren.
ERKLAEREND = (
    re.compile(r"^Was \*\*nicht\*\*", re.I),
    re.compile(r"^Offene Vorbedingung", re.I),
)

EINTRAG_H = re.compile(r"^### `?(?:src|include|tools)/")
EINTRAG_Z = re.compile(r"^\| `(?:src|include|tools)/")
STAND = re.compile(
    r"Stand ([0-9-]+): (\d+) vollzogen, (\d+) vorgemerkt, "
    r"(\d+) aufgel(?:ö|oe)st")


def zaehle(text: str) -> tuple[dict[str, int], list[str]]:
    """({rubrik: anzahl}, warnungen) aus dem Dokumenttext."""
    stand = {"vollzogen": 0, "vorgemerkt": 0, "aufgeloest": 0}
    warnungen: list[str] = []
    abschnitt = None
    rubrik = None
    for zeile in text.split("\n"):
        m = re.match(r"^## (.+?)\s*$", zeile)
        if m:
            abschnitt = m.group(1)
            rubrik = None
            if any(e.match(abschnitt) for e in ERKLAEREND):
                continue
            for muster, r in RUBRIK:
                if muster.match(abschnitt):
                    rubrik = r
                    break
            continue
        if abschnitt is None:
            continue
        if not (EINTRAG_H.match(zeile) or EINTRAG_Z.match(zeile)):
            continue
        if rubrik:
            stand[rubrik] += 1
        elif not any(e.match(abschnitt) for e in ERKLAEREND):
            warnungen.append(
                "Abschnitt „%s“ fuehrt Eintraege, gehoert aber zu "
                "keiner Rubrik — entweder in RUBRIK eintragen oder als "
                "erklaerend kennzeichnen." % abschnitt[:60])
    return stand, sorted(set(warnungen))


def check(repo) -> list[str]:
    """Schnittstelle fuer check_consistency.py.

    Die Prosazeile darf stehen bleiben — sie ist fuer Menschen da. Sie
    darf nur nicht mehr von der Messung abweichen.
    """
    p = Path(repo) / ZIEL
    if not p.is_file():
        return []
    text = p.read_text(encoding="utf-8", errors="replace")
    stand, warnungen = zaehle(text)
    fehler = list(warnungen)
    m = STAND.search(text)
    if not m:
        return fehler + [
            "docs/QUARANTINE.md: die Standzeile fehlt oder hat einen "
            "anderen Wortlaut. Gemessen: %d vollzogen, %d vorgemerkt, "
            "%d aufgeloest." % (stand["vollzogen"], stand["vorgemerkt"],
                                stand["aufgeloest"])]
    behauptet = (int(m.group(2)), int(m.group(3)), int(m.group(4)))
    gemessen = (stand["vollzogen"], stand["vorgemerkt"],
                stand["aufgeloest"])
    if behauptet != gemessen:
        fehler.append(
            "docs/QUARANTINE.md: die Standzeile behauptet %s, gemessen "
            "an den Tabellen ist %s (vollzogen, vorgemerkt, aufgeloest). "
            "Diese Zahl speist ueber gen_stand.py eine Release-Kennzahl "
            "— sie war bei ihrer ersten Messung falsch (MF-742)."
            % (behauptet, gemessen))
    return fehler


# ── Selbsttest ──────────────────────────────────────────────────────────

FAELLE = [
    ("gemischtes Dokument",
     "> **Stand 2026-01-01: 1 vollzogen, 2 vorgemerkt, 1 aufgelöst.**\n"
     "## Vollzogen\n"
     "### `src/a.c` (+ Header)\n"
     "## Vorgemerkt — wartet\n"
     "| Datei | Zeilen |\n|---|---|\n"
     "| `src/b.c` | 10 |\n| `include/c.h` | 20 |\n"
     "## Rehabilitiert (Weg 1)\n"
     "| `src/d.c` | egal |\n",
     {"vollzogen": 1, "vorgemerkt": 2, "aufgeloest": 1}, 0),
    ("erklaerender Abschnitt zaehlt NICHT",
     "## Was **nicht** auf dieser Liste steht, und warum\n"
     "| `src/x.c` | Waise |\n| `src/y.c` | entfernt |\n",
     {"vollzogen": 0, "vorgemerkt": 0, "aufgeloest": 0}, 0),
    ("unbekannter Abschnitt mit Eintraegen -> Warnung",
     "## Irgendwas Neues\n| `src/z.c` | 1 |\n",
     {"vollzogen": 0, "vorgemerkt": 0, "aufgeloest": 0}, 1),
    ("Prosa und Tabellenzeilen ohne Pfad zaehlen nicht",
     "## Vorgemerkt — wartet\n"
     "| Datei | Zeilen | Weg |\n|---|---|---|\n"
     "Ein Satz ueber `src/nicht_gezaehlt.c` mitten im Text.\n",
     {"vollzogen": 0, "vorgemerkt": 0, "aufgeloest": 0}, 0),
]


def selbsttest() -> int:
    gut = 0
    for name, text, erwartet, warn_n in FAELLE:
        stand, warnungen = zaehle(text)
        ok = (stand == erwartet and len(warnungen) == warn_n)
        gut += ok
        print("  %s %-46s %s" % ("ok " if ok else "ROT", name, stand))
        if not ok:
            print("      erwartet %s, %d Warnung(en)" % (erwartet, warn_n))

    # Der Fall, der das Werkzeug ausgeloest hat: die echte Standzeile
    # sagte „0 aufgeloest", die Tabelle fuehrte zwei.
    text = ("> **Stand 2026-01-01: 1 vollzogen, 0 vorgemerkt, "
            "0 aufgelöst.**\n"
            "## Vollzogen\n### `src/a.c`\n"
            "## Rehabilitiert (Weg 1)\n| `src/d.c` | x |\n"
            "| `src/e.c` | y |\n")
    p = Path(__import__("tempfile").mkdtemp()) / ZIEL
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(text, encoding="utf-8")
    f = check(p.parents[1])
    ok = any("behauptet (1, 0, 0)" in x for x in f)
    gut += ok
    print("  %s %-46s" % ("ok " if ok else "ROT",
                          "Standzeile weicht ab -> Befund"))
    print("Selbsttest: %d/%d" % (gut, len(FAELLE) + 1))
    return 0 if gut == len(FAELLE) + 1 else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selbsttest", action="store_true")
    a = ap.parse_args()
    if a.selbsttest:
        return selbsttest()
    stand, warnungen = zaehle(
        (WURZEL / ZIEL).read_text(encoding="utf-8", errors="replace"))
    print("Quarantaene-Stand, aus den Tabellen abgeleitet:")
    for k, v in stand.items():
        print("  %-12s %d" % (k, v))
    for w in warnungen:
        print("  ! " + w)
    fehler = check(WURZEL)
    print("Abweichungen: %d" % len(fehler))
    for f in fehler:
        print("  " + f)
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
