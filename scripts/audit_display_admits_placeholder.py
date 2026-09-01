#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Eine Anzeige, die im Quelltext zugibt zu erfinden — und es dem
Benutzer nicht sagt (MF-569, wiederhergestellt MF-735)

── Warum diese Datei zweimal geschrieben wurde ──────────────────────────

Sie wurde in **MF-569** angelegt und in `check_consistency.py` als Tor 34
(„Anzeige gibt Platzhalter zu") verdrahtet. Sie war eine **wortgleiche
Kopie von `audit_unbounded_alloc.py`** — Vorlage kopiert, Rumpf nie
ersetzt. Gemessen am 2026-08-31: beide Werkzeuge lieferten auf dem echten
Baum dieselbe Liste, Element fuer Element.

Tor 34 hat also **nie geprueft, was sein Name sagt.** Es hat Tor 33 ein
zweites Mal ausgefuehrt. Gefunden hat das der Pruefstand
`scripts/audit_selbsttest.py` (MF-735) — beim Lesen der Erkennungsmuster
fuer einen gepflanzten Fall, nicht durch einen roten Lauf: ein
Doppelgaenger ist immer gruen, wenn sein Vorbild gruen ist.

Das ist die vierzehnte Auspraegung desselben Musters wie MF-567/578/598/
633/651/652/668/671/678/703/708/710/718 — nur eine Ebene hoeher: nicht
eine Aufzaehlung, die veraltet, sondern ein **Pruefer, der nicht prueft.**
Ein Tor ohne Selbsttest kann seinen eigenen Ausfall nicht melden.

── Was gesucht wird ─────────────────────────────────────────────────────

In `src/*.cpp` (der Oberflaeche): eine Funktion, deren **Quelltext
zugibt**, dass die angezeigten Werte nicht aus dem Abbild stammen
(„Placeholder", „for now", „in full implementation", „hardcoded"), in der
aber **keine der angezeigten Zeichenketten** einen Vorbehalt traegt.

Der Vorbehalt im Kommentar steht an der einen Stelle, wo ihn niemand
liest, der das Werkzeug benutzt. Fuer einen Forensiker ist eine erfundene
Verzeichnisliste von einer echten nicht zu unterscheiden.

── Rotbeweis, aus dem Baum ──────────────────────────────────────────────

Die zwei von MF-569 reparierten Dateien sind die Eichung. Gemessen gegen
`dcf800a5^` (vorher) und `dcf800a5` (nachher):

    src/statustab.cpp     vorher: FEUERT      nachher: still
    src/explorertab.cpp   vorher: FEUERT      nachher: still

Vorher: `// Placeholder: mark all as allocated for now`, danach jeder
Block gruen mit „F". Nachher: derselbe Kommentar steht noch da — jetzt
aber zeigt die Tabelle „?" und ueber ihr steht ein Satz. **Der
Unterschied liegt in der angezeigten Zeichenkette, nicht im Kommentar**,
und genau daran misst dieses Werkzeug.

Fest gehalten in `scripts/audit_selbsttest.py` unter diesem Namen.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[1]

# Ein Eingestaendnis — nur im Kommentar, nicht im Code selbst.
EINGESTAENDNIS = re.compile(
    r"(?://|/\*|\*)[^\n]*\b("
    r"placeholder"
    # „for now" stand hier und ist RAUS. Gemessen am echten Baum: es
    # trug **5 der 6** Fehlalarme, und keinen einzigen echten Fund.
    #
    #   samdisk/do.cpp:10        „For now, rely on the file size" —
    #                            eine Erkennungs-Strategie, keine Anzeige
    #   flux_histogram:436       „For now, assume first peak is base
    #                            timing" — eine Algorithmus-Annahme
    #   hardwaretab:1358         „For now we flag it loudly in the status
    #                            bar" — beschreibt den EHRLICHEN Rueckfall
    #   recovery_dialog:492      zitiert im Kommentar den ENTFERNTEN Code
    #                            von MF-115
    #
    # Beide Rotbeweis-Faelle ueberleben ohne die Vokabel: statustab sagt
    # „placeholder", explorertab „will be used when". Ein Wort, das jeden
    # zweiten Kommentar im Baum trifft, ist kein Eingestaendnis.
    r"|in full implementation"
    r"|hardcoded|hard-coded"
    r"|dummy data"
    r"|fake data"
    r"|simulated (?:data|values)"
    r"|will be (?:used|implemented) when"
    r"|when real \w+ (?:parsing |reading )?is implemented"
    r"|not yet (?:implemented|wired|read)"
    r")\b", re.I)

# Was der Benutzer zu sehen bekommt.
#
# JEDE Zeichenkette im Rumpf, nicht nur die an `setText`/`tr`. Die erste
# Fassung sah nur die Aufrufe — und war damit blind fuer genau die
# Haelfte des Vorbilds: `ExplorerTab::readDirectory()` baut seine
# dreizehn erfundenen Eintraege in eine `QList<FileEntry>` und gibt sie
# zurueck; angezeigt werden sie eine Funktion weiter. Gemessen an der
# Vorher-Fassung von `dcf800a5`: das Werkzeug meldete `statustab.cpp`
# und schwieg zu `explorertab.cpp` — der Haelfte, die MF-569 zuerst
# nennt.
#
# Mindestens EIN Zeichen, nicht zwei. Der Vorbehalt, den MF-569 gesetzt
# hat, ist woertlich `setText("?")` — eine Zeichenkette der Laenge 1. Mit
# `{2,}` sah das Werkzeug sie nicht und meldete die reparierte Fassung
# als Fund. Gefangen hat das der gepflanzte `sauber`-Fall, nicht der
# echte Baum: dort trug ein langer Hinweissatz daneben das Fragezeichen
# und deckte den Fehler zu.
ANZEIGE = re.compile(r'"((?:[^"\\]|\\.)+)"')

# Ein Vorbehalt IN der angezeigten Zeichenkette. Nur das entlastet.
VORBEHALT = re.compile(
    r"\?"
    r"|\bunknown\b|\bunbekannt\b"
    r"|not (?:read|available|parsed|implemented)"
    r"|nicht (?:gelesen|verfuegbar|geprueft)"
    r"|\bno data\b|\bkeine Daten\b"
    r"|\bplaceholder\b"
    r"|\bnot supported\b"
    # Der Bootsektor-Hexdump in `statustab.cpp` ist der Gegenbeweis, den
    # MF-569 selbst nennt („macht es richtig und beweist, dass es
    # geht"): er zeigt `..` statt Bytes und schreibt darueber
    # „(Boot sector data would be displayed here when loaded from disk
    # image)". Ohne diese zwei Wendungen meldet das Werkzeug die eine
    # Stelle im Baum, die es vorbildlich macht.
    r"|would be (?:displayed|shown|read)"
    r"|when loaded from", re.I)

# `setPlaceholderText` ist Qt-Vokabular fuer den Hinweis in einem leeren
# Eingabefeld — kein Eingestaendnis, sondern das Gegenteil: dort steht
# nichts, und das Feld sagt es. Ohne diese Ausnahme meldet das Werkzeug
# jedes Suchfeld im Baum.
UNSCHULDIG = re.compile(r"setPlaceholderText|placeholderText\s*[:=]", re.I)

# Ein Kommentar, der eine ENTFERNTE Erfindung zitiert, ist die Doku eines
# Fixes — nicht der Fix, der fehlt. Zwei im Baum, beide echt entlastet:
#
#   uft_recovery_dialog.cpp:492  „MF-115: This page used to display
#                                 int recovered = badBefore / 2;
#                                 // placeholder estimate"
#   hardwaretab.cpp:426          „MF-170: ... removed from the controller
#                                 combo ... surfaced a 'Backend not yet
#                                 wired' messagebox"
#
# Beide sind vorbildlich: sie halten fest, was da war und warum es weg
# ist. Ein Tor, das die Dokumentation eines Fixes als Fund meldet,
# bestraft genau das Verhalten, das es herbeifuehren will.
HISTORIE = re.compile(
    r"\bused to\b|\bno longer\b|\bwas removed\b|\bremoved from\b"
    r"|\bstand (?:hier|frueher)\b|\bentfernt\b|\bwar (?:hier|frueher)\b"
    r"|\bbis (?:MF-)?\d+\b"
    # Fuenfte Schaerfung. Drei weitere Stellen derselben Klasse, die
    # erst sichtbar wurden, als „for now" das Rauschen nicht mehr
    # verdeckte:
    #   ProtectionAnalysisWidget:292  „der frueher an dieser Stelle
    #                                  stehende Kommentar (…)"
    #   fluxengine_provider_v2:192    „(was hard-coded \"ibm\")"
    # Zum Vergleich: `widerspruch.py` brauchte in derselben Sitzung
    # ebenfalls fuenf Durchgaenge, 18 von 28 Erstbefunden waren Fehler
    # des Pruefers. Das ist die Regel, nicht die Ausnahme — und der
    # Grund, warum ein Tor ohne Selbsttest nichts wert ist.
    r"|\bfrueher\b|\bpreviously\b|\bwas hard-?coded\b|\bformerly\b", re.I)

FUNC_START = re.compile(r"^[A-Za-z_][\w \t*:&<>,]*\**\s*\w+\s*\([^;]*$")

_KOMMENTAR = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)


