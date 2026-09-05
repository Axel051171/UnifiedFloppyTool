#!/usr/bin/env python3
"""Wer „ich kann schreiben" sagt, muss in seiner Datei eine Schreiboperation
haben (MF-883).

── Warum es dieses Tor gibt ────────────────────────────────────────────

Ein `write_track`, das seinen Puffer aendert und `UFT_OK` meldet, ohne dass
ein Byte die Platte erreicht, ist in einem forensischen Werkzeug schlimmer
als ein Fehler: der Benutzer glaubt, gespeichert zu haben.

Diese Klasse ist dreimal aufgetreten:

  MF-522  D64/D81  — dort steht die Lehre im Kommentar: „Eine Erfolgs-
                     meldung ohne Tat ist in einem forensischen Werkzeug
                     schlimmer als ein Fehler."
  MF-880  PRO      — die Zusage stand an DREI Stellen: Rueckgabewert,
                     Merkmalstafel (`{"Write", SUPPORTED}`) und
                     `.capabilities` (`UFT_FORMAT_CAP_WRITE`).
  MF-883  NEUN     — 86F, CP/M, CQM, DCM, DMS, IMD, MSA, SAP, SCL.

Beim zweiten und dritten Mal hat kein Tor gewarnt, obwohl seit MF-658 eines
laeuft: `tests/test_capability_manifest.c:147` misst `"Write"` gegen
`p->write_track != NULL` — es prueft, ob ein FUNKTIONSZEIGER EXISTIERT,
nicht ob die Funktion etwas tut (P3-154). Ein Speicher-Schreiber erfuellt
diese Bedingung vollstaendig.

── Was hier gemessen wird, und was ausdruecklich nicht ─────────────────

Gemessen wird eine SCHARFE, nicht raterische Bedingung:

    ein Plugin beansprucht `UFT_FORMAT_CAP_WRITE`
    UND hat ein `write_track` != NULL
    UND in seiner ganzen Datei steht keine einzige Schreiboperation

„Keine einzige" heisst: kein `fwrite`, `fputc`, `fputs`, `fprintf`,
`ftruncate`, `_chsize`, `WriteFile`, `pwrite` — im kommentar- und
zeichenkettenfreien Text. Das ist bewusst KONSERVATIV: eine Datei, in der
irgendwo ein `fwrite` steht, wird durchgelassen, auch wenn dieses `fwrite`
vom Schreibpfad aus unerreichbar ist. Eine Erreichbarkeitsanalyse waere
genauer und zugleich ratender; ein Tor, das falsch meckert, wird
abgeschaltet.

Die Luecke ist damit benannt statt versteckt: es gibt im Baum Dateien mit
einem echten `uft_<fmt>_write()` OHNE Aufrufer neben einem Plugin-Pfad, der
nur in den Speicher schreibt (u.a. mgt, udi, apridisk, cfi, posix, qrst,
rcpmfs, hardsector). Dieses Tor sieht sie NICHT. Wer sie angeht, braucht
den Prueffall aus P3-154: schreiben -> `close()` -> NEU OEFFNEN -> lesen
-> vergleichen, je Format mit einem Pruefabbild.

Und ein zweiter Befund gehoert dazu gesagt, weil er die Lage verschaerft:
`plugin->flush` wird im ganzen Baum von NIEMANDEM gerufen (gemessen ueber
`git ls-files`, kommentarfrei). `uft_disk_close()` ruft nur `close`. Selbst
ein Plugin MIT Flush kaeme nicht durch. Solange das so ist, ist ein
Speicher-Schreiber ohne `fwrite` in der eigenen Datei ausnahmslos wirkungslos.

── Grundlinie ──────────────────────────────────────────────────────────

0, seit MF-883. Sie darf nur fallen. Ein neuer Fall ist keine Nachlaessig-
keit, sondern eine Entscheidung — und die gehoert begruendet, nicht
gemittelt.

Dateimenge aus `git ls-files` (MF-636), nicht aus einer gepflegten Liste.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

GRUNDLINIE = 0   # Plugins mit Schreibzusage ohne jede Schreiboperation

SCHREIBEN = re.compile(
    r"\b(fwrite|fputc|fputs|fprintf|ftruncate|_chsize|WriteFile|pwrite)\s*\(")

STRUKT = re.compile(r"uft_format_plugin_t\s+(\w+)\s*=\s*\{(.*?)\n\}\s*;", re.S)


def entkerne(t: str) -> str:
    """Kommentare und Zeichenketten raus — ein `fwrite` im Kommentar ist
    keine Tat, und ein `"fwrite"` als Text erst recht nicht."""
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    t = re.sub(r"//[^\n]*", " ", t)
    t = re.sub(r'"(\\.|[^"\\])*"', '""', t)
    t = re.sub(r"'(\\.|[^'\\])*'", "''", t)
    return t


def dateien(repo: Path) -> list:
    try:
        r = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard",
             "*.c"],
            cwd=str(repo), capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if r.returncode != 0:
        return None
    return [p for p in r.stdout.replace("\r", "").split("\n") if p.endswith(".c")]


def messe(repo: Path):
    """-> (befunde, fehler). befunde ist None, wenn git nicht befragbar war."""
    pfade = dateien(repo)
    if pfade is None:
        # Lieber alles durchlassen UND es sagen, als still eine Luecke lassen.
        return None, ["Guard-frei: `git ls-files` war nicht befragbar, "
                      "dieses Tor hat NICHTS geprueft."]

    befunde = []
    for rel in pfade:
        p = repo / rel
        try:
            roh = p.read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "uft_format_plugin_t" not in roh:
            continue
        t = entkerne(roh)
        if "uft_format_plugin_t" not in t:
            continue
        hat_schreiben = bool(SCHREIBEN.search(t))
        if hat_schreiben:
            continue           # konservativ: Datei kann schreiben

        for m in STRUKT.finditer(t):
            koerper = m.group(2)
            if "UFT_FORMAT_CAP_WRITE" not in koerper:
                continue
            wt = re.search(r"\.write_track\s*=\s*(\w+)", koerper)
            if not wt or wt.group(1) == "NULL":
                continue
            name = "?"
            roh_m = re.search(
                re.escape(m.group(1)) + r"\s*=\s*\{(.*?)\n\}\s*;", roh, re.S)
            if roh_m:
                nm = re.search(r'\.name\s*=\s*"([^"]*)"', roh_m.group(1))
                if nm:
                    name = nm.group(1)
            befunde.append((rel, name, wt.group(1)))
    return befunde, []


def check(repo) -> list:
    befunde, fehler = messe(Path(repo))
    if befunde is None:
        return fehler

    for rel, name, fn in befunde:
        fehler.append(
            "%s (Plugin \"%s\"): beansprucht UFT_FORMAT_CAP_WRITE und hat "
            "`.write_track = %s`, aber in der ganzen Datei steht keine "
            "Schreiboperation. Ein `write_track`, das UFT_OK meldet, ohne "
            "dass ein Byte die Platte erreicht, ist schlimmer als ein "
            "Fehler. Entweder wirklich schreiben (dann gilt die "
            "EINFRIER-REGEL: benannte Referenz, gemessene Zahlen, Referenz "
            "im Header) — oder die Zusage an ALLEN DREI Stellen "
            "zuruecknehmen: Rueckgabewert, Merkmalstafel, .capabilities."
            % (rel, name, fn))

    if len(befunde) > GRUNDLINIE:
        fehler.append(
            "%d Plugin(s) mit Schreibzusage ohne Schreiboperation, "
            "Grundlinie %d. Die Grundlinie darf nur fallen."
            % (len(befunde), GRUNDLINIE))
    return fehler


PLUGIN_GERUEST = """
#include "uft/uft_format_plugin.h"
static uft_error_t x_write_track(uft_disk_t *d, int c, int h,
                                 const uft_track_t *t) {
%s
    return UFT_OK;
}
const uft_format_plugin_t uft_format_plugin_x = {
    .name = "X",
    .write_track = %s,
    .capabilities = %s,
};
"""


def _selbsttest(repo: Path) -> int:
    """Vor dem Nenner (MF-693). Eine Erstfassung meldete einmal „3/3" und
    lieferte gemessen 0/3 — seither laeuft der Selbsttest ueber `check()`
    selbst, nicht ueber eine Hilfsfunktion."""
    import tempfile

    faelle = [
        # (Rumpf, write_track-Zeiger, capabilities, soll meckern?)
        ("    memcpy(d->plugin_data, t, 4);", "x_write_track",
         "UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE", True),
        # echtes fwrite in der Datei -> durchlassen
        ("    fwrite(t, 1, 4, (FILE*)d->plugin_data);", "x_write_track",
         "UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE", False),
        # keine Schreibzusage -> egal
        ("    memcpy(d->plugin_data, t, 4);", "x_write_track",
         "UFT_FORMAT_CAP_READ", False),
        # write_track NULL -> egal
        ("    memcpy(d->plugin_data, t, 4);", "NULL",
         "UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE", False),
        # fwrite NUR im Kommentar -> muss meckern (Entkernung greift)
        ("    /* frueher stand hier fwrite(...) */\n"
         "    memcpy(d->plugin_data, t, 4);", "x_write_track",
         "UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE", True),
        # fwrite nur als Zeichenkette -> muss meckern
        ('    log("fwrite(x)");', "x_write_track",
         "UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE", True),
    ]

    gut = 0
    for i, (rumpf, zeiger, caps, soll) in enumerate(faelle, 1):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True)
            (p / "plug.c").write_text(PLUGIN_GERUEST % (rumpf, zeiger, caps),
                                      encoding="utf-8")
            errs = check(p)
            hat = any("Schreiboperation" in e for e in errs)
            if hat == soll:
                gut += 1
            else:
                print("  Selbsttest %d: erwartet meckern=%s, bekommen %s"
                      % (i, soll, hat))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest(repo)

    if "-v" in sys.argv or "--list" in sys.argv:
        befunde, _ = messe(repo)
        if befunde is not None:
            print("Schreibzusage ohne Schreiboperation: %d (Grundlinie %d)"
                  % (len(befunde), GRUNDLINIE))
            for rel, name, fn in befunde:
                print("  %-46s %-14s %s" % (rel, name, fn))
            print()

    errs = check(repo)
    if not errs:
        print("OK: kein Plugin verspricht Schreiben ohne Schreiboperation.")
        return 0
    print("FAIL: %d Befund(e)" % len(errs))
    for e in errs:
        print("  %s" % e)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
