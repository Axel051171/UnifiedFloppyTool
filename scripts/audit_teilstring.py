#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
substring_audit.py — findet Teilstring-Fallen in C-Quellen und Prüfskripten.

    python3 tools/substring_audit.py src include
    python3 tools/substring_audit.py --python scripts
    python3 tools/substring_audit.py --json src | jq .

WAS ES PRUEFT
-------------
C-Regeln, alle auf TOKEN, nicht auf Zeichen:

  C1  strstr/strcasestr mit kurzem Literal auf einem Pfad- oder Namensfeld
      -> Endungspruefung, die irgendwo trifft. `.st` steckt in jedem Pfad
         mit "st" darin.
  C2  strstr/memmem mit einem Literal <= 8 Zeichen in einem Erkennungspfad
      -> Magic-Suche statt Vergleich an fester Stelle. Ein Abbild, das ein
         Archiv ENTHAELT, wird dann zum Archiv.
  C3  strncmp(a, b, strlen(b)) oder strncmp(a, "lit", strlen("lit"))
      -> Praefixvergleich, wo Gleichheit gemeint war. "d8" trifft d80 und
         d82.
  C4  strncmp/memcmp mit einer Laenge, die KUERZER ist als das Literal
      -> halber Vergleich, meist ein Tippfehler
  C5  strncmp/memcmp mit einer Laenge, die LAENGER ist als das Literal
      -> liest hinter das Literal, undefiniert
  C6  sizeof auf einem Zeiger als Laengenargument
      -> vergleicht 8 Byte statt der Zeichenkette

Python-Regel:

  P1  re.search/match/findall/sub mit einem Muster aus Wortzeichen, das
      keine Wortgrenze traegt
      -> genau MF-1171. `int(` findet `print(`.

WAS ES NICHT TUT
----------------
Es wertet keine Makros aus und folgt keinen Einbindungen. Eine Regel, die
`STRSTR(x,y)` hinter einem Makro nicht sieht, ist eine bekannte Luecke und
keine stille: `--list-macros` zeigt alle Makrodefinitionen, deren Rumpf
eine der geprueften Funktionen nennt, damit man sie von Hand nachsieht.

Und es entscheidet nie allein: jeder Fund traegt eine Einschaetzung
(`sicher` / `pruefen`), und `pruefen` heisst, dass ein Mensch hinsehen
muss. Ein Pruefer, der alles fuer sicher erklaert, ist so wertlos wie
einer, der nichts findet.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from c_lex import lex, Tok, KIND_ID, KIND_STR, KIND_NUM, KIND_PUNCT  # noqa
from audit_common import (Finding, walk, add_common_args, run_and_report,  # noqa
                          load_baseline, apply_baseline,
                          C_EXT)

# ── Konfiguration ────────────────────────────────────────────────────────

#: Funktionen, die einen Teilstring SUCHEN — nie zur Erkennung geeignet.
SEARCH_FNS = {'strstr', 'strcasestr', 'memmem', 'strrstr'}

#: Funktionen, die mit einer Laenge VERGLEICHEN.
#  BERICHTIGT A-027/A-028: hier stand
#  `{'strncmp','strncasecmp','memcmp','strncpy','strncat'}` — Kopieren und
#  Vergleichen in EINEM Satz. C4 und C5 haben aber je Familie
#  ENTGEGENGESETZTE Bedeutung, und der Befundtext sagte es selbst
#  („Bei strncmp/memcmp ist das undefiniertes Verhalten"), waehrend die
#  Regel `strncpy` anschlug.
#
#  Gemessen am Baum: von 70 Fundstellen „sicher" waren **21** C5-Treffer,
#  und ALLE 21 waren `strncpy`. `strncpy(dst, "lit", n)` mit
#  `n > strlen(lit)` ist korrekt und idiomatisch — der Standard fuellt den
#  Rest mit Nullbytes und liest die Quelle nur bis zu ihrer Null. Fuer ein
#  Feld fester Breite ist das genau der richtige Aufruf.
#
#  Das ist die eigene Klasse dieses Pruefers, auf ihn selbst angewandt.
NCMP_FNS = {'strncmp', 'strncasecmp', 'memcmp', 'bcmp'}

#: Funktionen, die mit einer Laenge KOPIEREN. Dort ist die Bedeutung von
#  „Laenge gegen Literal" umgekehrt:
#
#    n > len(lit)   korrekt — der Rest wird mit Nullbytes gefuellt
#    n < len(lit)   KUERZUNG, und bei `strncpy` zusaetzlich kein
#                   Nullabschluss. Das ist der schaerfere Fall.
NCPY_FNS = {'strncpy', 'strncat'}

