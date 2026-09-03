#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Eintraege in docs/OPEN_ITEMS.md ohne Belegtyp (MF-843).

── Woher der Vorschlag kommt ───────────────────────────────────────────────

Der Eigentuemer hat in dieser Analysereihe zweimal eine eigene Behauptung
zuruecknehmen muessen — die Aussagekraft der FAT-Volumeseriennummer und
die `0x01`-Ersetzung durch Burst Nibbler. Beide Male, weil eine
BESCHREIBUNG als MESSUNG weitergegeben wurde. Sein Schluss:

    „Eine Regel, die ihren Belegtyp mitfuehrt, kann nicht versehentlich
     zu einem Befund aufsteigen — und `may_assert = false` ist
     maschinell durchsetzbar, wo Vorsicht es nicht ist."

Das ist richtig, und dieselbe Klasse ist mir hier ebenfalls unterlaufen:
P3-65 (3) hielt fest, `atari_check.c` pruefe Eintraege hinter der
Verzeichnis-Endmarke nicht — uebernommen ohne Messung, und falsch (der
Prueflauf gab es, er konnte nur nicht feuern, MF-835).

── Warum KEINE C-Struktur ──────────────────────────────────────────────────

Der Entwurf sah `uft_rule_meta_t` mit `uft_evidence_t` und `may_assert`
im Code vor. Als C-Struktur waere das ein Feld, das niemand liest —
genau der Fall, den `scripts/audit_dead_fields.py` seit MF-831 misst
(1231 solche Felder im Bestand). Ein Belegtyp, den kein Verbraucher
abfragt, schuetzt niemanden.

Die tragfaehige Fassung ist die mittlere Spalte von `OPEN_ITEMS.md` —
dort steht der Beleg BEREITS, nur ohne Zwang. Dieses Tor macht daraus
eine Zahl.

── Was gemessen wird ───────────────────────────────────────────────────────

Jede Zeile `| P3-NNN | … | BELEG | BEWERTUNG |`: traegt die mittlere
Spalte eine Belegangabe?

── Grenze des Verfahrens, ausdruecklich ────────────────────────────────────

