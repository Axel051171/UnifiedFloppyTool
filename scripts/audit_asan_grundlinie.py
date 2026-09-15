#!/usr/bin/env python3
"""Der ASan-Vollauf misst — jetzt wird die Messung auch gelesen (MF-1160).

## Die Klemme, die dieses Tor aufloest

`.github/workflows/sanitizers.yml` fuehrt zwei Laeufe: ein SCHARFES Tor
ueber eine benannte Teilmenge (seit MF-517, ohne `|| true`) und einen
BERICHTENDEN Vollauf ueber die ganze Suite. Der Kopf des Vollaufs sagt
seinen Grund selbst:

    „Jetzt laeuft die ganze Suite, damit der Rueckstand GEMESSEN im Log
     steht statt geschaetzt zu werden. `|| true` bleibt hier bewusst:
     den Rueckstand scharf zu schalten, ohne ihn vorher zu kennen, waere
     derselbe Fehler in die andere Richtung."

Das ist richtig — und unvollstaendig. Eine Messung, die niemand gegen
etwas haelt, ist eine Zahl im Protokoll. Der Ausweg ist nicht „scharf",
sondern **fallend**: der Rueckstand wird aufgeschrieben, und ab dann darf
er nur sinken.

    bekannte ASan-Fehler: 26
    neue Fehler:           0     <- rot
    behobene Fehler:       2     <- rot, bis sie aus der Grundlinie raus sind
    verbleibend:          24

Dieselbe Bauform wie Tor 57 (`audit_schreibzusage.py`, MF-883/930),
`audit_typkollision.py` (MF-1155) und `audit_sondendoktrin.py` (MF-1153).

## Warum dieses Tor NICHT in check_consistency.py steht

Es braucht ein ASan-Protokoll. Das entsteht nur in CI (Linux + clang);
auf der Entwicklermaschine dieses Baums steht MinGW, und ASan mit
Leckerkennung gibt es dort nicht. Ein Eintrag in `check_consistency.py`
waere ein Tor, das bei jedem lokalen Lauf nichts findet, weil es seinen
Gegenstand nicht sehen kann — genau die Klasse MF-1000, gegen die dieser
Baum seine Tore schreibt. Es laeuft deshalb dort, wo sein Gegenstand
entsteht: im Sanitizer-Auftrag.

## Was gezaehlt wird, und was ausdruecklich nicht

Gezaehlt werden **Testnamen**, die ctest als fehlgeschlagen meldet. Nicht
gezaehlt werden Byte- und Allokationssummen der Leckberichte: die
schwanken zwischen Laeufen (Reihenfolge, Zeitpunkt, Allokator), und eine
Grundlinie auf schwankenden Zahlen ist entweder immer rot oder nie. Die
Summen werden BERICHTET, damit die Groessenordnung sichtbar bleibt.

## Was dieses Tor nicht sehen kann (MF-1000)

  - **Ein Test, der gar nicht gebaut wurde**, faellt hier nicht auf. Der
    Bauschritt des Auftrags traegt `|| true`, und das ist eine eigene
    Luecke — sie steht als P3-411 (b), nicht hier.
  - **Ein Leck ohne fehlgeschlagenen Test.** Mit
    `ASAN_OPTIONS=detect_leaks=1:abort_on_error=1` faerbt ein Leck den
    Test, also faellt er in die Namensliste. Ohne `abort_on_error` waere
    ein Leck nur Text im Protokoll, und dieses Tor saehe es nicht. Der
    Auftrag setzt es; wer es entfernt, entwertet dieses Tor still.
  - **Die Unterscheidung Test-Harness-Leck gegen Produkt-Leck.** Die
    verlangt eine Eigentuemer-Entscheidung und steht in P3-411 (b). Bis
    dahin zaehlt dieses Tor beide, und es sagt das.
  - **Ein umbenannter Test** sieht aus wie ein neuer plus ein behobener.
    Das ist gewollt: eine Umbenennung soll die Grundlinie anfassen.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
GRUNDLINIE = "docs/asan_baseline.txt"

# ctest schreibt am Ende:
#
#     The following tests FAILED:
#             210 - test_freezer (Failed)
#             211 - test_foo (Subprocess aborted)
#
# Fuehrende Nummer, Bindestrich, Name, Grund in Klammern.
FEHLZEILE = re.compile(r"^\s*\d+\s*-\s*(\S+)\s*\((.+)\)\s*$")
BLOCKKOPF = re.compile(r"^The following tests FAILED:\s*$")

# LeakSanitizer-Summe, nur zur Berichterstattung:
#     SUMMARY: AddressSanitizer: 16487424 byte(s) leaked in 21813 allocation(s).
LECKSUMME = re.compile(
    r"SUMMARY:\s+AddressSanitizer:\s+(\d+)\s+byte\(s\)\s+leaked\s+in\s+(\d+)\s+allocation")


def lies_protokoll(text: str) -> tuple[list[str], int, int, int]:
    """(fehlgeschlagene Testnamen, Bytes, Allokationen, Leckberichte)"""
    namen: list[str] = []
    im_block = False
    for z in text.splitlines():
        if BLOCKKOPF.match(z):
            im_block = True
            continue
        if im_block:
            m = FEHLZEILE.match(z)
            if m:
                namen.append(m.group(1))
                continue
            # Der Block endet an der ersten Zeile, die kein Eintrag ist.
            if z.strip():
                im_block = False

    bytes_ges = 0
    allok_ges = 0
    berichte = 0
    for m in LECKSUMME.finditer(text):
        bytes_ges += int(m.group(1))
        allok_ges += int(m.group(2))
        berichte += 1

    # Reihenfolge stabil halten, Doppelnennungen zusammenfassen:
    # ctest kann denselben Namen bei Wiederholungen mehrfach auflisten.
    eindeutig = sorted(set(namen))
    return eindeutig, bytes_ges, allok_ges, berichte


def lies_grundlinie(pfad: Path) -> set[str] | None:
    """None = es gibt keine Grundlinie (noch nicht gesaet)."""
    if not pfad.exists():
        return None
    namen = set()
    for z in pfad.read_text(encoding="utf-8").splitlines():
        z = z.split("#", 1)[0].strip()
        if z:
            namen.add(z)
    return namen


def pruefe(gefunden: list[str], bekannt: set[str]) -> list[str]:
    fehler = []
    neu = sorted(set(gefunden) - bekannt)
    behoben = sorted(bekannt - set(gefunden))
    for n in neu:
        fehler.append(
            f"{n}: NEU unter ASan fehlgeschlagen. Nicht in {GRUNDLINIE}. "
            f"Entweder das Leck/den Fehler beheben oder — mit "
            f"Eigentuemer-Entscheidung — in die Grundlinie aufnehmen.")
    for n in behoben:
        fehler.append(
            f"{n}: faellt nicht mehr aus — Zeile aus {GRUNDLINIE} entfernen, "
            f"damit die Grundlinie SINKT. Eine Grundlinie, die Behobenes "
            f"weiterfuehrt, ist ein Dauerparkplatz (Tor-57-Bauform).")
    return fehler


# ── Selbsttest ──────────────────────────────────────────────────────────────
# Vor der Messung, weil eine Erstfassung von `tuersucher.py` einmal
# „Selbsttest 3/3" meldete und gemessen 0/3 lieferte. Jeder Fall prueft die
# Richtung, die er behauptet.

LOG_ZWEI_FEHLER = """\
    Start 209: test_a
