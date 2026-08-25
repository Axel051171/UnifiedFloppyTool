#!/usr/bin/env python3
"""Ein TODO ohne Plan ist ein Bug (MF-587)

── Die Regel, die es schon gab ──────────────────────────────────────────

`.claude/CLAUDE.md`, woertlich:

    Keine `TODO`/`FIXME`/`XXX`-Kommentare in committed Code ohne
    Issue-Link oder Eintrag in `KNOWN_ISSUES.md`. „TODO" ohne Plan = Bug.

Gemessen bei der Release-Vorbereitung 4.1.6: **35 Marken, davon 33 ohne
jeden Verweis.** Die Regel stand seit Monaten und niemand hat sie
gezaehlt.

── Warum das zu diesem Release gehoert ──────────────────────────────────

Ein unverwiesenes TODO ist eine Absichtserklaerung ohne Adressaten. Es
sieht aus wie ein Plan und ist keiner — dieselbe Form wie alles andere,
was diese Pruefsitzung gefunden hat:

    "For now, just copy the file"       -> "Conversion complete!"
    "sample entries based on format"    -> eine Dateiliste
    "mark all as allocated for now"     -> eine gruene Belegungskarte

In allen drei Faellen stand die Absicht im Quelltext, und die Wirkung war
eine Behauptung. Der Unterschied zwischen einem ehrlichen Stub und einer
stillen Luecke ist genau der Verweis: WER kuemmert sich, und WO steht es.

── Was als Verweis zaehlt ───────────────────────────────────────────────

`MF-NNN`, `PN-NN`, `#NNN`, oder ein Verweis auf `KNOWN_ISSUES.md` /
`OPEN_ITEMS.md` in derselben Zeile oder den zwei Zeilen darueber. Ein
Verweis auf ein TODO-Dokument (`*_INTEGRATION_TODO.md`) zaehlt ebenfalls
— das IST der Plan.

── Was dieses Tor NICHT kann ────────────────────────────────────────────

Es prueft, ob ein Verweis DASTEHT, nicht ob er stimmt. Ein `MF-999`, das
es nie gab, sieht fuer das Tor richtig aus. Es ist eine Schranke gegen
das unverwiesene TODO, kein Beleg fuer einen echten Plan.

Die Grundlinie ist die gemessene Zahl bei Einfuehrung. Sie darf sinken,
nie steigen — und das ist der Punkt: neue unverwiesene TODOs feuern,
der Altbestand wird abgearbeitet statt eingefroren.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

MARK = re.compile(r"\b(TODO|FIXME|XXX)\b")
REF = re.compile(r"MF-\d+|P\d-\d+|#\d+|KNOWN_ISSUES|OPEN_ITEMS|_TODO\.md")

EXTS = {".c", ".cpp", ".h", ".hpp"}
ROOTS = ("src", "include")

# Gemessen bei Einfuehrung (MF-587). Sinken erlaubt, steigen nicht.
#
# Die erste Zahl im Kopf dieses Tors war 31 — aus einem `grep`, das
# `XXX:` MIT Doppelpunkt suchte. Das Tor sucht auf Wortgrenze. Es findet
# damit 35 Marken, davon 2 mit Verweis
# (`TODO(docs/KNOWN_ISSUES.md §7.4)` in uft_adf.c), bleiben 33.
#
# Zwei Zaehlungen derselben Sache, und die bequemere war die kleinere.
# Genau die Form, gegen die dieses Release geschrieben ist — hier im
# eigenen Werkzeug, gefunden weil die Zahlen nicht zusammenpassten.
BASELINE_MAX = 33


def scan(repo: Path) -> list[tuple[str, int, str]]:
    hits: list[tuple[str, int, str]] = []
    for root in ROOTS:
        d = repo / root
        if not d.exists():
            continue
        for p in sorted(d.rglob("*")):
            if p.suffix not in EXTS or not p.is_file():
                continue
            try:
                lines = p.read_text(encoding="utf-8",
                                    errors="replace").splitlines()
            except OSError:
                continue
            rel = p.relative_to(repo).as_posix()
            for i, line in enumerate(lines):
                if not MARK.search(line):
                    continue
                # Der Verweis darf in dieser Zeile oder den zwei
                # darueber stehen — ein Kommentarblock erklaert oft
                # zuerst und markiert dann.
                window = "\n".join(lines[max(0, i - 2):i + 1])
                if REF.search(window):
                    continue
                hits.append((rel, i + 1, line.strip()[:110]))
    return hits


def check(repo: Path) -> list[str]:
    hits = scan(repo)
    if len(hits) <= BASELINE_MAX:
        return []
    errors = [
        f"{len(hits)} unverwiesene TODO/FIXME/XXX — die Grundlinie ist "
        f"{BASELINE_MAX} (gemessen MF-587). Sie darf sinken, nicht steigen."
    ]
    for rel, ln, txt in hits[BASELINE_MAX:]:
        errors.append(f"  {rel}:{ln}: {txt}")
    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    hits = scan(repo)
    errs = check(repo)
    print(f"TODO/FIXME ohne Verweis (root={repo}):")
    print(f"  Grundlinie            : {BASELINE_MAX}")
    print(f"  gefunden              : {len(hits)}")
    print(f"  Befunde               : {len(errs)}")
    if "--list" in sys.argv:
        for rel, ln, txt in hits:
            print(f"    {rel}:{ln}: {txt}")
    for e in errs[:40]:
        print(f"    {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    sys.exit(main())
