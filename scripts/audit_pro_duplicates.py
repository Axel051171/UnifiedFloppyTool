#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tor 53: dieselbe Quelldatei mehrfach in einer .pro-Datei (MF-863).

── Der Fund ─────────────────────────────────────────────────────────────

`UnifiedFloppyTool.pro` fuehrte 2081 Quellzeilen fuer 582 verschiedene
Dateien. 27 Pfade standen mehrfach da; 16 davon **94-mal**, weil derselbe
16-Zeilen-Block (`src/analysis/events/`, `src/analysis/denoise/`) jedem
`SOURCES +=`-Block im Baum vorangestellt worden war. 1499 der 3389 Zeilen
— 44 % der Datei — waren Wiederholung.

Gemessen hat das nichts kaputtgemacht: qmake erzeugte vor und nach der
Bereinigung dieselben **624** Objekte. Der Schaden ist anderer Art. Eine
`.pro`-Datei ist die Liste dessen, was UEBERSETZT wird; wenn dieselbe Zeile
94-mal darin steht, kann niemand mehr durch Ansehen entscheiden, ob eine
Datei absichtlich in einem bestimmten Block steht. Genau das war bei
`src/formats/mfi/uft_mfi.c` die Frage, als der zweite MFI-Leser auffiel.

── Warum das Tor die Geltungsbereiche mitprueft ─────────────────────────

Eine Dublette ist NUR dann ein Befund, wenn beide Vorkommen unbedingt
gelten. Steht eine Datei einmal in `win32 { }` und einmal in `unix { }`,
ist das richtig und kein Fehler. Wer nach Pfadgleichheit allein
entdoppelt, entfernt sie aus genau dem Zweig, der sie braucht.

Vor der Bereinigung gemessen: alle 27 Pfade standen an **jeder**
Fundstelle unbedingt.

── Grundlinie ───────────────────────────────────────────────────────────

0. Kein neuer Mehrfacheintrag im unbedingten Geltungsbereich.

── Selbsttest ───────────────────────────────────────────────────────────

Laeuft vor der eigentlichen Messung und bricht bei roter Abnahme ab
(MF-693: eine Erstfassung meldete „Selbsttest 3/3" und lieferte 0/3).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / 'scripts'))

GRUNDLINIE = 0

QUELLE = re.compile(r'^\s*([A-Za-z0-9_./+-]+\.(?:c|cpp|cc|cxx|m|mm))\s*(\\?)\s*$')


def _bereiche(zeilen: list[str]) -> list[list[str]]:
    """Je Zeile die Liste der offenen Blockkoepfe (`win32 {`, `unix {`, …)."""
    stapel: list[str] = []
    aus: list[list[str]] = []
    for z in zeilen:
        aus.append(list(stapel))
        ohne = z.split('#')[0]
        for ch in ohne:
            if ch == '{':
                stapel.append(ohne.split('{')[0].strip() or '<anon>')
            elif ch == '}' and stapel:
                stapel.pop()
    return aus


def messen(text: str) -> list[tuple[str, list[int]]]:
    """Pfade, die mehr als einmal UNBEDINGT gelistet sind."""
    zeilen = text.replace('\r\n', '\n').split('\n')
    bereich = _bereiche(zeilen)

    unbedingt: dict[str, list[int]] = {}
    for nr, z in enumerate(zeilen):
        m = QUELLE.match(z)
        if not m:
            continue
        if bereich[nr]:
            continue                    # bedingt -> kein Befund
        unbedingt.setdefault(m.group(1), []).append(nr + 1)

    return sorted((p, o) for p, o in unbedingt.items() if len(o) > 1)


def _selbsttest() -> bool:
    faelle = [
        # (Text, erwartete Zahl der Befunde, Beschreibung)
        ('SOURCES += \\\n    a.c \\\n    b.c\n', 0, 'sauber'),
        ('SOURCES += \\\n    a.c \\\n    a.c\n', 1, 'echte Dublette'),
        ('SOURCES += a.c\nwin32 {\n    SOURCES += a.c\n}\n', 0,
         'einmal unbedingt, einmal in win32 -> KEIN Befund'),
        ('win32 {\n    SOURCES += a.c\n}\nunix {\n    SOURCES += a.c\n}\n', 0,
         'zwei Plattformzweige -> KEIN Befund'),
        ('SOURCES += \\\n    a.c \\\n    b.c \\\n    a.c \\\n    b.c\n', 2,
         'zwei Dubletten'),
    ]
    ok = 0
    for text, erwartet, was in faelle:
        got = len(messen(text))
        if got == erwartet:
            ok += 1
        else:
            print('  SELBSTTEST ROT: %s -> %d statt %d' % (was, got, erwartet))
    print('  Selbsttest: %d/%d' % (ok, len(faelle)))
    return ok == len(faelle)


def main() -> int:
    print('Tor 53: Mehrfacheintraege in .pro-Dateien (MF-863)')
    print('=' * 68)
    if not _selbsttest():
        print('ABBRUCH: der Selbsttest ist rot — die Messung darunter waere')
        print('         wertlos, ihre Null keine Entwarnung.')
        return 2

    # MF-636: die Dateimenge kommt aus git, nicht aus einer gepflegten Liste.
    kandidaten: list[Path] = []
    try:
        from repo_scope import repo_files                # type: ignore
        alle = repo_files(WURZEL)
    except Exception:
        alle = None
    if alle is None:
        # Ist git nicht befragbar, lassen wir durch UND sagen es.
        print('  HINWEIS: git nicht befragbar — Rueckfall auf glob. Eine')
        print('           Null aus diesem Lauf deckt nur den Wurzelordner.')
        kandidaten = sorted(WURZEL.glob('*.pro')) + sorted(WURZEL.glob('*.pri'))
    else:
        kandidaten = sorted(p for p in alle
                            if p.suffix in ('.pro', '.pri'))

    gesamt = 0
    for rel in kandidaten:
        pfad = rel if rel.is_absolute() else WURZEL / rel
        if not pfad.is_file():
            continue
        befunde = messen(pfad.read_text(encoding='utf-8', errors='replace'))
        if not befunde:
            continue
        print()
        print('%s:' % pfad.relative_to(WURZEL))
        for p, orte in befunde:
            gesamt += len(orte) - 1
            print('  %-56s %2dx  Z%s' % (p, len(orte),
                                         ','.join(str(o) for o in orte[:6])))

    print()
    print('=' * 68)
    print('ueberzaehlige unbedingte Eintraege: %d (Grundlinie %d)'
          % (gesamt, GRUNDLINIE))
    if gesamt > GRUNDLINIE:
        print()
        print('Eine Quelldatei gehoert genau einmal unbedingt in die')
        print('Bauliste. Steht sie mehrfach da, kann niemand mehr durch')
        print('Ansehen entscheiden, welcher Block sie absichtlich fuehrt.')
        return 1
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
