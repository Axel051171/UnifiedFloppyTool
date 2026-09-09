#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Tor 62: wer einen Sektor FUELLT, muss ihn KENNZEICHNEN (MF-980).

── DER ANLASS ────────────────────────────────────────────────────────

Neunzehn Sektor-Leser im Baum trugen dieselbe Zeile:

    if (fread(buf, 1, SS, p->file) != SS) {
        memset(buf, 0xE5, SS);
    }
    uft_format_add_sector(track, s, buf, SS, cyl, head);

Und `uft_format_add_sector_with_id()` setzt fuer JEDEN Sektor, den es
anlegt, unbedingt

    sector.status = UFT_SECTOR_OK;
    uft_sector_set_crc(&sector, true);      /* "good sector" */
    uft_sector_set_id_crc(&sector, true);

Ein abgeschnittenes Abbild lieferte damit Sektoren voller **erfundener**
0xE5, markiert als „gelesen, CRC gueltig" — von echten 0xE5-Daten nicht
zu unterscheiden. `uft_atr.c` nannte es im Kommentar sogar „forensic
fill on read error"; eine Fuellung ohne Kennzeichnung ist das Gegenteil
davon.

Das ist die dritte Zeile des Projektmottos: „Kein Bit verloren. Keine
stille Veraenderung. **Keine erfundenen Daten**."

Rotbeweis: `tests/test_kurze_datei_erfindet_keine_sektoren.c`. Er fiel
mit `Sektor 4 stand nicht in der Datei, gilt aber als gelesen
(status=0x00, crc_ok=1, data[0]=0xE5)`.

── WAS DIESES TOR PRUEFT ─────────────────────────────────────────────

Je `read_track`/`write_track` eines Plugins, im kommentar- und
zeichenkettenfreien Text:

    enthaelt der Rumpf ein `memset(...)` im Fehlerzweig eines `fread`
    UND einen `uft_format_add_sector*(...)`
    ABER KEIN `uft_format_mark_last_missing(...)`

Dann gilt: gefuellt, aber nicht gekennzeichnet.

── WAS ES AUSDRUECKLICH NICHT SIEHT ──────────────────────────────────

Es prueft die ANWESENHEIT der Kennzeichnung im selben Rumpf, nicht ihre
Erreichbarkeit. Ein `mark_last_missing` hinter einer Bedingung, die nie
wahr wird, wuerde durchgelassen. Das ist bewusst konservativ: eine
Erreichbarkeitsanalyse waere ratender, als ein Tor tragen darf — und die
Kennzeichnung steht in allen 19 Faellen direkt neben ihrer Bedingung.

Ebenso unsichtbar: Fuellungen, die nicht `memset` heissen (Schleife,
`calloc`), und Leser, die den Sektor gar nicht erst anlegen (das ist der
richtige Weg und braucht keine Kennzeichnung).

── Grundlinie ────────────────────────────────────────────────────────

0, seit MF-980. Sie darf nur dort bleiben.

Dateimenge aus `git ls-files` (MF-636), nicht aus einer Pflegeliste.
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

GRUNDLINIE = 0

FUELLT = re.compile(r"memset\s*\(")
FREAD = re.compile(r"\bfread\s*\(")
ANLEGT = re.compile(r"uft_format_add_sector(?:_with_id)?\s*\(")
KENNZEICHNET = re.compile(r"uft_format_mark_last_missing\s*\(")


def entkerne(t: str) -> str:
    t = re.sub(r"/\*.*?\*/", " ", t, flags=re.S)
    t = re.sub(r"//[^\n]*", " ", t)
    return re.sub(r'"(\\.|[^"\\])*"', '""', t)


def dateien(repo: Path):
    try:
        aus = subprocess.run(
            ["git", "ls-files", "--cached", "--others", "--exclude-standard"],
            cwd=str(repo), capture_output=True, text=True, timeout=60)
    except (OSError, subprocess.SubprocessError):
        return None
    if aus.returncode != 0:
        return None
    return [f for f in aus.stdout.split("\n")
            if f.startswith("src/") and f.endswith(".c")
            and not f.startswith(("src/samdisk", "src/a8rawconv"))]


def rumpf(t: str, name: str):
    m = re.search(r"^[A-Za-z_][\w \t\*]*\b%s\s*\([^;{]*\)\s*\{" % re.escape(name),
                  t, re.M)
    if not m:
        return None
    tiefe, i = 0, m.end() - 1
    while i < len(t):
        if t[i] == "{":
            tiefe += 1
        elif t[i] == "}":
            tiefe -= 1
            if tiefe == 0:
                return t[m.end():i]
        i += 1
    return None


def fuellt_im_fehlerzweig(r: str) -> bool:
    """Wird im FEHLERZWEIG eines `fread` gefuellt?

    Nicht: „irgendwo im Rumpf steht ein memset". Diese Unterscheidung
    ist der Unterschied zwischen einem Tor und einem Aergernis —
    gemessen an zwei Fehlalarmen der ersten Fassung:

      `uft_fds_plugin.c`  memset als INITIALISIERUNG, kurzer Lesevorgang
                          bricht mit `return UFT_ERROR_IO` ab
      `uft_dsk_cpc.c`     memset als Initialisierung, kurzer Lesevorgang
                          macht `break` und legt den Sektor gar nicht an

    Beide sind richtig. Gesucht ist die Form „Bedingung mit fread,
    Zweig fuellt und laeuft weiter".
    """
    for m in re.finditer(r"\bif\s*\(", r):
        # Bedingung ausklammern
        i, tiefe = m.end() - 1, 0
        while i < len(r):
            if r[i] == "(":
                tiefe += 1
            elif r[i] == ")":
                tiefe -= 1
                if tiefe == 0:
                    break
            i += 1
        if i >= len(r):
            continue
        bedingung = r[m.end():i]
        if not FREAD.search(bedingung):
            continue
        # Zweig einlesen: Block oder eine Anweisung
        j = i + 1
        while j < len(r) and r[j] in " \t\r\n":
            j += 1
        if j < len(r) and r[j] == "{":
            tiefe, k = 0, j
            while k < len(r):
                if r[k] == "{":
                    tiefe += 1
                elif r[k] == "}":
                    tiefe -= 1
                    if tiefe == 0:
                        break
                k += 1
            zweig = r[j:k + 1]
        else:
            k = r.find(";", j)
            zweig = r[j:k + 1] if k > 0 else r[j:j + 200]
        if FUELLT.search(zweig):
            return True
    return False


