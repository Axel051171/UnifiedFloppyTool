#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""sichtprotokoll.py — die Brandmauer als Vorrichtung statt als Zusage
(MF-740)

`kontamination.py` prueft NACH dem Bau, ob Ausdruecke der Vorlage im
Neubau stehen. Das ist ein Nachweis am Ergebnis. Was fehlte, ist der
Nachweis am **Vorgang**: welche Dateien hat welche Hand ueberhaupt
geoeffnet?

Bisher war die Antwort darauf eine Zusage. Zusagen ohne Beleg sind in
diesem Baum die Ausgangslage jedes Befundes gewesen — die fabrizierten
Parser waren gruen, der Kopierschutz-Katalog war „unterstuetzt", Tor 34
war eine Kopie von Tor 33. Eine Brandmauer, die nur behauptet wird, ist
dieselbe Klasse.

── Die zwei Haelften, und welche mechanisch ist ─────────────────────────

**Mechanisch: der Arbeitsbaum.** Hand B arbeitet in einem Baum, in dem
die Vorlage **nicht liegt**. Das laesst sich pruefen — nach Pfad UND
nach SHA-256, damit eine Umbenennung nicht durchrutscht. Wer nicht
lesen kann, was nicht da ist, ist nicht kontaminiert.

**Deklarativ: das Protokoll.** Welche Dateien eine Hand geoeffnet hat,
kann dieses Werkzeug nicht messen — es sitzt nicht im Agenten. Es
protokolliert, was gemeldet wird, und prueft die Meldung gegen die
eingefrorene Vorlagenmenge. Ein Lauf B, der eine Vorlagendatei meldet,
ist rot; ein Lauf B, der schweigt, ist unbelegt.

**Diese Unterscheidung steht hier ausdruecklich, weil das Werkzeug
sonst mehr verspraeche, als es haelt.** Die mechanische Haelfte ist die
tragende: sie kommt ohne Ehrlichkeit des Gemeldeten aus.

── Der Ablauf ───────────────────────────────────────────────────────────

    1. einfrieren     Vorlage nach Pfad + SHA-256 festhalten
    2. protokoll A    Hand A liest die Vorlage und schreibt NUR eine
                      Verhaltensbeschreibung
    3. arbeitsbaum    einen Baum ohne die Vorlage nachweisen
    4. protokoll B    Hand B meldet, was sie geoeffnet hat
    5. pruefen        B-Protokoll gegen die Vorlagenmenge

Aufruf:
    sichtprotokoll.py --selbsttest
    sichtprotokoll.py --einfrieren <datei>...
    sichtprotokoll.py --protokoll A --dateien <datei>...
    sichtprotokoll.py --arbeitsbaum <pfad>
    sichtprotokoll.py --pruefen
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import sys
import tempfile
from pathlib import Path

WURZEL = Path(__file__).resolve().parents[3]
ARBEIT = Path(__file__).resolve().parents[1] / "work"
VORLAGE_JSON = ARBEIT / "vorlage.json"
PROTOKOLL_JSON = ARBEIT / "sichtprotokoll.json"


def sha(p: Path) -> str:
    h = hashlib.sha256()
    h.update(p.read_bytes())
    return h.hexdigest()


def _lade(p: Path, vorgabe):
    if not p.is_file():
        return vorgabe
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return vorgabe


def einfrieren(dateien: list[Path], ziel: Path = VORLAGE_JSON,
               basis: Path | None = None) -> dict:
    """Die Vorlage nach Pfad UND Inhalt festhalten.

    `basis` ist einspeisbar, damit der Selbsttest nicht vom Zustand des
    Arbeitsbaums abhaengt — dieselbe Vorkehrung wie `diff_fn` in
    `konvergenz.py`. Ohne sie fiel der Test auf einen absoluten Pfad
    zurueck und kopierte eine Datei auf sich selbst.
    """
    wurzel = Path(basis) if basis else WURZEL
    eintraege = {}
    for d in dateien:
        p = Path(d)
        if not p.is_file():
            raise FileNotFoundError(str(p))
        try:
            rel = p.resolve().relative_to(wurzel.resolve()).as_posix()
        except ValueError:
            rel = p.resolve().as_posix()
        eintraege[rel] = sha(p)
    ziel.parent.mkdir(parents=True, exist_ok=True)
    ziel.write_text(json.dumps({"vorlage": eintraege}, indent=2,
                               ensure_ascii=False), encoding="utf-8")
    return eintraege


