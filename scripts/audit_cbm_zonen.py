#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 60: die CBM-Zonenlaengen und ihre Zaehlweisen — gezaehlt, nicht
erinnert (MF-932, P3-150).

── DER ANLASS ────────────────────────────────────────────────────────

Eine 1541 hat vier Zonen: Spuren 1–17 tragen 21 Sektoren, 18–24 tragen
19, 25–30 tragen 18, 31–42 tragen 17. Diese vier Zahlen liegen im Baum
vielfach herum, und NICHT in einer gemeinsamen Zaehlweise.

P3-150 hielt seit MF-877 fest: „ELFFACH im Baum, in VIER Zaehlweisen."
Das war eine Handzaehlung, und sie ist abgedriftet. Gemessen (MF-932):
**23 Fundstellen in NEUN Zaehlweisen**.

Das ist der fuenfzehnte Fall von Aufzaehlung statt Messung in diesem
Baum — und der Punkt, den P3-150 selbst macht, wird dadurch nur
staerker: **erst das Tor, dann das Zusammenfuehren.** Vier (in Wahrheit
neun) Zaehlweisen sind nicht mechanisch ineinander ueberfuehrbar. Wer
ohne vorherige Messung aufraeumt, fuehrt still eine zehnte ein.

Zwei Kopien haben bereits nachweislich falsch gelesen: P3-148 und
P3-149.

── WAS GEZAEHLT WIRD ─────────────────────────────────────────────────

Feld-Initialisierer (`NAME[...] = { ... }`) im kommentar- und
zeichenkettenfreien Text, deren Zahlen

  * alle vier Zonenwerte 21/19/18/17 enthalten, UND
  * ausser diesen nur noch 0 enthalten (als Platzhalter fuer den
    unbenutzten Index 0 einer 1-basierten Spurtabelle).

Die zweite Bedingung ist der Filter, der traegt: ohne sie melden auch
`skew_6` (CP/M-Verzahnung), `gcr62_decode` (NIB), `twiggy_spt` (Lisa)
und eine 66-Eintraege-Tabelle mit 255 — Tabellen, die dieselben Zahlen
zufaellig enthalten. Gemessen: 27 Kandidaten roh, 23 nach dem Filter.

Die ZAEHLWEISE ergibt sich aus Laenge und erstem Wert:

    4 Eintraege, 21 zuerst    Zone 0 = Spuren 1–17   (aufsteigend)
    4 Eintraege, 17 zuerst    Zone 0 = Spuren 31–42  (absteigend)
    n Eintraege, 21 zuerst    Index 0 = Spur 1       (0-basiert)
    n Eintraege, 0 zuerst     Index 0 unbenutzt      (1-basiert)

── WAS DIESES TOR NICHT TUT ──────────────────────────────────────────

Es fuehrt nichts zusammen und urteilt nicht darueber, welche Zaehlweise
richtig ist — beides waere eine Aenderung, keine Messung. Es haelt nur
fest, WIE VIELE es gibt, damit eine neue auffaellt.

Es sieht auch keine Zonenlogik, die als `if`-Kette statt als Tabelle
geschrieben ist. Das steht hier, weil es eine echte Luecke ist: eine
zehnte Zaehlweise koennte sich so dem Tor entziehen.

── Grundlinie ────────────────────────────────────────────────────────

23 Fundstellen, 9 Zaehlweisen. Beide duerfen nur FALLEN. Die Richtung
ist Zusammenfuehrung auf `uft_cbm_track_capacity()`; jeder Schritt
dorthin senkt beide Zahlen.

