#!/usr/bin/env python3
"""Tor: `git init` nur mit bereinigter Umgebung (MF-1282).

    python scripts/audit_git_umgebung.py            # Befunde, rc 1 wenn welche
    python scripts/audit_git_umgebung.py --selftest # nur der Selbsttest

── WAS DAS TOR HAELT ────────────────────────────────────────────────────

Git exportiert `GIT_DIR` in jeden Haken. Ein Unterprozess, der danach
`git init` gegen ein Pruefverzeichnis ruft, richtet damit nicht dieses
Verzeichnis ein, sondern das Depot, auf das `GIT_DIR` zeigt — und zeigt
es auf einen NEBENBAUM, setzt git dabei `core.bare = true` in die
gemeinsame Konfiguration. Der Hauptbaum ist danach fuer git nicht mehr
vorhanden. Begruendung und Sandkasten-Nachstellung: `scripts/git_env.py`.

── WARUM NUR `git init` UND NICHT JEDER GIT-AUFRUF ──────────────────────

Das ist gemessen und nicht bequem gewaehlt. Lesende Aufrufe wie
`git ls-files` oder `git show` laufen in diesem Baum mit `cwd` auf der
Baumwurzel — dort ist das geerbte `GIT_DIR` genau das GEWOLLTE Depot,
eine Bereinigung waere dort sogar falsch, weil sie in einem Nebenbaum
das falsche Depot treffen koennte.

Gefaehrlich ist git gegen ein FREMDES Verzeichnis, und in diesem Baum
ist das ausnahmslos `git init` in einem Pruefverzeichnis. Wer spaeter
einen anderen schreibenden Aufruf gegen ein Fremdverzeichnis baut, traegt
ihn hier nach — die Liste steht in `SCHREIBENDE`.

── WARUM `ast` UND NICHT `grep` ─────────────────────────────────────────

Ein zeilenweiser Filter hat bei der ersten Messung dieses Befundes
`gen_familien.py` FALSCH beurteilt: dort steht `env=u` auf der dritten
Zeile des Aufrufs, und `grep -v "env="` sah nur die erste. Aus 11
Fundstellen wurden so 12. Ein Tor, das seine eigene Zahl falsch zaehlt,
ist schlimmer als keines (MF-1163).
"""
from __future__ import annotations

import argparse
import ast
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

#: Git-Unterbefehle, die ein Depot VERAENDERN und deshalb nie ohne
#: bereinigte Umgebung gegen ein Fremdverzeichnis laufen duerfen.
SCHREIBENDE = ("init",)

#: Verzeichnisse, in denen gesucht wird — REPO-RELATIV geprueft, nicht
#: ueber die absoluten Pfadbestandteile. Genau daran ist
#: `check_consistency.py::check_test_lib_targets()` gescheitert, das
#: jeden Pfad verwirft, dessen ABSOLUTE Bestandteile `.claude` enthalten
#: (P3-518) — eine Auscheckung unter so einem Verzeichnis verliert damit
#: alles.
ORTE = ("scripts", "tests", "tools")


def _ist_git_aufruf(knoten):
    """Ruft dieser Knoten ein Unterprozess-Werkzeug mit git als Programm?

    @return die Befehlswoerter als Liste, sonst None.
    """
    if not isinstance(knoten, ast.Call):
        return None
    ziel = knoten.func
    name = None
    if isinstance(ziel, ast.Attribute):
        name = ziel.attr
    elif isinstance(ziel, ast.Name):
        name = ziel.id
    if name not in ("run", "check_output", "Popen", "call", "check_call"):
        return None
    if not knoten.args:
        return None
    erstes = knoten.args[0]
    if not isinstance(erstes, (ast.List, ast.Tuple)):
        return None
    woerter = []
    for e in erstes.elts:
        if isinstance(e, ast.Constant) and isinstance(e.value, str):
            woerter.append(e.value)
        else:
            woerter.append(None)   # berechnet — Platzhalter
    if not woerter or woerter[0] != "git":
        return None
    return woerter


def _hat_env(knoten):
    return any(k.arg == "env" for k in knoten.keywords)