def protokollieren(lauf: str, dateien: list[str],
                   ziel: Path = PROTOKOLL_JSON) -> dict:
    """Melden, was eine Hand geoeffnet hat. Deklarativ, nicht gemessen."""
    d = _lade(ziel, {})
    d.setdefault(lauf, [])
    for f in dateien:
        if f not in d[lauf]:
            d[lauf].append(f)
    ziel.parent.mkdir(parents=True, exist_ok=True)
    ziel.write_text(json.dumps(d, indent=2, ensure_ascii=False),
                    encoding="utf-8")
    return d


def arbeitsbaum_sauber(baum: Path,
                       vorlage: dict[str, str]) -> list[str]:
    """MECHANISCH: liegt eine Vorlagendatei in diesem Baum?

    Zwei Wege, weil einer allein zu umgehen ist:
      * nach PFAD — faengt die unveraenderte Datei
      * nach SHA-256 ueber den ganzen Baum — faengt die umbenannte
    """
    befunde = []
    hashes = {v: k for k, v in vorlage.items()}
    for rel in vorlage:
        if (baum / rel).is_file():
            befunde.append("Vorlage liegt im Arbeitsbaum: %s" % rel)
    for p in sorted(baum.rglob("*")):
        if not p.is_file():
            continue
        try:
            h = sha(p)
        except OSError:
            continue
        if h in hashes:
            r = p.relative_to(baum).as_posix()
            if r not in vorlage:
                befunde.append(
                    "Vorlage UMBENANNT im Arbeitsbaum: %s ist inhaltlich "
                    "%s" % (r, hashes[h]))
    return befunde


def pruefen(vorlage: dict[str, str], protokoll: dict) -> list[str]:
    """DEKLARATIV: hat Hand B eine Vorlagendatei gemeldet?"""
    befunde = []
    b = protokoll.get("B", [])
    if not b:
        befunde.append(
            "Lauf B hat nichts protokolliert — die Trennung ist damit "
            "unbelegt, nicht bestanden. Ein leeres Protokoll ist kein "
            "sauberes Protokoll.")
    for f in b:
        if f in vorlage:
            befunde.append(
                "Lauf B meldet eine VORLAGENDATEI: %s — die Brandmauer "
                "ist gebrochen, der Nachbau ist keiner." % f)
    if not protokoll.get("A"):
        befunde.append(
            "Lauf A hat nichts protokolliert — dann gibt es keine "
            "Verhaltensbeschreibung, aus der Hand B arbeiten koennte.")
    return befunde


# ── Selbsttest ──────────────────────────────────────────────────────────