def messe(repo: Path):
    pfade = dateien(repo)
    if pfade is None:
        return None, ["Guard-frei: `git ls-files` war nicht befragbar, "
                      "dieses Tor hat NICHTS geprueft."]
    befunde = []
    for rel in pfade:
        try:
            roh = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "uft_format_add_sector" not in roh:
            continue
        t = entkerne(roh)
        for feld in ("read_track", "write_track"):
            m = re.search(r"\.%s\s*=\s*(\w+)" % feld, t)
            if not m or m.group(1) == "NULL":
                continue
            r = rumpf(t, m.group(1))
            if r is None:
                continue
            if not ANLEGT.search(r):
                continue
            if not fuellt_im_fehlerzweig(r):
                continue
            if KENNZEICHNET.search(r):
                continue
            befunde.append((rel, feld, m.group(1)))
    return befunde, []


def check(repo) -> list:
    befunde, fehler = messe(Path(repo))
    if befunde is None:
        return fehler
    for rel, feld, fn in befunde:
        fehler.append(
            "%s (%s = %s): fuellt einen kurzen Lesevorgang mit `memset` und "
            "legt den Sektor an, ohne ihn zu kennzeichnen. "
            "`uft_format_add_sector*()` setzt UFT_SECTOR_OK und beide "
            "CRC-Flags auf „gut\" — die Fuellung ist damit von echten Daten "
            "nicht zu unterscheiden. Nach dem Anlegen "
            "`uft_format_mark_last_missing(track)` rufen (MF-980), oder den "
            "Sektor gar nicht erst anlegen." % (rel, feld, fn))
    if len(befunde) > GRUNDLINIE:
        fehler.append(
            "%d Leser fuellen ohne zu kennzeichnen, Grundlinie %d. "
            "„Keine erfundenen Daten\" ist die dritte Zeile des Mottos."
            % (len(befunde), GRUNDLINIE))
    return fehler


def _selbsttest() -> int:
    """Vor dem Nenner (MF-693) — ueber `check()` selbst."""
    import tempfile

    GERUEST = """
#include "uft/uft_format_common.h"
static uft_error_t x_read_track(uft_disk_t *d, int cyl, int head,
                                uft_track_t *track) {
    uint8_t buf[256];
    for (int s = 0; s < 16; s++) {
%s
    }
    return UFT_OK;
}
const uft_format_plugin_t uft_format_plugin_x = {
    .name = "X",
    .read_track = x_read_track,
};
"""

    faelle = [
        ("fuellt ohne Kennzeichnung",
         "        if (fread(buf, 1, 256, d->f) != 256) memset(buf, 0xE5, 256);\n"
         "        uft_format_add_sector(track, s, buf, 256, cyl, head);", 1),
        ("fuellt UND kennzeichnet",
         "        const bool k = (fread(buf, 1, 256, d->f) != 256);\n"
         "        if (k) memset(buf, 0xE5, 256);\n"
         "        uft_format_add_sector(track, s, buf, 256, cyl, head);\n"
         "        if (k) uft_format_mark_last_missing(track);", 0),
        ("bricht ab statt zu fuellen",
         "        if (fread(buf, 1, 256, d->f) != 256) return UFT_ERROR_IO;\n"
         "        uft_format_add_sector(track, s, buf, 256, cyl, head);", 0),
        ("memset nur im Kommentar",
         "        /* frueher: memset(buf, 0xE5, 256); */\n"
         "        if (fread(buf, 1, 256, d->f) != 256) return UFT_ERROR_IO;\n"
         "        uft_format_add_sector(track, s, buf, 256, cyl, head);", 0),
    ]

    gut = 0
    for name, rumpf_text, soll in faelle:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True)
            (p / "src" / "formats" / "x").mkdir(parents=True, exist_ok=True)
            (p / "src" / "formats" / "x" / "x.c").write_text(
                GERUEST % rumpf_text, encoding="utf-8")
            ist = len(messe(p)[0])
            if ist == soll:
                gut += 1
            else:
                print("  ROT  %-34s erwartet %d, gemessen %d"
                      % (name, soll, ist))
    print("Selbsttest: %d/%d" % (gut, len(faelle)))
    return 0 if gut == len(faelle) else 1


def main() -> int:
    sys.stdout.reconfigure(encoding="utf-8")
    repo = Path(__file__).resolve().parent.parent
    if "--selftest" in sys.argv:
        return _selbsttest()
    befunde, fehler = messe(repo)
    if befunde is None:
        print("\n".join(fehler))
        return 1
    print("Fuellt ohne zu kennzeichnen: %d (Grundlinie %d)"
          % (len(befunde), GRUNDLINIE))
    for rel, feld, fn in befunde:
        print("  %-50s %-12s %s" % (rel, feld, fn))
    errs = check(repo)
    if not errs:
        print("OK")
        return 0
    print("FAIL: %d Befund(e)" % len(errs))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