209/478 Test #209: test_a ....................   Passed    0.31 sec
    Start 210: test_freezer
210/478 Test #210: test_freezer ..............***Failed    0.02 sec
SUMMARY: AddressSanitizer: 1024 byte(s) leaked in 8 allocation(s).

97% tests passed, 2 tests failed out of 478

The following tests FAILED:
        210 - test_freezer (Failed)
        377 - test_b (Subprocess aborted)
Errors while running CTest
"""

LOG_OHNE_FEHLER = """\
100% tests passed, 0 tests failed out of 478

Total Test time (real) = 352.30 sec
"""

LOG_DOPPELT = """\
The following tests FAILED:
        210 - test_freezer (Failed)
        210 - test_freezer (Failed)
Errors while running CTest
"""

LOG_ZWEI_LECKS = """\
SUMMARY: AddressSanitizer: 100 byte(s) leaked in 2 allocation(s).
SUMMARY: AddressSanitizer: 200 byte(s) leaked in 3 allocation(s).

The following tests FAILED:
        1 - test_x (Failed)
"""

FAELLE = [
    ("zwei Fehler werden erkannt",
     lambda: lies_protokoll(LOG_ZWEI_FEHLER)[0] == ["test_b", "test_freezer"]),
    ("Leck-Summe wird gelesen, nicht gezaehlt",
     lambda: lies_protokoll(LOG_ZWEI_FEHLER)[1:] == (1024, 8, 1)),
    ("ein Protokoll ohne Fehler ergibt eine leere Liste",
     lambda: lies_protokoll(LOG_OHNE_FEHLER)[0] == []),
    ("derselbe Name zweimal zaehlt einmal",
     lambda: lies_protokoll(LOG_DOPPELT)[0] == ["test_freezer"]),
    ("zwei Leckberichte werden summiert",
     lambda: lies_protokoll(LOG_ZWEI_LECKS)[1:] == (300, 5, 2)),
    ("ein NEUER Fehler ist rot",
     lambda: len(pruefe(["test_a", "test_b"], {"test_a"})) == 1),
    ("ein BEHOBENER Fehler ist rot (Grundlinie muss sinken)",
     lambda: len(pruefe(["test_a"], {"test_a", "test_b"})) == 1),
    ("unveraendert ist gruen",
     lambda: pruefe(["test_a", "test_b"], {"test_a", "test_b"}) == []),
    ("Kommentare und Leerzeilen der Grundlinie werden ignoriert",
     lambda: _grundlinie_probe()),
]


def _grundlinie_probe() -> bool:
    import tempfile
    with tempfile.TemporaryDirectory() as td:
        p = Path(td) / "b.txt"
        p.write_text("# Kopf\n\ntest_a\ntest_b  # warum\n", encoding="utf-8")
        return lies_grundlinie(p) == {"test_a", "test_b"}


def selbsttest() -> int:
    gut = 0
    for titel, f in FAELLE:
        try:
            ok = bool(f())
        except Exception as exc:                             # noqa: BLE001
            ok = False
            titel += f"  [{type(exc).__name__}: {exc}]"
        gut += ok
        print(f"  [{'ok' if ok else 'ROT'}] {titel}")
    print(f"Selbsttest {gut}/{len(FAELLE)}")
    return 0 if gut == len(FAELLE) else 1


def main() -> int:
    ap = argparse.ArgumentParser(
        description="ASan-Rueckstand gegen eine fallende Grundlinie (MF-1160)")
    ap.add_argument("--log", help="ctest-Protokoll des ASan-Vollaufs")
    ap.add_argument("--selbsttest", action="store_true")
    ap.add_argument("--grundlinie", action="store_true",
                    help="gegen die Grundlinie pruefen (Rueckgabe 1 bei Abweichung)")
    ap.add_argument("--schreibe-grundlinie", action="store_true",
                    help="Grundlinie aus diesem Protokoll setzen")
    args = ap.parse_args()

    if args.selbsttest:
        return selbsttest()

    print("Selbsttest vor der Messung:")
    if selbsttest() != 0:
        print("Selbsttest ROT — Messung wird nicht ausgefuehrt.", file=sys.stderr)
        return 1
    print()

    if not args.log:
        ap.error("--log fehlt")
    pfad = Path(args.log)
    if not pfad.exists():
        # Kein Protokoll ist KEINE Entwarnung. Ein Tor, das bei fehlendem
        # Gegenstand gruen meldet, ist die Falle aus MF-1000.
        print(f"FEHLER: {args.log} gibt es nicht. Der ASan-Vollauf muss sein "
              f"Protokoll dorthin schreiben; ohne Protokoll ist der "
              f"Rueckstand UNGEMESSEN, nicht null.", file=sys.stderr)
        return 1

    text = pfad.read_text(encoding="utf-8", errors="replace")
    gefunden, bytes_ges, allok, berichte = lies_protokoll(text)

    bekannt = lies_grundlinie(WURZEL / GRUNDLINIE)

    print(f"ASan-Vollauf, gelesen aus {args.log}")
    print(f"  fehlgeschlagene Tests       : {len(gefunden)}")
    if berichte:
        print(f"  Leckberichte                : {berichte}"
              f"  ({bytes_ges} Byte in {allok} Allokationen)")
    else:
        print(f"  Leckberichte                : 0 im Protokoll gefunden")
    for n in gefunden:
        print(f"    {n}")

    if args.schreibe_grundlinie:
        ziel = WURZEL / GRUNDLINIE
        kopf = (
            "# Grundlinie fuer scripts/audit_asan_grundlinie.py (MF-1160).\n"
            "# Ein ctest-Testname je Zeile: faellt heute unter ASan aus.\n"
            "# FALLENDE Grundlinie (Tor-57-Bauform): die Zahl darf nur sinken.\n"
            "#   ein NEUER Name        -> rot\n"
            "#   ein BEHOBENER Name    -> rot, bis die Zeile hier verschwindet\n"
            "# Gesaet werden darf sie nur aus einem ECHTEN CI-Lauf. Auf der\n"
            "# Entwicklermaschine dieses Baums gibt es kein ASan (MinGW), eine\n"
            "# von Hand geschriebene Zahl waere eine Behauptung.\n"
        )
        ziel.write_text(kopf + "".join(f"{n}\n" for n in gefunden),
                        encoding="utf-8")
        print(f"\n-> {GRUNDLINIE}: {len(gefunden)} Namen")
        return 0

    if bekannt is None:
        print(f"\n  Grundlinie                  : es gibt noch keine")
        print(f"  {GRUNDLINIE} fehlt. Damit ist dieser Lauf ein BERICHT und")
        print(f"  kein Tor — und das ist ein Zustand, nicht ein Ergebnis.")
        print(f"  Saeen aus DIESEM Lauf:")
        print(f"      python3 scripts/audit_asan_grundlinie.py \\")
        print(f"          --log {args.log} --schreibe-grundlinie")
        print(f"  Solange sie fehlt, kann ein neuer ASan-Fehler nicht roeten.")
        return 0

    print(f"  Grundlinie                  : {len(bekannt)}")
    fehler = pruefe(gefunden, bekannt)
    neu = len(set(gefunden) - bekannt)
    behoben = len(bekannt - set(gefunden))
    print(f"  neue Fehler                 : {neu}")
    print(f"  behobene Fehler             : {behoben}")
    print(f"  verbleibend                 : {len(set(gefunden) & bekannt)}")
    if fehler:
        print("\nBefunde:")
        for f in fehler:
            print(f"  {f}")
        print("\nFAIL: der ASan-Rueckstand ist nicht gefallen (MF-1160).")
        return 1
    print("\nOK: kein neuer ASan-Fehler, Grundlinie aktuell.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
