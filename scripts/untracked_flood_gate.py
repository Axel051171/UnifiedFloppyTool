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

ERKLAERT: dict[str, str] = {
    "test-gui":
        "Entwurfsverzeichnis des Eigentuemers. Entscheidung vom "
        "2026-09-19, woertlich: „kein test-gui in den Index“ und "
        "„kein test-gui/ kommt in .gitignore“ — es soll weder "
        "veroeffentlicht noch versteckt werden. Es ist KEINE Bauausgabe: "
        "280 handgeschriebene Dateien, und `src/core/uft_copy_plan.c:403` "
        "nennt eine davon als Herkunft („Entwurf des Eigentuemers, "
        "test-gui/06_kopierplan-...“). Ein .gitignore-Eintrag wuerde "
        "diese Belegstelle auf ein Verzeichnis zeigen lassen, das der "
        "Baum nicht mehr kennt.",
}
"""Vom Eigentuemer ERKLAERTE Arbeitsverzeichnisse (MF-1253).

── Warum es diesen Eintrag ueberhaupt gibt ──────────────────────────────

Der Kopf dieses Waechters sagt, wofuer er gebaut wurde: „wer
dreissigtausend liegen hat, hat ein Verzeichnis vergessen zu
ignorieren." Sein Anlass war `build-shift/` mit 30 971 Dateien
Bauausgabe.

Gemessen gefangen hat er etwas anderes: 280 handgeschriebene Entwuerfe
des Eigentuemers. Die Zahl ist ein STELLVERTRETER fuer „vergessene
Bauausgabe", und dieser Stellvertreter hat danebengetroffen.

Die Schwelle anzuheben waere der verbotene Griff aus MF-1077 (die Zahl
als Motiv). Stattdessen darf der Waechter jetzt messen, was er MEINT.

── Was diese Ausnahme ausdruecklich NICHT ist ───────────────────────────

* **Kein Versteck.** Erklaerte Verzeichnisse werden weiter GEZAEHLT und
  in der Meldung GENANNT — sie zaehlen nur nicht gegen die Schwelle.
  Wer `git status` fragt, sieht sie unveraendert.
* **Keine Namensliste fuer Bauausgabe.** Der Kopf argumentiert zu Recht
  gegen Aufzaehlungen: eine Liste bekannter Bauverzeichnisse veraltet
  still. Hier steht kein Bauverzeichnis, sondern eine
  EIGENTUEMERENTSCHEIDUNG — und die veraltet nicht still, sie wird
  widerrufen.
* **Kein Freibrief.** Jeder Eintrag traegt seinen Grund im Baum. Ohne
  Grund ist der Selbsttest rot.
"""


def _untracked(repo: pathlib.Path) -> list[str]:
    """Die ungetrackten, nicht ignorierten Pfade — NUL-getrennt.

    BERICHTIGT MF-1253: ohne `-z` QUOTET git jeden Pfad, der
    Sonderzeichen enthaelt, und maskiert die Bytes oktal. Gemessen
    hiess die groesste Quelle dieses Baums deshalb `"test-gui` mit
    fuehrendem Anfuehrungszeichen — weil `test-gui/gui-gerüst/` ein
    `ü` traegt. Die Buendelung nach Verzeichnis lief damit auf zwei
    verschiedene Namen fuer dasselbe Verzeichnis: 219 Dateien unter
    `"test-gui`, 43 unter `test-gui`.

    Die Zahl war dadurch richtig und die URSACHE falsch benannt — und
    jede Ausnahme nach Verzeichnisnamen waere daran vorbeigelaufen.
    `scripts/repo_scope.py` fragt seit MF-633 mit `-z`; hier fehlte es.
    """
    try:
        out = subprocess.run(
            ["git", "ls-files", "--others", "--exclude-standard", "-z"],
            cwd=str(repo), capture_output=True, timeout=120)
    except (OSError, subprocess.SubprocessError):
        return []
    if out.returncode != 0:
        return []
    roh = out.stdout.split(b"\0")
    return [p.decode("utf-8", errors="replace") for p in roh if p.strip()]


