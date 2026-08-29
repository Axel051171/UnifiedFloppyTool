#!/usr/bin/env python3
"""Das Zaehlwerk des Scout-Abgleichs, gegen seine eigenen Fehler (MF-681).

    python tests/test_scout_stand.py

── Warum es diesen Test gibt ────────────────────────────────────────────

Der Nenner 78/22/19/37 ist die Steuergroesse der gesamten Restarbeit am
Scout-Rueckstand: er entscheidet, was noch angefasst wird und was schon
erledigt ist. Dieses Zaehlwerk lag beim ersten Lauf **dreimal**
hintereinander falsch, und jedes Mal in eine Richtung, die Arbeit
verschwinden liess:

  1. **Namensgleichheit.** `joncampbell123/floppytools` galt als
     geklont, weil `Datamuseum-DK/FloppyTools` unter dem
     Verzeichnisnamen `FloppyTools` liegt. Zwei Projekte, ein Name.
  2. **Erwaehnung statt Begutachtung.** Ein Treffer des blossen
     Repo-Namens zaehlte als erledigt — auch wenn das fremde Gutachten
     das Repo nur als Verweis nannte.
  3. **Alteintrag ohne Konvention.** `gwnbd` meldete sich als OFFEN,
     obwohl sein Gutachten vorlag: der Eintrag entstand vor der
     Bezeichner-Konvention und hatte kein `slug`-Feld.

Alle drei sind behoben. Dieser Test stellt sie als Fixtures nach und
wird rot, sobald eine zurueckkehrt — dieselbe Vorsorge wie das
Aufzaehlungs-Tor aus MF-633.

── Was er absichtlich NICHT prueft ──────────────────────────────────────

Die Zahlen des echten Baums. Ein Test, der `78` festschreibt, waere beim
naechsten uebergebenen Repo rot, ohne dass etwas kaputt ist. Geprueft
wird das **Verfahren** an kuenstlichen Faellen, nicht der Bestand.
"""
from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(WURZEL / "scripts"))

import scout_stand as st  # noqa: E402

fehler = 0


def pruefe(bedingung, text):
    global fehler
    if not bedingung:
        print("  FAIL " + text)
        fehler += 1


def baue_baum(tmp: Path, klone: dict[str, str]) -> Path:
    """Legt work/ mit echten git-Repos an, deren origin gesetzt ist.

    Echte Repos, weil die Zuordnung `git remote get-url origin` fragt —
    ein Attrappen-Verzeichnis wuerde den Weg testen, den es nicht gibt.
    """
    work = tmp / "work"
    work.mkdir(parents=True)
    for name, url in klone.items():
        d = work / name
        d.mkdir()
        for cmd in (["git", "init", "-q"],
                    ["git", "remote", "add", "origin", url]):
            subprocess.run(cmd, cwd=str(d), capture_output=True, timeout=30)
    return work


def test_namensgleichheit():
    """Zwei Projekte, ein Name: nur das geklonte darf als geklont gelten."""
    with tempfile.TemporaryDirectory() as t:
        tmp = Path(t)
        work = baue_baum(tmp, {
            "FloppyTools": "https://github.com/Datamuseum-DK/FloppyTools.git",
        })
        klone = st.klon_slugs(work)

        datamuseum = {"name": "FloppyTools", "slug": "Datamuseum-DK/FloppyTools"}
        campbell = {"name": "floppytools", "slug": "joncampbell123/floppytools"}

        pruefe(st.status_von(datamuseum, "", {}, klone)
               == "geklont, kein Gutachten",
               "das wirklich geklonte Repo muss als geklont gelten")
        pruefe(st.status_von(campbell, "", {}, klone) == "OFFEN",
               "ein gleichnamiges FREMDES Repo darf NICHT als geklont "
               "gelten — genau dieser Fehler liess Arbeit verschwinden")


