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
zeichenkettenfreien Text, ZWEI Wege — und keiner davon darf ohne
`uft_format_mark_last_missing(...)` bleiben:

    (a) ein `memset(...)` im FEHLERZWEIG eines `fread`,
        zusammen mit einem `uft_format_add_sector*(...)`

    (b) ein `uft_format_add_empty_sector(...)` — der legt per
        Definition einen Sektor an, der keine gelesenen Daten traegt,
        und geht durch dieselbe Helferkette, die UFT_SECTOR_OK setzt
        (MF-981)

Dazu eine dritte Messung, die NICHT an der Helferkette haengt:

    (c) ein `memset(X->data, 0xE5, ...)` ohne
        `uft_sector_mark_missing(X)` in den folgenden 6 Zeilen —
        gemessen am FUELLORT, auf DERSELBEN Variablen (MF-1001)

── Warum es (c) gibt: das Tor sah an sich selbst vorbei ──────────────

(a) und (b) steigen aus, wenn `uft_format_add_sector` in der Datei
nicht vorkommt. Neun Plugins schreiben aber direkt in
`track->sectors[s]`, setzen `sect->status = UFT_SECTOR_OK` von Hand —
und zwar BEVOR die Fuellentscheidung faellt — und fuellen im Fehlzweig
0xE5. Sie hielten die Regel dieses Tores nicht ein und wurden nie
gemeldet, weil sie den gemessenen Weg gar nicht benutzen.

`uft_hardsector.c` war der schaerfste Fall: es zaehlt im selben Zweig
`result->bad_sectors++`, WEISS also, dass der Sektor erfunden ist, und
liess ihn trotzdem als gueltig gekennzeichnet stehen.

**Und (c) selbst musste zweimal gemessen werden.** Die erste Fassung
zaehlte jedes `memset(..., 0xE5, ...)` je DATEI und traf damit auch den
SCHREIBpfad, wo ein Ausgabepuffer voraufgefuellt wird (`uft_opus.c:308`,
`uft_mgt.c:210`) — richtiges Verhalten, kein erfundener Lesewert. Drei
Handproben haben es gefangen, nicht das Lesen. Deshalb ist das Ziel
scharf auf `->data` eingegrenzt, und deshalb steht die Regel „kein
Massenbefund ohne drei Handproben" ueber diesem Tor.

── WAS ES AUSDRUECKLICH NICHT SIEHT ──────────────────────────────────

**Eine Kennzeichnung im Rumpf befriedigt das Tor fuer ALLE Fuellwege
derselben Funktion.** Gemessen an `td0_read_track()`, das zwei hat:
wird eine der beiden `mark_last_missing`-Zeilen entfernt, bleibt das Tor
stumm; erst wenn beide fehlen, meldet es. Die Grenze ist also nicht
theoretisch — sie ist vorgefuehrt.

Es prueft ebenso die ANWESENHEIT, nicht die Erreichbarkeit: ein
`mark_last_missing` hinter einer Bedingung, die nie wahr wird, wuerde
durchgelassen. Beides ist bewusst konservativ; eine Fluss- oder
Erreichbarkeitsanalyse waere ratender, als ein Tor tragen darf.

Unsichtbar bleibt auch die dritte Form aus MF-981: ein **genullter
`calloc`-Puffer**, den ein Dekoder nur teilweise fuellt, waehrend die
volle Sektorgroesse weitergereicht wird. Dagegen hilft kein Muster,
sondern nur ein Zaehler im Leser selbst (`decoded_len` in
`uft_td0.c`) — und ein Test, der ihn ausloest.

Leser, die den Sektor gar nicht erst anlegen, sind der richtige Weg und
brauchen keine Kennzeichnung.

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
LEER_ANLEGT = re.compile(r"uft_format_add_empty_sector\s*\(")
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


