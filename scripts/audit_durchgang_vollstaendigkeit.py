#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Abnahme des Architektur-Durchgangs: faellt eine Datei zwischen die Phasen?
(MF-966, Kriterium 1 aus `docs/plans/ARCHITEKTUR_DURCHGANG.md` §6)

Der Durchgang hat `src/` in elf Phasen zerlegt. Die Zerlegung ist nur
dann eine, wenn **jede** Datei in genau einer Phase liegt — sonst ist
„alles gelesen" eine Aussage ueber die Phasen und nicht ueber den Baum.

Beim ersten Lauf fielen **sechs** Dateien durch: `src/crc/config.h`,
`src/crc/CMakeLists.txt`, `src/whdload/whd_crc16.c`,
`src/whdload/whdload_resload_api.c`, `src/compat/uft_fnmatch.c`,
`src/tracks/crc.h`. Zwei davon trugen einen Befund (P3-267/P3-268).

── Woher die Phasen-Zuordnung kommt ────────────────────────────────────

Aus dem Plan selbst, aus dem Block `<!-- PHASEN-SCOPE -->`. Er steht
dort, weil der Plan die Zerlegung **behauptet**; dieses Skript haelt die
Behauptung gegen `git ls-files`. Eine im Skript gepflegte Liste waere
die Aufzaehlung-statt-Messung, gegen die CLAUDE.md geschrieben ist —
hier ist die Liste die *Aussage*, und die Messung prueft sie.

Aufruf:
    python scripts/audit_durchgang_vollstaendigkeit.py
    python scripts/audit_durchgang_vollstaendigkeit.py --selbsttest
"""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent
PLAN = "docs/plans/ARCHITEKTUR_DURCHGANG.md"
BLOCK = re.compile(r"<!-- PHASEN-SCOPE -->(.*?)<!-- /PHASEN-SCOPE -->", re.S)
ZEILE = re.compile(r"^\s*([0-9a-f]+)\s*:\s*(.+?)\s*$", re.M)


def lies_scope(text: str) -> dict[str, list[str]]:
    """{phase: [praefix, ...]} aus dem Plan."""
    m = BLOCK.search(text)
    if not m:
        return {}
    aus: dict[str, list[str]] = {}
    for zm in ZEILE.finditer(m.group(1)):
        aus[zm.group(1)] = [p.strip() for p in zm.group(2).split()
                            if p.strip()]
    return aus


def _dateien(repo: Path) -> list[str]:
    r = subprocess.run(["git", "ls-files", "src"], cwd=repo,
                       capture_output=True, text=True)
    return [p for p in r.stdout.split("\n") if p]


def _passt(pfad: str, praefix: str) -> bool:
    """`src/formats/` deckt alles darunter; `src/*.cpp` nur die Wurzel."""
    if praefix.endswith("/*"):
        rumpf = praefix[:-2]
        return (pfad.startswith(rumpf + "/")
                and "/" not in pfad[len(rumpf) + 1:])
    return pfad.startswith(praefix.rstrip("/") + "/")


def messe(repo) -> tuple[list[str], dict[str, list[str]]]:
    """(unabgedeckte Dateien, {pfad: [phase, phase]} bei Doppelung)."""
    repo = Path(repo)
    p = repo / PLAN
    if not p.is_file():
        return [], {}
    scope = lies_scope(p.read_text(encoding="utf-8", errors="replace"))
    if not scope:
        return [], {}
    offen: list[str] = []
    doppelt: dict[str, list[str]] = {}
    for datei in _dateien(repo):
        traeger = [ph for ph, prs in scope.items()
                   if any(_passt(datei, pr) for pr in prs)]
        if not traeger:
            offen.append(datei)
        elif len(traeger) > 1:
            doppelt[datei] = traeger
    return sorted(offen), doppelt


def check(repo) -> list[str]:
    """Schnittstelle fuer check_consistency.py."""
    offen, doppelt = messe(repo)
    fehler = []
    if offen:
        fehler.append(
            "Der Architektur-Durchgang beansprucht, `src/` vollstaendig "
            "zerlegt zu haben; %d Datei(en) liegen in KEINER Phase: %s. "
            "Entweder sie gehoeren in eine (dann in den PHASEN-SCOPE-Block "
            "von %s eintragen), oder die Vollstaendigkeit ist keine."
            % (len(offen), ", ".join(offen[:6])
               + (" …" if len(offen) > 6 else ""), PLAN))
    for datei, phasen in sorted(doppelt.items())[:5]:
        fehler.append(
            "%s liegt in mehreren Phasen (%s) — eine Zerlegung ordnet "
            "jede Datei genau einmal zu." % (datei, ", ".join(phasen)))
    return fehler


# ── Selbsttest ──────────────────────────────────────────────────────────

def selbsttest() -> int:
    gut = 0
    faelle = [
        ("Praefix deckt Unterbaum",
         _passt("src/formats/c64/x.c", "src/formats"), True),
        ("Wurzel-Muster deckt NUR die Wurzel",
         _passt("src/toolstab.cpp", "src/*"), True),
        ("Wurzel-Muster deckt NICHT tiefer",
         _passt("src/gui/a.cpp", "src/*"), False),
        ("Praefix trifft keinen Namensanfang",
         _passt("src/formats_alt/x.c", "src/formats"), False),
    ]
    for name, ist, soll in faelle:
        ok = ist == soll
        gut += ok
        print("  %s %-46s" % ("ok " if ok else "ROT", name))

    text = ("<!-- PHASEN-SCOPE -->\n"
            "1: src/core src/*\n"
            "2: src/flux\n"
            "<!-- /PHASEN-SCOPE -->\n")
    s = lies_scope(text)
    ok = s == {"1": ["src/core", "src/*"], "2": ["src/flux"]}
    gut += ok
    print("  %s %-46s %s" % ("ok " if ok else "ROT", "Block gelesen", s))

    ok = lies_scope("kein Block hier") == {}
    gut += ok
    print("  %s %-46s" % ("ok " if ok else "ROT",
                          "fehlender Block -> leer, kein Absturz"))
    print("Selbsttest: %d/%d" % (gut, len(faelle) + 2))
    return 0 if gut == len(faelle) + 2 else 1


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--selbsttest", action="store_true")
    a = ap.parse_args()
    if a.selbsttest:
        return selbsttest()
    offen, doppelt = messe(WURZEL)
    print("Architektur-Durchgang — Vollstaendigkeit der Zerlegung von src/")
    print("  Dateien ohne Phase : %d" % len(offen))
    for f in offen:
        print("     " + f)
    print("  Dateien in mehreren: %d" % len(doppelt))
    for f, ph in sorted(doppelt.items())[:10]:
        print("     %-52s %s" % (f, ", ".join(ph)))
    fehler = check(WURZEL)
    print("Abweichungen: %d" % len(fehler))
    for f in fehler:
        print("  " + f)
    return 1 if fehler else 0


if __name__ == "__main__":
    sys.exit(main())