def test_erwaehnung_ist_keine_begutachtung():
    """Ein Verweis im fremden Gutachten ist kein Urteil."""
    texte = ("Zum Vergleich siehe WinUAE, das denselben Weg geht. "
             "Gegenstand dieses Zyklus ist excess-c64/lib1541img.")
    erwaehnt = {"name": "WinUAE", "slug": "tonioni/WinUAE"}
    gegenstand = {"name": "lib1541img", "slug": "excess-c64/lib1541img"}

    pruefe(st.status_von(gegenstand, texte, {}, set()) == "begutachtet",
           "der volle Bezeichner im Gutachten heisst begutachtet")
    pruefe(st.status_von(erwaehnt, texte, {}, set())
           == "nur erwaehnt (pruefen)",
           "ein blosser Namenstreffer ist eine FRAGE, kein Urteil")


def test_alteintrag_ohne_bezeichner():
    """Ohne `slug` kann ein Eintrag kein Urteil tragen — und das muss
    auffallen, statt als OFFEN durchzugehen und Arbeit zu erfinden."""
    texte = "Gegenstand: N0t4R0b0t/gwnbd, EUPL-1.2."
    ohne = {"name": "gwnbd"}
    mit = {"name": "gwnbd", "slug": "N0t4R0b0t/gwnbd"}

    pruefe(st.status_von(mit, texte, {}, set()) == "begutachtet",
           "mit Bezeichner wird das vorhandene Gutachten gefunden")
    pruefe(st.status_von(ohne, texte, {}, set()) != "begutachtet",
           "ohne Bezeichner darf NICHT geraten werden")


def test_negativliste_zaehlt():
    """Frueher bewertete Repos sind nicht offen."""
    neg = {"lipro-cpm4l/cpmtools": {"status": "bewertet", "grund": "x"}}
    e = {"name": "cpmtools", "slug": "lipro-cpm4l/cpmtools"}
    pruefe(st.status_von(e, "", neg, set()) == "frueher bewertet",
           "ein Eintrag der Negativliste ist nicht offen")


def test_unbekanntes_ist_offen():
    """Der Normalfall: nichts spricht dafuer, also offen."""
    e = {"name": "irgendwas", "slug": "jemand/irgendwas"}
    pruefe(st.status_von(e, "andere Texte", {}, set()) == "OFFEN",
           "ohne jeden Beleg ist ein Auftrag offen")


def test_auftragsliste_ist_lesbar():
    """Die echte Datei muss gueltiges JSON sein und Bezeichner tragen.

    Das ist die einzige Pruefung am echten Bestand — sie schreibt keine
    Zahl fest, sondern nur, dass die Konvention eingehalten ist. Ohne
    `slug` traegt ein Eintrag kein Urteil (Fehler 3 oben).
    """
    p = WURZEL / "tools" / "uft-scout" / "data" / "auftraege.json"
    if not p.is_file():
        print("  uebersprungen: auftraege.json fehlt")
        return
    d = json.loads(p.read_text(encoding="utf-8"))
    ohne = [e.get("name") for e in d.get("eintraege", []) if not e.get("slug")]
    pruefe(not ohne,
           "diese Auftraege haben keinen Bezeichner und koennen darum "
           "kein Urteil tragen: %s" % ", ".join(map(str, ohne)))


def main() -> int:
    print("Scout-Zaehlwerk gegen seine eigenen drei Fehler (MF-681)\n")
    for f in (test_namensgleichheit,
              test_erwaehnung_ist_keine_begutachtung,
              test_alteintrag_ohne_bezeichner,
              test_negativliste_zaehlt,
              test_unbekanntes_ist_offen,
              test_auftragsliste_ist_lesbar):
        vorher = fehler
        f()
        print("  %-4s %s" % ("ok" if fehler == vorher else "FAIL",
                             f.__name__))
    print("\n%s (%d Abweichungen)" %
          ("FEHLGESCHLAGEN" if fehler else "OK", fehler))
    return 1 if fehler else 0


if __name__ == "__main__":
    raise SystemExit(main())
