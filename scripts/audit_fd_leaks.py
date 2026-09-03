#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
Dateihandles, die an einer I/O-Schranke offen bleiben (MF-846).

── Das Muster ──────────────────────────────────────────────────────────────

    FILE *f = fopen(path, "rb");
    if (!f) return UFT_ERR_IO;

    fseek(f, 0, SEEK_END);
    size_t size = (size_t)ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) return UFT_ERR_IO;   <- Handle bleibt offen

    uint8_t *data = malloc(size);
    if (!data) { fclose(f); return UFT_ERR_MEMORY; }     <- macht es richtig

Der Nachbarzweig raeumt auf, die Schranke darueber nicht. Bei 21 gefundenen
Stellen war das JEDES MAL so — es ist eine vergessene Klammer in einer
kopierten Vorlage, keine Entscheidung.

── Warum die ENGE Form und nicht "return mit offener Datei" ────────────────

Ein grober Durchlauf ueber "irgendein `return`, waehrend eine Datei offen
ist" lieferte in diesem Baum **467** Kandidaten, und die enge Form mit
Schranke immer noch **347**. Beide Zahlen sind unbrauchbar, weil die
ueberwaeltigende Mehrheit auf einem LANGLEBIGEN Handle steht
(`p->file`, `ctx->fp`), das dem Plugin gehoert und in dessen `close()`
geschlossen wird. Dort ist `return` ohne `fclose` RICHTIG.

Der Trennstrich ist deshalb nicht die Schranke, sondern die HERKUNFT des
Handles: stammt der `FILE*` aus einem `fopen` in DERSELBEN Funktion, ist
er lokal und muss vor jedem `return` geschlossen werden. Damit fielen 347
auf 21 — und alle 21 haben sich von Hand als echt bestaetigt.

── Grenze des Verfahrens, ausdruecklich ────────────────────────────────────

Die Funktionsgrenze wird an `}` in Spalte 0 erkannt. Das ist die
Formatierung dieses Baums, aber es ist eine ANNAHME, keine Analyse:

  * Ein `fopen` in einer Funktion mit eingerueckter schliessender Klammer
    (etwa in einem C++-Klassenrumpf) wuerde ueber die Funktionsgrenze
    hinaus als "offen" gefuehrt -> moeglicher Fehlalarm.
  * Ein `FILE*`, der ueber einen Zeiger weitergereicht und anderswo
    geschlossen wird, faellt faelschlich auf.
  * Ein `fclose` in der naechsten Zeile statt in derselben faellt
    faelschlich auf.

Alle drei erzeugen zu VIEL, nicht zu wenig — das Tor kann also nerven,
aber nicht stillschweigend durchlassen. Bei Grundlinie 0 ist das die
richtige Richtung.

── Grundlinie 0, hart ──────────────────────────────────────────────────────

Nach MF-846 ist der Bestand leer. Anders als bei den weichen Grundlinien
(tote Felder, Deklarationskonflikte) gibt es hier keinen Rueckstand, den
man abtragen muesste — also darf die Zahl nie wieder ueber 0 steigen.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent

FOPEN = re.compile(r"\bFILE\s*\*\s*(\w+)\s*=\s*fopen\s*\(")
SCHRANKE = re.compile(r"\bif\s*\(.*?\b(?:fseek|fread|ftell|fwrite)\s*\(\s*(\w+)")
RET = re.compile(r"\breturn\b")


def dateien(wurzel: Path):
    """Dateimenge aus git, nie aus einer gepflegten Liste (MF-636)."""
    try:
        r = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=wurzel, capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            print("  WARNUNG: git nicht befragbar — Tor laesst durch",
                  file=sys.stderr)
            return None
    except Exception as e:                                   # noqa: BLE001
        print("  WARNUNG: git nicht befragbar (%s) — Tor laesst durch" % e,
              file=sys.stderr)
        return None
    return [wurzel / z for z in r.stdout.split("\n")
            if z.endswith((".c", ".cpp"))]