Die Erkennung arbeitet mit einem WORTSCHATZ („gemessen", „Rotbeweis",
„gelesen", „disassembliert", „vom Eigentuemer", …). Das ist eine
Aufzaehlung, und dieser Baum hat gute Gruende, Aufzaehlungen zu
misstrauen — hier ist sie aber unvermeidbar, weil der gepruefte Text
Prosa ist und kein Datensatz. Zwei Folgen, die man kennen muss:

  * Ein Eintrag mit einem Beleg in ANDEREN Worten faellt faelschlich auf.
  * Ein Eintrag, der das Wort „gemessen" schreibt, ohne gemessen zu
    haben, faellt NICHT auf. Das Tor prueft die FORM, nicht die Wahrheit.

Es ersetzt also keine Sorgfalt. Es verhindert nur, dass die Zahl der
belegfreien Eintraege still WAECHST — und das ist genau das, was der
Vorschlag wollte.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
DOKUMENT = "docs/OPEN_ITEMS.md"
GRUNDLINIE = WURZEL / "docs" / "evidence_marker_baseline.txt"

ZEILE = re.compile(r"^\|\s*(P3-\d+)\s*\|")

# Wortschatz der Belegarten. Bewusst klein gehalten und hier sichtbar,
# damit die Grenze des Verfahrens nachlesbar bleibt.
BELEG = re.compile(
    r"gemessen|nachgerechnet|nachgemessen|Rotbeweis|Gegenprobe"
    r"|gelesen|Quelle|woertlich|disassembliert|abgerufen"
    r"|vom Eigent|gemeldet von|abgeleitet|hergeleitet",
    re.I)

# Die MF-Nummer, unter der der Beleg entstanden ist.
# Ohne erzwungene Klammer davor: die Belegspalte schreibt sowohl
# „(MF-843)" als auch „…, MF-843)". Die erste Fassung verlangte die
# oeffnende Klammer und lehnte damit den eigenen Stil ab — gefangen,
# als das Tor die zwei Eintraege dieses Commits meldete.
MFNR = re.compile(r"\bMF-\d+\b")


def dokument(wurzel: Path) -> str | None:
    try:
        r = subprocess.run(
            ["git", "ls-files", "--cached", "--others",
             "--exclude-standard", DOKUMENT],
            cwd=wurzel, capture_output=True, text=True, timeout=60)
        if r.returncode != 0 or not r.stdout.strip():
            print("  WARNUNG: %s nicht in git — Tor laesst durch" % DOKUMENT,
                  file=sys.stderr)
            return None
    except Exception as e:                                   # noqa: BLE001
        print("  WARNUNG: git nicht befragbar (%s) — Tor laesst durch" % e,
              file=sys.stderr)
        return None
    p = wurzel / DOKUMENT
    return p.read_text(encoding="utf-8", errors="replace") if p.exists() else None


def pruefe_text(text: str):
    ohne, gesamt = [], 0
    for z in text.splitlines():
        m = ZEILE.match(z)
        if not m:
            continue
        gesamt += 1
        # Spaltenaufbau: | P3-NN | Beschreibung | BELEG | Bewertung |
        # -> split ergibt ['', ' P3-NN ', ' Beschreibung ', ' BELEG ', …],
        # der Beleg steht also an Index 3. Die erste Fassung nahm Index 2
        # und pruefte damit die BESCHREIBUNG — vom Selbsttest gefangen,
        # und die daraus abgeleitete Zahl war entsprechend falsch.
        # KEINE Spaltenzerlegung. Zwei Anlaeufe sind daran gescheitert:
        # Index 3 von vorn traf bei Zeilen mit `|` im Beschreibungstext
        # die falsche Spalte, Index -2 von hinten bei Zeilen mit `|` in
        # der Bewertung. Die Prosa enthaelt zu viele Trennstriche.
        #
        # Geprueft wird stattdessen die GANZE Zeile auf ZWEI Merkmale,
        # und beide zusammen sind die tatsaechliche Konvention dieses
        # Dokuments: WIE es festgestellt wurde (Wortschatz) und WOMIT
        # (die MF-Nummer, unter der es gemessen wurde). Der Wortschatz
        # allein ist zu grosszuegig — er trifft auch „gemessen" im
        # Beschreibungstext.
        if not (BELEG.search(z) and MFNR.search(z)):
            ohne.append(m.group(1))
    return ohne, gesamt


def selbsttest() -> bool:
    ok = 0
    mit = "| P3-99 | Befund | im Baum gemessen (MF-999) | offen |"
    ohne = "| P3-98 | Befund | irgendwoher | offen |"
    kein = "Dies ist eine gewoehnliche Zeile ohne Tabelle."

    o, g = pruefe_text(mit)
    if g == 1 and not o:
        ok += 1
    else:
        print("  SELBSTTEST 1 ROT: belegter Eintrag gemeldet:", o)

    o, g = pruefe_text(ohne)
    if g == 1 and o == ["P3-98"]:
        ok += 1
    else:
        print("  SELBSTTEST 2 ROT: belegfreier Eintrag nicht erkannt:", o, g)

    # GEGENBEWEIS: Fliesstext darf nicht mitgezaehlt werden — sonst
    # waere die Grundlinie von der Prosa im Dokument abhaengig.
    o, g = pruefe_text(kein)
    if g == 0:
        ok += 1
    else:
        print("  SELBSTTEST 3 ROT: Fliesstext als Eintrag gezaehlt")

    print("  Selbsttest %d/3" % ok)
    return ok == 3


def grenze():
    if not GRUNDLINIE.exists():
        return None
    for z in GRUNDLINIE.read_text(encoding="utf-8").splitlines():
        z = z.split("#")[0].strip()
        if z.isdigit():
            return int(z)
    return None


def check(repo=None):
    global WURZEL, GRUNDLINIE
    if repo:
        WURZEL = Path(repo)
        GRUNDLINIE = WURZEL / "docs" / "evidence_marker_baseline.txt"
    g = grenze()
    if g is None:
        return []
    t = dokument(WURZEL)
    if t is None:
        return []
    ohne, _ = pruefe_text(t)
    if len(ohne) <= g:
        return []
    return ["%d Eintraege ohne Belegtyp > Grundlinie %d: %s"
            % (len(ohne), g, ", ".join(ohne[-5:]))]


def main() -> int:
    print("audit_evidence_marker (MF-843)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2
    t = dokument(WURZEL)
    if t is None:
        return 0
    ohne, gesamt = pruefe_text(t)
    print("  P3-Eintraege                : %d" % gesamt)
    print("  davon ohne Belegtyp         : %d" % len(ohne))
    if ohne:
        print("    %s" % ", ".join(ohne))
    g = grenze()
    if g is None:
        print("  keine Grundlinie — nur Bericht")
        return 0
    print("  Grundlinie                  : %d" % g)
    if len(ohne) > g:
        print("  FEHLER: die Zahl ist gestiegen.")
        return 1
    if len(ohne) < g:
        print("  Hinweis: Grundlinie auf %d senken." % len(ohne))
    print("  OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