#: Bezeichner, die auf einen Dateipfad hindeuten.
PATH_HINTS = re.compile(r'(path|fname|filename|file|name|ext|url|arg)',
                        re.I)

#: Verzeichnisse, in denen eine Magic-Suche besonders weh tut.
DETECT_DIRS = ('detect', 'formats', 'parsers', 'probe', 'fs')

#: Endungen, die im UFT-Vorrat gegenseitige Teilstrings sind. Wer eine
#: davon mit strstr prueft, hat keinen Randfall, sondern die Regel.
RISKY_EXTS = {
    '.st': ('.stx', '.st0', 'jeder Pfad mit "st"'),
    '.fd': ('.fdi',),
    '.im': ('.img', '.imd'),
    '.do': ('.dot', 'jeder Pfad mit "dos"'),
    '.po': ('.pot',),
    '.d6': ('.d64', '.d67'),
    '.d8': ('.d80', '.d81', '.d82'),
    '.a2': ('.a2r',),
    '.g6': ('.g64',),
    '.sc': ('.scp', '.scl', '.scr'),
    '.td': ('.td0',),
}


# Finding, walk(), Grundlinie und alle Ausgabeformate kommen aus
# audit_common — zwei Kopien waeren genau die Doppelhaltung, die Regel K4
# meldet.
# ── Hilfen auf der Tokenfolge ────────────────────────────────────────────

def _args(toks, i):
    """Argumentliste eines Aufrufs, der bei toks[i] (Bezeichner) beginnt.

    Liefert eine Liste von Token-Listen, eine je Argument, oder None wenn
    bei toks[i+1] keine offene Klammer steht. Klammern und eckige
    Klammern werden mitgezaehlt, damit `f(g(a,b), c)` zwei Argumente
    ergibt und nicht drei."""
    if i + 1 >= len(toks) or toks[i + 1].text != '(':
        return None
    depth = 0
    cur, out = [], []
    j = i + 1
    while j < len(toks):
        t = toks[j]
        if t.kind == KIND_PUNCT and t.text in '([{':
            depth += 1
            if depth == 1:
                j += 1; continue
        elif t.kind == KIND_PUNCT and t.text in ')]}':
            depth -= 1
            if depth == 0:
                out.append(cur); return out
        if depth == 1 and t.kind == KIND_PUNCT and t.text == ',':
            out.append(cur); cur = []
        else:
            cur.append(t)
        j += 1
    return None


def _only_str(arg):
    """Der Wert, wenn das Argument aus GENAU einem Zeichenkettenliteral
    besteht — sonst None. Aneinandergehaengte Literale ("ab" "cd") werden
    zusammengefasst, weil C das auch tut."""
    strs = [t for t in arg if t.kind == KIND_STR]
    if not strs or len(strs) != len(arg):
        return None
    return ''.join(t.value for t in strs)


def _int_value(arg):
    """Der Wert, wenn das Argument eine einzelne Ganzzahl ist."""
    if len(arg) != 1 or arg[0].kind != KIND_NUM:
        return None
    txt = arg[0].text.rstrip('uUlLzZ').replace("'", '')
    try:
        return int(txt, 0)
    except ValueError:
        return None


def _is_strlen_of(arg, other_arg):
    """True, wenn @p arg `strlen(X)` ist und X dem @p other_arg
    entspricht — der klassische Praefixvergleich."""
    if len(arg) < 3 or arg[0].kind != KIND_ID or arg[0].text != 'strlen':
        return False
    inner = [t.text for t in arg[2:] if t.text != ')']
    return inner == [t.text for t in other_arg]


def _text_of(arg):
    return ' '.join(t.text for t in arg)


# ── C-Regeln ─────────────────────────────────────────────────────────────