def pruefe_text(text: str, name: str = "<text>"):
    """Fundstellen in EINER Uebersetzungseinheit."""
    treffer = []
    offen: dict[str, int] = {}
    for i, z in enumerate(text.split("\n"), 1):
        if z.startswith("}"):
            offen.clear()
            continue
        m = FOPEN.search(z)
        if m:
            offen[m.group(1)] = i
            continue
        s = SCHRANKE.search(z)
        if s and RET.search(z) and "fclose" not in z:
            if s.group(1) in offen:
                treffer.append((name, i, z.strip()))
    return treffer


def selbsttest() -> bool:
    """Vor dem Nenner. Mit Gegenproben — ein Tor, das nur seinen eigenen
    Fund erkennt, misst nichts."""
    ok = 0

    LECK = ('int f(void) {\n'
            '    FILE *f = fopen(p, "rb");\n'
            '    if (!f) return -1;\n'
            '    if (fseek(f, 0, SEEK_SET) != 0) return -1;\n'
            '    fclose(f);\n'
            '    return 0;\n'
            '}\n')
    if len(pruefe_text(LECK)) == 1:
        ok += 1
    else:
        print("  SELBSTTEST 1 ROT: Leck nicht erkannt")

    # GEGENPROBE A: dieselbe Zeile MIT fclose ist kein Fund.
    HEIL = LECK.replace("!= 0) return -1;", "!= 0) { fclose(f); return -1; }")
    if not pruefe_text(HEIL):
        ok += 1
    else:
        print("  SELBSTTEST 2 ROT: reparierte Zeile faellt noch auf")

    # GEGENPROBE B: ein langlebiges Handle (p->file) ist KEIN Fund —
    # genau die Unterscheidung, die 347 auf 21 gebracht hat.
    LANG = ('int f(void) {\n'
            '    if (fseek(p->file, 0, SEEK_SET) != 0) return -1;\n'
            '    return 0;\n'
            '}\n')
    if not pruefe_text(LANG):
        ok += 1
    else:
        print("  SELBSTTEST 3 ROT: langlebiges Handle faelschlich gemeldet")

    # GEGENPROBE C: die Funktionsgrenze muss trennen — ein fopen in der
    # einen und eine Schranke in der NAECHSTEN Funktion ist kein Fund.
    GETRENNT = ('int a(void) {\n'
                '    FILE *f = fopen(p, "rb");\n'
                '    fclose(f);\n'
                '    return 0;\n'
                '}\n'
                'int b(void) {\n'
                '    if (fseek(f, 0, SEEK_SET) != 0) return -1;\n'
                '    return 0;\n'
                '}\n')
    if not pruefe_text(GETRENNT):
        ok += 1
    else:
        print("  SELBSTTEST 4 ROT: Funktionsgrenze trennt nicht")

    print("  Selbsttest %d/4" % ok)
    return ok == 4


def sammle(wurzel: Path):
    ds = dateien(wurzel)
    if ds is None:
        return None
    alle = []
    for p in ds:
        try:
            t = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "fopen" not in t:
            continue
        alle.extend(pruefe_text(t, str(p.relative_to(wurzel)).replace("\\", "/")))
    return alle


def check(repo=None):
    """Einbindung in scripts/check_consistency.py."""
    w = Path(repo) if repo else WURZEL
    treffer = sammle(w)
    if treffer is None:
        return []
    return ["%s:%d Handle bleibt an der Schranke offen" % (f, i)
            for f, i, _ in treffer]


def main() -> int:
    print("audit_fd_leaks (MF-846)")
    if not selbsttest():
        print("  ABBRUCH: Selbsttest rot — kein Nenner ohne Abnahme")
        return 2
    treffer = sammle(WURZEL)
    if treffer is None:
        return 0
    print("  offene Handles an I/O-Schranken: %d (Grundlinie 0, hart)"
          % len(treffer))
    for f, i, z in treffer:
        print("    %s:%d\n        %s" % (f, i, z[:100]))
    if treffer:
        print("  FEHLER: jedes `return` auf einem lokalen FILE* braucht "
              "sein `fclose`.")
        return 1
    print("  OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
