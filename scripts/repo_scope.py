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


def uebersprungen(repo: Path, pfad: Path, skip) -> bool:
    """Liegt `pfad` unterhalb eines zu ueberspringenden Verzeichnisses?

    Gemessen wird **relativ zur Repo-Wurzel** — und genau darin liegt
    der Zweck. Acht Tore trugen bis MF-1246 die Zeile

        if any(s in p.parts for s in SKIP_DIRS): continue

    und `p.parts` sind ALLE Pfadstuecke, auch die oberhalb des
    Repositoriums. Liegt ein Auscheck unter einem Verzeichnis, das
    `.claude`, `build`, `release`, `debug`, `proto` oder `.git` heisst,
    uebersprang das Tor damit JEDE Datei — und meldete danach
    Massenbefunde gegen den Baum, weil seine Grundlinien „ist keine
    Kollision mehr" sagten.

    GEMESSEN an demselben Commit `422ca4aa`, gleiche Pruefer, nur ein
    anderer Pfad:

        Auscheck unter .../.claude/jobs/...    191 Befunde
        Auscheck unter .../AppData/Local/Temp    0 Befunde
        echter `git clone` dorthin           rc=0, 0 in JEDER Kategorie

    Das ist der Spiegel von Tor 64: dort ein Test, der nicht rot werden
    KANN — hier ein Tor, das nicht gruen werden kann.

    Die MENGE der zu ueberspringenden Namen bleibt bei jedem Tor, denn
    sie unterscheidet sich begruendet (`enum_macro_conflicts` hat
    eigene Eintraege). Geteilt ist nur die REGEL, wie sie angewandt
    wird.

    @return `True` auch fuer Pfade AUSSERHALB von `repo` — was nicht im
            Baum liegt, ist nicht Sache eines Tores ueber diesen Baum.
    """
    try:
        teile = pfad.resolve().relative_to(repo.resolve()).parts
    except (ValueError, OSError):
        return True
    return any(s in teile for s in skip)


def _selbsttest() -> int:
    """Beweist, dass die Verankerung wirkt — und dass die alte Regel
    an derselben Stelle FALSCH liegt."""
    import tempfile

    gruen = 0
    rot = 0

    def zusage(bedingung: bool, text: str) -> None:
        nonlocal gruen, rot
        if bedingung:
            gruen += 1
            print("   [ok ] %s" % text)
        else:
            rot += 1
            print("   [ROT] %s" % text)

    skip = {".git", "build", "proto", ".claude", "release", "debug"}

    with tempfile.TemporaryDirectory() as td:
        # Der Auscheck liegt unter einem Verzeichnis, das eine Sperre
        # traegt — genau die Lage des Auftragsverzeichnisses
        # (`~/.claude/jobs/...`).
        wurzel = Path(td) / ".claude" / "auscheck"
        (wurzel / "src").mkdir(parents=True)
        (wurzel / "build").mkdir(parents=True)
        drin = wurzel / "src" / "x.c"
        drin.write_text("int x;\n", encoding="utf-8")
        gebaut = wurzel / "build" / "y.c"
        gebaut.write_text("int y;\n", encoding="utf-8")

        # DIE ALTE REGEL, woertlich — sie muss hier FALSCH liegen.
        alt_drin = any(s in drin.parts for s in skip)
        zusage(alt_drin,
               "ROT-PROBE: die alte Regel uebersprang `src/x.c`, "
               "weil `.claude` im Pfad oberhalb steht")

        # DIE NEUE REGEL.
        zusage(not uebersprungen(wurzel, drin, skip),
               "verankert: `src/x.c` wird GEPRUEFT")
        zusage(uebersprungen(wurzel, gebaut, skip),
               "verankert: `build/y.c` wird weiterhin uebersprungen")

        # GEGENPROBE: sie sagt nicht einfach immer nein.
        zusage(uebersprungen(wurzel, wurzel / "release" / "z.c", skip),
               "GEGENPROBE: `release/z.c` uebersprungen")
        zusage(uebersprungen(wurzel, Path(td) / "daneben.c", skip),
               "GEGENPROBE: was ausserhalb der Wurzel liegt, "
               "ist nicht Sache dieses Tores")
        zusage(not uebersprungen(wurzel, wurzel / "include" / "a.h", skip),
               "GEGENPROBE: `include/a.h` wird geprueft")

    print("\nSELBSTTEST %d/%d" % (gruen, gruen + rot))
    return 1 if rot else 0


if __name__ == "__main__":
    import sys
    if "--selbsttest" in sys.argv:
        sys.exit(_selbsttest())
    root = Path(__file__).resolve().parents[1]
    f = repo_files(root)
    if f is None:
        print("git nicht befragbar")
        sys.exit(2)
    print("Dateien im Baum (verfolgt + neu, nicht ignoriert): %d" % len(f))
    fremd = [p for p in f if "uft-scout" in p.parts and "work" in p.parts]
    print("davon unter tools/uft-scout/work/: %d (muss 0 sein)" % len(fremd))
    sys.exit(0 if not fremd else 1)