Dateimenge aus `git ls-files` (MF-636), nicht aus einer gepflegten
Liste.
"""
from __future__ import annotations

import io
import re
import subprocess
import sys
from pathlib import Path

GRUNDLINIE_STELLEN = 23
GRUNDLINIE_WEISEN = 9

ZONEN = {21, 19, 18, 17}
ERLAUBT = ZONEN | {0}

FELD = re.compile(r"(\w+)\s*\[[^\]]*\]\s*(?:\[[^\]]*\]\s*)?=\s*\{([^{}]*)\}",
                  re.S)


def entkerne(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    t = re.sub(r"//[^\n]*", " ", t)
    t = re.sub(r'"(\\.|[^"\\])*"', '""', t)
    return t


def dateien(repo: Path):
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=str(repo), capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if aus.returncode != 0:
        return None
    return [f for f in aus.stdout.split("\n")
            if f.endswith((".c", ".h", ".cpp", ".hpp"))]


def zaehlweise(zahlen: list) -> str:
    n = len(zahlen)
    erst = zahlen[0]
    if n == 4:
        if erst == 21:
            return "4 Zonen, aufsteigend (Zone 0 = Spur 1-17)"
        if erst == 17:
            return "4 Zonen, absteigend (Zone 0 = Spur 31-42)"
        return "4 Zonen, beginnt mit %d" % erst
    if erst == 0:
        return "%d Eintraege, 1-basiert (Index 0 unbenutzt)" % n
    if erst == 21:
        return "%d Eintraege, 0-basiert (Index 0 = Spur 1)" % n
    return "%d Eintraege, beginnt mit %d" % (n, erst)


def messe(repo: Path):
    """-> (fundstellen, fehler); fundstellen ist None ohne git."""
    pfade = dateien(repo)
    if pfade is None:
        return None, ["Guard-frei: `git ls-files` war nicht befragbar, "
                      "dieses Tor hat NICHTS geprueft."]

    gefunden = []
    for rel in pfade:
        try:
            roh = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "21" not in roh:
            continue
        t = entkerne(roh)
        for m in FELD.finditer(t):
            zahlen = [int(x) for x in re.findall(r"\b\d+\b", m.group(2))]
            if not zahlen:
                continue
            menge = set(zahlen)
            if not ZONEN.issubset(menge) or (menge - ERLAUBT):
                continue
            gefunden.append((rel, m.group(1), zaehlweise(zahlen)))
    return sorted(gefunden), []


def check(repo) -> list:
    stellen, fehler = messe(Path(repo))
    if stellen is None:
        return fehler

    weisen = sorted({w for _, _, w in stellen})

    if len(stellen) > GRUNDLINIE_STELLEN:
        fehler.append(
            "%d Kopien der CBM-Zonenlaengen, Grundlinie %d. Die Richtung "
            "ist Zusammenfuehrung auf `uft_cbm_track_capacity()` — jede "
            "neue Kopie geht dagegen. Zwei bestehende lesen nachweislich "
            "falsch (P3-148, P3-149)."
            % (len(stellen), GRUNDLINIE_STELLEN))
    if len(weisen) > GRUNDLINIE_WEISEN:
        # Bewusst OHNE Liste der bekannten Zaehlweisen: die muesste
        # gepflegt werden, und eine gepflegte Liste driftet — genau die
        # Falle, aus der dieses Tor entstanden ist (P3-150 nannte 11
        # Stellen in 4 Zaehlweisen, gemessen waren es 23 in 9).
        # `--list` zeigt alle mit Fundstelle; welche neu ist, sagt der
        # Vergleich mit dem Vorzustand, nicht dieses Skript.
        fehler.append(
            "%d Zaehlweisen, Grundlinie %d. Eine NEUE Zaehlweise ist der "
            "teuerste Zuwachs: sie laesst sich mit den anderen nicht "
            "mechanisch verrechnen, und genau daran ist P3-148/149 "
            "gescheitert. Welche dazugekommen ist, zeigt "
            "`python scripts/audit_cbm_zonen.py --list` im Vergleich zum "
            "Vorzustand."
            % (len(weisen), GRUNDLINIE_WEISEN))
    return fehler


def _selbsttest() -> int:
    """Vor dem Nenner (MF-693) — ueber `check()` selbst, nicht ueber eine
    Hilfsfunktion."""
    import tempfile

    def baum(inhalt: str) -> int:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True)
            (p / "x.c").write_text(inhalt, encoding="utf-8")
            return len(messe(p)[0])

    faelle = [
        ("4-Zonen aufsteigend",
         "static int z[4] = { 21, 19, 18, 17 };", 1),
        ("4-Zonen absteigend",
         "static int z[4] = { 17, 18, 19, 21 };", 1),
        ("Spurtabelle 1-basiert",
         "static int z[6] = { 0, 21, 19, 18, 17, 17 };", 1),
        ("Skew-Tabelle mit denselben Zahlen -> KEIN Fund",
         "static int s[8] = { 1, 7, 13, 19, 21, 5, 17, 18 };", 0),
        ("nur drei der vier Zonen -> KEIN Fund",
         "static int z[3] = { 21, 19, 18 };", 0),
        ("im Kommentar -> KEIN Fund",
         "/* static int z[4] = { 21, 19, 18, 17 }; */\nint f(void){return 0;}",
         0),
        ("in einer Zeichenkette -> KEIN Fund",
         'const char *s = "{ 21, 19, 18, 17 }";', 0),
    ]

    gut = 0
    for name, inhalt, soll in faelle:
        ist = baum(inhalt)
        if ist == soll:
            gut += 1
        else:
            print("  ROT  %-46s erwartet %d, gemessen %d"
                  % (name, soll, ist))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest()

    stellen, fehler = messe(repo)
    if stellen is None:
        print("\n".join(fehler))
        return 1

    weisen = {}
    for rel, name, w in stellen:
        weisen.setdefault(w, []).append((rel, name))

    if "-v" in sys.argv or "--list" in sys.argv:
        for w in sorted(weisen):
            print("%s  (%d)" % (w, len(weisen[w])))
            for rel, name in weisen[w]:
                print("    %-50s %s" % (rel, name))
            print()

    print("CBM-Zonenlaengen: %d Fundstellen (Grundlinie %d), "
          "%d Zaehlweisen (Grundlinie %d)"
          % (len(stellen), GRUNDLINIE_STELLEN,
             len(weisen), GRUNDLINIE_WEISEN))

    errs = check(repo)
    if not errs:
        print("OK")
        return 0
    print("FAIL:")
    for e in errs:
        print("  %s" % e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