def audit_c(path: str, src: str):
    out = []
    toks = list(lex(src))
    in_detect = any(('/' + d + '/') in path.replace(os.sep, '/')
                    for d in DETECT_DIRS)

    for i, t in enumerate(toks):
        if t.kind != KIND_ID:
            continue

        # ── C1 / C2: suchende Funktionen ────────────────────────────────
        if t.text in SEARCH_FNS:
            a = _args(toks, i)
            if not a or len(a) < 2:
                continue
            # memmem(haystack, haystacklen, needle, needlelen) — die Nadel
            # steht an Position 3, nicht 2. Wer das nicht unterscheidet,
            # prueft bei memmem die Laenge statt des Musters und meldet
            # jeden Fall nur als "pruefen". Genau so ist es mir beim
            # ersten Lauf gegen die Koederdatei ergangen.
            if t.text == 'memmem' and len(a) >= 3:
                needle = _only_str(a[2])
            else:
                needle = _only_str(a[1])
            hay = _text_of(a[0])

            if needle is not None and needle.startswith('.') and len(needle) <= 5:
                clash = RISKY_EXTS.get(needle[:3].lower())
                out.append(Finding(
                    'C1', path, t.line, t.col, 'sicher',
                    f'{t.text}(..., "{needle}") — Endung wird GESUCHT, '
                    f'nicht am Ende geprueft',
                    (f'"{needle}" trifft auch in: '
                     + ', '.join(clash) if clash else
                     f'"{needle}" trifft an jeder Stelle des Pfades') +
                    '. Ersatz: uft_suffix_eq().',
                    f'{t.text}({hay}, "{needle}")'))
                continue

            if needle is not None and PATH_HINTS.search(hay):
                out.append(Finding(
                    'C1', path, t.line, t.col, 'pruefen',
                    f'{t.text}() auf einem Pfad- oder Namensfeld',
                    'Wenn das eine Endung oder ein Namensende sein soll, '
                    'gehoert uft_suffix_eq() hin.',
                    f'{t.text}({hay}, "{needle}")'))
                continue

            if needle is not None and in_detect and 1 <= len(needle) <= 8:
                out.append(Finding(
                    'C2', path, t.line, t.col, 'sicher',
                    f'{t.text}(..., "{needle}") im Erkennungspfad',
                    f'Eine {len(needle)}-Zeichen-Kennung wird im ganzen '
                    'Puffer gesucht. Ein Abbild, das so etwas ENTHAELT, '
                    'wird dann dafuer gehalten. Ersatz: uft_magic_at().',
                    f'{t.text}({hay}, "{needle}")'))
                continue

            if in_detect:
                out.append(Finding(
                    'C2', path, t.line, t.col, 'pruefen',
                    f'{t.text}() im Erkennungspfad',
                    'Suchen statt Vergleichen an fester Stelle. Pruefen, '
                    'ob die Formatbeschreibung einen Versatz nennt.',
                    f'{t.text}({hay}, '
                    f'{_text_of(a[2] if t.text == "memmem" and len(a) > 2 else a[1])})'))

        # ── C3–C6: vergleichende Funktionen mit Laenge ──────────────────
        elif t.text in NCMP_FNS or t.text in NCPY_FNS:
            a = _args(toks, i)
            if not a or len(a) < 3:
                continue
            lit0 = _only_str(a[0])
            lit1 = _only_str(a[1])
            lit = lit1 if lit1 is not None else lit0
            other = a[0] if lit1 is not None else a[1]
            n = _int_value(a[2])

            # C3: Laenge ist strlen des Vergleichsarguments
            if _is_strlen_of(a[2], a[0]) or _is_strlen_of(a[2], a[1]):
                out.append(Finding(
                    'C3', path, t.line, t.col, 'sicher',
                    f'{t.text}(a, b, strlen(b)) — Praefixvergleich',
                    'Das trifft jeden laengeren Namen mit demselben '
                    'Anfang: "d8" trifft d80, d81 und d82. Wenn '
                    'Gleichheit gemeint ist, gehoert strcmp() hin — oder '
                    'uft_id_eq() fuer Format-IDs.',
                    f'{t.text}({_text_of(a[0])}, {_text_of(a[1])}, '
                    f'{_text_of(a[2])})'))
                continue

            # C6: sizeof auf etwas, das ein Zeiger sein koennte
            if any(tk.kind == KIND_ID and tk.text == 'sizeof' for tk in a[2]):
                arg_txt = _text_of(a[2])
                if '*' in arg_txt or not ('[' in arg_txt or ']' in arg_txt):
                    out.append(Finding(
                        'C6', path, t.line, t.col, 'pruefen',
                        f'{t.text}(..., {arg_txt}) — sizeof als Laenge',
                        'Auf einem Zeiger ergibt sizeof die Zeigergroesse '
                        '(8), nicht die Laenge der Zeichenkette. Bei einem '
                        'echten Feld ist es richtig — also nachsehen.',
                        f'{t.text}(..., ..., {arg_txt})'))
                continue

            if lit is None or n is None:
                continue

            kopiert = t.text in NCPY_FNS

            if n < len(lit):
                if kopiert:
                    # A-027/A-028: bei einer KOPIERfunktion ist das der
                    # schaerfere Fall. `strncpy(dst, "SpartaDOS", 8)`
                    # speichert „SpartaDO" und schreibt KEIN Nullbyte.
                    # Gemessen dreimal in `uft_xfd_parser_v2.c`, bei einem
                    # Feld von 32 Byte — die 8 ist die Laenge eines
                    # ANDEREN Begriffs (Atari-Dateiname).
                    out.append(Finding(
                        'C4', path, t.line, t.col, 'sicher',
                        f'{t.text}(..., "{lit}", {n}) — kopiert nur {n} '
                        f'von {len(lit)} Zeichen',
                        'Der Name wird still gekuerzt, und bei strncpy '
                        'fehlt dann der Nullabschluss. Bei einem Feld '
                        'fester Breite gehoert sizeof(feld) - 1 hin.',
                        f'{t.text}({_text_of(other)}, "{lit}", {n})'))
                else:
                    out.append(Finding(
                        'C4', path, t.line, t.col, 'sicher',
                        f'{t.text}(..., "{lit}", {n}) — nur {n} von '
                        f'{len(lit)} Zeichen verglichen',
                        'Der Rest des Literals wird nie geprueft. '
                        'Entweder ist die Zahl falsch oder das Literal '
                        'zu lang.',
                        f'{t.text}({_text_of(other)}, "{lit}", {n})'))
            elif n > len(lit) and not kopiert:
                out.append(Finding(
                    'C5', path, t.line, t.col, 'sicher',
                    f'{t.text}(..., "{lit}", {n}) — {n} Zeichen bei einem '
                    f'{len(lit)}-Zeichen-Literal',
                    'Liest hinter das Literal. Bei strncmp/memcmp ist das '
                    'undefiniertes Verhalten, auch wenn es meist '
                    'gutgeht.',
                    f'{t.text}({_text_of(other)}, "{lit}", {n})'))
    return out


