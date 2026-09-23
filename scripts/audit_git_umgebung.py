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


# MF-1324: wie viele Dateien der letzte `sammle()`-Lauf wirklich
# angesehen hat. -1 heisst „noch nicht gelaufen".
LETZTE_MENGE = -1


def sammle(wurzel):
    """MF-1324: die Dateimenge kommt aus GIT, nicht aus `rglob`.

    Hier stand `basis.rglob("*.py")` — ein roher Dateisystembaum. Der
    nimmt alles mit, was auf der Platte liegt, auch die gitignorierten
    Fremdklone unter `tools/uft-scout/work/`. Gemessen meldete dieses Tor
    dadurch ACHT Befunde, die samt und sonders fremde Python-2-Dateien
    betreffen:

        tools/uft-scout/work/atrcopy/test_data/create_binary.py:69
        tools/uft-scout/work/OpenCBM/xu1541/bootloader/check.py:38
        ... (sechs weitere)

    „Missing parentheses in call to 'print'" ist Python 2. Diese Dateien
    gehoeren nicht zu diesem Baum, niemand baut sie, und ihr Inhalt sagt
    ueber die git-Umgebung UNSERER Haken nichts. Acht Befunde, die jeden
    Commit blockieren und die niemand beheben kann, ohne fremden Code
    anzufassen.

    Das ist woertlich der Grundsatz aus `CLAUDE.md` §MF-636: „Wer in
    einem Skript entscheidet, WELCHE Dateien geprueft werden, fragt
    `git ls-files` — nie eine hartkodierte Verzeichnisliste." Der Helfer
    dafuer liegt seit damals bereit; dieses Tor hat ihn nicht benutzt.

    Gemessen nach der Umstellung: `repo_scope.repo_files()` liefert 3478
    Dateien, davon 0 unter `tools/uft-scout/work/`.

    Faellt git aus, laesst `repo_scope` alles durch UND sagt es (P3-421).
    Dann meldet dieses Tor wieder die Fremdklone — lautstark und mit
    Hinweis, was diesem Baum lieber ist als ein stilles Loch.
    """
    import repo_scope

    # `repo_files()` liefert ABSOLUTE Pfade — gemessen
    # `WindowsPath('C:/.../src/...')`. Ein Vergleich gegen relative wuerde
    # ALLES wegwerfen und das Tor blind machen; die erste Fassung dieser
    # Aenderung tat genau das und meldete „0 Befunde", was wie Erfolg
    # aussah. Gefangen hat es die Gegenprobe im Selbsttest (MF-1324).
    erlaubt = {Path(x).resolve() for x in repo_scope.repo_files(wurzel)}

    alle = []
    gesehen = 0
    for ort in ORTE:
        basis = wurzel / ort
        if not basis.is_dir():
            continue
        for p in sorted(basis.rglob("*.py")):
            rel = p.relative_to(wurzel).as_posix()
            if p.resolve() not in erlaubt:
                continue
            gesehen += 1
            for zeile, befehl in pruefe_datei(p):
                alle.append((rel, zeile, befehl))

    # MF-1324: das Tor sagt, WIE VIEL es angesehen hat.
    #
    # „0 Befunde" heisst zweierlei: geprueft und nichts gefunden, oder gar
    # nicht geprueft. Ohne diese Zahl sind die beiden nicht zu
    # unterscheiden — und die erste Fassung des Filters war gemessen der
    # zweite Fall: sie verglich relative gegen absolute Pfade, warf alles
    # weg und meldete Erfolg. Der Selbsttest prueft die Zahl.
    global LETZTE_MENGE
    LETZTE_MENGE = gesehen
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

    # ── MF-1324: die Dateiauswahl von `sammle()` ──────────────────────
    #
    # Der Selbsttest darueber ruft `pruefe_datei()` DIREKT und sagt damit
    # nichts ueber die Auswahl. Genau dort lag der Fehler: `rglob` nahm
    # gitignorierte Fremdklone mit. Ein Tor, dessen Auswahl niemand
    # prueft, kann still das Falsche messen — und hat es acht Befunde
    # lang getan.
    #
    # Die Probe stellt beide Richtungen: eine Datei, die git NICHT kennt,
    # muss uebersprungen werden; eine, die es kennt, muss ankommen. Nur
    # die erste Haelfte zu pruefen waere die Falle aus MF-1019 („eine
    # Sicherung, die eine Klaerung vertritt, gehoert in beide Richtungen
    # gemessen").
    try:
        import repo_scope
        bekannt = {Path(x).resolve() for x in repo_scope.repo_files(WURZEL)}
        als_text = {x.as_posix() for x in bekannt}
        pruefe(len(bekannt) > 0,
               "repo_scope liefert keine Datei — dann misst sammle() nichts")
        fremd = [r for r in als_text if "uft-scout/work/" in r]
        pruefe(not fremd,
               "repo_scope fuehrt %d Datei(en) unter tools/uft-scout/work/ — "
               "dann greift der Ausschluss nicht" % len(fremd))

        # Die Gegenrichtung: das Tor selbst steht in git und muss in der
        # geprueften Menge auftauchen. Ohne sie waere „0 Befunde" auch
        # dann gruen, wenn der Filter ALLES wegwirft — und genau das ist
        # in der ersten Fassung passiert.
        selbst = (WURZEL / "scripts" / "audit_git_umgebung.py").resolve()
        pruefe(selbst in bekannt,
               "%s steht nicht in der geprueften Menge — dann wirft der "
               "Filter zu viel weg" % selbst.as_posix())
        # Und jetzt `sammle()` SELBST, nicht nur seine Zutaten.
        # Ohne diesen Lauf bleibt ein blindes Tor gruen: ein Filter,
        # der alles wegwirft, findet 0 Befunde und sieht aus wie
        # Erfolg. Gemessen ist das genau einmal passiert.
        sammle(WURZEL)
        pruefe(LETZTE_MENGE > 0,
               "sammle() hat %d Dateien angesehen — ein Tor, das"
               " nichts prueft, meldet immer 0 Befunde" % LETZTE_MENGE)
    except Exception as e:                      # pragma: no cover
        pruefe(False, "Auswahlprobe nicht durchfuehrbar: %s" % e)

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
