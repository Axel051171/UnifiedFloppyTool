#!/usr/bin/env python3
"""Waechter: wie viel liegt ungetrackt UND nicht ignoriert im Baum? (MF-652)

Der Anlass ist gemessen, nicht gedacht. `.gitignore` fuehrte seit jeher
`build/` und `build_*/` — mit Unterstrich. Ein `build-shift/` mit
BINDESTRICH war dadurch nicht abgedeckt und hielt **30 971** Dateien,
die `git status` als "untracked" auswies: ein `git add -A` an der
Wurzel haette einen kompletten Build-Baum veroeffentlicht.

Das ist die fuenfte Auflage desselben Fehlers (MF-567 Abbruch-Codes,
MF-578 Offscreen-Tests, MF-598 SKIP_RETURN_CODE, MF-633 SKIP_DIRS,
MF-651 Attributions-Formulierungen): aufgezaehlt wurde EINE Auspraegung,
durchgefallen ist die Nachbarin.

Dieser Waechter zaehlt deshalb nicht Muster, sondern das ERGEBNIS. Er
weiss nicht, wie das naechste Build-Verzeichnis heisst — er merkt nur,
dass ploetzlich sehr viel Unerfasstes herumliegt. Ein Mass, das keine
Namensliste braucht, veraltet auch nicht.

Die Schwelle ist bewusst grosszuegig: sie soll eine FLUT fangen, nicht
normales Arbeiten stoeren. Wer zehn neue Dateien schreibt, bevor er sie
hinzufuegt, ist der Normalfall; wer dreissigtausend liegen hat, hat ein
Verzeichnis vergessen zu ignorieren.
"""
from __future__ import annotations

import pathlib
import subprocess
import sys

SCHWELLE = 200
"""Ab hier wird gemeldet. Gemessen nach der MF-652-Bereinigung liegt der
Baum bei 1 (ein frisch geschriebenes Skript). Zweihundert laesst jede
plausible Arbeitssitzung durch und faengt jedes vergessene
Build-/Werkzeug-Verzeichnis, denn die bringen Tausende mit."""

ZEIGE = 8
"""So viele Verzeichnisse werden in der Meldung benannt — genug, um die
Ursache zu sehen, ohne die Ausgabe zu fluten."""


def _untracked(repo: pathlib.Path) -> list[str]:
    try:
        out = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard"],
            cwd=str(repo), capture_output=True, text=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return []
    if out.returncode != 0:
        return []
    return [l for l in out.stdout.splitlines() if l.strip()]


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py."""
    repo = pathlib.Path(repo)
    dateien = _untracked(repo)
    if len(dateien) < SCHWELLE:
        return []

    # Wo kommen sie her? Nach oberstem Verzeichnis buendeln, damit die
    # Meldung die URSACHE nennt und nicht 30 000 Symptome.
    nach_dir: dict[str, int] = {}
    for f in dateien:
        top = f.split("/", 1)[0] if "/" in f else "(Wurzel)"
        nach_dir[top] = nach_dir.get(top, 0) + 1
    grosse = sorted(nach_dir.items(), key=lambda kv: -kv[1])[:ZEIGE]
    wo = ", ".join("%s (%d)" % (d, n) for d, n in grosse)

    return ["%d Dateien liegen ungetrackt UND nicht ignoriert im Baum "
            "(Schwelle %d). Groesste Quellen: %s. Ein `git add -A` an der "
            "Wurzel wuerde sie veroeffentlichen. Wenn das Arbeitsreste "
            "sind, gehoert das Verzeichnis in .gitignore; wenn es Quellen "
            "sind, gehoeren sie in den Index. Anlass: MF-652 — "
            "`build_*/` war ignoriert, `build-shift/` mit Bindestrich "
            "nicht, und hielt 30 971 Dateien."
            % (len(dateien), SCHWELLE, wo)]


def main() -> int:
    repo = pathlib.Path(__file__).resolve().parent.parent
    fehler = check(repo)
    for f in fehler:
        print(f)
    n = len(_untracked(repo))
    print("ungetrackt und nicht ignoriert: %d (Schwelle %d)"
          % (n, SCHWELLE))
    return 1 if fehler else 0


if __name__ == "__main__":
    raise SystemExit(main())