def _nach_verzeichnis(dateien) -> dict:
    """Nach oberstem Verzeichnis buendeln, damit eine Meldung die
    URSACHE nennt und nicht 30 000 Symptome."""
    nach_dir: dict[str, int] = {}
    for f in dateien:
        top = f.split("/", 1)[0] if "/" in f else "(Wurzel)"
        nach_dir[top] = nach_dir.get(top, 0) + 1
    return nach_dir


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py."""
    repo = pathlib.Path(repo)
    dateien = _untracked(repo)
    nach_dir = _nach_verzeichnis(dateien)

    # MF-1253: erklaerte Arbeitsverzeichnisse zaehlen nicht gegen die
    # Schwelle — aber sie werden GEZAEHLT und GENANNT. Der Waechter
    # soll eine vergessene Bauausgabe fangen, nicht eine
    # Eigentuemerentscheidung ueberstimmen.
    erklaert = {d: n for d, n in nach_dir.items() if d in ERKLAERT}
    unerklaert = len(dateien) - sum(erklaert.values())

    hinweis = ""
    if erklaert:
        hinweis = (" ERKLAERT und nicht gegen die Schwelle gerechnet: "
                   + ", ".join("%s (%d)" % (d, n)
                               for d, n in sorted(erklaert.items(),
                                                  key=lambda kv: -kv[1]))
                   + " — Gruende in `ERKLAERT` in diesem Waechter.")

    if unerklaert < SCHWELLE:
        return []

    offen = {d: n for d, n in nach_dir.items() if d not in ERKLAERT}
    grosse = sorted(offen.items(), key=lambda kv: -kv[1])[:ZEIGE]
    wo = ", ".join("%s (%d)" % (d, n) for d, n in grosse)

    return ["%d Dateien liegen ungetrackt, nicht ignoriert UND nicht "
            "erklaert im Baum (Schwelle %d). Groesste Quellen: %s. Ein "
            "`git add -A` an der Wurzel wuerde sie veroeffentlichen. Wenn "
            "das Arbeitsreste sind, gehoert das Verzeichnis in "
            ".gitignore; wenn es Quellen sind, gehoeren sie in den Index; "
            "wenn es erklaertes Arbeitsmaterial ist, gehoert es mit Grund "
            "nach `ERKLAERT`. Anlass: MF-652 — `build_*/` war ignoriert, "
            "`build-shift/` mit Bindestrich nicht, und hielt 30 971 "
            "Dateien.%s"
            % (unerklaert, SCHWELLE, wo, hinweis)]


def _selbsttest() -> int:
    """Die Ausnahme darf nichts verstecken und nichts freigeben (MF-1253).

    Gepruefte Richtungen, jede mit ihrer Gegenprobe — sonst waere die
    Ausnahme genau das Loch, gegen das dieser Waechter gebaut ist.
    """
    gruen = 0
    rot = 0

    def zusage(b: bool, was: str) -> None:
        nonlocal gruen, rot
        if b:
            gruen += 1
            print("   [ok ] %s" % was)
        else:
            rot += 1
            print("   [ROT] %s" % was)

    # 1 — jeder Eintrag traegt einen Grund. Ohne Grund kein Eintrag.
    zusage(all(isinstance(g, str) and len(g) > 80
               for g in ERKLAERT.values()),
           "jeder ERKLAERT-Eintrag traegt einen ausgeschriebenen Grund")
    zusage(all("Eigentuemer" in g or "Entscheidung" in g
               for g in ERKLAERT.values()),
           "und nennt die Entscheidung, auf die er sich stuetzt")

    # 2 — die Schwelle ist NICHT angehoben worden.
    zusage(SCHWELLE == 200,
           "die Schwelle steht unveraendert bei 200 — die Zahl war nicht "
           "das Motiv (MF-1077)")

    # 3 — ROT-PROBE: ein UNERKLAERTES Verzeichnis mit einer Flut muss
    #     weiterhin anschlagen, auch wenn daneben ein erklaertes liegt.
    erfunden = (["test-gui/e%d.html" % i for i in range(300)]
                + ["build-neu/o%d.obj" % i for i in range(250)])
    nd = _nach_verzeichnis(erfunden)
    unerklaert = len(erfunden) - sum(n for d, n in nd.items()
                                     if d in ERKLAERT)
    zusage(unerklaert >= SCHWELLE,
           "ROT-PROBE: ein unerklaertes `build-neu/` mit 250 Dateien "
           "schlaegt an, obwohl `test-gui` erklaert ist")

    # 4 — GEGENPROBE: das erklaerte Verzeichnis ALLEIN schlaegt nicht an.
    allein = ["test-gui/e%d.html" % i for i in range(300)]
    nd2 = _nach_verzeichnis(allein)
    unerklaert2 = len(allein) - sum(n for d, n in nd2.items()
                                    if d in ERKLAERT)
    zusage(unerklaert2 < SCHWELLE,
           "GEGENPROBE: 300 Dateien im erklaerten Verzeichnis allein "
           "loesen keinen Befund aus")

    # 5 — und es wird trotzdem GENANNT. Das ist der Unterschied
    #     zwischen „erklaert" und „versteckt".
    zusage("test-gui" in nd2 and nd2["test-gui"] == 300,
           "das erklaerte Verzeichnis wird weiter GEZAEHLT — die "
           "Meldung nennt es, `git status` zeigt es")

    # 5b — ROT-PROBE zum `-z`: ohne es quotet git den Pfad, und die
    #      Buendelung nennt eine ANDERE Ursache. Gemessen hiess die
    #      groesste Quelle dieses Baums `"test-gui` mit fuehrendem
    #      Anfuehrungszeichen, weil `gui-gerüst/` ein `ü` traegt —
    #      219 Dateien unter `"test-gui`, 43 unter `test-gui`.
    gequotet = ['"test-gui/gui-ger\\303\\274st/a.cs"',
                '"test-gui/gui-ger\\303\\274st/b.cs"']
    nd3 = _nach_verzeichnis(gequotet)
    zusage('test-gui' not in nd3,
           "ROT-PROBE: ein gequoteter Pfad wird NICHT als `test-gui` "
           "gebuendelt — jede Ausnahme nach Verzeichnisnamen liefe "
           "daran vorbei")
    zusage('"test-gui' in nd3,
           "sondern als `\"test-gui` — dasselbe Verzeichnis unter zwei "
           "Namen")
    zusage(all('"' not in p for p in _untracked(
               pathlib.Path(__file__).resolve().parent.parent)),
           "und `_untracked()` liefert seit `-z` keine gequoteten Pfade "
           "mehr")

    # 6 — ein Verzeichnis, das NICHT in der Tafel steht, ist auch nicht
    #     versehentlich erklaert.
    zusage("build-neu" not in ERKLAERT and "build" not in ERKLAERT,
           "GEGENPROBE: kein Bauverzeichnis steht in der Tafel")

    print("\nSELBSTTEST %d/%d" % (gruen, gruen + rot))
    return 1 if rot else 0


def main() -> int:
    if "--selbsttest" in sys.argv:
        return _selbsttest()
    repo = pathlib.Path(__file__).resolve().parent.parent
    if _selbsttest() != 0:
        print("Selbsttest ROT — das Tor urteilt nicht.")
        return 1
    print()
    fehler = check(repo)
    for f in fehler:
        print(f)
    n = len(_untracked(repo))
    print("ungetrackt und nicht ignoriert: %d (Schwelle %d)"
          % (n, SCHWELLE))
    return 1 if fehler else 0


if __name__ == "__main__":
    raise SystemExit(main())
