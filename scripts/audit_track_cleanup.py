#!/usr/bin/env python3
"""Wer liest eine Spur und raeumt sie nicht auf? (MF-531)

── Der Vertrag ──────────────────────────────────────────────────────────

`read_track()` fuellt `uft_track_t` ueber `uft_track_add_sector()`. Das
alloziert das `sectors`-Feld **und** je Sektor die Daten. Freigegeben wird
mit `uft_track_cleanup()` (oder `uft_track_free()` / `uft_track_release()`
fuer die Zeiger-Variante).

Wer eine Spur liest und das unterlaesst, leckt sie — pro Spur, pro
Aufruf. In einer Oberflaeche, die eine Diskette mit 160 Spuren oeffnet und
das mehrfach tut, summiert sich das schnell.

Der LeakSanitizer-Volllauf der CI (MF-517) nennt `uft_track_add_sector()`
als haeufigste Allokationsstelle. Das ist aber die Stelle, an der
alloziert wird — nicht die, an der der Fehler sitzt. Dieses Skript sucht
die zweite.

── Wie gemessen wird ────────────────────────────────────────────────────

Fuer jede Funktion unter `src/`, die `read_track(` aufruft: liegt im
selben Funktionsrumpf ein Aufruf einer der Aufraeum-Funktionen?

Das ist bewusst grob. Es findet nicht jeden Fall (eine Spur kann an einen
Aufrufer weitergereicht werden, der sie aufraeumt) und meldet deshalb
"pruefen", nicht "Fehler". Was es zuverlaessig findet, ist die haeufigste
Bauart: lesen, benutzen, vergessen.

Testcode steht bewusst NICHT im Bericht. Ein Test, der leckt, ist
unschoen und verstellt den Blick auf den Bericht — aber er trifft keinen
Benutzer.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

FUNC = re.compile(r"^[ \t]*(?:static[ \t]+)?[A-Za-z_][\w \t*]*?[ \t*]"
                  r"(\w+)[ \t]*\([^;{]*\)\s*\{", re.M)
# Nur die `uft_track_t*`-Variante zaehlt.
#
# Es gibt eine ZWEITE read_track-API (`uft_format_handler_t`), die einen
# vom Aufrufer gestellten `uint8_t buf[]` fuellt. Die alloziert nichts und
# braucht kein Aufraeumen. Die erste Fassung dieses Skripts unterschied das
# nicht und meldete `uft_advanced_get_track_quality()` als Leck — dort
# liegt der Puffer auf dem Stapel (`uint8_t track_buf[16384]`).
#
# Ein Messwerkzeug, das falsch misst, ist schlimmer als keines. Dieselbe
# Lehre wie beim Verwaisten-Tor (306 gegen 228, MF-509), beim Banner-Audit
# (12 gegen 6, MF-511) und beim read_track-Vertrag (18 gegen 12, MF-516).
#
# Erkannt wird deshalb ueber den DEKLARIERTEN TYP im selben Rumpf, nicht
# ueber die Gestalt des Aufrufs. Ein Versuch ueber das letzte Argument
# (`&\w*track\w*`) fiel prompt auf `&track_size` herein — ein `size_t`.
# Der Typ ist entscheidbar, der Name ist es nicht.
READ = re.compile(r"read_track\s*\(")
USES_TRACK_T = re.compile(r"\buft_track_t\b")
CLEAN = re.compile(r"\b(uft_track_cleanup|uft_track_free|uft_track_release|"
                   r"uft_track_dispose)\s*\(")


def body_of(text: str, start: int) -> str:
    depth, i = 0, text.index("{", start)
    j = i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    return text[i:]


def strip_comments(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    return re.sub(r"//[^\n]*", " ", t)


def scan(root: pathlib.Path, where="src"):
    hits = []
    for p in sorted((root / where).rglob("*.c")):
        raw = p.read_text(encoding="utf-8", errors="replace")
        t = strip_comments(raw)
        for m in FUNC.finditer(t):
            name = m.group(1)
            # Die read_track-Implementierungen selbst fuellen die Spur, sie
            # raeumen sie nicht auf. Sie sind nicht der Aufrufer.
            if "read_track" in name:
                continue
            try:
                body = body_of(t, m.start())
            except ValueError:
                continue
            if (READ.search(body) and USES_TRACK_T.search(body)
                    and not CLEAN.search(body)):
                rel = str(p.relative_to(root)).replace("\\", "/")
                hits.append((rel, t[:m.start()].count("\n") + 1, name))
    return hits


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py — nur als Bericht gedacht.

    Gibt bewusst KEINE Befunde zurueck: die Heuristik ist zu grob, um
    einen Commit zu blockieren. Wer sie scharf schalten will, braucht
    zuerst eine Grundlinie wie beim Verwaisten-Tor.
    """
    return []


def main() -> int:
    where = "tests" if "--tests" in sys.argv else "src"
    hits = scan(ROOT, where)
    print("%s: Aufrufer von read_track(uft_track_t*) ohne Aufraeumen: %d"
          % (where, len(hits)))
    for rel, line, name in hits:
        print("    %s:%d  %s()" % (rel, line, name))
    if not hits:
        print("  (keiner)")
    print("\nHinweis: 'pruefen', nicht 'Fehler' — eine Spur kann an einen")
    print("Aufrufer weitergereicht werden, der sie aufraeumt.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