def pruefe_datei(pfad):
    """@return Liste von (zeile, befehl) ohne bereinigte Umgebung.

    Eine Datei, die sich nicht zerlegen laesst, ist ein BEFUND und kein
    Grund zu schweigen. Das ist am Bau dieses Tores gemessen: ein
    fehlerhafter Masseneingriff hat acht Skripte mit `IndentationError`
    hinterlassen, und die erste Fassung meldete daraufhin „0 Befunde" —
    gruen, WEIL die Dateien kaputt waren. Das ist die Klasse
    MF-1000/Tor 64: ein Tor, das nicht rot werden kann.
    """
    try:
        text = pfad.read_text(encoding="utf-8")
    except OSError as e:
        return [(0, "NICHT LESBAR: %s" % e)]
    try:
        baum = ast.parse(text, filename=str(pfad))
    except SyntaxError as e:
        return [(e.lineno or 0, "NICHT ZERLEGBAR: %s" % e.msg)]
    befunde = []
    for knoten in ast.walk(baum):
        woerter = _ist_git_aufruf(knoten)
        if woerter is None:
            continue
        if not any(w in SCHREIBENDE for w in woerter if w):
            continue
        if _hat_env(knoten):
            continue
        befunde.append((knoten.lineno,
                        " ".join(w if w else "<berechnet>" for w in woerter)))
    return befunde


def sammle(wurzel):
    alle = []
    for ort in ORTE:
        basis = wurzel / ort
        if not basis.is_dir():
            continue
        for p in sorted(basis.rglob("*.py")):
            rel = p.relative_to(wurzel).as_posix()
            for zeile, befehl in pruefe_datei(p):
                alle.append((rel, zeile, befehl))
    return alle


def selbsttest():
    """Das Tor gegen seine eigenen Fehler."""
    import tempfile
    fehler = 0

    def pruefe(bedingung, text):
        nonlocal fehler
        if not bedingung:
            print("  FAIL " + text)
            fehler += 1

    faelle = [
        ('subprocess.run(["git", "init", "-q"], cwd=d)', 1,
         "ein nackter git-init-Aufruf MUSS auffallen"),
        ('subprocess.run(["git", "init", "-q"], cwd=d, env=u)', 0,
         "mit env= ist er in Ordnung"),
        ('subprocess.run(["git", "init", "-q"],\n'
         '               cwd=d,\n'
         '               env=u)', 0,
         "env= auf einer SPAETEREN Zeile zaehlt auch — genau hier hat ein "
         "zeilenweiser Filter falsch gezaehlt"),
        ('subprocess.run(["git", "ls-files"], cwd=w)', 0,
         "lesende Aufrufe sind nicht betroffen"),
        ('subprocess.run(["git", "status"], cwd=w)', 0,
         "auch sonst kein schreibender Unterbefehl"),
        ('subprocess.run([werkzeug, "init"], cwd=d)', 0,
         "ein berechnetes Programm ist nicht als git erkennbar — das Tor "
         "erfindet dafuer keinen Befund"),
        ('subprocess.run(["git", "init"], cwd=d, capture_output=True,\n'
         '               timeout=60)', 1,
         "viele Schluesselwoerter, aber kein env="),
        ('def f():\n'
         '    pass\n'
         '        import os', 1,
         "eine NICHT ZERLEGBARE Datei ist ein Befund — schweigen hiesse "
         "gruen werden, weil sie kaputt ist"),
    ]

    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        (tmp / "scripts").mkdir()
        for i, (quelle, erwartet, text) in enumerate(faelle):
            p = tmp / "scripts" / ("f%d.py" % i)
            p.write_text("import subprocess\nd=w=u=werkzeug=None\n" + quelle,
                         encoding="utf-8")
            n = len(pruefe_datei(p))
            pruefe(n == erwartet,
                   "%s — erwartet %d, gemessen %d" % (text, erwartet, n))

    import git_env
    pruefe(git_env._selbsttest() == 0, "git_env.py Selbsttest")

    print("Selbsttest: %s (%d Abweichungen)" %
          ("FEHLGESCHLAGEN" if fehler else "OK", fehler))
    return 1 if fehler else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()

    if selbsttest() != 0:
        return 1
    if a.selftest:
        return 0

    befunde = sammle(WURZEL)
    print("git init ohne bereinigte Umgebung: %d" % len(befunde))
    for rel, zeile, befehl in befunde:
        print("  %s:%d  %s" % (rel, zeile, befehl))
    if befunde:
        print("\nAbhilfe: `from git_env import git_umgebung` und "
              "`env=git_umgebung()` an den Aufruf.")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
