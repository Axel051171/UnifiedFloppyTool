#!/usr/bin/env python3
"""Eine Umgebung, in der `git -C <pfad>` wirklich <pfad> meint (MF-1282).

── DIE REGEL ────────────────────────────────────────────────────────────

Git EXPORTIERT `GIT_DIR` in jeden Haken — `pre-commit`, `pre-push`,
`commit-msg`. Ein Unterprozess, der danach git gegen ein ANDERES
Verzeichnis ruft, wird davon UEBERSTIMMT: `-C` und `cwd=` wechseln nur
das Arbeitsverzeichnis, welches Depot gemeint ist, bestimmt `GIT_DIR`.

Wer git gegen ein Pruefverzeichnis ruft, ruft es also ohne diese
Bereinigung gegen das UMGEBENDE Depot.

── WAS DAS GEKOSTET HAT ─────────────────────────────────────────────────

Zeigt das geerbte `GIT_DIR` auf einen NEBENBAUM — `…/.git/worktrees/<n>`,
also nicht auf ein Verzeichnis namens `.git` —, dann setzt `git init`
dabei `core.bare = true` in die GEMEINSAME Konfiguration. Der Hauptbaum
antwortet danach auf jeden Befehl mit „fatal: this operation must be run
in a work tree".

Im Sandkasten nachgestellt, Schritt fuer Schritt:

    vorher  core.bare = false
    (cd fremd && GIT_DIR=<depot>/.git/worktrees/neben git init -q)
    nachher core.bare = true
    git rev-parse --show-toplevel
    -> fatal: this operation must be run in a work tree

Gegenprobe, und sie entscheidet die Bauform: mit `GIT_DIR` auf ein
Verzeichnis namens `.git` — ein gewoehnliches Depot statt eines
Nebenbaums — bleibt `core.bare` **false**. Deshalb ist es im Hauptbaum
nie aufgefallen: dort exportiert git `GIT_DIR=.git` RELATIV, und das
zeigt im Pruefverzeichnis auf dessen eigenes `.git`.

`docs/OPEN_ITEMS.md` P3-463 und `.claude/AUFGABEN.md` haben diesen
Schaden zweimal als „Ursache nicht gefunden" abgelegt.

── UND WARUM DIESE DATEI EXISTIERT ──────────────────────────────────────

MF-1279 hat die Bereinigung in `scripts/scout_stand.py` eingebaut und
den Fall damit fuer EINEN Aufrufer geschlossen. Das war die Klasse
„Aufzaehlung statt Messung": gemessen rufen **dreizehn** Stellen
`git init`, elf davon waren danach noch ungereinigt. Eine Regel, die
dreizehn Stellen brauchen, gehoert nicht in ein Scout-Skript (D3).

Gehalten wird sie von `scripts/audit_git_umgebung.py`.
"""
from __future__ import annotations

import os

#: Die Variablen, mit denen git ein Depot festlegt. Alles andere bleibt
#: in der Umgebung — `PATH`, Anmeldedaten, Proxy.
GIT_DEPOT_VARIABLEN = (
    "GIT_DIR",
    "GIT_WORK_TREE",
    "GIT_INDEX_FILE",
    "GIT_COMMON_DIR",
    "GIT_OBJECT_DIRECTORY",
    "GIT_ALTERNATE_OBJECT_DIRECTORIES",
    "GIT_PREFIX",
    "GIT_NAMESPACE",
)


def git_umgebung(basis=None):
    """Die Umgebung ohne die Depot-Variablen.

    @param basis  Wovon ausgegangen wird; ohne Angabe `os.environ`.
    @return       Ein NEUES Woerterbuch; `os.environ` bleibt unberuehrt.
    """
    quelle = os.environ if basis is None else basis
    return {k: v for k, v in quelle.items() if k not in GIT_DEPOT_VARIABLEN}


def _selbsttest():
    """Wird von `audit_git_umgebung.py` gerufen — ein Helfer ohne Test
    ist eine Zusage, der man glaubt."""
    fehler = 0

    def pruefe(bedingung, text):
        nonlocal fehler
        if not bedingung:
            print("  FAIL " + text)
            fehler += 1

    roh = {"PATH": "/usr/bin", "GIT_DIR": "/x/.git",
           "GIT_WORK_TREE": "/x", "GIT_INDEX_FILE": "/x/.git/index",
           "GIT_COMMON_DIR": "/x/.git", "GIT_OBJECT_DIRECTORY": "/o",
           "GIT_ALTERNATE_OBJECT_DIRECTORIES": "/a", "GIT_PREFIX": "sub/",
           "GIT_NAMESPACE": "ns", "GIT_AUTHOR_NAME": "wer"}
    sauber = git_umgebung(roh)

    for v in GIT_DEPOT_VARIABLEN:
        pruefe(v not in sauber, "%s muss gestrichen sein" % v)
    pruefe(sauber.get("PATH") == "/usr/bin", "PATH bleibt erhalten")
    pruefe(sauber.get("GIT_AUTHOR_NAME") == "wer",
           "GIT_AUTHOR_NAME ist KEINE Depot-Variable und bleibt — wer sie "
           "streicht, nimmt einem Commit seinen Urheber")
    pruefe(roh.get("GIT_DIR") == "/x/.git",
           "die uebergebene Abbildung darf nicht veraendert werden")
    pruefe(len(GIT_DEPOT_VARIABLEN) == 8,
           "acht Variablen, nicht mehr und nicht weniger — gemessen %d"
           % len(GIT_DEPOT_VARIABLEN))

    print("%s (%d Abweichungen)" %
          ("FEHLGESCHLAGEN" if fehler else "OK", fehler))
    return 1 if fehler else 0


if __name__ == "__main__":
    raise SystemExit(_selbsttest())