# Messung (c), MF-1001: die Fuellung geht gar nicht durch die
# Helferkette, sondern direkt in den Puffer des Sektors.
#
# Die Messungen (a) und (b) oben setzen `uft_format_add_sector*` in der
# Datei voraus — `messe()` steigt sonst sofort aus. Neun Plugins
# schreiben aber direkt in `track->sectors[s]`, setzen
# `sect->status = UFT_SECTOR_OK` von Hand (und zwar VOR der
# Fuellentscheidung) und fuellen im Fehlzweig 0xE5. Sie gingen an
# diesem Tor vorbei, obwohl es genau ihre Regel haelt.
#
# Gemessen wird deshalb am FUELLORT statt an der Funktion, und die
# Bedingung ist scharf: ein `memset(X->data, 0xE5, ...)` muss innerhalb
# der naechsten Zeilen ein `uft_sector_mark_missing(X)` nach sich
# ziehen — auf DERSELBEN Variablen.
#
# Warum das Ziel `->data` sein muss und nicht irgendein Puffer: eine
# erste Fassung zaehlte jedes `memset(..., 0xE5, ...)` je Datei und traf
# damit auch den SCHREIBpfad, wo ein Ausgabepuffer voraufgefuellt wird
# (`uft_opus.c`, `uft_mgt.c`). Das ist richtiges Verhalten. Die
# Handprobe hat es gefangen, nicht das Lesen — Regel des Durchgangs:
# kein Massenbefund ohne drei Handproben.
FUELLT_SEKTOR = re.compile(
    r"memset\s*\(\s*(\w+)\s*->\s*data\s*,\s*0x[eE]5\s*,")

#: Zeilen, innerhalb derer die Kennzeichnung folgen muss. Sechs, weil
#: die Fuellung in allen gemessenen Faellen unmittelbar davor steht;
#: mehr Spielraum wuerde eine Kennzeichnung in einem ANDEREN Zweig
#: durchgehen lassen.
FENSTER = 6


def messe_direkt(repo: Path):
    """Fuellungen direkt in den Sektorpuffer ohne Kennzeichnung."""
    pfade = dateien(repo)
    if pfade is None:
        return None, ["Guard-frei: `git ls-files` war nicht befragbar, "
                      "Messung (c) hat NICHTS geprueft."]
    befunde = []
    for rel in pfade:
        try:
            roh = (repo / rel).read_text(encoding="utf-8", errors="replace")
        except OSError:
            continue
        if "0xE5" not in roh and "0xe5" not in roh:
            continue
        t = entkerne(roh)
        zeilen = t.splitlines()
        for i, z in enumerate(zeilen):
            m = FUELLT_SEKTOR.search(z)
            if not m:
                continue
            var = m.group(1)
            fenster = "\n".join(zeilen[i:i + FENSTER + 1])
            if re.search(r"uft_sector_mark_missing\s*\(\s*%s\s*\)"
                         % re.escape(var), fenster):
                continue
            if re.search(r"mark_last_missing\s*\(", fenster):
                continue
            befunde.append((rel, i + 1, var))
    return befunde, []


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
            if KENNZEICHNET.search(r):
                continue
            # (a) Fuellung im Fehlerzweig eines fread
            if ANLEGT.search(r) and fuellt_im_fehlerzweig(r):
                befunde.append((rel, feld, m.group(1), "fread-Fehlerzweig"))
                continue
            # (b) MF-981: `uft_format_add_empty_sector()` legt einen
            #     Sektor an, der per Definition keine gelesenen Daten
            #     traegt — und geht durch dieselbe Helferkette, die
            #     UFT_SECTOR_OK setzt. Kein fread, kein memset; die
            #     erste Torfassung sah diesen Weg nicht (TD0).
            if LEER_ANLEGT.search(r):
                befunde.append((rel, feld, m.group(1), "add_empty_sector"))
    return befunde, []


