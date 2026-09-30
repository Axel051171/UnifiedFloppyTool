#!/usr/bin/env python3
"""Kein Menueeintrag ohne Wirkung oder Begruendung (MF-1627).

    python scripts/audit_menue_aktionen.py            # Befunde
    python scripts/audit_menue_aktionen.py --selftest # Selbsttest

── Warum es dieses Tor gibt ─────────────────────────────────────────────
Gemessen am 2026-09-30: `forms/mainwindow.ui` fuehrte 20 Menueeintraege,
deren Name in keiner Quelldatei vorkam — „Read Disk", „Write Disk",
„Connect", „Motor On", „Convert…", die Sprachwahl. Jeder war anklickbar
und tat nichts. Die Lage stand seit MF-662 in `docs/OPEN_ITEMS.md`
(„tote Menuepunkte: 21 von 30"); aufgeschrieben und stehen gelassen ist
nicht behoben.

── Was es verlangt ──────────────────────────────────────────────────────
Jede `<action name="X">` des Hauptfensters steht in `src/mainwindow.cpp`
in genau einer dieser Formen (Kommentare zaehlen nicht):

    connect(ui->X, ...           eigener Handler
    alsFenster(ui->X, ...        oeffnet ein Menuefenster (MF-1297)
    menueKnopf(ui->X, ...        drueckt den Knopf eines Reiters (MF-1627)
    abschalten(ui->X, "Grund")   gesperrt, und der Tooltip sagt warum

Der Grund darf auch ein Bezeichner sein, den dieselbe Datei mit einem
nicht leeren `tr("…")` belegt. Eine Abschaltung ohne Grund (leere
Zeichenkette) zaehlt nicht.

── Was es NICHT sehen kann ──────────────────────────────────────────────
Ob ein `connect` zur Laufzeit wirklich erreicht wird (ein Zweig, der nie
laeuft) und ob der Handler das tut, was die Beschriftung sagt. Es prueft
die Zuordnung, nicht die Wirkung — die Wirkung pruefen die Tests der
Reiter, deren Knoepfe hier gedrueckt werden.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

UI = Path('forms/mainwindow.ui')
CPP = Path('src/mainwindow.cpp')

_AKTION = re.compile(r'<action\s+name="([A-Za-z_][A-Za-z0-9_]*)"')
_FORMEN = re.compile(
    r'\b(?:connect|alsFenster|menueKnopf)\s*\(\s*ui->([A-Za-z_][A-Za-z0-9_]*)\s*,')
_ABSCHALTEN = re.compile(
    r'\babschalten\s*\(\s*ui->([A-Za-z_][A-Za-z0-9_]*)\s*,\s*(?:tr\s*\(\s*)?"((?:[^"\\]|\\.)*)"')
# abschalten(ui->X, grund) mit einem Bezeichner: er zaehlt, wenn dieselbe
# Datei ihn mit einem nicht leeren tr("...") bzw. "..." belegt.
_ABSCHALTEN_VAR = re.compile(
    r'\babschalten\s*\(\s*ui->([A-Za-z_][A-Za-z0-9_]*)\s*,\s*([A-Za-z_][A-Za-z0-9_]*)\s*\)')
_BELEGUNG = r'\b{name}\s*=\s*(?:tr\s*\(\s*)?"((?:[^"\\]|\\.)*)"'


def _ohne_kommentare(text: str) -> str:
    """Kommentare entfernen, Zeichenketten stehen lassen."""
    aus, i, n = [], 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            j = i + 1
            while j < n and text[j] != '"':
                j += 2 if text[j] == '\\' else 1
            aus.append(text[i:j + 1])
            i = j + 1
        elif text.startswith('//', i):
            j = text.find('\n', i)
            i = n if j < 0 else j
        elif text.startswith('/*', i):
            j = text.find('*/', i + 2)
            i = n if j < 0 else j + 2
        else:
            aus.append(c)
            i += 1
    return ''.join(aus)


def befunde(ui_text: str, cpp_text: str) -> list[str]:
    aktionen = sorted(set(_AKTION.findall(ui_text)))
    code = _ohne_kommentare(cpp_text)
    gebunden = set(_FORMEN.findall(code))
    begruendet = {n for n, grund in _ABSCHALTEN.findall(code) if grund.strip()}
    for n, var in _ABSCHALTEN_VAR.findall(code):
        m = re.search(_BELEGUNG.format(name=re.escape(var)), code)
        if m and m.group(1).strip():
            begruendet.add(n)
    return [f'{a}: weder verbunden noch mit Begruendung abgeschaltet'
            for a in aktionen if a not in gebunden and a not in begruendet]


def check(repo: Path) -> list[str]:
    try:
        ui_text = (repo / UI).read_text(encoding='utf-8')
        cpp_text = (repo / CPP).read_text(encoding='utf-8')
    except OSError as e:
        return [f'Menueaktionen: Datei nicht lesbar ({e})']
    return befunde(ui_text, cpp_text)


def _selftest() -> int:
    ui = ('<action name="actionA"/><action name="actionB"/>'
          '<action name="actionC"/><action name="actionD"/>'
          '<action name="actionE"/><action name="actionF"/>')
    faelle = [
        ('(i) connect bindet', 'connect(ui->actionA, &QAction::triggered, this, f);', 'actionA', False),
        ('(ii) Fenster bindet', 'alsFenster(ui->actionB, w, tr("W"));', 'actionB', False),
        ('(iii) Knopf bindet', 'menueKnopf(ui->actionC, seite, "btnX");', 'actionC', False),
        ('(iv) Abschaltung mit Grund', 'abschalten(ui->actionD, tr("fehlt noch"));', 'actionD', False),
        ('(v) Abschaltung ohne Grund zaehlt nicht', 'abschalten(ui->actionE, tr(""));', 'actionE', True),
        ('(vi) nur im Kommentar zaehlt nicht', '// connect(ui->actionF, a, b, c);', 'actionF', True),
        ('(vii) blosse Nennung zaehlt nicht', 'ui->actionF->setText(x);', 'actionF', True),
        ('(viii) Blockkommentar zaehlt nicht', '/* menueKnopf(ui->actionF, s, "b"); */', 'actionF', True),
        ('(ix) Grund als belegter Bezeichner',
         'const QString g = tr("fehlt"); abschalten(ui->actionE, g);', 'actionE', False),
        ('(x) Bezeichner mit leerem Grund zaehlt nicht',
         'const QString g = tr(""); abschalten(ui->actionE, g);', 'actionE', True),
    ]
    rot = 0
    for name, cpp, aktion, erwartet_befund in faelle:
        b = befunde(ui, cpp)
        hat = any(x.startswith(aktion + ':') for x in b)
        ok = hat == erwartet_befund
        print(f'  [{"ok " if ok else "ROT"}] {name}')
        rot += not ok
    print(f'Selbsttest: {len(faelle) - rot}/{len(faelle)}')
    return 1 if rot else 0


def main() -> int:
    if '--selftest' in sys.argv:
        return _selftest()
    repo = Path(__file__).resolve().parent.parent
    b = check(repo)
    for x in b:
        print(x)
    print(f'Menueaktionen ohne Wirkung oder Begruendung: {len(b)}')
    return 1 if b else 0


if __name__ == '__main__':
    sys.exit(main())