def _ohne_kommentare(text: str) -> str:
    """Kommentare durch Leerraum ersetzen, Zeilenzahl erhalten.

    Fuer den ANZEIGE-Scan zwingend. `ANZEIGE` paart Anfuehrungszeichen
    paarweise ab; ein einzelnes `"` oder ein Apostroph IN einem Kommentar
    verschiebt jede Paarung danach um eins. Gemessen an der
    Vorher-Fassung von `explorertab.cpp`: eine der falsch gepaarten
    „Zeichenketten" lautete

        });\\n    } else {\\n        // Generic - show placeholder\\n

    — sie enthielt das Wort „placeholder", der Vorbehalts-Test griff, und
    die Funktion entlastete sich mit ihrem eigenen Kommentar. Das Tor
    schwieg zu genau der Haelfte des Vorbilds, die MF-569 zuerst nennt.
    """
    return _KOMMENTAR.sub(lambda m: "\n" * m.group(0).count("\n"), text)

# Begruendete Ausnahmen: Datei -> Warum. Abgeglichen wird nach ANZAHL je
# Datei, nicht nach Zeile (die Lehre aus MF-566: eine Zeilennummer als
# Schluessel laesst jede Verschiebung oberhalb rot werden, und die
# billigste Antwort darauf ist Hochzaehlen ohne Nachlesen).
GRUNDLINIE: dict[str, int] = {}