def check(repo) -> list:
    befunde, fehler = messe(Path(repo))
    if befunde is None:
        return fehler
    for rel, feld, fn, art in befunde:
        fehler.append(
            "%s (%s = %s, %s): legt einen Sektor an, ohne ihn zu "
            "kennzeichnen. "
            "`uft_format_add_sector*()` setzt UFT_SECTOR_OK und beide "
            "CRC-Flags auf „gut\" — die Fuellung ist damit von echten Daten "
            "nicht zu unterscheiden. Nach dem Anlegen "
            "`uft_format_mark_last_missing(track)` rufen (MF-980/981), oder "
            "den Sektor gar nicht erst anlegen." % (rel, feld, fn, art))
    if len(befunde) > GRUNDLINIE:
        fehler.append(
            "%d Leser fuellen ohne zu kennzeichnen, Grundlinie %d. "
            "„Keine erfundenen Daten\" ist die dritte Zeile des Mottos."
            % (len(befunde), GRUNDLINIE))

    # Messung (c), MF-1001 — die Helferkette wird gar nicht benutzt.
    direkt, dfehler = messe_direkt(Path(repo))
    if direkt is None:
        return fehler + dfehler
    for rel, zeile, var in direkt:
        fehler.append(
            "%s:%d: `memset(%s->data, 0xE5, ...)` ohne "
            "`uft_sector_mark_missing(%s)`. Der Sektor wurde GEFUELLT, "
            "nicht gelesen — und `status` steht in diesen Lesern schon "
            "auf UFT_SECTOR_OK, bevor die Fuellentscheidung faellt. "
            "Erfundene 0xE5 sind dann von echten 0xE5-Daten nicht zu "
            "unterscheiden (MF-1001)." % (rel, zeile, var, var))
    if direkt:
        fehler.append(
            "%d Fuellorte schreiben direkt in den Sektorpuffer, "
            "Grundlinie 0." % len(direkt))
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

    # ── Messung (c), MF-1001 ────────────────────────────────────────
    #
    # Eigenes Geruest, weil diese Faelle die Helferkette gerade NICHT
    # benutzen — das ist ihr Kennzeichen.
    GERUEST_C = """
#include "uft/uft_format_common.h"
static void x_fuellt(uft_track_t *track, const uint8_t *data,
                     size_t size, size_t data_pos, uint16_t ss) {
    uft_sector_t *sect = &track->sectors[0];
    sect->status = UFT_SECTOR_OK;
    sect->data = malloc(ss);
    if (sect->data) {
%s
    }
}
"""

    faelle_c = [
        ("direkt gefuellt, nicht gekennzeichnet",
         "        if (data_pos + ss <= size) {\n"
         "            memcpy(sect->data, data + data_pos, ss);\n"
         "        } else {\n"
         "            memset(sect->data, 0xE5, ss);\n"
         "        }", 1),
        ("direkt gefuellt UND gekennzeichnet",
         "        if (data_pos + ss <= size) {\n"
         "            memcpy(sect->data, data + data_pos, ss);\n"
         "        } else {\n"
         "            memset(sect->data, 0xE5, ss);\n"
         "            uft_sector_mark_missing(sect);\n"
         "        }", 0),
        ("Kennzeichnung auf ANDERER Variablen zaehlt nicht",
         "        if (data_pos + ss <= size) {\n"
         "            memcpy(sect->data, data + data_pos, ss);\n"
         "        } else {\n"
         "            memset(sect->data, 0xE5, ss);\n"
         "            uft_sector_mark_missing(anderer);\n"
         "        }", 1),
        ("Ausgabepuffer im Schreibpfad ist KEIN Befund",
         "        memset(ausgabe, 0xE5, ss);\n"
         "        memcpy(ausgabe, sect->data, ss);", 0),
        ("Fuellung nur im Kommentar",
         "        /* frueher: memset(sect->data, 0xE5, ss); */\n"
         "        memcpy(sect->data, data + data_pos, ss);", 0),
    ]

    for name, rumpf_text, soll in faelle_c:
        with tempfile.TemporaryDirectory() as d:
            p = Path(d)
            subprocess.run(["git", "init", "-q"], cwd=d, capture_output=True)
            (p / "src" / "formats" / "x").mkdir(parents=True, exist_ok=True)
            (p / "src" / "formats" / "x" / "x.c").write_text(
                GERUEST_C % rumpf_text, encoding="utf-8")
            ist = len(messe_direkt(p)[0])
            if ist == soll:
                gut += 1
            else:
                print("  ROT  (c) %-30s erwartet %d, gemessen %d"
                      % (name, soll, ist))

    gesamt = len(faelle) + len(faelle_c)
    print("Selbsttest: %d/%d" % (gut, gesamt))
    return 0 if gut == gesamt else 1


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
    for rel, feld, fn, art in befunde:
        print("  %-46s %-12s %-22s %s" % (rel, feld, fn, art))
    errs = check(repo)
    if not errs:
        print("OK")
        return 0
    print("FAIL: %d Befund(e)" % len(errs))
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