def list_macros(path: str, src: str):
    """Makrodefinitionen, deren Rumpf eine gepruefte Funktion nennt.

    Das ist die bekannte Luecke, ausdruecklich gemacht: hinter
    `#define FIND(a,b) strstr(a,b)` sieht keine Regel etwas."""
    out = []
    watched = SEARCH_FNS | NCMP_FNS
    for t in lex(src, keep_comments=False):
        if t.kind != 'pp' or not t.text.lstrip('#').lstrip().startswith('define'):
            continue
        for w in watched:
            if re.search(r'\b' + w + r'\s*\(', t.text):
                out.append(Finding(
                    'M1', path, t.line, t.col, 'pruefen',
                    f'Makro nennt {w}()',
                    'Aufrufe dieses Makros sieht keine Regel. Von Hand '
                    'nachsehen.',
                    t.text.strip()[:100]))
                break
    return out


# ── Python-Regel ─────────────────────────────────────────────────────────

_RE_CALL = re.compile(
    r"""\bre\.(search|match|fullmatch|findall|finditer|sub|subn|split|compile)
        \s*\(\s*(r?b?)(['"])(?P<pat>(?:\\.|(?!\3).)*)\3""", re.X)


def audit_python(path: str, src: str):
    """P1: ein Regex-Muster aus Wortzeichen ohne Wortgrenze.

    Genau MF-1171. Wir arbeiten hier absichtlich mit einem Regex — bei
    Python-Quellen ist die Gefahr geringer, weil das Muster selbst ein
    Literal ist und nicht in Code eingebettet steht. Die Grenze wird
    benannt, nicht verschwiegen."""
    out = []
    for m in _RE_CALL.finditer(src):
        pat = m.group('pat')
        line = src.count('\n', 0, m.start()) + 1

        # Ein Muster ohne Wortzeichen kann nicht in ein Wort geraten.
        fn = m.group(1)

        # A-028, Defekt A: `re.fullmatch` ist BEIDSEITIG verankert und
        # kann per Definition nicht mitten in einen laengeren Namen
        # geraten — hier gibt es nichts zu melden.
        if fn == 'fullmatch':
            continue

        # A-028, Defekt B: hier stand `\\[dDwWsSbBAZ]` — eine Aufzaehlung
        # von Escapes, und sie war unvollstaendig. `\n`, `\t`, `\r`, `\f`,
        # `\v` und der verdoppelte Backslash fehlten, also blieben deren
        # Buchstaben als „Wortzeichen" stehen. Gemessen:
        #
        #   Muster `\t\n\r`       -> „Wortzeichen" tnr
        #   Muster `\\\s*\n\s*`   -> „Wortzeichen" sns
        #
        # Das erste enthaelt GAR KEIN Wortzeichen, und die Regel meldete
        # drei. Zwei TORE des Baums standen deshalb als Fundstelle da
        # (`check_consistency.py:46`, `verify_build_sources.py:203`),
        # obwohl ihr Muster nur Zwischenraum zusammenfasst.
        #
        # `\\.` deckt JEDES Escape ab, auch die, die noch keiner
        # aufgeschrieben hat — dieselbe Lehre wie MF-636: eine Regel statt
        # einer Aufzaehlung. Reihenfolge: Escapes zuerst, sonst frisst die
        # Zeichenklasse `[^\]]*` das `\]` eines Escapes.
        core = re.sub(r'\\.', '', pat, flags=re.S)
        core = re.sub(r'\[[^\]]*\]|\(\?[^)]*\)', '', core)
        # A-028, Defekt F: eine WIEDERHOLUNGSANGABE ist kein Literal.
        # `{4,}` lieferte die Ziffer 4 als „Wortzeichen", und
        # `audit_attribution_licence.py:132` stand deshalb mit
        # `\"[^\"]{4,}\"` als Fundstelle da — einem Muster, das nach Abzug
        # der Escapes und der Zeichenklasse aus NICHTS ausser `{4,}`
        # besteht.
        core = re.sub(r'\{\d*(?:,\d*)?\}', '', core)
        if not re.search(r'[A-Za-z0-9_]', core):
            continue
        # Wortgrenze oder Ankerung vorhanden -> in Ordnung.
        if r'\b' in pat or pat.startswith('^') or pat.endswith('$') \
           or r'\A' in pat or r'\Z' in pat or '(?<' in pat:
            continue

        sev = 'sicher' if len(core) <= 6 else 'pruefen'

        # A-028, Defekt A: `re.match` ist am ANFANG verankert — das ist
        # keine Auslegung, sondern die Definition (`re.match('README',
        # 'XREADME') is None`). Die Meldung „kein Anker" war dort
        # sachlich falsch, und die Einschaetzung `sicher` zu hoch: was
        # fehlen KANN, ist der Anker am ENDE, und ob der gebraucht wird,
        # kann diese Regel nicht wissen. Gemessene Fundstelle:
        # `tools/uft-scout/scripts/vermessen.py:249` mit
        # `re.match(r"README")` — fuer „faengt der Name mit README an"
        # richtig.
        if fn == 'match':
            out.append(Finding(
                'P1', path, line, m.start() - src.rfind('\n', 0, m.start()),
                'pruefen',
                f're.match(r"{pat}") — am Anfang verankert, aber nicht '
                f'am Ende',
                'Trifft jeden laengeren Namen mit demselben ANFANG: '
                '"README" trifft auch "README.md.bak". Wenn Gleichheit '
                'gemeint ist, gehoert $ ans Ende oder re.fullmatch hin.',
                pat[:60]))
            continue

        out.append(Finding(
            'P1', path, line, m.start() - src.rfind('\n', 0, m.start()),
            sev,
            f're.{fn}(r"{pat}") — kein \\b, kein Anker',
            'Das trifft mitten in laengeren Namen: "int(" findet "print(". '
            'Entweder \\b setzen, ankern, oder — bei C-Quellen — '
            'c_lex.py benutzen.',
            pat[:60]))
    return out


