#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Keine Faehigkeitszusage ohne den Vorbehalt, den die Messung verlangt
(MF-985).

    python scripts/audit_faehigkeitszusage.py [--liste]
    python scripts/audit_faehigkeitszusage.py --selbsttest

── Warum es dieses Tor gibt ─────────────────────────────────────────────

`docs/CAPABILITIES.md` trug bis MF-983 den Satz

    55+ historische Schutz-Schemes erkannt (V-MAX!, RapidLok, …)
    **Erkennung != Bypass.**

Praesens, ohne Einschraenkung — waehrend `CLAUDE.md` und `README.md` seit
MF-508/509 das Gegenteil sagen und `docs/BACKLOG.md` C1 die Zahlen unter
einem Tor haelt: von 363 Funktionen sind **9** von aussen gerufen.

Dieselbe Datei behauptete „Alle 8 DeepRead-Module sind … in der GUI
zugaenglich"; gemessen ist **1 von 8** (MF-767).

Warum kein vorhandenes Tor das sah: `audit_protection_claims.py` prueft
die **Zahl im C1-Eintrag** gegen den Baum. Es kann nicht bemerken, dass
ein viertes Dokument die Behauptung nackt weitertraegt. Das Tor hielt
eine Buchhaltung aktuell, nicht eine Zusage.

── Was dieses Tor prueft ────────────────────────────────────────────────

Fuer eine kleine Zahl von Gegenstaenden, deren Erreichbarkeit im Baum
**gemessen** ist, gilt: wo sie in einem getrackten `.md` als Faehigkeit
genannt werden, muss in der Naehe ein Vorbehalt stehen.

Der Vorbehalt ist keine Formalie. Er ist der Unterschied zwischen
„das Werkzeug kann das" und „der Code dafuer liegt im Baum" — und genau
diesen Unterschied hat dieses Projekt sich zur Aufgabe gemacht
(`CLAUDE.md`, Konfliktordnung: Ehrlichkeit vor Vollstaendigkeit).

── Was dieses Tor NICHT sehen kann ──────────────────────────────────────

Ehrlich benannt, weil eine Messung, die ihre Grenze verschweigt, als
Entwarnung missverstanden wird:

  * **Es kennt nur die Gegenstaende in `GEGENSTAENDE`.** Eine
    Faehigkeitszusage in Worten, die dort nicht stehen, ist unsichtbar.
    Das ist eine Aufzaehlung, und Aufzaehlungen veralten in diesem Baum
    (viermal belegt, siehe `scripts/repo_scope.py`). Der Unterschied zu
    den vier Faellen dort: hier steht neben jedem Gegenstand die
    **Messung**, die ihn begruendet — wer einen Gegenstand ergaenzt,
    ergaenzt eine Zahl, keine Meinung.
  * **Naehe ist keine Bedeutung.** Steht ein Vorbehalt im Umfeld, der
    etwas anderes einschraenkt, zaehlt das Tor ihn mit. Es findet
    fehlende Vorbehalte, nicht falsche.
  * Historische Dokumente sind ausgenommen (siehe `HISTORISCH`) — ein
    Changelog beschreibt, was eine Release **damals** angekuendigt hat.
    Ihn nachtraeglich zu berichtigen waere Geschichtsfaelschung.
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

from repo_scope import make_filter  # noqa: E402

# Gegenstand -> (Muster der Zusage, Messung die ihn begruendet)
#
# Ein Eintrag gehoert hierher, wenn (1) der Baum eine Zahl dazu MISST und
# (2) diese Zahl der Zusage widerspricht. Nicht: was jemand fuer
# uebertrieben haelt.
GEGENSTAENDE: list[tuple[str, str, str]] = [
    ("Kopierschutz-Katalog",
     r"55\+",
     "363 Funktionen in src/protection/, 9 von aussen gerufen "
     "(docs/BACKLOG.md C1, scripts/audit_protection_claims.py)"),
    ("DeepRead-Module",
     r"[Aa]lle 8 DeepRead|8 DeepRead-Module sind",
     "1 von 8 erreichbar; die 5 Forensik-Module haben 13 Funktionen "
     "mit null Aufrufern ausserhalb (MF-767, nachgemessen MF-983)"),
]

