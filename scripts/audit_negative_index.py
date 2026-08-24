#!/usr/bin/env python3
"""Ein Index aus cyl/head ohne untere Schranke (MF-560)

── Warum es dieses Tor gibt ─────────────────────────────────────────────

Die CI hat gefunden, was lokal nicht zu sehen ist:

    ERROR: AddressSanitizer: heap-buffer-overflow
    READ of size 4
      #0 mfi_read_track   src/formats/mfi/uft_mfi.c:205
      #1 run_one_plugin_open   tests/test_disk_open_fuzz.c:761

Die Stelle lautete:

    int idx = cyl * pdata->heads + head;
    if (idx >= pdata->track_count) return UFT_ERROR_INVALID_STATE;
    mfi_track_entry_t *te = &pdata->tracks[idx];
    if (te->compressed_size == 0) ...            <- Zeile 205

**Nur die obere Schranke.** Ein negatives `cyl` oder `head` ergibt einen
negativen Index, kommt an `>=` vorbei und greift vor das Feld.

MF-516/522 hat genau diese Klasse in 54 Dateien behoben — mit
`if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;` am
Funktionsanfang. Drei Stellen (mfi einmal, d77 zweimal) rechnen den Index
VOR der Schranke aus und sind dabei durchgerutscht.

**Sechster Fall in der Pruef-Sitzung MF-534…560, in dem eine reparierte
Stelle unreparierte Geschwister hatte** (MF-526, MF-550, MF-554, MF-555,
MF-559, jetzt MF-560). Von Hand findet man sie nicht zuverlaessig.

Lokal faellt das nicht auf: MinGW hat weder `libasan` noch `libubsan`
(`cannot find -lasan`). Ein Lesen knapp vor einem Feld faultet unter
Windows nicht — es liefert still falsche Daten. Dieses Tor ersetzt den
Sanitizer nicht, aber es faengt die Bauart, bevor die CI sie findet.

── Was gesucht wird ─────────────────────────────────────────────────────

In `src/formats/**`, je Funktion:

    (1)  ein Index wird aus `cyl` und/oder `head` berechnet
    (2)  er wird gegen eine OBERE Schranke geprueft (`>=` oder `>`)
    (3)  aber nirgends gegen 0

── Grenzen ──────────────────────────────────────────────────────────────

Textuell. Nicht gefunden werden Faelle, in denen die untere Schranke in
einer gerufenen Funktion steckt, oder in denen der Index aus einer
Zwischenvariablen kommt, die nicht `idx`/`index` heisst.

Ebenfalls nicht gemeldet werden Funktionen, die `cyl`/`head` nur
VERGLEICHEN statt zu indizieren — `jv3_read_track` und `nfd_read_track`
laufen ueber ihre Sektorliste und pruefen `s->c != cyl`. Ein negatives
`cyl` trifft dort nichts. Beide waren Fehlalarme des ersten Suchmusters
und sind hier ausdruecklich ausgenommen.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

# Ein Index, der aus cyl/head entsteht.
# NUR vorzeichenbehaftete Indizes.
#
# MF-560: der erste Lauf meldete sieben Treffer, SECHS davon Fehlalarme.
# Drei kamen daher, dass der Index `size_t` oder `uint32_t` ist: ein
# negativer Wert wird dort zu einer sehr grossen Zahl, und die OBERE
# Schranke faengt ihn. Zwei weitere, weil die Koordinaten selbst
# `uint8_t` sind und gar nicht negativ werden koennen. Einer, weil die
# untere Schranke `idx >= 0` lautet statt `idx < 0`.
#
# Ein Tor, das sechs von sieben danebenliegt, wird beim naechsten Mal
# nicht gelesen — und faengt dann auch den echten Fall nicht mehr.
IDX_FROM_COORD = re.compile(
    r"\b(?:int|long|signed)\s+(\w*(?:idx|index)\w*)\s*=\s*"
    r"[^;]*\b(?:cyl|cylinder|head)\b[^;]*;")

# Vorzeichenlose Koordinaten koennen nicht negativ werden.
UNSIGNED_COORDS = re.compile(
    r"\b(?:uint\d+_t|unsigned)\s+(?:cyl|cylinder|head)\b")

# Eine obere Schranke auf denselben Namen.
def upper_bound(name: str) -> re.Pattern:
    return re.compile(r"\b" + re.escape(name) + r"\s*(?:>=|>)\s*\w")

# Eine untere Schranke — auf den Index ODER auf die Koordinaten.
def lower_bound(name: str) -> re.Pattern:
    """Untere Schranke — auf den Index ODER auf die Koordinaten.

    MF-560: die erste Fassung kannte nur `name < 0`. `uft_ldbs.c` schreibt
    aber `if (idx >= 0 && idx < track_count)` — dieselbe Schranke, andere
    Richtung. Das Tor meldete sie als fehlend.
    """
    n = re.escape(name)
    return re.compile(
        r"\b" + n + r"\s*<\s*0"
        r"|\b" + n + r"\s*>=\s*0"
        r"|\b" + n + r"\s*>\s*-\s*1"
        r"|\b(?:cyl|cylinder|head)\s*<\s*0")

FUNC_START = re.compile(r"^[A-Za-z_][\w \t*]*\**\s*\w+\s*\([^;]*$")

BASELINE: dict[str, str] = {}


def _strip_comments(text: str) -> str:
    def keep(m):
        return "\n" * m.group(0).count("\n")
    text = re.sub(r"/\*.*?\*/", keep, text, flags=re.S)
    return re.sub(r"//[^\n]*", "", text)


def _functions(text: str):
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        if FUNC_START.match(lines[i]) and not lines[i].lstrip().startswith(
                ("if", "for", "while", "switch", "return", "else")):
            j = i
            while j < len(lines) and "{" not in lines[j]:
                if ";" in lines[j]:
                    j = -1
                    break
                j += 1
            if j < 0 or j >= len(lines):
                i += 1
                continue
            depth, k = 0, j
            while k < len(lines):
                depth += lines[k].count("{") - lines[k].count("}")
                if depth == 0 and k > j:
                    break
                k += 1
            yield i, lines[i:k + 1]
            i = k + 1
            continue
        i += 1


def check(repo: Path) -> list[str]:
    errors: list[str] = []
    d = repo / "src" / "formats"
    if not d.exists():
        return errors

    for p in sorted(d.rglob("*.c")):
        try:
            raw = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        text = _strip_comments(raw)
        rel = p.relative_to(repo).as_posix()

        for start, body_lines in _functions(text):
            body = "\n".join(body_lines)
            head_line = body_lines[0] if body_lines else ""
            if UNSIGNED_COORDS.search(head_line):
                continue          # uint8_t cyl / uint8_t head
            for m in IDX_FROM_COORD.finditer(body):
                name = m.group(1)
                if not upper_bound(name).search(body):
                    continue                     # gar keine Schranke -> anderer Fall
                if lower_bound(name).search(body):
                    continue                     # gedeckelt, gut
                lineno = start + body[:m.start()].count("\n") + 1
                key = f"{rel}:{lineno}:{name}"
                if key in BASELINE:
                    continue
                errors.append(
                    f"{rel}:{lineno}: `{name}` entsteht aus cyl/head und "
                    f"wird nur nach OBEN geprueft. Ein negatives `cyl` oder "
                    f"`head` kommt an `>=` vorbei und greift vor das Feld. "
                    f"Vorbild MF-560: ASan fand genau das in mfi_read_track, "
                    f"lokal unsichtbar (MinGW hat keinen Sanitizer). Fix: "
                    f"`if (cyl < 0 || head < 0) return "
                    f"UFT_ERROR_INVALID_PARAM;` VOR der Rechnung.")

    return errors


def main() -> int:
    repo = Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errs = check(repo)
    print(f"Index aus cyl/head ohne untere Schranke (root={repo}):")
    print(f"  begruendete Ausnahmen : {len(BASELINE)}")
    print(f"  Befunde               : {len(errs)}")
    for e in errs:
        print(f"    {e}")
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
