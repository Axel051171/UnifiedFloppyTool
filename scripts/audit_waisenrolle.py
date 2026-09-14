#!/usr/bin/env python3
"""Tor: die Waisenrolle ist vollstaendig, belegt, und keine Zeile verschwindet.

MF-1114, auf Eigentuemer-Anweisung: „ein Register statt eines Sweeps …
Das Tor prueft dann zwei Dinge, mehr nicht: jedes Symbol ohne Aufrufer
steht im Register und keine Zeile verschwindet ohne Statuswechsel plus
Beleg. Neue Waisen fallen ab da beim Commit auf, nicht bei einem Sweep
drei Monate spaeter."

── Warum nur die Art `schreiber` vollstaendig geprueft wird ─────────────

„Jedes Symbol ohne Aufrufer" sind in diesem Baum **3559** (Marke `WAISE`
oder `NUR_TESTS`, gemessen von `tools/uft-innendienst/scripts/
tuersucher.py`). Ein Tor, das 3559 Registerzeilen verlangt, erzwingt
Buchhaltung — und die Zahl faellt bei schaerferer Frage auf 1777 bzw.
**289**, ohne dass eine Zeile Code besser wird. Genau das verbietet
MF-1077 (G2): die Kennzahl als Motiv.

Vollstaendig geprueft wird deshalb die Art, die **etwas verspricht** und
**maschinell eindeutig** messbar ist: `uft_<fmt>_write` — ein fertiger
Dateischreiber, dessen Erreichbarkeit Tor 57 schon kennt. Die uebrigen
Arten stehen als Sammelzeilen mit Beleg; sie brauchen einen
Registereintrag, keine Arbeit.

**Und dass diese Wahl richtig ist, hat der erste Lauf belegt:** er fand
**13** Schreiber, wo MF-930 elf gezaehlt hatte — `uft_atx_write` und
`uft_ibm3740_write` standen auf keiner Liste, auch nicht im Kopf von
Tor 57. Fuenfzehnter Fall von Aufzaehlung statt Messung.

── Die drei Pruefungen ─────────────────────────────────────────────────

  T1  Jedes `uft_<fmt>_write` im Baum hat eine Zeile der Art
      `schreiber`. (Neuzugang faellt beim Commit auf.)
  T2  Jede Zeile mit Status `verdrahtet`, `absicht`, `zurueckgenommen`
      oder `ohne_koerper` traegt einen `MF-`/`P3-`/`P0-`-Beleg.
  T3  Keine Zeile ist gegenueber `HEAD` verschwunden. Verglichen wird
      das Muster; ein Statuswechsel ist erlaubt, ein Wegfall nicht.

Aufruf:  python scripts/audit_waisenrolle.py [--selftest]
Exit:    0 = keine Befunde, 1 = Befunde, 2 = Selbsttest rot
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
ROLLE = WURZEL / "docs" / "WAISEN_ROLL.md"

ARTEN = {"schreiber", "deklaration", "erkennung", "schutz",
         "deepread", "skelett", "experiment"}
STATUS = {"verdrahtet", "offen", "ohne_koerper", "absicht",
          "zurueckgenommen"}
# `offen` darf einen Plan-Anker statt einer Nummer tragen; alle anderen
# verlangen einen Beleg.
BELEG_PFLICHT = STATUS - {"offen"}

RE_BELEG = re.compile(r"\b(MF|P0|P1|P2|P3)-\d+\b")
# Eine Rollenzeile: | `muster` | art | status | beleg |
RE_ZEILE = re.compile(
    r"^\|\s*`?([^`|]+?)`?\s*\|\s*([a-z_]+)\s*\|\s*([a-z_]+)\s*\|([^|]*)\|\s*$")
# Ein Dateischreiber: `uft_error_t uft_<name>_write(` am Zeilenanfang.
RE_SCHREIBER = re.compile(r"^uft_error_t\s+(uft_[a-z0-9_]+_write)\b", re.M)


def rolle_lesen(text: str) -> list[tuple[str, str, str, str]]:
    """Alle Rollenzeilen als (muster, art, status, beleg)."""
    zeilen = []
    for z in text.splitlines():
        m = RE_ZEILE.match(z)
        if not m:
            continue
        muster, art, status, beleg = (g.strip() for g in m.groups())
        # Die Feldtafel im Dokumentkopf hat dieselbe Form; sie traegt
        # keine gueltige Art und faellt damit heraus.
        if art not in ARTEN or status not in STATUS:
            continue
        zeilen.append((muster, art, status, beleg))
    return zeilen


def schreiber_im_baum(wurzel: Path) -> set[str]:
    """Alle `uft_<fmt>_write` aus `git ls-files src/formats`."""
    try:
        aus = subprocess.run(
            ["git", "ls-files", "src/formats"],
            cwd=wurzel, capture_output=True, text=True, timeout=120)
    except Exception:
        return set()
    namen: set[str] = set()
    for rel in aus.stdout.split():
        if not rel.endswith(".c"):
            continue
        p = wurzel / rel
        try:
            namen.update(RE_SCHREIBER.findall(
                p.read_text(encoding="utf-8", errors="replace")))
        except OSError:
            pass
    return namen


def rolle_aus_head(wurzel: Path) -> str:
    """Die Rolle, wie sie in `HEAD` steht — Blob, nicht Arbeitsbaum.

    MF-1096 Sperre 3: mit `core.autocrlf=true` sind die Bytes im
    Arbeitsbaum nicht die Bytes im Blob. Fuer einen Zeilenvergleich ist
    das gleichgueltig, fuer eine Pruefsumme nicht — und die Gewohnheit
    gehoert richtig.
    """
    try:
        aus = subprocess.run(
            ["git", "show", "HEAD:docs/WAISEN_ROLL.md"],
            cwd=wurzel, capture_output=True, text=True, timeout=60)
        return aus.stdout if aus.returncode == 0 else ""
    except Exception:
        return ""


def pruefe(rolle_text: str, schreiber: set[str], head_text: str) -> list[str]:
    befunde: list[str] = []
    zeilen = rolle_lesen(rolle_text)

    if not zeilen:
        befunde.append(
            "docs/WAISEN_ROLL.md enthaelt keine lesbare Rollenzeile — "
            "entweder ist die Tabellenform kaputt oder dieses Tor liest "
            "die falsche Datei. Beides gehoert angesehen.")
        return befunde

    muster = {m for m, _, _, _ in zeilen}

    # T1: Vollstaendigkeit bei der Art `schreiber`.
    gefuehrt = {m for m, art, _, _ in zeilen if art == "schreiber"}
    for name in sorted(schreiber - gefuehrt):
        befunde.append(
            f"`{name}` ist ein Dateischreiber im Baum und hat KEINE Zeile "
            f"in docs/WAISEN_ROLL.md. Genau so sind `uft_atx_write` und "
            f"`uft_ibm3740_write` jahrelang an jeder Liste vorbeigelaufen "
            f"(MF-1114). Zeile anlegen: `| {name} | schreiber | "
            f"offen/verdrahtet | <MF-Nummer> |`")
    for name in sorted(gefuehrt - schreiber):
        befunde.append(
            f"`{name}` steht als Art `schreiber` in der Rolle, ist im Baum "
            f"aber nicht (mehr) als `uft_error_t {name}(` auffindbar. "
            f"Entweder wurde er entfernt — dann gehoert der Status auf "
            f"`zurueckgenommen` mit Beleg — oder der Name ist verschrieben.")

    # T2: Beleg-Pflicht.
    for m, art, status, beleg in zeilen:
        if status in BELEG_PFLICHT and not RE_BELEG.search(beleg):
            befunde.append(
                f"`{m}` ({art}) traegt Status `{status}` ohne Beleg. "
                f"Ein Status, der sich nicht belegt, ist eine Behauptung; "
                f"nur `offen` darf einen Plan-Anker statt einer Nummer "
                f"haben.")

    # T3: Keine Zeile verschwindet.
    if head_text:
        alt = {m for m, _, _, _ in rolle_lesen(head_text)}
        for m in sorted(alt - muster):
            befunde.append(
                f"`{m}` stand in HEAD und ist verschwunden. Eine Rolle "
                f"prueft die ABWESENHEIT — ein Wegfall verdeckt genau das, "
                f"wogegen sie steht. Statuswechsel ja, Wegfall nein "
                f"(MF-1077).")
    return befunde


# ── Selbsttest ──────────────────────────────────────────────────────────

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

    kopf = "| Muster | Art | Status | Beleg |\n|---|---|---|---|\n"

    r_ok = kopf + "| `uft_a_write` | schreiber | verdrahtet | MF-1 |\n"
    zusage(rolle_lesen(r_ok) == [("uft_a_write", "schreiber",
                                  "verdrahtet", "MF-1")],
           "eine Zeile wird zerlegt")
    zusage(len(rolle_lesen(kopf)) == 0,
           "die Kopfzeile und der Trenner sind KEINE Rollenzeilen")

    tafel = kopf + "| **Muster** | Symbolname oder Glob | x | y |\n"
    zusage(len(rolle_lesen(tafel)) == 0,
           "eine Zeile ohne gueltige Art faellt heraus")

    # T1
    zusage(any("KEINE Zeile" in b
               for b in pruefe(r_ok, {"uft_a_write", "uft_b_write"}, "")),
           "ein ungefuehrter Schreiber wird gemeldet")
    zusage(not any("KEINE Zeile" in b
                   for b in pruefe(r_ok, {"uft_a_write"}, "")),
           "ein gefuehrter Schreiber ist kein Befund")
    zusage(any("nicht (mehr)" in b
               for b in pruefe(r_ok, set(), "")),
           "eine Zeile ohne Gegenstand im Baum wird gemeldet")

    # T2
    r_ohne = kopf + "| `uft_a_write` | schreiber | verdrahtet |  |\n"
    zusage(any("ohne Beleg" in b
               for b in pruefe(r_ohne, {"uft_a_write"}, "")),
           "`verdrahtet` ohne Beleg wird gemeldet")
    r_offen = kopf + "| `uft_a_write` | schreiber | offen | Plan-Anker |\n"
    zusage(not any("ohne Beleg" in b
                   for b in pruefe(r_offen, {"uft_a_write"}, "")),
           "`offen` darf einen Plan-Anker tragen")

    # T3
    r_head = kopf + ("| `uft_a_write` | schreiber | offen | P3-1 |\n"
                     "| `uft_b_write` | schreiber | offen | P3-1 |\n")
    r_neu = kopf + "| `uft_a_write` | schreiber | offen | P3-1 |\n"
    zusage(any("verschwunden" in b
               for b in pruefe(r_neu, {"uft_a_write"}, r_head)),
           "eine verschwundene Zeile wird gemeldet")
    r_gewechselt = kopf + ("| `uft_a_write` | schreiber | verdrahtet | MF-9 |\n"
                           "| `uft_b_write` | schreiber | offen | P3-1 |\n")
    zusage(not any("verschwunden" in b
                   for b in pruefe(r_gewechselt,
                                   {"uft_a_write", "uft_b_write"}, r_head)),
           "ein Statuswechsel ist KEIN Wegfall")

    zusage(any("keine lesbare Rollenzeile" in b for b in pruefe("", set(), "")),
           "eine leere Rolle wird gemeldet")

    print(f"SELBSTTEST {gut}/{gesamt}")
    return 0 if gut == gesamt else 2


def check(repo) -> list[str]:
    """Einstieg fuer `scripts/check_consistency.py`."""
    wurzel = Path(repo)
    rolle = wurzel / "docs" / "WAISEN_ROLL.md"
    if not rolle.exists():
        return [f"{rolle} fehlt — die Waisenrolle ist die einzige Stelle, "
                f"die einen Wegfall bemerken kann (MF-1114)."]
    return pruefe(rolle.read_text(encoding="utf-8", errors="replace"),
                  schreiber_im_baum(wurzel),
                  rolle_aus_head(wurzel))


def main() -> int:
    if "--selftest" in sys.argv:
        return _selftest()

    befunde = check(WURZEL)
    zeilen = rolle_lesen(ROLLE.read_text(encoding="utf-8", errors="replace")) \
        if ROLLE.exists() else []
    schreiber = schreiber_im_baum(WURZEL)

    print("Waisenrolle (MF-1114)")
    print(f"  Zeilen in der Rolle        : {len(zeilen)}")
    print(f"  Dateischreiber im Baum     : {len(schreiber)}")
    print(f"  davon in der Rolle gefuehrt: "
          f"{len({m for m, a, _, _ in zeilen if a == 'schreiber'} & schreiber)}")
    print(f"  Befunde                    : {len(befunde)}")
    for b in befunde:
        print(f"    - {b}")
    return 1 if befunde else 0


if __name__ == "__main__":
    sys.exit(main())
