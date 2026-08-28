#!/usr/bin/env python3
"""Welche Dateien gehoeren zum Baum? Genau die, die CI auch sieht (MF-633).

── Warum es diese Datei gibt ────────────────────────────────────────────

`enum_macro_conflicts.py` und `extern_decl_conflicts.py` gingen ueber den
ganzen Baum und liessen eine **hartkodierte** Liste aus: `{".git",
"build", "proto", ".claude", "release", "debug", ...}`. Das ist die
Aufzaehlung bekannter Faelle, und sie ist in diesem Baum dreimal still
veraltet (MF-567, MF-578, MF-598).

Beim vierten Mal war es der Scout: `tools/uft-scout/work/` ist
gitignored, enthaelt aber geklonte **Fremd-Repos**. Nach dem Klon von
nibtools meldete das Konsistenz-Tor drei Befunde aus fremdem C-Code —
`cbm_open()` mit 3 gegen 5 Parametern in zwei OpenCBM-Headern, und
`SECTOR_OK` als Makro. Alles richtig gesehen, alles vollkommen
belanglos: **CI sieht diese Dateien nie.** Ein Tor, das lokal auf
Dateien rot wird, die nicht im Repo sind, ist schlimmer als kein Tor —
es erzieht dazu, rote Tore zu uebergehen.

── Die Regel statt der Liste ────────────────────────────────────────────

Gefragt wird git, nicht ein Verzeichnisname:

    git ls-files --cached --others --exclude-standard

Das ist genau die Menge „verfolgt ODER neu und nicht ignoriert" — also
was nach einem Commit in CI ankaeme. Eine neue, noch nicht hinzugefuegte
Quelldatei wird damit weiterhin geprueft (sonst koennte man das Tor
umgehen, indem man `git add` unterlaesst); ein ignoriertes Verzeichnis
faellt heraus, ohne dass es jemand aufzaehlen muss.

Faellt der git-Aufruf aus (kein Repo, kein git im Pfad), liefert
`repo_files()` `None`. Aufrufer muessen diesen Fall behandeln — und zwar
sichtbar, nicht durch stilles Weiterlaufen auf dem alten Weg.
"""
from __future__ import annotations

import subprocess
from pathlib import Path


def repo_files(repo: Path) -> set[Path] | None:
    """Die Dateien, die CI sehen wuerde — absolute, aufgeloeste Pfade.

    @return `None`, wenn git nicht befragt werden konnte. Nie eine
            unvollstaendige Menge ohne Hinweis: eine zu kleine Menge
            liesse ein Tor stillschweigend blind werden.
    """
    try:
        r = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             "-z"],
            cwd=str(repo), capture_output=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return None
    if r.returncode != 0:
        return None

    out: set[Path] = set()
    for raw in r.stdout.split(b"\0"):
        if not raw:
            continue
        try:
            rel = raw.decode("utf-8")
        except UnicodeDecodeError:
            rel = raw.decode("utf-8", errors="replace")
        out.add((repo / rel).resolve())
    return out


def make_filter(repo: Path):
    """Ein Praedikat `(Path) -> bool`: gehoert der Pfad zum Baum?

    Ist git nicht befragbar, laesst das Praedikat alles durch **und sagt
    es**: der Aufrufer bekommt `(filter, warnung)` und soll die Warnung
    ausgeben. Lieber ein paar Fremdbefunde mit Hinweis als eine stille
    Luecke.
    """
    files = repo_files(repo)
    if files is None:
        return (lambda p: True,
                "repo_scope: `git ls-files` nicht verfuegbar — es wird der "
                "GANZE Verzeichnisbaum geprueft, auch ignorierte Pfade")
    return (lambda p: p.resolve() in files), None


if __name__ == "__main__":
    import sys
    root = Path(__file__).resolve().parents[1]
    f = repo_files(root)
    if f is None:
        print("git nicht befragbar")
        sys.exit(2)
    print("Dateien im Baum (verfolgt + neu, nicht ignoriert): %d" % len(f))
    fremd = [p for p in f if "uft-scout" in p.parts and "work" in p.parts]
    print("davon unter tools/uft-scout/work/: %d (muss 0 sein)" % len(fremd))
    sys.exit(0 if not fremd else 1)