def _funktionen(text: str):
    """(Startzeile, Zeilen) je Funktionsrumpf — Klammern zaehlen."""
    zeilen = text.split("\n")
    i = 0
    while i < len(zeilen):
        if FUNC_START.match(zeilen[i]) and "{" not in zeilen[i]:
            j = i
            while j < len(zeilen) and "{" not in zeilen[j]:
                j += 1
                if j - i > 6:
                    break
        elif FUNC_START.match(zeilen[i]) or (
                "{" in zeilen[i] and re.match(r"^\w[\w \t*:&<>,]*\w\s*\(",
                                              zeilen[i])):
            j = i
        else:
            i += 1
            continue
        if j >= len(zeilen) or "{" not in zeilen[j]:
            i += 1
            continue
        tiefe = 0
        k = j
        while k < len(zeilen):
            tiefe += zeilen[k].count("{") - zeilen[k].count("}")
            if tiefe <= 0 and k > j:
                break
            k += 1
        if k - i > 1:
            yield i + 1, zeilen[i:k + 1]
        i = k + 1


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    gesehen: dict[str, int] = {}
    treffer: list[tuple[str, int, str]] = []

    d = Path(repo) / "src"
    if not d.exists():
        return errors

    # NUR die Oberflaeche. Das Tor fragt, was der BENUTZER zu sehen
    # bekommt — ein `.c` in `src/formats/` zeigt nichts an, es liefert
    # Daten. Die erste Fassung nahm beides und meldete 35 Stellen, 29
    # davon in Parsern. Dieselbe Lage wie beim `widerspruch.py`
    # (18 von 28 Erstbefunden waren Fehler des Pruefers): ein Tor, das
    # ueberwiegend danebenliegt, wird beim naechsten Mal nicht gelesen.
    for p in sorted(d.rglob("*.cpp")):
        try:
            text = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        rel = p.relative_to(repo).as_posix()

        for start, zeilen in _funktionen(text):
            rumpf = "\n".join(zeilen)

            m = None
            for kandidat in EINGESTAENDNIS.finditer(rumpf):
                # Der Kommentarblock, in dem das Eingestaendnis steht.
                block = ""
                for k in _KOMMENTAR.finditer(rumpf):
                    if k.start() <= kandidat.start() < k.end():
                        block = k.group(0)
                        break
                if block and HISTORIE.search(block):
                    continue            # zitiert Entferntes, kein Fund
                m = kandidat
                break
            if m is None:
                continue
            # Die Qt-Vokabel `setPlaceholderText` in derselben Zeile ist
            # kein Eingestaendnis.
            zeile_des_funds = rumpf[:m.start()].count("\n")
            if UNSCHULDIG.search(zeilen[zeile_des_funds]):
                # weitersuchen: vielleicht gibt es ein echtes daneben
                rest = rumpf[m.end():]
                m2 = EINGESTAENDNIS.search(rest)
                if not m2:
                    continue

            angezeigt = ANZEIGE.findall(_ohne_kommentare(rumpf))
            if not angezeigt:
                continue                   # zeigt gar nichts an
            if any(VORBEHALT.search(s) for s in angezeigt):
                continue                   # der Benutzer erfaehrt es

            gesehen[rel] = gesehen.get(rel, 0) + 1
            treffer.append((rel, start + zeile_des_funds,
                            m.group(1).strip()))

    for rel, lineno, wort in treffer:
        if gesehen[rel] <= GRUNDLINIE.get(rel, 0):
            continue
        errors.append(
            f"{rel}:{lineno}: der Quelltext gibt zu, dass die Werte "
            f"erfunden sind („{wort}“) — aber keine der "
            f"angezeigten Zeichenketten sagt es dem Benutzer. Vorbild "
            f"MF-569: die Belegungskarte zeigte jede Diskette als leer, "
            f"jeder Block gruen mit „F“, der Vorbehalt stand "
            f"nur im Quelltext. Fuer einen Forensiker ist so eine Anzeige "
            f"von einer echten nicht zu unterscheiden. Fix: ein „?“ "
            f"statt des erfundenen Werts UND ein Satz in der Anzeige, "
            f"nicht im Kommentar.")

    for rel, n in GRUNDLINIE.items():
        extra = n - gesehen.get(rel, 0)
        if extra > 0:
            errors.append(
                f"{rel}: {extra} begruendete Ausnahme(n) ohne Fundstelle "
                f"— erledigt, bitte aus GRUNDLINIE entfernen.")

    return errors


def main() -> int:
    ziel = Path(sys.argv[1]) if len(sys.argv) > 1 else WURZEL
    fehler = check(ziel)
    print("Anzeigen, die Erfindung nur im Quelltext zugeben: %d"
          % len(fehler))
    for f in fehler:
        print("  " + f)
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