# Woerter, die einen Vorbehalt tragen. Bewusst weit gefasst: dieses Tor
# soll fehlende Vorbehalte finden, nicht Formulierungen vorschreiben.
VORBEHALT = re.compile(
    r"kein(?:en)? Aufrufer|nicht verdrahtet|unverdrahtet|Bestand, nicht|"
    r"no caller|not a feature|nie gegen|ungepr(?:ü|ue)ft|"
    r"P0-2\b|\bC1\b|Katalog|erreichbar|Heuristik|MF-508|MF-767|MF-983|"
    r"Ehrlichkeits|honest|caveat|Vorbehalt|berichtigt|"
    # MF-985: aus einem GEMESSENEN Fehlalarm ergaenzt, nicht aus
    # Geschmack. `docs/KNOWN_ISSUES.md:5803` sagt „Die Oberflaeche ruft
    # davon nichts auf" — ein Vorbehalt, nur in Worten, die dieses
    # Muster nicht kannte. Der Fall steht als Selbsttest fest.
    r"nichts auf|trug die beworbene|"
    # MF-985, zweiter gemessener Fehlalarm: `docs/KNOWN_ISSUES.md`
    # PROT-3 leitet mit „Gemessen:" die widerlegende Zahl ein und
    # traegt „UEBERWACHT" in der Ueberschrift. Beides ist in diesem
    # Baum ein Vorbehalt. **Eng gefasst** aufgenommen — mit Doppelpunkt
    # bzw. als Statuswort —, weil ein blosses „gemessen" ueberall steht
    # und dann auch eine Zusage freispraeche, die eine BESTAETIGENDE
    # Zahl nennt.
    r"Gemessen:|UEBERWACHT|\u00dcBERWACHT",
    re.I)

# Der Vorbehalt muss im ABSATZ der Fundstelle stehen, nicht irgendwo in
# der Naehe.
#
# MF-985, gemessen: der erste Entwurf suchte in +/- 600 Zeichen und war
# damit **nicht rotbeweisbar**. Aus `docs/CAPABILITIES.md` den Vorbehalt
# entfernt — das Tor meldete weiter 0, weil der NACHBARABSCHNITT (die
# DeepRead-Notiz) die Woerter „erreichbar", „Katalog", „MF-767" traegt.
# Ein Vorbehalt, der zu einer anderen Aussage gehoert, ist keiner zu
# dieser. Ein Tor, das nicht feuern kann, ist schlimmer als keins.
ABSATZ = "\n\n"

# Kategorisch historisch — kein Ist-Zustand, darum ausgenommen.
HISTORISCH = re.compile(r"(^|/)(CHANGELOG|RELEASE_NOTES)\.md$", re.I)


def _pruefe_text(pfad: str, text: str) -> list[tuple[str, int, str]]:
    """[(gegenstand, zeile, messung)] fuer Zusagen ohne Vorbehalt."""
    if HISTORISCH.search(pfad):
        return []
    treffer = []
    for name, muster, messung in GEGENSTAENDE:
        for m in re.finditer(muster, text):
            # Absatzgrenzen um die Fundstelle. Eine Tabellenzeile zaehlt
            # als eigener Absatz, sonst deckt eine Zeile die Nachbarzeile.
            links = text.rfind(ABSATZ, 0, m.start())
            links = 0 if links < 0 else links + len(ABSATZ)
            zeilenanfang = text.rfind("\n", 0, m.start())
            if zeilenanfang >= 0 and text[zeilenanfang + 1:].startswith("|"):
                links = zeilenanfang + 1
            rechts = text.find(ABSATZ, m.end())
            rechts = len(text) if rechts < 0 else rechts
            if text[links:].startswith("|"):
                ende = text.find("\n", m.end())
                rechts = min(rechts, len(text) if ende < 0 else ende)
            if VORBEHALT.search(text[links:rechts]):
                continue
            treffer.append((name, text[:m.start()].count("\n") + 1, messung))
    return treffer


def messe(repo) -> list[tuple[str, str, int, str]]:
    """[(datei, gegenstand, zeile, messung)] ueber alle getrackten .md."""
    repo = Path(repo)
    behalten, warnung = make_filter(repo)
    aus = []
    if warnung:
        print("  ! " + warnung, file=sys.stderr)
    for p in sorted(repo.rglob("*.md")):
        rel = p.relative_to(repo).as_posix()
        if not behalten(p):
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        for name, zeile, messung in _pruefe_text(rel, text):
            aus.append((rel, name, zeile, messung))
    return aus


def check(repo) -> list[str]:
    """Schnittstelle fuer check_consistency.py. Grundlinie: 0."""
    return [
        "%s:%d nennt „%s“ als Faehigkeit, ohne Vorbehalt im Umfeld. "
        "Gemessen: %s. Eine Zusage ohne den Vorbehalt, den die Messung "
        "verlangt, ist der Unterschied zwischen „das Werkzeug kann das“ "
        "und „der Code liegt im Baum“." % (datei, zeile, name, messung)
        for datei, name, zeile, messung in messe(repo)
    ]


