#!/usr/bin/env python3
"""Wer fuellt `uft_track_t` von Hand statt ueber die API? (MF-516)

── Der Vertrag ──────────────────────────────────────────────────────────

`uft_track_t.sectors` ist ein **dynamischer Zeiger** mit `sector_capacity`,
kein Feld:

    uft_sector_t*  sectors;          ///< Dynamic sector array
    size_t         sector_count;
    size_t         sector_capacity;
    uft_sector_t   sectors_fixed[64]; ///< Legacy fixed array

Gefuellt wird er ueber `uft_track_add_sector()`, das bei Bedarf
realloziert. `uft_track_init()` alloziert **nichts** — es nullt die
Struktur und setzt Zylinder und Kopf. Nach `uft_track_init()` ist
`sectors` also NULL.

Wer in einem `read_track` direkt `track->sectors[s] = ...` schreibt,
schreibt damit durch einen Nullzeiger — unabhaengig davon, was der
Aufrufer getan hat. So ein `read_track` kann nie funktioniert haben.

Gefunden wurde das an `mgt_read_track` (MF-516): der Fuzzer reichte eine
gueltige D81-Datei an MGT weiter, dessen Sonde zugestimmt hatte, und der
Prozess fiel.

── Was dieses Skript meldet ─────────────────────────────────────────────

Fuer jede Funktion, deren Name auf `read_track` endet und die ein
`uft_track_t*` nimmt:

  SCHREIBT   ein direktes `->sectors[...] = ` im Rumpf, OHNE dass der
             Rumpf je eine der Anlege-APIs ruft. Das ist der Fehler:
             entweder ueber uft_track_add_sector() gehen, oder `sectors`
             selbst auf `sectors_fixed` zeigen lassen und
             `sector_capacity` setzen. Wer die API benutzt und danach in
             sectors[] nachfasst, steht hier NICHT — das ist die uebliche
             Nachbearbeitung und korrekt.
  OHNE-INIT  kein `uft_track_init()` im Rumpf. Fuer sich genommen kein
             Fehler — die Struktur kommt vom Aufrufer — aber zusammen mit
             SCHREIBT ist es einer.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

FUNC = re.compile(
    r"^[ \t]*(?:static[ \t]+)?uft_error_t[ \t]+(\w*read_track\w*)[ \t]*\("
    r"[^;{]*uft_track_t[ \t]*\*[^;{]*\)[ \t]*\{", re.M)

WRITE = re.compile(r"->sectors\s*\[[^\]]+\]\s*(?:\.\w+\s*)?=(?!=)")

# Die APIs, die `sectors` ueberhaupt erst anlegen. Wer eine davon benutzt,
# darf danach in sectors[] nachfassen — das ist kein Fehler, sondern die
# uebliche Nachbearbeitung (z.B. `.deleted = true` am zuletzt
# hinzugefuegten Sektor).
#
# Die erste Fassung dieses Skripts unterschied das nicht und meldete 18
# Fehler, darunter `dsk_read_track`, das nachweislich laeuft. Ein
# Messwerkzeug, das falsch misst, ist schlimmer als keines — dieselbe
# Lehre wie beim Verwaisten-Tor (306 gegen 228) und beim Banner-Audit
# (12 gegen 6).
ADD_API = re.compile(r"\b(uft_track_add_sector|uft_format_add_sector|"
                     r"uft_track_reserve_sectors)\s*\(")


def body_of(text: str, start: int) -> str:
    depth, i = 0, text.index("{", start)
    j = i
    while j < len(text):
        if text[j] == "{":
            depth += 1
        elif text[j] == "}":
            depth -= 1
            if depth == 0:
                return text[i:j + 1]
        j += 1
    return text[i:]


def main() -> int:
    bad, noinit, ok = [], [], 0
    for p in sorted((ROOT / "src").rglob("*.c")):
        t = p.read_text(encoding="utf-8", errors="replace")
        for m in FUNC.finditer(t):
            body = body_of(t, m.start())
            rel = str(p.relative_to(ROOT)).replace("\\", "/")
            line = t[:m.start()].count("\n") + 1
            has_init = "uft_track_init" in body
            uses_api = ADD_API.search(body) is not None
            writes = WRITE.search(body)
            if writes and not uses_api:
                bad.append((rel, line, m.group(1), has_init))
            elif not has_init:
                noinit.append((rel, line, m.group(1)))
            else:
                ok += 1

    print("read_track-Funktionen geprueft: %d" % (len(bad) + len(noinit) + ok))
    print("  sauber (init + API)         : %d" % ok)
    print("  ohne uft_track_init         : %d" % len(noinit))
    print("  SCHREIBT direkt in sectors[]: %d" % len(bad))

    if bad:
        print("\nFEHLER — schreibt durch einen Zeiger, den niemand setzt:")
        for rel, line, fn, hi in bad:
            print("    %s:%d  %s()%s"
                  % (rel, line, fn, "" if hi else "  (auch ohne uft_track_init)"))
    if "--detail" in sys.argv and noinit:
        print("\nOhne uft_track_init (fuer sich kein Fehler):")
        for rel, line, fn in noinit:
            print("    %s:%d  %s()" % (rel, line, fn))

    return 1 if bad else 0


def check(repo) -> list:
    """Schnittstelle fuer check_consistency.py."""
    out = []
    for p in sorted((repo / "src").rglob("*.c")):
        t = p.read_text(encoding="utf-8", errors="replace")
        for m in FUNC.finditer(t):
            body = body_of(t, m.start())
            if WRITE.search(body) and not ADD_API.search(body):
                rel = str(p.relative_to(repo)).replace("\\", "/")
                out.append("%s: %s() schreibt direkt in track->sectors[] — "
                           "der Zeiger ist NULL, bis uft_track_add_sector() "
                           "ihn setzt" % (rel, m.group(1)))
    return out


if __name__ == "__main__":
    sys.exit(main())
