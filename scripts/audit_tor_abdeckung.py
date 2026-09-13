#!/usr/bin/env python3
"""Tor: die scharfen Sanitizer-Stufen nennen neun Tests — gibt es die noch?

MF-1110. Die beiden scharfen Stufen in `.github/workflows/sanitizers.yml`
(ASan und UBSan) waehlen ihre Prueflinge ueber

    ctest --no-tests=error --tests-regex "test_a|test_b|…"

Das ist eine **Aufzaehlung von Namen**, und die veraltet still — die
Klasse, die dieser Baum viermal bezahlt hat (MF-567/578/598/633, in
`CLAUDE.md` unter „Dateimengen kommen aus git").

**Was `--no-tests=error` NICHT abfaengt, und das ist der Kern:** die
Option feuert nur, wenn die Regex **gar keinen** Test trifft. Faellt
einer von neun weg — umbenannt, in `EXCLUDED_TESTS` gewandert, oder in
CI nicht uebersetzt, weil der Bauschritt `cmake --build … || true`
traegt — dann laeuft die Stufe mit **acht** und meldet **gruen**.
Niemand zaehlt. Genau die Gestalt von MF-1000 / Tor 64: ein Tor, das
schmaler ist als sein Gegenstand, meldet zuverlaessig Erfolg.

Dieses Tor prueft deshalb zwei Dinge, und keines davon aus einer
gepflegten Liste:

  T1  Die Regex wird AUS DEM WORKFLOW gelesen, nicht hier wiederholt.
      Jeder darin genannte Name muss eine Testquelle unter `tests/`
      haben und darf nicht in `EXCLUDED_TESTS` stehen.
  T2  Beide scharfen Stufen muessen DIESELBE Regex tragen. Sie steht
      heute zweimal woertlich im Workflow; zwei Kopien driften.

Die erwartete Anzahl wird aus der Regex ABGELEITET (Zahl der
`|`-getrennten Alternativen) — eine zweite gepflegte Zahl waere genau
der Fehler, den das Tor verhindern soll (MF-1077).

Was dieses Tor bewusst NICHT sieht: ob das Testziel in CI wirklich
gebaut wurde. Das kann nur der CI-Lauf selbst messen, und dafuer traegt
`sanitizers.yml` seit MF-1110 einen eigenen Zaehlschritt. Hier steht die
statische Haelfte, die auf jedem Rechner und im Pre-Commit laeuft.

Aufruf:  python scripts/audit_tor_abdeckung.py [--selftest]
Exit:    0 = keine Befunde, 1 = Befunde, 2 = Selbsttest rot
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
WORKFLOW = WURZEL / ".github" / "workflows" / "sanitizers.yml"
TESTS_CMAKE = WURZEL / "tests" / "CMakeLists.txt"
TESTS_DIR = WURZEL / "tests"

# Zwei Formen tragen dieselbe Namensliste, und beide kommen im Workflow
# wirklich vor:
#   --tests-regex "test_a|test_b"     (das scharfe Tor selbst)
#   REGEX="test_a|test_b"             (der Zaehlschritt aus MF-1110)
RE_TESTS_REGEX = re.compile(r'--tests-regex\s+"([^"]+)"')
RE_REGEX_ZUWEISUNG = re.compile(r'^\s*REGEX="([^"]+)"', re.MULTILINE)


def regexe_aus_workflow(text: str) -> list[str]:
    """Alle Namenslisten im Workflow, in Reihenfolge ihres Auftretens.

    **Shell-Variablen werden uebersprungen, und das ist keine Nachlaessigkeit,
    sondern eine Korrektur an diesem Tor selbst.** Die erste Fassung griff
    jede `--tests-regex`-Zeile — und wurde damit sofort rot an dem
    Zaehlschritt, den MF-1110 im selben Commit eingefuehrt hat: der ruft
    `ctest -N --tests-regex "$REGEX"`, und `$REGEX` ist kein Testname.
    Das Tor hat also die eigene Aenderung gefangen; gemessen waren es
    „6 Zeilen, 2 verschiedene Listen".

    Ein Wert, der mit `$` beginnt, ist eine Referenz auf eine Liste, die
    anderswo in derselben Datei steht — und genau die wird ueber
    `REGEX="…"` mitgelesen. Es faellt damit keine Liste aus der Messung.
    """
    gefunden = [w for w in RE_TESTS_REGEX.findall(text)
                if not w.lstrip().startswith("$")]
    gefunden += RE_REGEX_ZUWEISUNG.findall(text)
    return gefunden


def namen_aus_regex(regex: str) -> list[str]:
    """Die `|`-getrennten Alternativen, leere verworfen."""
    return [t.strip() for t in regex.split("|") if t.strip()]


def ausgeschlossene(text: str) -> set[str]:
    """Die Namen aus `set(EXCLUDED_TESTS … )` in tests/CMakeLists.txt.

    Kommentarzeilen fallen weg, damit ein Testname, der nur in einer
    Begruendung vorkommt, nicht als ausgeschlossen gilt — genau dieser
    Fehler hat MF-1090 die Entfernung einer Datei blockiert (dort griff
    eine Pruefung den Basisnamen statt den Pfad).
    """
    anfang = text.find("set(EXCLUDED_TESTS")
    if anfang < 0:
        return set()
    tiefe = 0
    ende = anfang
    for i in range(text.find("(", anfang), len(text)):
        if text[i] == "(":
            tiefe += 1
        elif text[i] == ")":
            tiefe -= 1
            if tiefe == 0:
                ende = i
                break
    block = text[anfang:ende]
    namen: set[str] = set()
    for zeile in block.splitlines():
        ohne_kommentar = zeile.split("#", 1)[0]
        namen.update(re.findall(r'"([A-Za-z0-9_]+)"', ohne_kommentar))
    return namen


def quelle_vorhanden(name: str) -> bool:
    return ((TESTS_DIR / f"{name}.c").exists()
            or (TESTS_DIR / f"{name}.cpp").exists())


def pruefe(workflow_text: str, cmake_text: str) -> list[str]:
    befunde: list[str] = []

    regexe = regexe_aus_workflow(workflow_text)
    if not regexe:
        befunde.append(
            "keine `--tests-regex`-Zeile im Workflow gefunden — entweder "
            "wurde die scharfe Stufe entfernt oder dieses Tor liest die "
            "falsche Datei. Beides gehoert angesehen.")
        return befunde

    # T2: alle scharfen Stufen tragen dieselbe Liste.
    einzig = set(regexe)
    if len(einzig) != 1:
        befunde.append(
            f"die {len(regexe)} `--tests-regex`-Zeilen tragen "
            f"{len(einzig)} VERSCHIEDENE Listen — zwei Kopien driften. "
            "Gefunden: " + " || ".join(sorted(einzig)))

    # T1: jeder genannte Name existiert und ist nicht ausgeschlossen.
    aus = ausgeschlossene(cmake_text)
    for regex in sorted(einzig):
        for name in namen_aus_regex(regex):
            if not quelle_vorhanden(name):
                befunde.append(
                    f"`{name}` steht in der scharfen Stufe, hat aber "
                    f"keine Quelle unter tests/ — die Stufe laeuft mit "
                    f"einem Pruefling weniger und meldet trotzdem gruen "
                    f"(`--no-tests=error` greift nur bei NULL Treffern).")
            if name in aus:
                befunde.append(
                    f"`{name}` steht in der scharfen Stufe UND in "
                    f"EXCLUDED_TESTS — es wird nie gebaut, die Stufe "
                    f"prueft es nie.")
    return befunde


# -- Selbsttest ---------------------------------------------------------

def _selftest() -> int:
    gut = 0
    gesamt = 0

    def zusage(bedingung: bool, text: str) -> None:
        nonlocal gut, gesamt
        gesamt += 1
        if bedingung:
            gut += 1
        else:
            print(f"  SELBSTTEST FAIL: {text}")

    zusage(namen_aus_regex("a|b|c") == ["a", "b", "c"],
           "Regex in drei Namen zerlegen")
    zusage(namen_aus_regex("a||b") == ["a", "b"],
           "leere Alternative faellt weg")
    zusage(regexe_aus_workflow('x --tests-regex "p|q" y') == ["p|q"],
           "Regex aus einer Zeile lesen")
    zusage(regexe_aus_workflow('--tests-regex "a" \n --tests-regex "b"')
           == ["a", "b"], "zwei Regexe in Reihenfolge")
    # Die Korrektur an diesem Tor selbst, festgenagelt: eine
    # Shell-Variable ist kein Testname.
    zusage(regexe_aus_workflow('--tests-regex "$REGEX"') == [],
           "eine Shell-Variable wird NICHT als Namensliste gelesen")
    zusage(regexe_aus_workflow('          REGEX="a|b"\n') == ["a|b"],
           "die REGEX=-Zuweisung des Zaehlschritts wird gelesen")
    zusage(regexe_aus_workflow('  REGEX="a|b"\n  x --tests-regex "$REGEX"')
           == ["a|b"],
           "Zuweisung plus Referenz ergibt GENAU EINE Liste — sonst "
           "meldete das Tor einen Drift, den es selbst erzeugt hat")

    cm = 'set(EXCLUDED_TESTS\n  "test_x"  # weil\n  "test_y"\n)\n'
    zusage(ausgeschlossene(cm) == {"test_x", "test_y"},
           "EXCLUDED_TESTS lesen")
    cm2 = 'set(EXCLUDED_TESTS\n  # Grund nennt "test_z"\n  "test_x"\n)\n'
    zusage(ausgeschlossene(cm2) == {"test_x"},
           "ein Name NUR im Kommentar zaehlt nicht (MF-1090-Klasse)")

    # Rotbeweis: erfundener Name muss einen Befund geben.
    wf = '--tests-regex "test_gibt_es_nicht_1110"'
    zusage(len(pruefe(wf, "")) == 1,
           "ein Name ohne Quelle wird gemeldet")

    # Ein ausgeschlossener Name wird als solcher gemeldet.
    wf2 = '--tests-regex "test_libdsk_formats"'
    befunde = pruefe(wf2, cm.replace("test_x", "test_libdsk_formats"))
    zusage(any("EXCLUDED_TESTS" in b for b in befunde),
           "ein ausgeschlossener Name wird gemeldet")

    # Zwei verschiedene Listen -> Drift-Befund.
    wf3 = '--tests-regex "a" \n --tests-regex "b"'
    zusage(any("VERSCHIEDENE" in b for b in pruefe(wf3, "")),
           "zwei abweichende Listen werden gemeldet")

    # Gegenprobe: zwei GLEICHE Listen sind kein Drift-Befund.
    wf4 = '--tests-regex "a" \n --tests-regex "a"'
    zusage(not any("VERSCHIEDENE" in b for b in pruefe(wf4, "")),
           "zwei gleiche Listen sind kein Befund")

    print(f"SELBSTTEST {gut}/{gesamt}")
    return 0 if gut == gesamt else 2


def check(repo: Path) -> list[str]:
    """Einstieg fuer `scripts/check_consistency.py` (Muster der 56 Tore).

    Nimmt die Wurzel als Argument statt die Modulkonstanten zu benutzen,
    damit das Tor auch aus einem anderen Arbeitsverzeichnis laeuft.
    """
    wf = Path(repo) / ".github" / "workflows" / "sanitizers.yml"
    cm = Path(repo) / "tests" / "CMakeLists.txt"
    if not wf.exists():
        return [f"{wf} fehlt — die scharfen Sanitizer-Stufen sind nicht "
                f"auffindbar, also ist ihre Abdeckung ungemessen."]
    return pruefe(wf.read_text(encoding="utf-8", errors="replace"),
                  cm.read_text(encoding="utf-8", errors="replace")
                  if cm.exists() else "")


def main() -> int:
    if "--selftest" in sys.argv:
        return _selftest()

    if not WORKFLOW.exists():
        print(f"FEHLER: {WORKFLOW} fehlt")
        return 1
    workflow_text = WORKFLOW.read_text(encoding="utf-8", errors="replace")
    cmake_text = TESTS_CMAKE.read_text(encoding="utf-8", errors="replace")

    befunde = pruefe(workflow_text, cmake_text)

    alle = regexe_aus_workflow(workflow_text)
    einzig = set(alle)
    anzahl = len(namen_aus_regex(next(iter(einzig)))) if len(einzig) == 1 else 0

    print("Tor-Abdeckung der scharfen Sanitizer-Stufen (MF-1110)")
    print(f"  gefundene Namenslisten   : {len(alle)}  "
          f"(scharfe Tore + Zaehlschritte)")
    print(f"  davon verschieden        : {len(einzig)}  (1 = kein Drift)")
    print(f"  Prueflinge je Liste      : {anzahl}")
    print(f"  Befunde                  : {len(befunde)}")
    for b in befunde:
        print(f"    - {b}")
    return 1 if befunde else 0


if __name__ == "__main__":
    sys.exit(main())