# ── Selbsttest ──────────────────────────────────────────────────────────

def selbsttest() -> int:
    gut = 0
    faelle = [
        ("nackte Zusage -> Treffer",
         "docs/X.md", "Das Werkzeug erkennt 55+ Schutzverfahren.", 1),
        ("Zusage mit Vorbehalt -> sauber",
         "docs/X.md",
         "Der Katalog der 55+ Verfahren hat keinen Aufrufer.", 0),
        ("Vorbehalt im selben Absatz -> sauber",
         "docs/X.md",
         "55+ Schemes." + ("x" * 300) + " Der Katalog ist nicht verdrahtet.",
         0),
        # Der Fall, an dem der erste Entwurf scheiterte: der Vorbehalt
        # steht im NACHBARABSCHNITT und gehoert zu etwas anderem.
        ("Vorbehalt im Nachbarabsatz -> Treffer",
         "docs/X.md",
         "55+ Schemes erkannt.\n\nGanz anderes Thema: das ist nicht "
         "verdrahtet.", 1),
        ("Vorbehalt in der Tabellenzeile selbst -> sauber",
         "docs/X.md",
         "| A | B |\n| 55+ Schemes | Katalog ohne Aufrufer |\n| C | D |", 0),
        ("Vorbehalt in der NACHBARzeile der Tabelle -> Treffer",
         "docs/X.md",
         "| A | B |\n| 55+ Schemes | ja |\n| C | nicht verdrahtet |", 1),
        ("CHANGELOG ist ausgenommen",
         "CHANGELOG.md", "Unified copy protection API (55+ schemes).", 0),
        ("RELEASE_NOTES ebenso",
         "RELEASE_NOTES.md", "55+ schemes shipped.", 0),
        ("DeepRead nackt -> Treffer",
         "docs/X.md", "Alle 8 DeepRead-Module stehen bereit.", 1),
        ("DeepRead mit Messung -> sauber",
         "docs/X.md",
         "Alle 8 DeepRead-Module: 1 von 8 erreichbar (MF-767).", 0),
        ("Text ohne Gegenstand -> sauber",
         "docs/X.md", "Dieses Dokument nennt keine Faehigkeit.", 0),
        # Der gemessene Fehlalarm aus dem ersten Lauf
        # (docs/KNOWN_ISSUES.md:5803) — festgehalten, damit die
        # Erweiterung des Vokabulars nicht spaeter still zurueckfaellt.
        ("Vorbehalt in anderen Worten -> sauber",
         "docs/X.md",
         "trug die beworbene Kernfunktion \u201e55+ Schemes\u201c. "
         "Die Oberflaeche ruft davon nichts auf.", 0),
        # Zweiter gemessener Fehlalarm (KNOWN_ISSUES PROT-3).
        ("\u201eGemessen:\u201c leitet die widerlegende Zahl ein -> sauber",
         "docs/X.md",
         "Die Doku fuehrt \u201e55+ Verfahren\u201c als Kernfunktion. Gemessen:", 0),
        # Die Gegenprobe dazu: eine BESTAETIGENDE Zahl darf nicht
        # freisprechen — darum steht der Doppelpunkt im Muster.
        ("\u201egemessen\u201c ohne Doppelpunkt spricht NICHT frei",
         "docs/X.md",
         "55+ Schemes erkannt, gemessen an 39 Dateien.", 1),
    ]
    for name, pfad, text, erwartet in faelle:
        ist = len(_pruefe_text(pfad, text))
        ok = ist == erwartet
        gut += ok
        print("  %s %-46s %d (erwartet %d)"
              % ("ok " if ok else "ROT", name, ist, erwartet))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--liste", action="store_true",
                    help="alle Fundstellen zeigen, auch die sauberen")
    ap.add_argument("--selbsttest", action="store_true")
    a = ap.parse_args()
    if a.selbsttest:
        return selbsttest()

    treffer = messe(WURZEL)
    print("Faehigkeitszusagen ohne Vorbehalt: %d" % len(treffer))
    for datei, name, zeile, _ in treffer:
        print("  %-34s Z.%-6d %s" % (datei, zeile, name))
    print("\nGeprueft werden %d Gegenstaende: %s"
          % (len(GEGENSTAENDE), ", ".join(g[0] for g in GEGENSTAENDE)))
    return 1 if treffer else 0


if __name__ == "__main__":
    sys.exit(main())
