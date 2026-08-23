#!/usr/bin/env python3
"""Was hinter den Skeleton-Bannern wirklich steht (ARCH-3, MF-511).

── Warum es dieses Skript gibt ──────────────────────────────────────────

`audit_skeleton_headers.py` zaehlt laut eigenem Kopfkommentar nur
`uft_*`-praefixierte Prototypen. Es meldet 0 — und das stimmt fuer diese
Teilmenge. ARCH-3 hat gemessen, dass 22 der 34 Banner-Header trotzdem
Prototypen ohne Definition tragen, weil die ohne Praefix auskommen.

Dieses Skript misst die Banner-Header vollstaendig und teilt jeden
fehlenden Prototypen in genau eine von drei Lagen ein — die drei, die
ueberhaupt zulaessige Ausgaenge haben:

  PHANTOM     kein Aufrufer, keine Definition. Ein Versprechen, das
              niemand einloest und niemand einfordert. Derselbe Fall wie
              uft_audit_trail.h / uft_forensic_report.h in MF-366, wo die
              Antwort Loeschen war.

  GEBROCHEN   ein Aufrufer, aber keine Definition. Das ist schlimmer als
              PHANTOM: hier haengt Code an einem Symbol, das der Linker
              nur deshalb nicht anmahnt, weil die Uebersetzungseinheit
              nicht mitgebaut wird.

  UMBENANNT   keine Definition unter DIESEM Namen, aber es gibt eine
              Definition, deren Name den Prototypnamen enthaelt oder
              umgekehrt (z.B. `woz_read_track` gegen
              `uft_woz_read_track`). Der Verdacht ist dann nicht
              "fehlt", sondern "zweite Namensfassung derselben Sache" —
              und das ist zu pruefen, nicht zu loeschen.

Die Einteilung ist eine MESSUNG, keine Entscheidung. Was mit einem Fall
geschieht, entscheidet ein Mensch oder ein eigener Commit mit Beleg.
"""
from __future__ import annotations

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]

BANNER = re.compile(r"(?i)\b(skeleton|stub[- ]?header|not implemented)\b")

# Prototyp: Rueckgabetyp + Name + '(' ... ')' + ';' auf Dateiebene.
PROTO = re.compile(
    r"^[ \t]*(?!#)(?:extern[ \t]+)?"          # kein Praeprozessor
    r"(?!return\b|else\b|typedef\b)"
    r"[A-Za-z_][\w \t*]*?[ \t*]"               # Rueckgabetyp
    r"(\w+)[ \t]*\([^;{]*\)[ \t]*;",           # Name(...) ;
    re.M)

# Definition: dasselbe, aber mit '{' statt ';'.
#
# Das `\s*` vor dem `{` ist nicht kosmetisch. Die erste Fassung verlangte
# `\)[ \t]*\{` — also die Klammer auf DERSELBEN Zeile. Der verbreitete
# Stil in diesem Baum setzt sie auf die naechste:
#
#     uft_error_t uft_verify_result_to_json(const uft_verify_result_t *r,
#                                           char *buf)
#     {
#
# Damit meldete das Skript vier definierte Funktionen als "GEBROCHEN".
# Ein Messwerkzeug, das falsch misst, ist schlimmer als keines — dieselbe
# Lehre wie beim Verwaisten-Tor (306 gegen 228, MF-509).
DEFN = re.compile(
    r"^[ \t]*(?:static[ \t]+|inline[ \t]+|extern[ \t]+)*"
    r"[A-Za-z_][\w \t*]*?[ \t*]"
    r"(\w+)[ \t]*\([^;{]*\)\s*\{",
    re.M)

CALL = re.compile(r"\b(\w+)[ \t]*\(")

SKIP_NAMES = {"if", "for", "while", "switch", "sizeof", "return", "defined"}


def strip_comments(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    return re.sub(r"//[^\n]*", " ", t)


def sources() -> list[pathlib.Path]:
    out = []
    for d in ("src", "tests"):
        for ext in ("*.c", "*.cpp"):
            out += list((ROOT / d).rglob(ext))
    return out


def main() -> int:
    headers = [p for p in (ROOT / "include").rglob("*.h")]
    headers += [p for p in (ROOT / "src").rglob("*.h")]

    banner = []
    for h in headers:
        t = h.read_text(encoding="utf-8", errors="replace")
        head = t[:4000]
        if BANNER.search(head):
            banner.append((h, t))

    # Alle Definitionen und alle Aufrufstellen im Baum, einmal.
    defined: set[str] = set()
    called: dict[str, list[str]] = {}
    for s in sources():
        t = strip_comments(s.read_text(encoding="utf-8", errors="replace"))
        for m in DEFN.findall(t):
            defined.add(m)
        rel = str(s.relative_to(ROOT)).replace("\\", "/")
        for m in CALL.findall(t):
            if m not in SKIP_NAMES:
                called.setdefault(m, []).append(rel)

    rows = []
    for h, t in banner:
        body = strip_comments(t)
        protos = {m for m in PROTO.findall(body) if m not in SKIP_NAMES}
        missing = sorted(p for p in protos if p not in defined)
        if not missing:
            rows.append((h, protos, [], [], []))
            continue
        phantom, broken, renamed = [], [], []
        for name in missing:
            kin = [d for d in defined
                   if (name in d or d in name) and abs(len(d) - len(name)) <= 8
                   and d != name]
            if kin:
                renamed.append((name, sorted(kin)[:3]))
            elif called.get(name):
                broken.append((name, sorted(set(called[name]))[:3]))
            else:
                phantom.append(name)
        rows.append((h, protos, phantom, broken, renamed))

    rows.sort(key=lambda r: -(len(r[2]) + len(r[3]) + len(r[4])))

    n_stale = sum(1 for r in rows if not (r[2] or r[3] or r[4]))
    n_ph = sum(len(r[2]) for r in rows)
    n_br = sum(len(r[3]) for r in rows)
    n_re = sum(len(r[4]) for r in rows)

    print("Banner-Header gesamt          : %d" % len(rows))
    print("  davon Banner veraltet       : %d" % n_stale)
    print("  davon mit offenen Prototypen: %d" % (len(rows) - n_stale))
    print()
    print("Prototypen ohne Definition    : %d" % (n_ph + n_br + n_re))
    print("  PHANTOM   (kein Aufrufer)   : %d" % n_ph)
    print("  GEBROCHEN (Aufrufer da)     : %d" % n_br)
    print("  UMBENANNT (Namensverwandter): %d" % n_re)

    if "--detail" not in sys.argv:
        return 0

    print("\n" + "=" * 72)
    for h, protos, ph, br, re_ in rows:
        if not (ph or br or re_):
            continue
        rel = str(h.relative_to(ROOT)).replace("\\", "/")
        print("\n%s  (%d Prototypen)" % (rel, len(protos)))
        for n in ph:
            print("    PHANTOM    %s" % n)
        for n, where in br:
            print("    GEBROCHEN  %-32s <- %s" % (n, ", ".join(where)))
        for n, kin in re_:
            print("    UMBENANNT  %-32s ~ %s" % (n, ", ".join(kin)))

    print("\n" + "=" * 72)
    print("Banner veraltet (nichts offen) — Banner gehoert weg:")
    for h, protos, ph, br, re_ in rows:
        if ph or br or re_:
            continue
        print("    %s  (%d Prototypen, alle definiert)"
              % (str(h.relative_to(ROOT)).replace("\\", "/"), len(protos)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