# ── Ablauf ───────────────────────────────────────────────────────────────

# A-028, Defekt I: hier standen `C_EXT` und `walk()` ein ZWEITES Mal —
# Reste einer unvollstaendigen Auslagerung. Zeile 59 importiert beide aus
# `audit_common`, und diese Kopie darunter hat sie ueberschattet: die
# GEMEINSAME `walk()`, die die Dateimenge aus `git ls-files` holt, war
# damit toter Code, und es galt wieder die hartkodierte Ausschlussliste.
#
# Das ist D3 in seiner unangenehmsten Form — das Wissen wird zweimal
# gehalten, und die ZWEITE Kopie gewinnt. Entfernt; `walk` und `C_EXT`
# kommen ausschliesslich aus `audit_common`.


# ── Ablauf ───────────────────────────────────────────────────────────────

RULE_HELP = {
    'C1': 'Endung wird gesucht statt am Ende geprueft',
    'C2': 'Kennung wird im Puffer gesucht statt an festem Versatz geprueft',
    'C3': 'Praefixvergleich statt Gleichheit',
    'C4': 'Vergleich kuerzer als das Literal',
    'C5': 'Vergleich laenger als das Literal',
    'C6': 'sizeof als Laengenargument',
    'M1': 'Makro versteckt einen geprueften Aufruf',
    'P1': 'Regex ohne Wortgrenze oder Anker',
}