def selbsttest() -> int:
    tmp = Path(tempfile.mkdtemp(prefix="uft_sicht_"))
    gut = 0
    faelle = []
    try:
        quelle = tmp / "quelle"
        (quelle / "src").mkdir(parents=True)
        v = quelle / "src" / "vorlage.c"
        v.write_text("int geheim(void) { return 42; }\n", encoding="utf-8")
        vj = tmp / "vorlage.json"
        eintraege = einfrieren([v], ziel=vj, basis=quelle)
        faelle.append(("Vorlage eingefroren",
                       list(eintraege) == ["src/vorlage.c"]))

        # 1 · sauberer Arbeitsbaum
        baum = tmp / "sauber"
        (baum / "src").mkdir(parents=True)
        (baum / "src" / "neu.c").write_text("int f(void){return 0;}\n",
                                            encoding="utf-8")
        faelle.append(("sauberer Baum -> keine Befunde",
                       arbeitsbaum_sauber(baum, eintraege) == []))

        # 2 · Vorlage liegt drin, gleicher Name
        schmutz = tmp / "schmutz"
        rel = list(eintraege)[0]
        (schmutz / rel).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy(v, schmutz / rel)
        faelle.append(("Vorlage im Baum -> Befund",
                       arbeitsbaum_sauber(schmutz, eintraege) != []))

        # 3 · Vorlage UMBENANNT — der Fall, den ein Pfadvergleich
        #     allein durchlaesst. Genau dafuer der SHA-Weg.
        getarnt = tmp / "getarnt"
        getarnt.mkdir()
        shutil.copy(v, getarnt / "harmlos.c")
        b3 = arbeitsbaum_sauber(getarnt, eintraege)
        faelle.append(("umbenannte Vorlage -> Befund",
                       any("UMBENANNT" in x for x in b3)))

        # 4 · Lauf B meldet eine Vorlagendatei
        faelle.append(("B meldet Vorlage -> Befund",
                       any("Brandmauer" in x for x in
                           pruefen(eintraege,
                                   {"A": ["egal"], "B": [rel]}))))

        # 5 · Lauf B schweigt -> UNBELEGT, nicht bestanden
        faelle.append(("B schweigt -> unbelegt",
                       any("unbelegt" in x for x in
                           pruefen(eintraege, {"A": ["egal"], "B": []}))))

        # 6 · sauberer Durchgang
        faelle.append(("A und B sauber -> keine Befunde",
                       pruefen(eintraege,
                               {"A": [rel], "B": ["spec.md"]}) == []))

        # 7 · Lauf A fehlt -> Befund
        faelle.append(("A fehlt -> Befund",
                       any("Lauf A" in x for x in
                           pruefen(eintraege, {"B": ["spec.md"]}))))

        # 8 · geaenderte Vorlage faellt NICHT unter den SHA-Weg —
        #     das ist beabsichtigt und muss sichtbar sein: der
        #     Pfadvergleich traegt ihn.
        geaendert = tmp / "geaendert"
        geaendert.mkdir()
        (geaendert / rel).parent.mkdir(parents=True, exist_ok=True)
        (geaendert / rel).write_text("int geheim(void) { return 43; }\n",
                                     encoding="utf-8")
        faelle.append(("geaenderte Vorlage am PFAD -> Befund",
                       arbeitsbaum_sauber(geaendert, eintraege) != []))
    finally:
        shutil.rmtree(tmp, ignore_errors=True)

    for name, ok in faelle:
        print("  %s %s" % ("ok " if ok else "ROT", name))
        gut += bool(ok)
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--einfrieren", nargs="+", metavar="DATEI")
    ap.add_argument("--protokoll", choices=("A", "B"))
    ap.add_argument("--dateien", nargs="*", default=[])
    ap.add_argument("--arbeitsbaum", metavar="PFAD")
    ap.add_argument("--pruefen", action="store_true")
    a = ap.parse_args()

    if a.selbsttest:
        return selbsttest()

    if a.einfrieren:
        e = einfrieren([Path(x) for x in a.einfrieren])
        print("Vorlage eingefroren: %d Datei(en)" % len(e))
        for k, v in e.items():
            print("  %s  %s" % (v[:12], k))
        return 0

    if a.protokoll:
        d = protokollieren(a.protokoll, a.dateien)
        print("Lauf %s: %d Datei(en) protokolliert"
              % (a.protokoll, len(d[a.protokoll])))
        return 0

    vorlage = _lade(VORLAGE_JSON, {}).get("vorlage", {})
    if not vorlage:
        print("Keine eingefrorene Vorlage — erst `--einfrieren`.",
              file=sys.stderr)
        return 1

    befunde: list[str] = []
    if a.arbeitsbaum:
        befunde += arbeitsbaum_sauber(Path(a.arbeitsbaum), vorlage)
    if a.pruefen:
        befunde += pruefen(vorlage, _lade(PROTOKOLL_JSON, {}))
    if not (a.arbeitsbaum or a.pruefen):
        ap.print_help()
        return 0

    print("Befunde: %d" % len(befunde))
    for b in befunde:
        print("  " + b)
    return 1 if befunde else 0


if __name__ == "__main__":
    sys.exit(main())
