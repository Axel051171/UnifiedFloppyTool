#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
audit_codefallen.py — vier Code-Fallen, die keine Testabdeckung ersetzt.

    python scripts/audit_codefallen.py src include
    python scripts/audit_codefallen.py --only K4 src include
    python scripts/audit_codefallen.py --format sarif --out k.sarif src include
    python scripts/audit_codefallen.py --tor
    python scripts/audit_codefallen.py --selbsttest

Herkunft: Paket `neue-ideen/Vier Code-Fallen.zip` des Eigentuemers
(„das ist code von mir"), Posten `A-029`. Geruest, Grundlinienmechanik
und Ausgabeformate kommen aus `audit_common` — dieselbe EINE Stelle wie
bei `audit_teilstring.py`, weil zwei Pruefer mit zwei Mechanismen genau
die Doppelhaltung waeren, gegen die Regel K4 hier antritt (D3).


DIE REGELN
----------

K4  Physikalische Konstante in mehr als einer Datei
    Belegt vom Eigentuemer: `uft_hfe.c:556` erraet 12500 und 6250,
    waehrend `uft_fdc_gaps.h:181` fuer dasselbe Profil `.rpm = 360` und
    `.track_bytes = 10416` korrekt fuehrt. Fuer jedes 360-U/min-Format
    liegt der Schreiber 20 % daneben.
    Wissen doppelt gehalten wird einmal falsch.

K6  Warnung hinter einer Schleife im begrenzten Puffer
    In `uft_meta_report()` stand die Warnung „Liste unvollstaendig" am
    Ende — und fiel bei vielen Eintraegen der Puffergrenze zum Opfer.
    Eine Warnung, die man wegkuerzen kann, ist keine.

P2  Verschachtelte Kommentarklammer
    Ein `/*` in einem Dokublock: C kennt keine Verschachtelung, der
    aeussere Block endet am ERSTEN `*/`, alles dahinter wird Code.
    Deckungsgleich mit `gcc -Wcomment`, das in `-Wall` steckt.

P3  `#undef` vor spaetem Gebrauch
    Der Praeprozessor arbeitet textuell. Ein `#undef APP` in einem
    fruehen Rueckgabezweig nimmt das Makro fuer den REST der Datei weg,
    nicht nur fuer den Zweig.


WAS SIE NICHT TUN
-----------------
Keine Regel wertet Makros aus oder folgt Einbindungen. K4 kennt nur die
Konstanten aus ihrer Liste — `--add-constants` erweitert sie.


SECHS DEFEKTE, GEMESSEN AM BAUM — DAS PAKET HATTE IHN NIE GESEHEN
-----------------------------------------------------------------
Der Bericht des Eigentuemers sagt selbst: „Die Pruefer haben deinen Baum
nie gesehen. Fuenfter Bericht in Folge mit diesem Satz." Der erste Lauf
ueber `src` + `include` (1401 Dateien) ergab **37 Funde** — und sechs
Defekte, vier davon von `gcc` als fremder Hand entschieden.

(A) P2 meldete EINEN Fund je Kommentarblock, `gcc` meldet jeden.
    Gemessen an `include/uft/gui/wiring_runtime.h:13`: die Zeile traegt
    `forms/*.actions.yaml` UND `forms/*.ui`, also zwei `/*`-Folgen;
    `gcc -Wcomment` nennt 13:45 und 13:70, die Regel nannte eine.
    Jetzt wird jedes Vorkommen gemeldet.

(B) P3 war BLIND fuer seinen eigenen Kopffall. Die Neudefinitions-Suche
    lief ueber den GANZEN Rest der Datei: stand irgendwo dahinter ein
    `#define X`, schwieg die Regel — auch wenn der kaputte Gebrauch
    DAVOR lag. Gemessen an einem gepflanzten Fall, den `gcc` mit
    „implicit declaration of function 'KAPUTT'" bestaetigt: die Regel
    meldete **nichts**. Die Grenze ist jetzt die NAECHSTE Neudefinition
    nach dem `#undef`, nicht das Dateiende.

(C) P3 konnte nicht unterscheiden, WEM das Makro gehoert. Gemessen
    waren **alle 8** Funde im Baum die entgegengesetzte Absicht: das
    `#undef` gibt einen FREMDEN Namen frei, damit er als C-Bezeichner
    dienen kann —

        uft_ipf_caps.c:18     #undef LoadImage  (Windows-Makro)
                              spaeter: `->LoadImage =`, ein Strukturfeld
        uft_format_parsers.h  #undef UFT_IMD_MODE_*  (6x)
                              spaeter: `{ UFT_IMD_MODE_500K_FM =`, ein
                              enum, das genau diese Namen DEFINIERT
        uft_ipf_caps.h:278    spaeter: `* LoadImage )`, ein Zeigerfeld

    Alle acht trugen die Einschaetzung `sicher`, also blockierend — das
    Tor haette am ersten Tag jeden Pull Request abgewiesen, genau der
    Fehlschlag, vor dem der Eigentuemer im Workflow-Kopf warnt.
    Die Unterscheidung ist jetzt: **definiert die Datei das Makro
    selbst, vor dem `#undef`?** Ohne eigene Definition nimmt das
    `#undef` nichts weg, was dieser Datei gehoert. Das trennt alle
    zehn gemessenen Faelle richtig, und der Koeder des Eigentuemers
    (`fixture_code_a.c:104`, `#define APP` … `#undef APP` … `APP(...)`)
    schlaegt weiter an.

(D) K4 kollidierte mit NACHSCHLAGETAFELN. Gemessen in
    `src/formats/dms/uft_dms.c:200` und `:203`:

        0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,
        0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041,

    Das ist eine CRC-16-Tafel. Eine solche Tafel enthaelt am Ende jeden
    2-Byte-Wert — **jede zweibytige Konstante der Liste kollidiert also
    mit jeder CRC-Tafel im Baum**, und gemeldet wurde „0x2200
    (JV3-Datenbeginn)" in einer DMS-Datei. Ein Zahlenlauf von vier oder
    mehr kommagetrennten Literalen gilt jetzt als Tafel und zaehlt
    nicht.

(E) Die Konstantenliste verletzte ihr EIGENES Kriterium. Ihr Kopf sagt:
    „Sektorgroessen und Rundungen stehen bewusst nicht drin. 512, 1024,
    2, 8, 16 kommen berechtigt ueberall vor, und eine Regel, die sie
    meldet, wird abgeschaltet." Gemessen gehoeren `84`, `108`, `288`
    und `360` in dieselbe Klasse:

        84   189 Zeilen, davon 25 mit irgendeinem Luecken-Wort
        108   20 Zeilen — erster Treffer `src/core/uft_error_codes.h:39`,
              und dort ist 108 `UFT_ERROR_PERMISSION_DENIED`
        288   49 Zeilen
        360  185 Zeilen — erster Treffer `visualdiskdialog.cpp:221`

    Sie sind NICHT geloescht (MF-1077: Fehlklassifikation wird
    umgeschrieben, nicht entfernt), sondern stehen in
    `KONSTANTEN_OHNE_ZUSAMMENHANG` und schweigen ohne
    `--kleine-konstanten`. Der Selbsttest haelt das als Verbot fest.

(F) `_norm_num()` streifte Hex-ZIFFERN als Ganzzahl-Suffixe ab.
    `text.rstrip('uUlLzZfF')` trifft bei einem Hex-Literal die Ziffern
    `f` und `F`. Gemessen:

        0xFF        -> 0x00        0xFFFF     -> 0x00
        0xAF        -> 0x0A        0xEF       -> 0x0E
        0xFFFFFFFF  -> 0x00

    Im Baum enden **3110** Hex-Literale auf `f`/`F`. Heute traegt die
    Liste keinen solchen Wert, der Defekt ist also latent — aber er
    bricht genau die Hex/Dezimal-Vereinigung, die das Werkzeug zusagt:
    dezimal `175` normiert auf `0xAF`, das Literal `0xAF` auf `0x0A`,
    derselbe Wert bekommt zwei Schluessel. Bei Hex werden jetzt nur
    `uUlLzZ` abgestreift.


DER 0x1900-FUND — BERICHTIGT MF-1243, ER IST EIN FEHLALARM
----------------------------------------------------------
Hier stand unter der Ueberschrift „WAS AUSDRUECKLICH KEIN DEFEKT IST":

    `0x1900 (DMK-Spurlaenge 5,25 Zoll DD)` sieht wie ein Fehlalarm aus
    und ist einer der wertvollsten Funde:
    `src/analysis/uft_track_analysis.c:261` fuehrt
    `.track_length_max = 6400` DEZIMAL, DMK dieselbe Groesse hex —
    dieselbe physikalische Groesse in zwei Schreibweisen an zwei
    Stellen. Genau K4s Zweck.

Nachgemessen tragen die vier Fundstellen VIER VERSCHIEDENE
physikalische Groessen, die nur denselben Zahlenwert haben:

  src/analysis/uft_track_analysis.c:261
      `.track_length_max` im Profil `UFT_PROFILE_BBC_ADFS` — die obere
      Toleranz eines ERKENNUNGSBANDS, zusammen mit
      `.track_length_min = 6200`, `.track_length_nominal = 6250` und
      `.long_track_threshold = 6350`, bei 250 kbit/s und 16 x 256.
      Keine Behaelterlaenge.
  src/formats/c64/uft_gcr_ops.c:933
      `if (size < 6400) return 0;` in `gcr_detect_density()` — eine
      C64-GCR-ZONENGRENZE.
  src/samdisk/udi.cpp:110
      `(tlen > 6400) ? DataRate::_500K : DataRate::_250K` — ein
      UDI-Datenraten-SCHWELLWERT, dazu Fremdcode.
  src/formats/dmk/uft_dmk_parser_v2.c:470
      die einzige echte DMK-Spurlaenge in der Liste — und sie liegt in
      einem `#ifdef DMK_PARSER_TEST`, das im ganzen Baum NIRGENDS
      definiert wird und bereits in `docs/selbsttest_baseline.txt:21`
      gefuehrt ist.

Und DMK haelt seine Konstante sauber: `uft_dmk_parser_v2.c:37` hat
`#define DMK_DD_TRACK_SIZE 0x1900  /* Double density track (6400) */` —
die Dezimalzahl steht im Kommentar daneben. Die beiden woertlichen
`0x1900` bei `:464`/`:470` gehen zwar an diesem `#define` vorbei, aber
sie stehen in nie uebersetztem Code; sie dort zu ersetzen waere
Politur an totem Text und nach MF-1077 ohne gemessenen Anlass.

Warum das hier stehen bleibt statt gestrichen zu werden: die Zeile hat
gesteuert, wo gesucht wird — sie nannte den Fund „einen der
wertvollsten" und stand im Kopf eines TORES. Das ist die Klasse MF-930,
und der Baum hat sie teuer bezahlt. Die Einordnung steht als `P3-493`.

Was K4 damit NICHT verliert: die Regel ist richtig. Sie kann nur Zahlen
vergleichen, keine Bedeutungen — deshalb ist jede ihrer Fundstellen ein
VERDACHT, und die Einordnung („dieselbe Groesse" gegen „derselbe Wert")
gehoert je Fundstelle von Hand gemacht.
"""

from __future__ import annotations

import argparse
import functools
import json
import os
import re
import sys
from collections import defaultdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from audit_common import (Finding, walk, add_common_args,      # noqa: E402
                          run_and_report, C_EXT, load_baseline,
                          apply_baseline)
from c_lex import (lex, KIND_ID, KIND_STR, KIND_NUM,           # noqa: E402
                   KIND_PUNCT, KIND_COMMENT, KIND_PP)


#: `c_lex.lex()` ist ein GENERATOR, und die vier Regeln brauchen
#  denselben Strom mehrfach: `collect_constants`, `rule_k6` und
#  `rule_p3` ohne Kommentare, `rule_p2` mit. Ohne diesen Zwischenspeicher
#  wird JEDE Datei VIERMAL zerlegt — gemessen brauchte das Tor damit
#  **35 s** gegen **10 s** bei Tor 66, das einmal zerlegt. `maxsize=2`
#  genuegt, weil die vier Aufrufe je Datei unmittelbar aufeinander
#  folgen; mehr wuerde nur Quelltext im Speicher halten.
@functools.lru_cache(maxsize=2)
def _lex(src: str, keep_comments: bool = False) -> tuple:
    return tuple(lex(src, keep_comments=keep_comments))


TOOL = 'codeFallen'

RULE_HELP = {
    'K4': 'Physikalische Konstante in mehr als einer Datei',
    'K6': 'Warnung hinter einer Schleife im begrenzten Puffer',
    'P2': 'Verschachtelte Kommentarklammer',
    'P3': '#undef vor spaetem Gebrauch',
}
ERROR_RULES = frozenset({'P2', 'P3'})

EMPTY_NOTE = ('Keine der vier Fallen gefunden. Das heisst NICHT, dass keine '
              'da ist —\nK4 kennt nur die Konstanten aus ihrer Liste '
              '(--add-constants erweitert sie),\nund Makros wertet keine '
              'Regel aus.')


# ── K4: Konstanten ───────────────────────────────────────────────────────
#
# Nur Konstanten mit einer BEDEUTUNG, die sich SELBST ausweist. Eine Zahl
# weist sich selbst aus, wenn sie nicht zufaellig entstehen kann: 737280
# ist eine Geometrieaussage, 84 ist eine Zahl.

KONSTANTEN = {
    # Rohkapazitaet einer Spur in Byte — der belegte Fall des Eigentuemers
    '12500':   'Spurkapazitaet 500 kbps @ 300 U/min',
    '10416':   'Spurkapazitaet 500 kbps @ 360 U/min',
    '10417':   'Spurkapazitaet 500 kbps @ 360 U/min (aufgerundet)',
    '6250':    'Spurkapazitaet 250 kbps @ 300 U/min',
    '5208':    'Spurkapazitaet 500 kbps @ 360 U/min, FM',
    '3125':    'Spurkapazitaet 250 kbps @ 300 U/min, FM',
    # Abbildgroessen — jede ist eine Geometrieaussage
    '737280':  '720K DS/DD (neunfach im Baum beansprucht)',
    '819200':  'Akai/Korg 80x2x5x1024',
    '901120':  'Amiga DD 80x2x11x512',
    '143360':  'Apple 35x16x256',
    '368640':  '360K DS/DD',
    '1474560': '1.44M HD',
    '1228800': '1.2M HD',
    '2949120': '2.88M ED',
    '655360':  'MSX 640K',
    '184320':  '180K SS/DD',
    '163840':  '160K SS/DD',
    '327680':  '320K DS/DD',
    # Fuellbytes — einbytig, aber formatspezifisch benannt
    '0x4E':    'MFM-Lueckenfuellbyte (IBM)',
    '0xE5':    'FAT-Verzeichnis frei / MFM-Fuellung',
    '0xF6':    'DOS-format-Fuellbyte',
    '0xAA':    'MFM-Kodierung von 0x00 (Atari-FM-Fuellung)',
    # Spurlaengen in Behaeltern
    '0x1900':  'DMK-Spurlaenge 5,25 Zoll DD',
    '0x2940':  'DMK-Spurlaenge 3,5 Zoll HD',
    '2901':    'JV3-Kopfeintraege je Block',
    '0x2200':  'JV3-Datenbeginn',
}

#: Defekt (E): diese vier standen in der Liste und verletzen ihr eigenes
#  Kriterium — sie kommen berechtigt ueberall vor. Sie sind NICHT
#  geloescht (MF-1077), sondern stumm; `--kleine-konstanten` schaltet sie
#  ein. Je Eintrag steht die Messung, die ihn hierher gebracht hat.
KONSTANTEN_OHNE_ZUSAMMENHANG = {
    '108':     'GAP3 1.44M Standard '
               '(20 Zeilen; erster Treffer ist '
               'UFT_ERROR_PERMISSION_DENIED in uft_error_codes.h:39)',
    '84':      'GAP3 Formatierwert '
               '(189 Zeilen im Baum, davon 25 mit einem Luecken-Wort)',
    '360':     'Drehzahl 5,25-Zoll-HD '
               '(185 Zeilen; erster Treffer visualdiskdialog.cpp:221)',
    '288':     'Drehzahl Atari-800-Laufwerk (49 Zeilen)',
}

#: Dateien, in denen eine Konstante erwartet wird und nicht zaehlt.
#: Tests nennen Konstanten zu Recht — das ist ihr Zweck.
HOME_PATTERNS = (
    r'(^|/)tests?/',
    r'(^|/)test_[^/]*$',
    r'_test\.(c|h|cpp)$',
    r'(^|/)fixtures?/',
)

#: Ab so vielen kommagetrennten Zahlenliteralen in Folge gilt der Lauf
#  als NACHSCHLAGETAFEL (Defekt D). Vier ist die kleinste Zahl, die eine
#  CRC-Tafel sicher trifft und eine Initialisierung wie `{ 0, 1, 2 }`
#  noch durchlaesst.
TAFEL_LAUF = 4


def _norm_num(text: str) -> str:
    """Zahl auf eine Vergleichsform bringen: Suffixe und Trennzeichen weg,
    Hex gross. `0x4Eu` und `0x4e` sind dieselbe Konstante.

    Defekt (F): hier stand `text.rstrip('uUlLzZfF')` fuer JEDE Zahl. Bei
    einem Hex-Literal sind `f` und `F` aber ZIFFERN — gemessen wurde
    `0xFF` zu `0x00` und `0xAF` zu `0x0A`, bei 3110 solchen Literalen im
    Baum. Ein Hex-Literal kann kein Float-Suffix tragen, also wird dort
    nur `uUlLzZ` abgestreift.
    """
    t = text.replace("'", '')
    if t[:2].lower() == '0x':
        t = t.rstrip('uUlLzZ')
        return '0x' + t[2:].upper().lstrip('0').rjust(2, '0')
    return t.rstrip('uUlLzZfF')


#: Wird vom Selbsttest abgeschaltet: die Koederdateien liegen unter
#: tests/, und K4 ueberspringt Testpfade zu Recht — nur dann kann der
#: Selbsttest die Regel nicht pruefen. Ein Schalter ist ehrlicher, als
#: die Koeder an einen unnatuerlichen Ort zu legen.
_HOME_FILTER = True


def set_home_filter(on: bool) -> None:
    global _HOME_FILTER
    _HOME_FILTER = on


def _is_home(path: str) -> bool:
    if not _HOME_FILTER:
        return False
    p = path.replace(os.sep, '/')
    return any(re.search(pat, p) for pat in HOME_PATTERNS)


def tafel_stellen(toks) -> set:
    """Indizes, die zu einem Zahlenlauf `NUM , NUM , NUM …` gehoeren.

    Defekt (D): ohne diesen Filter meldet K4 Eintraege von CRC- und
    Kodiertafeln als Konstanten — gemessen `0x2200 (JV3-Datenbeginn)` in
    `uft_dms.c:203`, mitten in einer CRC-16-Tafel. Eine 16-Bit-Tafel
    enthaelt am Ende jeden 2-Byte-Wert, also kollidiert JEDE zweibytige
    Konstante mit ihr.

    Der Lauf wird maximal gefasst, damit auch das erste und letzte
    Element einer Tafel erkannt wird — an den Raendern fehlt sonst der
    zahlenfoermige Nachbar.
    """
    drin = set()
    n = len(toks)
    i = 0
    while i < n:
        if toks[i].kind != KIND_NUM:
            i += 1
            continue
        lauf = [i]
        j = i
        while (j + 2 < n and toks[j + 1].kind == KIND_PUNCT
               and toks[j + 1].text == ',' and toks[j + 2].kind == KIND_NUM):
            lauf.append(j + 2)
            j += 2
        if len(lauf) >= TAFEL_LAUF:
            drin.update(lauf)
        i = j + 1
    return drin


def collect_constants(src, wanted):
    """Vorkommen der gesuchten Konstanten in dieser Datei."""
    toks = _lex(src)
    tafel = tafel_stellen(toks)
    hits = []

    for idx, t in enumerate(toks):
        if t.kind != KIND_NUM or idx in tafel:
            continue
        key = _norm_num(t.text)
        if key in wanted:
            hits.append((key, t.line, t.col, t.text))
            continue
        # Die andere Schreibweise derselben Zahl auch finden.
        try:
            wert = int(key, 0)
        except ValueError:
            continue
        for cand in (str(wert), '0x%02X' % wert):
            if cand in wanted:
                hits.append((cand, t.line, t.col, t.text))
                break
    return hits


def rule_k4(per_file, wanted):
    """Cross-file: eine Konstante in mehr als einer Nicht-Test-Datei.

    MF-1231: der Fund wird an der KLEINSTEN Fundstelle verankert, nicht
    an der erstgesehenen. K4 ist die einzige Regel ueber mehrere
    Dateien, und ihr Fingerabdruck enthaelt den Pfad dieser Verankerung
    — hing er an der Durchlaufreihenfolge, war er plattformabhaengig.
    Gemessen: lokal 0 neue Funde, in CI **12**, bei identischen
    Dateizahlen und nur anderem ersten Treffer. `walk()` sortiert seit
    derselben MF auch die Verzeichnisse; diese Sortierung hier ist die
    zweite, unabhaengige Schranke, damit ein spaeterer Umbau von
    `walk()` die Grundlinie nicht wieder entwertet.
    """
    where = defaultdict(list)          # konst -> [(pfad, zeile, spalte, text)]
    for path, hits in per_file.items():
        if _is_home(path):
            continue
        for key, line, col, text in hits:
            where[key].append((path.replace(os.sep, '/'), line, col, text))

    out = []
    for key, occ in sorted(where.items()):
        # Nach Pfad und Zeile sortieren — nicht nach Sehreihenfolge.
        occ = sorted(occ, key=lambda o: (o[0], o[1], o[2]))
        files = sorted({o[0] for o in occ})
        if len(files) < 2:
            continue

        # Ab drei Dateien ist es keine Absprache mehr, sondern Streuung.
        sev = 'sicher' if len(files) >= 3 else 'pruefen'
        first = occ[0]
        out.append(Finding(
            'K4', first[0], first[1], first[2], sev,
            f'{key} ({wanted[key]}) steht in {len(files)} Dateien',
            'Wissen doppelt gehalten wird einmal falsch. Belegt: '
            'uft_hfe.c erraet 12500, waehrend uft_fdc_gaps.h fuer '
            'dasselbe Profil 10416 korrekt fuehrt. Eine Stelle soll sie '
            'definieren, alle anderen sie nachschlagen.',
            f'{first[3]}',
            also=[f'{p}:{ln}' for p, ln, _, _ in occ[1:8]]))
    return out


# ── K6: Warnung hinter der Schleife ──────────────────────────────────────

WARN_WORDS = re.compile(r'(WARNUNG|WARNING|ACHTUNG|BEFUND|HINWEIS|CAUTION)')
BUF_PARAM = re.compile(r'(buf|buffer|out|dst|dest)', re.I)
LEN_PARAM = re.compile(r'(buflen|len|size|cap|n)$', re.I)
LOOP_KW = {'for', 'while'}


def rule_k6(path, src):
    """Eine Funktion, die in einen begrenzten Puffer schreibt, hat eine
    Warnung HINTER ihrer letzten Schleife.

    Naeherung ohne Parser: Funktionsgrenzen ueber die Klammerzaehlung,
    Puffer-Erkennung ueber die Parameternamen, und `snprintf` als Beleg,
    dass wirklich begrenzt geschrieben wird.

    BERICHTIGT (A-029): hier stand „ein Funktionskopf auf Tiefe 0", und
    der Code prueft die Tiefe nicht. Das ist richtig so, und die
    Messung sagt warum: in `include/uft/uft_format_parsers.h` oeffnet
    Zeile 31 `extern "C" {` innerhalb von `#ifdef __cplusplus` und
    schliesst erst am Dateiende in einem zweiten `#ifdef`. Ohne
    Praeprozessor ist Dateiebene dort die Tiefe **1**, nicht 0 — eine
    Tiefenpruefung wuerde die Regel in JEDEM Header mit `extern
    "C"`-Klammer blind machen. Der Schutz gegen innere `if (…) {` ist
    stattdessen der Sprung `i = k + 1`: der Rumpf einer erkannten
    Funktion wird uebersprungen.
    """
    toks = [t for t in _lex(src) if t.kind != KIND_PP]
    out = []

    i = 0
    n = len(toks)
    while i < n:
        # Ein Funktionskopf: name ( … ) {
        if toks[i].kind != KIND_ID or i + 1 >= n or toks[i + 1].text != '(':
            i += 1
            continue

        # Klammer schliessen
        depth, j = 0, i + 1
        while j < n:
            if toks[j].text == '(':
                depth += 1
            elif toks[j].text == ')':
                depth -= 1
                if depth == 0:
                    break
            j += 1
        if j >= n or j + 1 >= n or toks[j + 1].text != '{':
            i += 1
            continue

        params = toks[i + 2:j]
        names = [t.text for t in params if t.kind == KIND_ID]
        has_buf = any(BUF_PARAM.search(x) for x in names)
        has_len = any(LEN_PARAM.search(x) for x in names)

        # Rumpf abgrenzen
        depth, k = 0, j + 1
        body_start = k + 1
        while k < n:
            if toks[k].text == '{':
                depth += 1
            elif toks[k].text == '}':
                depth -= 1
                if depth == 0:
                    break
            k += 1
        body = toks[body_start:k]
        fname = toks[i].text

        if has_buf and has_len and body:
            bounded = any(t.kind == KIND_ID and
                          t.text in ('snprintf', 'vsnprintf', 'strncat',
                                     'strncpy')
                          for t in body)
            last_loop = None
            for idx, t in enumerate(body):
                if t.kind == KIND_ID and t.text in LOOP_KW:
                    last_loop = idx

            if bounded and last_loop is not None:
                for t in body[last_loop:]:
                    if t.kind == KIND_STR and WARN_WORDS.search(t.value):
                        word = WARN_WORDS.search(t.value).group(1)
                        out.append(Finding(
                            'K6', path, t.line, t.col, 'pruefen',
                            f'"{word}" hinter der letzten Schleife in '
                            f'{fname}()',
                            'Eine Warnung am Ende eines begrenzten Puffers '
                            'faellt bei vielen Eintraegen der Kuerzung zum '
                            'Opfer. Eine Warnung, die man wegkuerzen kann, '
                            'ist keine — sie gehoert vor die Schleife.',
                            t.value.strip()[:70]))
                        break

        i = k + 1 if k > i else i + 1
    return out


# ── P2: verschachtelte Kommentarklammer ──────────────────────────────────

def rule_p2(path, src):
    """Jeder Kommentaranfang INNERHALB eines Blockkommentars.

    Defekt (A): hier stand ein einzelnes `find('/*', 2)` und EIN Fund je
    Kommentar. `gcc -Wcomment` meldet jedes Vorkommen, und der
    Unterschied ist gemessen: `include/uft/gui/wiring_runtime.h:13`
    nennt `forms/*.actions.yaml` und `forms/*.ui`, gcc meldet 13:45 und
    13:70, die Regel meldete eines. Ein Pruefer, der weniger sagt als
    der Compiler, verschiebt Arbeit auf den naechsten Lauf.
    """
    out = []
    for t in _lex(src, True):
        if t.kind != KIND_COMMENT or not t.text.startswith('/*'):
            continue
        stelle = t.text.find('/*', 2)
        while stelle >= 0:
            line_off = t.text.count('\n', 0, stelle)
            frag = t.text[max(0, stelle - 20):stelle + 40].replace('\n', ' ')
            # Spalte: in der ersten Zeile relativ zum Kommentaranfang,
            # danach relativ zum letzten Umbruch.
            zeilenanfang = t.text.rfind('\n', 0, stelle) + 1
            col = (stelle - zeilenanfang + 1) if line_off else (t.col + stelle)
            out.append(Finding(
                'P2', path, t.line + line_off, col, 'sicher',
                'Kommentaranfang "/*" innerhalb eines Blockkommentars',
                'C kennt keine verschachtelten Kommentare: der aeussere '
                'Block endet am ERSTEN "*/", und alles dahinter wird Code. '
                'In einem Dokublock mit Beispielcode passiert das leicht. '
                'Deckungsgleich mit `gcc -Wcomment`, das in `-Wall` steckt.',
                frag.strip()[:70]))
            stelle = t.text.find('/*', stelle + 2)
    return out


# ── P3: #undef vor spaetem Gebrauch ──────────────────────────────────────

def _define_re(name: str):
    return re.compile(r'#\s*define\s+' + re.escape(name) + r'\b')


def rule_p3(path, src):
    """`#undef X`, das der Datei ihr EIGENES Makro wegnimmt, obwohl X
    danach noch gebraucht wird.

    Drei Bedingungen statt einer — beide Zusaetze sind gemessen:

    (a) Die Datei muss X selbst definieren, VOR dem `#undef`.
        Defekt (C): ohne das meldete die Regel alle 8 Faelle im Baum
        falsch, jeden als `sicher`. Dort gibt das `#undef` einen
        FREMDEN Namen frei, damit er als C-Bezeichner dienen kann:
        `#undef LoadImage` (Windows) vor `->LoadImage =`, und sechsmal
        `#undef UFT_IMD_MODE_*` vor einem `enum`, das genau diese Namen
        definiert. Nimmt das `#undef` nichts weg, was dieser Datei
        gehoert, ist es die Absicht und kein Versehen.

    (c) Die Grenze ist die NAECHSTE Neudefinition, nicht das Dateiende.
        Defekt (B): die alte Fassung suchte `#define X` im ganzen Rest
        und schwieg, wenn irgendwo dahinter eine stand — auch wenn der
        kaputte Gebrauch davor lag. Gemessen an einem gepflanzten Fall,
        den `gcc` mit „implicit declaration of function 'KAPUTT'"
        bestaetigt, meldete sie **nichts**.
    """
    toks = _lex(src)
    out = []

    for idx, t in enumerate(toks):
        if t.kind != KIND_PP:
            continue
        m = re.match(r'#\s*undef\s+([A-Za-z_]\w*)', t.text)
        if not m:
            continue
        name = m.group(1)
        dre = _define_re(name)

        # (a) gehoert das Makro dieser Datei?
        eigen = any(u.kind == KIND_PP and dre.match(u.text)
                    for u in toks[:idx])
        if not eigen:
            continue

        # (c) bis zur naechsten Neudefinition
        grenze = len(toks)
        for j in range(idx + 1, len(toks)):
            u = toks[j]
            if u.kind == KIND_PP and dre.match(u.text):
                grenze = j
                break

        # (b) Gebrauch in diesem Fenster
        spaeter = [u for u in toks[idx + 1:grenze]
                   if u.kind == KIND_ID and u.text == name]
        if not spaeter:
            continue

        out.append(Finding(
            'P3', path, t.line, t.col, 'sicher',
            f'#undef {name}, aber {name} wird danach noch '
            f'{len(spaeter)}x benutzt (erstmals Zeile {spaeter[0].line})',
            'Der Praeprozessor arbeitet textuell: nach dem #undef ist '
            'das Makro fuer den REST der Datei weg, nicht nur fuer den '
            'Zweig, in dem es steht. Ein #undef in einem fruehen '
            'Rueckgabezweig nimmt es dem restlichen Rumpf. Diese Datei '
            'definiert das Makro selbst — es ist also ihr eigenes, und '
            'bis zur naechsten Neudefinition steht dort nichts.',
            t.text.strip()[:60]))
    return out


def audit_c(path, src):
    """Alle Regeln, die EINE Datei allein entscheiden kann.

    K4 fehlt hier absichtlich: sie ist die einzige Regel, die den
    GANZEN Baum braucht — eine Konstante in einer Datei ist eine
    Definition, in zwei ist sie Doppelhaltung.
    """
    return rule_k6(path, src) + rule_p2(path, src) + rule_p3(path, src)


# ── Torschnittstelle (A-029) ─────────────────────────────────────────────

#: Zweite Grundlinie im GEMEINSAMEN Mechanismus aus `audit_common` —
#  eigene Datei, weil die Funde eine eigene Regelgruppe sind, aber
#  dasselbe Format wie `docs/teilstring_baseline.json`. Zwei Formate
#  waeren die Doppelhaltung aus K4.
GRUNDLINIE_REL = os.path.join('docs', 'codefallen_baseline.json')

#: Was `check()` ansieht — dieselben Wurzeln wie der Handlauf, damit Tor
#  und Hand dasselbe messen (MF-1177: die Rechnung gehoert an EINE Stelle).
C_WURZELN = ('src', 'include')


def _sammeln(wurzel, nur_sicher: bool = True) -> list:
    """Fundstellen ueber den ganzen Baum, K4 eingeschlossen.

    @param nur_sicher  Vorgabe `True` — das TOR urteilt ausschliesslich
                       ueber `sicher`, weil `pruefen` heisst „ein Mensch
                       muss hinsehen" und kein Urteil ist. Fuer die
                       GRUNDLINIE wird `False` gebraucht: sie ist eine
                       SCHULDENLISTE des Altbestands, keine Liste der
                       blockierenden Faelle (gemessen in MF-1229, wo
                       eine Grundlinie aus nur `sicher` im
                       Sicherheitsreiter 255 Warnungen stehen liess).
    """
    orte = [os.path.join(str(wurzel), w) for w in C_WURZELN]
    orte = [o for o in orte if os.path.exists(o)]
    if not orte:
        return []

    aus = []
    per_file = {}
    # `wurzel` MUSS durchgereicht werden — ohne sie filtert
    # `audit_common.walk()` gegen den echten Baum und ist in einem
    # gepflanzten Pruefbaum blind (Defekt C aus MF-1229).
    for p in walk(orte, C_EXT, wurzel):
        try:
            src = open(p, encoding='utf-8', errors='replace').read()
        except OSError:
            continue
        rel = os.path.relpath(p, str(wurzel))
        per_file[rel] = collect_constants(src, KONSTANTEN)
        aus += audit_c(rel, src)
    aus += rule_k4(per_file, KONSTANTEN)

    return [f for f in aus if f.severity == 'sicher'] if nur_sicher else aus


def check(wurzel) -> list:
    """Torschnittstelle fuer `scripts/audit_selbsttest.py` (MF-735).

    Gemeldet wird, was NICHT in der Grundlinie steht — also jede neue
    Code-Falle der Einschaetzung `sicher`.
    """
    grundlinie = load_baseline(os.path.join(str(wurzel), GRUNDLINIE_REL))
    neu, _bekannt = apply_baseline(_sammeln(wurzel), grundlinie)
    return ['%s:%s  %s' % (f.path.replace(os.sep, '/'), f.line, f.what)
            for f in neu]


# ── Selbsttest ───────────────────────────────────────────────────────────

def _c(quelle: str, pfad: str = 'src/formats/x.c') -> list:
    return audit_c(pfad, quelle)


def selbsttest() -> int:
    """Prueft den PRUEFER — je ein Fall pro behobenem Defekt.

    Ohne diese Faelle waere keiner der sechs Fixes bewacht, und der
    naechste Umbau koennte sie lautlos zurueckdrehen. Genau das ist bei
    der Uebernahme zweimal passiert: das Paket aus `A-028` und das aus
    `A-029` trugen **keinen** der acht Fixes aus MF-1228, obwohl beide
    NEUER waren. Ein Fix ohne Fall ist eine Absichtserklaerung.
    """
    gut = schlecht = 0

    def zusage(bedingung, text):
        nonlocal gut, schlecht
        if bedingung:
            gut += 1
            print('  [ok ] %s' % text)
        else:
            schlecht += 1
            print('  [ROT] %s' % text)

    wurzel = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    print('Defekt A — P2 meldet JEDES Vorkommen, nicht nur das erste')
    zwei = '/* a forms/*.actions.yaml und forms/*.ui b */\nint x;\n'
    zusage(len(rule_p2('x.c', zwei)) == 2,
           'zwei "/*" in EINEM Kommentar ergeben zwei Funde (gemessen: %d) '
           '— gcc -Wcomment meldet in wiring_runtime.h:13 auch zwei'
           % len(rule_p2('x.c', zwei)))
    zusage(len(rule_p2('x.c', '/* nur eines: forms/*.ui */\nint x;\n')) == 1,
           'ein "/*" ergibt genau einen Fund — kein Doppelzaehlen')
    zusage(not rule_p2('x.c', '/* ganz ohne */\nint x;\n'),
           'ein Kommentar ohne inneren Anfang ergibt nichts')

    print('Defekt B — P3: die Grenze ist die naechste Neudefinition')
    spaet = ('#define K(x) ((x)+1)\nstatic int f(int n){\n'
             '#undef K\n  n = K(n);\n#define K(x) ((x)+2)\n'
             '  return K(n);\n}\n')
    zusage([f for f in _c(spaet) if f.rule == 'P3'],
           'Neudefinition HINTER dem Gebrauch rettet nicht — gcc sagt dazu '
           '„implicit declaration of function"')
    frueh = ('#define H(x) ((x)*1)\nstatic int f(int n){\n'
             '#undef H\n#define H(x) ((x)*2)\n  return H(n);\n}\n')
    zusage(not [f for f in _c(frueh) if f.rule == 'P3'],
           'Neudefinition VOR dem Gebrauch rettet — sonst waere der Fix '
           'ein Falschalarm-Erzeuger')

    print('Defekt C — P3 fragt, WEM das Makro gehoert')
    fremd = ('#undef LoadImage\nstruct s { int LoadImage; };\n'
             'void f(struct s*p){ p->LoadImage = 1; }\n')
    zusage(not [f for f in _c(fremd) if f.rule == 'P3'],
           '#undef eines FREMDEN Namens ist kein Fund — im Baum waren alle '
           '8 Funde dieser Art, jeder als `sicher` blockierend')
    enum_fall = ('#undef UFT_IMD_MODE_500K_FM\n'
                 'typedef enum { UFT_IMD_MODE_500K_FM = 0 } m_t;\n')
    zusage(not [f for f in _c(enum_fall) if f.rule == 'P3'],
           'und auch dann nicht, wenn der spaetere Gebrauch ein enum ist, '
           'das den Namen DEFINIERT (uft_format_parsers.h:363-375)')
    eigen = '#define M(x) (x)\n#undef M\nint f(int n){ return M(n); }\n'
    zusage([f for f in _c(eigen) if f.rule == 'P3'],
           'das EIGENE Makro weggenommen IST ein Fund — der Fix hat die '
           'Regel nicht entkernt')

    print('Defekt D — K4 zaehlt keine Nachschlagetafeln')
    tafel = ('static const unsigned t[] = {\n'
             '  0x2800,0xE8C1,0xE981,0x2940,0xEB01,0x2BC0,0x2A80,0xEA41,\n'
             '  0x2200,0xE2C1,0xE381,0x2340,0xE101,0x21C0,0x2080,0xE041 };\n')
    zusage(not collect_constants(tafel, KONSTANTEN),
           'eine CRC-Tafel ergibt keine Konstanten (gemessen: uft_dms.c:200 '
           'und :203 meldeten 0x2940 und 0x2200)')
    zusage(len(collect_constants('static const int a = 0x2200;\n',
                                 KONSTANTEN)) == 1,
           'derselbe Wert EINZELN ist weiter ein Vorkommen — der Filter '
           'trifft den Lauf, nicht den Wert')
    zusage(collect_constants('static const int t[] = { 0x2200, 1, 2 };\n',
                             KONSTANTEN),
           'ein Lauf unter %d Literalen ist keine Tafel' % TAFEL_LAUF)

    print('Defekt G — K4 haengt nicht an der Durchlaufreihenfolge')
    treffer = [('737280', 1, 1, '737280')]
    vorwaerts = {'src/a/x.c': treffer, 'src/b/y.c': treffer,
                 'src/c/z.c': treffer}
    rueckwaerts = {'src/c/z.c': treffer, 'src/b/y.c': treffer,
                   'src/a/x.c': treffer}
    fv = rule_k4(vorwaerts, KONSTANTEN)
    fr = rule_k4(rueckwaerts, KONSTANTEN)
    zusage(len(fv) == 1 and len(fr) == 1,
           'beide Reihenfolgen ergeben genau einen Fund')
    zusage(fv and fr and fv[0].fingerprint() == fr[0].fingerprint(),
           'und DENSELBEN Fingerabdruck — vorher war er '
           'plattformabhaengig, lokal 0 Funde gegen 12 in CI')
    zusage(fv and fv[0].path == 'src/a/x.c',
           'verankert an der kleinsten Fundstelle, nicht an der '
           'erstgesehenen')
    rueck_sep = {'src\\b\\y.c': treffer, 'src\\a\\x.c': treffer,
                 'src\\c\\z.c': treffer}
    fs = rule_k4(rueck_sep, KONSTANTEN)
    zusage(fs and fs[0].fingerprint() == fv[0].fingerprint(),
           'und Backslash-Pfade ergeben denselben Fingerabdruck wie '
           'Schraegstriche (Windows gegen Linux)')

    print('Defekt E — kleine Konstanten schweigen ohne Schalter')
    for verboten in ('84', '108', '288', '360', '512', '16', '1024'):
        zusage(verboten not in KONSTANTEN,
               '%s steht NICHT in der aktiven Liste — es kommt berechtigt '
               'ueberall vor' % verboten)
    zusage(not (set(KONSTANTEN) & set(KONSTANTEN_OHNE_ZUSAMMENHANG)),
           'und keine Konstante steht in beiden Listen')
    zusage(len(KONSTANTEN_OHNE_ZUSAMMENHANG) == 4,
           'die vier sind aufbewahrt, nicht geloescht (MF-1077)')

    # Der Heimatfilter muss in BEIDE Richtungen geprueft werden. Der
    # Bericht des Eigentuemers sagt das ausdruecklich: „Der Selbsttest
    # prueft beides: dass der Filter wirkt, wenn er an ist, und dass die
    # Regel greift, wenn er aus ist." Ohne die erste Haelfte koennte der
    # Filter still alles verschlucken und der Selbsttest waere gruen,
    # weil er ihn fuer die Koeder ohnehin abschaltet.
    print('Der Heimatfilter wirkt, wenn er an ist')
    drei = {'tests/formats/a.c': [('737280', 1, 1, '737280')],
            'tests/formats/b.c': [('737280', 1, 1, '737280')],
            'tests/formats/c.c': [('737280', 1, 1, '737280')]}
    zusage(not rule_k4(drei, KONSTANTEN),
           'K4 schweigt in Testpfaden — ein Test, der eine Konstante '
           'nennt, tut genau seinen Zweck')
    set_home_filter(False)
    try:
        aus_filter = rule_k4(drei, KONSTANTEN)
    finally:
        set_home_filter(True)
    zusage(len(aus_filter) == 1,
           'und mit --no-home-filter greift sie dort (gemessen: %d) — '
           'sonst koennte der Filter still alles verschlucken'
           % len(aus_filter))

    print('Defekt F — Hex-Ziffern sind keine Suffixe')
    vorher = {'0xFF': '0x00', '0xAF': '0x0A', '0xEF': '0x0E',
              '0xFFFF': '0x00'}
    for lit, soll in (('0xFF', '0xFF'), ('0xAF', '0xAF'), ('0xEF', '0xEF'),
                      ('0xFFFF', '0xFFFF'), ('0x4Eu', '0x4E'),
                      ('0xE5', '0xE5')):
        zusage(_norm_num(lit) == soll,
               '%s normiert auf %s (vorher: %s)'
               % (lit, soll, vorher.get(lit, soll)))
    zusage(_norm_num('12500u') == '12500',
           'und bei Dezimalzahlen wird uUlL weiter abgestreift')
    zusage(_norm_num('1.5f') == '1.5',
           'ein Float-Suffix auch — nur bei Hex nicht')

    print('K6 kann anschlagen — sonst waere seine Null im Baum blind (D1)')
    k6_ja = ('void r(char*out, size_t buflen){ size_t n=0;\n'
             '  for(int i=0;i<3;i++){ n += snprintf(out+n, buflen-n, '
             '"%d", i); }\n'
             '  snprintf(out+n, buflen-n, "WARNUNG: unvollstaendig"); }\n')
    zusage([f for f in _c(k6_ja) if f.rule == 'K6'],
           'Warnung HINTER der Schleife ist ein Fund')
    k6_nein = ('void r(char*out, size_t buflen){ size_t n=0;\n'
               '  snprintf(out+n, buflen-n, "WARNUNG: unvollstaendig");\n'
               '  for(int i=0;i<3;i++){ n += snprintf(out+n, buflen-n, '
               '"%d", i); } }\n')
    zusage(not [f for f in _c(k6_nein) if f.rule == 'K6'],
           'Warnung VOR der Schleife ist keiner')

    print('Ausgabe haelt ein Zeichen ausserhalb von cp1252 aus')
    try:
        print('  (Zeichenprobe: ⁄ — ü)')
        zusage(True, 'die Ausgabe stirbt nicht an einem Zeichen')
    except UnicodeEncodeError:
        zusage(False, 'Ausgabe stirbt weiter an einem Zeichen')

    print('Koederdateien — die bekannte Antwort')
    a = os.path.join(wurzel, 'tests', 'formats', 'fixture_code_a.c')
    b = os.path.join(wurzel, 'tests', 'formats', 'fixture_code_b.c')
    if os.path.exists(a) and os.path.exists(b):
        set_home_filter(False)
        try:
            per_file, einzel = {}, []
            for pfad, rel in ((a, 'tests/formats/fixture_code_a.c'),
                              (b, 'tests/formats/fixture_code_b.c')):
                src = open(pfad, encoding='utf-8', errors='replace').read()
                per_file[rel] = collect_constants(src, KONSTANTEN)
                einzel += audit_c(rel, src)
            k4 = rule_k4(per_file, KONSTANTEN)
        finally:
            set_home_filter(True)
        zusage(len(k4) >= 3,
               'die beiden Koeder teilen mindestens 3 Konstanten '
               '(gemessen: %d)' % len(k4))
        zusage(any(f.rule == 'K6' for f in einzel),
               'Koeder A ergibt den K6-Fall')
        zusage(any(f.rule == 'P3' for f in einzel),
               'Koeder A ergibt den P3-Fall — er definiert APP selbst')
        zusage(any(f.rule == 'P2' for f in einzel),
               'Koeder B ergibt den P2-Fall')
        zusage(not any(f.rule == 'K4' for f in einzel),
               'und K4 steht NICHT in audit_c() — sie braucht den ganzen '
               'Baum')
    else:
        zusage(False, 'die Koederdateien fehlen — ohne sie prueft der '
                      'Selbsttest die Regeln nicht')

    print('\nSELBSTTEST %d/%d' % (gut, gut + schlecht))
    return 0 if schlecht == 0 else 1


# ── Ablauf ───────────────────────────────────────────────────────────────

def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    add_common_args(ap)
    ap.add_argument('--constants', metavar='DATEI',
                    help='eigene Konstantenliste (JSON: Wert -> Bedeutung), '
                         'ersetzt die eingebaute')
    ap.add_argument('--add-constants', metavar='DATEI',
                    help='eigene Liste ZUSAETZLICH zur eingebauten')
    ap.add_argument('--kleine-konstanten', action='store_true',
                    help='die vier Konstanten ohne Zusammenhang mitpruefen '
                         '(84, 108, 288, 360) — sie melden im Baum '
                         'ueberwiegend Fehltreffer, siehe Defekt E')
    ap.add_argument('--no-home-filter', action='store_true',
                    help='K4 auch in Test- und Fixture-Pfaden pruefen '
                         '(fuer den Selbsttest)')
    ap.add_argument('--selbsttest', action='store_true',
                    help='den PRUEFER pruefen (Hausform, MF-735)')
    ap.add_argument('--tor', action='store_true',
                    help='wie das Tor laufen: nur was NICHT in der '
                         'Grundlinie steht')
    ap.add_argument('--schreibe-grundlinie', action='store_true',
                    help='die Grundlinie neu schreiben — sie deckt K4 mit '
                         'ab, was --write-baseline je Datei nicht kann')
    # `roots` ist in `add_common_args` Pflicht; fuer --selbsttest/--tor
    # gibt es keine, also wird die Pflicht hier gelockert statt den
    # gemeinsamen Teil zu verbiegen.
    for a in ap._actions:
        if a.dest == 'roots':
            a.nargs = '*'
    args = ap.parse_args()

    if args.selbsttest:
        return selbsttest()

    if args.schreibe_grundlinie:
        from audit_common import write_baseline          # noqa: PLC0415
        wurzel = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        alle = _sammeln(wurzel, nur_sicher=False)
        ziel = os.path.join(wurzel, GRUNDLINIE_REL)
        alt = load_baseline(ziel) or {}
        # Die Grundlinie darf nur SINKEN. Waechst sie, ist das eine neue
        # Falle und keine Buchhaltung — dann bricht das Schreiben ab und
        # nennt die Zahl (Bauform Tor 57).
        if alt and len(alle) > alt.get('count', 0):
            print('ABBRUCH: %d Fundstellen gegen %d in der Grundlinie — '
                  'sie darf nur sinken.' % (len(alle), alt.get('count', 0)),
                  file=sys.stderr)
            return 2
        data = write_baseline(ziel, alle)
        print('-> %s (%d Fundstellen, %d Fingerabdruecke; vorher %d)'
              % (GRUNDLINIE_REL, data['count'], len(data['accepted']),
                 alt.get('count', 0)))
        return 0

    if args.tor:
        wurzel = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        offen = check(wurzel)
        gl = load_baseline(os.path.join(wurzel, GRUNDLINIE_REL)) or {}
        print('Code-Fallen: %d in der Grundlinie, %d NEU'
              % (gl.get('count', 0), len(offen)))
        for z in offen:
            print('  + %s' % z)
        return 1 if offen else 0

    if not args.roots:
        ap.error('ohne --selbsttest/--tor braucht es mindestens eine Wurzel')

    if args.no_home_filter:
        set_home_filter(False)

    wanted = dict(KONSTANTEN)
    if args.kleine_konstanten:
        wanted.update(KONSTANTEN_OHNE_ZUSAMMENHANG)
    if args.constants:
        with open(args.constants, encoding='utf-8') as fh:
            wanted = json.load(fh)
    if args.add_constants:
        with open(args.add_constants, encoding='utf-8') as fh:
            wanted.update(json.load(fh))

    findings, files = [], 0
    per_file = {}

    for p in walk(args.roots, C_EXT):
        files += 1
        try:
            src = open(p, encoding='utf-8', errors='replace').read()
        except OSError as e:
            print(f'{p}: nicht lesbar ({e})', file=sys.stderr)
            continue
        per_file[p] = collect_constants(src, wanted)
        findings += audit_c(p, src)

    findings += rule_k4(per_file, wanted)

    return run_and_report(args, findings, files, RULE_HELP, TOOL,
                          ERROR_RULES, EMPTY_NOTE)


if __name__ == '__main__':
    sys.exit(main())