TOOL = 'substringAudit'
ERROR_RULES = frozenset({'C1', 'C3', 'C4', 'C5'})

EMPTY_NOTE = ('Keine Teilstring-Falle gefunden. Das heisst NICHT, dass '
              'keine da ist —\nnur, dass keine der Regeln angeschlagen hat. '
              'Makros sieht keine von\nihnen: --list-macros zeigt, wo von '
              'Hand nachzusehen ist.')


# ── Torschnittstelle (A-027/A-028) ──────────────────────────────────────

#: Die EINE Grundlinie. Format und Mechanik kommen aus `audit_common`,
#  damit nicht zwei Pruefer zwei Formate halten — das waere genau die
#  Doppelhaltung, gegen die dieses Geruest und Regel K4 antreten (D3).
#
#  A-028: hier lag vorher `docs/teilstring_baseline.txt` mit einem
#  eigenen Zeilenformat (`pfad:regel:schnipsel`). Es ist durch den
#  gemeinsamen JSON-Mechanismus ersetzt — dieselbe Absicht, dieselben
#  Fundstellen, EIN Mechanismus. Der Fingerabdruck ist jetzt
#  `Finding.fingerprint()` (sha256 ueber Regel, Pfad und normierten
#  Schnipsel, 16 Stellen), und er nennt seine zwei Preise im eigenen
#  Docstring: zwei gleiche Stellen in einer Datei kollidieren (dafuer
#  `--strict-baseline`), eine umbenannte Datei markiert alles neu.
GRUNDLINIE_REL = os.path.join('docs', 'teilstring_baseline.json')

#: Was `check()` ansieht — dieselben Wurzeln wie der Handlauf, damit Tor
#  und Hand dasselbe messen (MF-1177: die Rechnung gehoert an EINE Stelle).
C_WURZELN = ('src', 'include')
PY_WURZELN = ('scripts', 'tools')


