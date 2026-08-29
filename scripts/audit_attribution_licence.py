#!/usr/bin/env python3
"""Nennt jede Attribution die Lizenz IHRER Quelle? (MF-651)

Hintergrund. `scripts/audit_spdx_policy.py` listet seit MF-636 die
Fliesstext-Attributionen — „Based on X", „Port of X" —, faellt aber
bewusst kein Urteil. Die Frage „wie viele davon nennen keine Lizenz?"
wurde bis MF-651 von Hand beantwortet, und eine handgepflegte Zahl
driftet; in diesem Baum ist das dreimal belegt.

Die Falle, in die der erste Entwurf lief, steht hier als Warnung:
er suchte einen Lizenzbezeichner IRGENDWO im 60-Zeilen-Kopf. Damit
zaehlte er unsere EIGENE `SPDX-License-Identifier: GPL-2.0-or-later`
mit — die seit MF-621 in fast jeder Datei steht — und konnte die
gestellte Frage gar nicht beantworten. Gemessen: 29 geheilte
Attributionen bewegten die Zahl um **null**. Ein Mass, das sich durch
die Sache nicht bewegen laesst, misst sie nicht.

Deshalb hier: die Lizenz muss NEBEN der Attribution stehen (im selben
Kommentarblock, Fenster +/- FENSTER Zeilen), und die eigene
SPDX-Zeile zaehlt ausdruecklich NICHT.

Aufruf:
    python scripts/audit_attribution_licence.py [--liste]
"""
from __future__ import annotations

import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import audit_spdx_policy as A  # noqa: E402

FENSTER = 6
"""Zeilen ober- und unterhalb der Attribution, die noch als 'daneben'
gelten. Sechs, weil ein umbrochener Lizenzsatz in diesem Baum bis zu
vier Zeilen braucht (gemessen an den libdsk-Koepfen)."""

LIZENZ = re.compile(
    r"\b(GPL|LGPL|AGPL|MIT|BSD|zlib|Apache|CC0|ISC|MPL|"
    r"[Pp]ublic [Dd]omain|proprietary|proprietaer|Unlicense)\b")

EIGENE_SPDX = re.compile(r"SPDX-License-Identifier")


def messe(repo: pathlib.Path):
    """(gesamt, ohne_lizenz) — ohne_lizenz als Liste von (datei, zeile, text)."""
    treffer = A.scan_attributions(repo)
    ohne = []
    for rel, zeile, text in treffer:
        try:
            zeilen = (repo / rel).read_text(
                encoding="utf-8", errors="replace").splitlines()
        except OSError:
            continue
        lo = max(0, zeile - 1 - FENSTER)
        hi = min(len(zeilen), zeile + FENSTER)
        umfeld = "\n".join(
            l for l in zeilen[lo:hi] if not EIGENE_SPDX.search(l))
        if not LIZENZ.search(umfeld):
            ohne.append((rel, zeile, text))
    return treffer, ohne


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py — Liste, kein Tor.

    Bewusst kein Fehler: eine Attribution ohne Lizenz ist nichts
    Verbotenes, sondern etwas Entscheidungsbeduerftiges (MF-636).
    """
    return []


def main() -> int:
    repo = pathlib.Path(__file__).resolve().parent.parent
    treffer, ohne = messe(repo)
    print("Attributionen gesamt          : %d" % len(treffer))
    print("davon ohne Lizenz DANEBEN     : %d" % len(ohne))
    if "--liste" in sys.argv:
        print()
        for rel, zeile, text in ohne:
            print("  %s:%d  %s" % (rel, zeile, text[:70]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
