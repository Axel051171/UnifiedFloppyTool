#!/usr/bin/env python3
"""konvergenz.py — Schiedsrichter fuer Werkzeug-Schleifen (MF-734).

    python tools/uft-innendienst/scripts/konvergenz.py \\
        --werkzeug scripts/mein_zensus.py \\
        --ziel     'python scripts/mein_zensus.py --selbsttest' \\
        --fixtures tools/uft-innendienst/fixtures/zensus \\
        --runden   6
    python tools/uft-innendienst/scripts/konvergenz.py --selbsttest

── Was das ist, und was es NICHT ist ────────────────────────────────────

Es faehrt die Schleife **nicht**. Es ist der **Schiedsrichter**: es
laesst eine Runde laufen, prueft die Zielbedingung, haelt fest was
geschah, und bricht ab, wenn eine der vier Regeln verletzt wird. Die
Aenderung am Werkzeug macht der Agent zwischen den Runden — denn genau
dort sitzt das Urteil, und Urteile gehoeren nicht in eine Schleife.

Wer das umdreht und den Loop seine eigene Aenderung waehlen laesst,
baut einen Auto-Fixer. Dieser Baum weiss, wohin das fuehrt: fuenf
Parser gegen erfundene Spezifikationen (FMT-2/3/10/11/12), elf Plugins
an einem Tag fuer „0 active stubs remaining" (MF-730).

── Die vier Regeln, jede aus einem Fehllauf dieses Baums ────────────────

**1 · Das Kriterium gehoert nicht der Schleife.** Fixtures und
Zielbedingung sind Eingang, nicht Ausgang. Mechanisch geprueft: SHA-256
jeder Fixture vor und nach jeder Runde. Aendert sich eine, bricht der
Lauf mit `VERFAHRENSFEHLER` ab.

Der Anlass ist frisch. Bei FMT-17 (MF-724) wurde der Rotbeweis rot
(`55/55 statt 60/55`); der billigste Zug waere gewesen, die Erwartung
anzupassen. Richtig war, **drei Zusicherungen umzukehren**, weil sie die
alte, falsche Lage festhielten. Eine Schleife mit Ziel „gruen" hat genau
einen billigen Zug: das Ziel aendern.

**2 · Eine Aenderung je Runde.** Geprueft ueber `git diff --name-only`:
beruehrt eine Runde mehr als eine Datei, bricht der Lauf ab. Zwei
Aenderungen gleichzeitig, und niemand weiss mehr, welche gewirkt hat —
und das Protokoll, das den Wert des Laufs ausmacht, wird wertlos.

**3 · Nur Werkzeuge, nie Produktcode.** Das Werkzeug darf nicht unter
`src/`, `include/` oder `tests/` liegen. Zensen, Pruefskripte,
Weisslisten, Eichlaeufe — Dinge, deren Fehler in ihren eigenen Zahlen
sichtbar werden. An einem Decoder haette die Schleife keine externe
Wahrheit, sondern nur den Test, den sie gerade gruen machen will.

**4 · Stagnation ist ein Ergebnis.** Zwei Runden ohne Verbesserung
heisst: mechanisch nicht loesbar, ein Mensch muss die Frage schaerfen.
Das Protokoll geht dann als Vorlage raus — mit allem, was versucht
wurde.

── Und eine fuenfte, die aus dieser Sitzung stammt ──────────────────────

**5 · Das Ziel darf nicht „mehr" heissen, sondern nur „belegt".** Der
5-and-3-Dekoder lieferte **454 von 455** Sektoren byteidentisch
(MF-721). Eine Schleife mit Ziel „alle 455" haette den letzten so lange
bearbeitet, bis er passt — und richtig war, ihn **nicht** zu dekodieren:
DOS 3.2 schreibt dort eine andere Kodierung, und die Pruefsumme trennt
die beiden nicht. Ein gruener Sektor waere erfundene Daten gewesen.

Darum prueft dieses Werkzeug die Zielbedingung als **Kommando mit
Rueckgabewert**, nicht als Zahl, die es selbst vergleicht. Wer „mehr"
als Ziel formuliert, muss das in seinem eigenen Selbsttest tun — und
dort faellt es auf.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import subprocess
import sys
import time

HIER = os.path.dirname(os.path.abspath(__file__))
WURZEL = os.path.normpath(os.path.join(HIER, "..", "..", ".."))

VERBOTEN = ("src/", "include/", "tests/")


class Verfahrensfehler(Exception):
    """Eine der vier Regeln wurde verletzt — der Lauf ist ungueltig."""


def _sha(pfad: str) -> str:
    h = hashlib.sha256()
    with open(pfad, "rb") as f:
        for block in iter(lambda: f.read(65536), b""):
            h.update(block)
    return h.hexdigest()


def fixture_stand(pfade: list[str]) -> dict[str, str]:
    """SHA-256 jeder Fixture. Verzeichnisse werden aufgeklappt."""
    stand: dict[str, str] = {}
    for p in pfade:
        if os.path.isdir(p):
            for wurzel, _, dateien in os.walk(p):
                for d in sorted(dateien):
                    voll = os.path.join(wurzel, d)
                    stand[os.path.relpath(voll, WURZEL)] = _sha(voll)
        elif os.path.isfile(p):
            stand[os.path.relpath(p, WURZEL)] = _sha(p)
    return stand


def pruefe_werkzeug(pfad: str) -> None:
    """Regel 3 — kein Produktcode."""
    rel = os.path.relpath(os.path.abspath(pfad), WURZEL).replace("\\", "/")
    for v in VERBOTEN:
        if rel.startswith(v):
            raise Verfahrensfehler(
                f"Regel 3: `{rel}` liegt unter `{v}` — die Schleife faehrt "
                f"nur Werkzeuge. An Produktcode haette sie keine externe "
                f"Wahrheit, sondern nur den Test, den sie gruen machen will.")


def geaenderte_dateien() -> list[str]:
    r = subprocess.run(["git", "diff", "--name-only"], cwd=WURZEL,
                       capture_output=True, text=True)
    if r.returncode != 0:
        return []
    return [z for z in r.stdout.split() if z]


def ziel_erreicht(kommando: str) -> tuple[bool, str]:
    r = subprocess.run(kommando, shell=True, cwd=WURZEL,
                       capture_output=True, text=True, timeout=900)
    ausgabe = (r.stdout or "") + (r.stderr or "")
    return r.returncode == 0, ausgabe.strip()


def fahre(werkzeug: str, ziel: str, fixtures: list[str],
          runden: int = 6, kennzahl: str | None = None,
          diff_fn=None) -> dict:
    """Eine Runde je Aufruf des Agenten — dieses Werkzeug haelt fest.

    Rueckgabe ist das Protokoll; der Aufrufer entscheidet, was er
    zwischen den Runden aendert.
    """
    pruefe_werkzeug(werkzeug)
    vorher = fixture_stand(fixtures)
    if not vorher:
        raise Verfahrensfehler(
            "Regel 1: keine Fixtures gefunden. Ohne Eingang, der der "
            "Schleife NICHT gehoert, gibt es kein Kriterium — nur eine "
            "Zielvorgabe.")

    # Regel 2 misst den Arbeitsverzeichnis-Diff. Der Aufrufer braucht
    # dafuer einen sauberen Baum — unfertige Aenderungen an anderer
    # Stelle wuerden als „zwei Aenderungen" gelesen. Der Selbsttest
    # reicht darum eine eigene Funktion herein, sonst haenge er vom
    # Zufall des Arbeitsstands ab (gemessen: er tat es).
    diff = diff_fn or geaenderte_dateien

    protokoll: list[dict] = []
    ohne_fortschritt = 0
    letzte_zahl: int | None = None

    for runde in range(1, runden + 1):
        beruehrt = diff()
        gruen, ausgabe = ziel_erreicht(ziel)

        # Regel 1 — Fixtures unveraendert?
        nachher = fixture_stand(fixtures)
        if nachher != vorher:
            geaendert = sorted(k for k in set(vorher) | set(nachher)
                               if vorher.get(k) != nachher.get(k))
            raise Verfahrensfehler(
                "Regel 1: Fixture(s) veraendert waehrend des Laufs — "
                + ", ".join(geaendert)
                + ". Wer sein Kriterium anfassen darf, erfuellt es durch "
                  "Abschwaechen. Der Lauf ist ungueltig.")

        zahl = None
        if kennzahl:
            for wort in ausgabe.replace(":", " ").split():
                if wort.isdigit():
                    zahl = int(wort)
                    break

        protokoll.append({
            "runde": runde,
            "gruen": gruen,
            "beruehrte_dateien": beruehrt,
            "zahl": zahl,
            "ausgabe": ausgabe[-600:],
        })

        if gruen:
            return {"ergebnis": "ZIEL ERREICHT", "runden": runde,
                    "protokoll": protokoll}

        # Regel 2 — eine Aenderung je Runde
        if len(beruehrt) > 1:
            raise Verfahrensfehler(
                f"Regel 2: Runde {runde} beruehrt {len(beruehrt)} Dateien "
                f"({', '.join(beruehrt[:4])}). Zwei Aenderungen gleichzeitig, "
                f"und niemand weiss mehr, welche gewirkt hat — das Protokoll "
                f"wird wertlos.")

        # Regel 4 — Stagnation
        if zahl is not None and letzte_zahl is not None and zahl >= letzte_zahl:
            ohne_fortschritt += 1
            if ohne_fortschritt >= 2:
                return {"ergebnis": "STAGNATION", "runden": runde,
                        "protokoll": protokoll,
                        "hinweis": "Zwei Runden ohne Verbesserung. Das "
                                   "Problem ist mechanisch nicht loesbar — "
                                   "ein Mensch muss die Frage schaerfen. "
                                   "Das Protokoll ist die Vorlage."}
        else:
            ohne_fortschritt = 0
        if zahl is not None:
            letzte_zahl = zahl

        break   # eine Runde je Aufruf; der Agent aendert und ruft erneut

    return {"ergebnis": "RUNDE GEFAHREN", "runden": len(protokoll),
            "protokoll": protokoll}


# ── Selbsttest: vor dem Nenner, nicht danach ────────────────────────────

def selbsttest() -> int:
    """Gepflanzte Faelle mit feststehender Antwort.

    Ein Schiedsrichter, der sich selbst bestaetigt, pfeift nicht
    (MF-693, MF-710, MF-718).
    """
    import tempfile
    fehler: list[str] = []

    with tempfile.TemporaryDirectory() as d:
        fix = os.path.join(d, "fixture.txt")
        with open(fix, "w", encoding="utf-8") as f:
            f.write("unantastbar\n")

        # 1 · Regel 3: Produktcode wird abgewiesen
        for pfad in ("src/formats/do/uft_do.c", "include/uft/x.h",
                     "tests/test_x.c"):
            try:
                pruefe_werkzeug(os.path.join(WURZEL, pfad))
                fehler.append(f"Regel 3 laesst {pfad} durch")
            except Verfahrensfehler:
                pass

        # … und ein Werkzeug wird angenommen
        try:
            pruefe_werkzeug(os.path.join(WURZEL, "scripts", "gen_stand.py"))
        except Verfahrensfehler:
            fehler.append("Regel 3 weist ein Werkzeug ab")

        # 2 · Regel 1: ohne Fixtures kein Lauf
        try:
            fahre("scripts/gen_stand.py", "exit 1", [], runden=1,
                  diff_fn=lambda: [])
            fehler.append("Regel 1 laesst einen Lauf ohne Fixtures zu")
        except Verfahrensfehler:
            pass

        # 3 · Regel 1: veraenderte Fixture bricht ab
        gift = os.path.join(d, "gift.py")
        with open(gift, "w", encoding="utf-8") as f:
            f.write("import io,sys\n"
                    "io.open(sys.argv[1],'a',encoding='utf-8').write('x')\n"
                    "sys.exit(1)\n")
        try:
            fahre("scripts/gen_stand.py",
                  f'"{sys.executable}" "{gift}" "{fix}"',
                  [fix], runden=1, diff_fn=lambda: [])
            fehler.append("Regel 1 bemerkt eine veraenderte Fixture nicht")
        except Verfahrensfehler as e:
            if "Regel 1" not in str(e):
                fehler.append(f"falsche Regel gemeldet: {e}")

        # 4 · Ziel erreicht -> sauberer Ausstieg
        with open(fix, "w", encoding="utf-8") as f:
            f.write("unantastbar\n")          # zuruecksetzen
        erg = fahre("scripts/gen_stand.py", "exit 0", [fix], runden=3,
                    diff_fn=lambda: [])
        if erg["ergebnis"] != "ZIEL ERREICHT":
            fehler.append(f"gruenes Ziel ergibt '{erg['ergebnis']}'")
        if erg["runden"] != 1:
            fehler.append("gruenes Ziel braucht mehr als eine Runde")

        # 5 · Rotes Ziel -> eine Runde, kein Erfolg
        erg = fahre("scripts/gen_stand.py", "exit 3", [fix], runden=3,
                    diff_fn=lambda: [])
        if erg["ergebnis"] == "ZIEL ERREICHT":
            fehler.append("rotes Ziel wird als erreicht gemeldet")

    gesamt = 8
    print(f"Selbsttest: {gesamt - len(fehler)}/{gesamt}")
    for f in fehler:
        print("  FAIL " + f)
    return 1 if fehler else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--werkzeug")
    ap.add_argument("--ziel")
    ap.add_argument("--fixtures", nargs="*", default=[])
    ap.add_argument("--runden", type=int, default=6)
    ap.add_argument("--kennzahl", help="Wort, dessen Zahl den Fortschritt misst")
    ap.add_argument("--protokoll", help="Datei fuer das Protokoll (JSON)")
    ap.add_argument("--selbsttest", action="store_true")
    a = ap.parse_args()

    if a.selbsttest:
        return selbsttest()
    if not (a.werkzeug and a.ziel):
        ap.error("--werkzeug und --ziel werden gebraucht (oder --selbsttest)")

    try:
        erg = fahre(a.werkzeug, a.ziel, a.fixtures, a.runden, a.kennzahl)
    except Verfahrensfehler as e:
        print("VERFAHRENSFEHLER\n  " + str(e))
        return 2

    erg["_zeit"] = time.strftime("%Y-%m-%d %H:%M:%S")
    erg["_werkzeug"] = a.werkzeug
    erg["_ziel"] = a.ziel
    print(f"{erg['ergebnis']}  (Runde {erg['runden']})")
    for r in erg["protokoll"]:
        z = f"  Zahl {r['zahl']}" if r["zahl"] is not None else ""
        print(f"  Runde {r['runde']}: {'gruen' if r['gruen'] else 'rot'}{z}"
              f"  beruehrt {len(r['beruehrte_dateien'])} Datei(en)")
    if "hinweis" in erg:
        print("\n  " + erg["hinweis"])
    if a.protokoll:
        with open(a.protokoll, "w", encoding="utf-8", newline="\n") as f:
            json.dump(erg, f, ensure_ascii=False, indent=1)
            f.write("\n")
        print(f"\n  Protokoll -> {a.protokoll}")
    return 0 if erg["ergebnis"] == "ZIEL ERREICHT" else 1


if __name__ == "__main__":
    raise SystemExit(main())