def _sammeln(wurzel, nur_sicher: bool = True) -> list:
    """Fundstellen ueber den ganzen Baum.

    @param nur_sicher  Vorgabe `True` — das TOR urteilt ausschliesslich
                       ueber `sicher`, weil `pruefen` heisst „ein Mensch
                       muss hinsehen" und kein Urteil ist.

                       Fuer die GRUNDLINIE wird `False` gebraucht, und
                       das ist gemessen begruendet: mit einer Grundlinie
                       aus nur `sicher` meldete der SARIF-Lauf **255**
                       Ergebnisse — den ganzen `pruefen`-Altbestand. 255
                       Warnungen am ersten Tag im Sicherheitsreiter
                       ertraenken jedes neue Signal, und die Grundlinie
                       ist ausdruecklich eine SCHULDENLISTE des
                       Altbestands, keine Liste der blockierenden Faelle.
                       Blockiert wird weiter nur ueber `--fail-on
                       sicher`; die Trennung liegt also dort, wo sie
                       hingehoert, und nicht im Inhalt der Grundlinie.
    """
    aus = []
    for wurzeln, py in ((C_WURZELN, False), (PY_WURZELN, True)):
        orte = [os.path.join(str(wurzel), w) for w in wurzeln]
        orte = [o for o in orte if os.path.exists(o)]
        if not orte:
            continue
        # `wurzel` MUSS durchgereicht werden — ohne sie filtert
        # `audit_common.walk()` gegen den echten Baum und ist in einem
        # gepflanzten Pruefbaum blind (siehe dort).
        for p in walk(orte, ('.py',) if py else C_EXT, wurzel):
            try:
                src = open(p, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            rel = os.path.relpath(p, str(wurzel))
            found = audit_python(rel, src) if py else audit_c(rel, src)
            aus += ([f for f in found if f.severity == 'sicher']
                    if nur_sicher else found)
    return aus


def check(wurzel) -> list:
    """Torschnittstelle fuer `scripts/audit_selbsttest.py` (MF-735).

    Gemeldet wird, was NICHT in der Grundlinie steht — also jede neue
    Teilstring-Falle der Einschaetzung `sicher`.
    """
    grundlinie = load_baseline(os.path.join(str(wurzel), GRUNDLINIE_REL))
    neu, _bekannt = apply_baseline(_sammeln(wurzel), grundlinie)
    return ['%s:%s  %s' % (f.path.replace(os.sep, '/'), f.line, f.what)
            for f in neu]


# ── Selbsttest ───────────────────────────────────────────────────────────

def _c(quelle: str, pfad: str = 'src/formats/x.c') -> list:
    return audit_c(pfad, quelle)


def _py(quelle: str) -> list:
    return audit_python('scripts/x.py', quelle)


def selbsttest() -> int:
    """Prueft den PRUEFER — je ein Fall pro behobenem Defekt.

    Ohne diese Faelle waere keiner der neun Fixes bewacht, und der
    naechste Umbau koennte sie lautlos zurueckdrehen. Genau das ist bei
    der Uebernahme zweimal passiert: das Paket aus `A-028` und das aus
    `A-029` trugen **keinen** der Fixes aus MF-1228, obwohl beide
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

    print('Defekt D — Kopieren ist nicht Vergleichen')
    zusage(not [x for x in _c('void f(char*d){ strncpy(d, "abc", 16); }')
                if x.rule == 'C5'],
           'strncpy(d, "abc", 16) ist KEIN C5 — der Standard fuellt den '
           'Rest mit Nullbytes (vorher: 21 Fehltreffer im Baum)')
    zusage([x for x in _c('int f(const char*a){ return strncmp(a,"abc",16); }')
            if x.rule == 'C5'],
           'strncmp(a, "abc", 16) IST C5 — dort liest es hinter das Literal')
    zusage([x for x in _c('void f(char*d){ strncpy(d, "SpartaDOS", 8); }')
            if x.rule == 'C4'],
           'strncpy(d, "SpartaDOS", 8) IST C4 — Kuerzung auf "SpartaDO"')

    print('Defekt A — re.match ist verankert, re.fullmatch beidseitig')
    zusage(not _py('re.fullmatch(r"README", s)'),
           're.fullmatch(r"README") ergibt KEINEN Fund — beidseitig '
           'verankert')
    r = _py('re.match(r"README", s)')
    zusage(len(r) == 1 and r[0].severity == 'pruefen',
           're.match(r"README") ergibt EINEN Fund der Einschaetzung '
           '`pruefen` — am Anfang verankert, aber nicht am Ende')
    zusage(r and 'kein Anker' not in r[0].what,
           'und die Meldung sagt NICHT mehr „kein Anker" — das war '
           'sachlich falsch')

    print('Defekt B — Escapes tragen keine Wortzeichen')
    zusage(not _py(r'''re.search(r"\t\n\r", s)'''),
           r're.search(r"\t\n\r") ergibt KEINEN Fund — das Muster hat gar '
           'kein Wortzeichen (vorher meldete die Regel drei: t, n, r)')
    zusage(not _py(r'''re.sub(r"\\\s*\n\s*", "", s)'''),
           r're.sub(r"\\\s*\n\s*") ergibt KEINEN Fund — vorher standen '
           'zwei Tore des Baums deshalb als Fundstelle da')

    print('Defekt F — eine Wiederholungsangabe ist kein Literal')
    zusage(not _py(r'''re.search(r"[^x]{4,}", s)'''),
           r're.search(r"[^x]{4,}") ergibt KEINEN Fund — die 4 ist eine '
           'Wiederholungszahl')

    print('Defekt I — walk() kommt AUSSCHLIESSLICH aus audit_common')
    import audit_common as _ac
    zusage(walk is _ac.walk,
           'die lokale Doppelung ist weg: `walk` ist dasselbe Objekt wie '
           '`audit_common.walk` — vorher ueberschattete eine zweite '
           'Fassung den gemeinsamen Fix')
    zusage(C_EXT is _ac.C_EXT,
           'und `C_EXT` ebenso — dasselbe Objekt, nicht eine Kopie mit '
           'gleichem Wert')

    print('Defekt J — der Fingerabdruck haengt nicht am Pfadtrenner')
    f_bs = Finding('C2', r'src\formats\x.c', 1, 1, 'sicher', '', '', 'strstr(a,"b")')
    f_fs = Finding('C2', 'src/formats/x.c', 1, 1, 'sicher', '', '', 'strstr(a,"b")')
    zusage(f_bs.fingerprint() == f_fs.fingerprint(),
           'derselbe Fund hat mit `\\` und mit `/` DENSELBEN '
           'Fingerabdruck — vorher waren es zwei, und eine auf Windows '
           'geschriebene Grundlinie war in CI (Linux) wirkungslos: JEDE '
           'bekannte Fundstelle waere als NEU gemeldet worden')

    print('Defekt C/E — Dateimenge aus git, Ausgabe haelt ein Zeichen aus')
    wurzel = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    fremd = os.path.join(wurzel, 'tools', 'uft-scout', 'work')
    if os.path.isdir(fremd):
        treffer = [p for p in walk([os.path.join(wurzel, 'tools')],
                                   ('.py',), wurzel)
                   if 'uft-scout' in p and 'work' in p]
        zusage(not treffer,
               'kein Fund aus tools/uft-scout/work/ — geklonte Fremd-Repos '
               'sind gitignoriert, und CI sieht sie nie')
    else:
        print('  [--- ] tools/uft-scout/work/ fehlt — Fall nicht pruefbar, '
              'und das wird gesagt statt uebersprungen')
    _ac._ausgabe_haerten()
    try:
        print('  (Zeichenprobe: ⁄ — ü)')
        zusage(True, 'ein Zeichen ausserhalb von cp1252 laesst die Ausgabe '
                     'nicht sterben')
    except UnicodeEncodeError:
        zusage(False, 'Ausgabe stirbt weiter an einem Zeichen')

    print('Koederdatei — die bekannte Antwort')
    koeder = os.path.join(wurzel, 'tests', 'formats', 'fixture_traps.c')
    if os.path.exists(koeder):
        src = open(koeder, encoding='utf-8', errors='replace').read()
        r = audit_c('tests/formats/fixture_traps.c', src)
        zusage(len(r) == 9,
               'die Koederdatei ergibt 9 Funde (gemessen: %d) — jede Falle '
               'genau einmal' % len(r))
        zusage(any(x.rule == 'C5' for x in r),
               'darunter ein ECHTER C5-Fall mit einer Vergleichsfunktion — '
               'die Trennung aus Defekt D hat die Regel nicht entkernt')
        zusage(any(x.rule == 'C2' and x.severity == 'sicher' for x in r),
               'und ein C2 der Einschaetzung `sicher` — ihr Pfad traegt '
               'absichtlich „formats", sonst greift die Regel nicht')
    else:
        zusage(False, 'die Koederdatei fehlt — ohne sie prueft der '
                      'Selbsttest die Regeln nicht')

    print('\nSELBSTTEST %d/%d' % (gut, gut + schlecht))
    return 0 if schlecht == 0 else 1


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    add_common_args(ap)
    ap.add_argument('--python', action='store_true',
                    help='Python-Quellen pruefen statt C')
    ap.add_argument('--list-macros', action='store_true',
                    help='nur die Makroluecke auflisten')
    ap.add_argument('--selbsttest', action='store_true',
                    help='den PRUEFER pruefen (Hausform, MF-735)')
    ap.add_argument('--tor', action='store_true',
                    help='wie das Tor laufen: nur was NICHT in der '
                         'Grundlinie steht')
    ap.add_argument('--schreibe-grundlinie', action='store_true',
                    help='die EINE Grundlinie neu schreiben — sie deckt '
                         'C- UND Python-Seite ab, was --write-baseline '
                         'in einem Lauf nicht kann')
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
        # Die Grundlinie traegt den GANZEN Altbestand, nicht nur die
        # blockierenden Faelle — siehe `_sammeln()`.
        alle = _sammeln(wurzel, nur_sicher=False)
        ziel = os.path.join(wurzel, GRUNDLINIE_REL)
        alt = load_baseline(ziel) or {}
        # A-028: die Grundlinie darf nur SINKEN. Waechst sie, ist das eine
        # neue Falle und keine Buchhaltung — dann bricht das Schreiben ab
        # und nennt die Zahl (Bauform Tor 57).
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
        print('Teilstring-Fallen: %d in der Grundlinie, %d NEU'
              % (gl.get('count', 0), len(offen)))
        for z in offen:
            print('  + %s' % z)
        return 1 if offen else 0

    if not args.roots:
        ap.error('ohne --selbsttest/--tor braucht es mindestens eine Wurzel')

    exts = ('.py',) if args.python else C_EXT
    findings, files = [], 0

    for p in walk(args.roots, exts):
        files += 1
        try:
            src = open(p, encoding='utf-8', errors='replace').read()
        except OSError as e:
            print(f'{p}: nicht lesbar ({e})', file=sys.stderr)
            continue
        if args.python:
            findings += audit_python(p, src)
        elif args.list_macros:
            findings += list_macros(p, src)
        else:
            findings += audit_c(p, src)

    return run_and_report(args, findings, files, RULE_HELP, TOOL,
                          ERROR_RULES, EMPTY_NOTE)


if __name__ == '__main__':
    sys.exit(main())
