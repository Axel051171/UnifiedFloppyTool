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
from dataclasses import dataclass, asdict

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from c_lex import lex, Tok, KIND_ID, KIND_STR, KIND_NUM, KIND_PUNCT  # noqa

# ── Konfiguration ────────────────────────────────────────────────────────

#: Funktionen, die einen Teilstring SUCHEN — nie zur Erkennung geeignet.
SEARCH_FNS = {'strstr', 'strcasestr', 'memmem', 'strrstr'}

#: Funktionen, die mit einer Laenge VERGLEICHEN.
#
#  BERICHTIGT bei der Uebernahme (A-027): hier stand
#  `{'strncmp','strncasecmp','memcmp','strncpy','strncat'}` — Kopieren und
#  Vergleichen in EINEM Satz. C4 und C5 haben aber je Familie
#  ENTGEGENGESETZTE Bedeutung, und der Befundtext sagte es selbst
#  („Bei strncmp/memcmp ist das undefiniertes Verhalten"), waehrend die
#  Regel `strncpy` anschlug.
#
#  Gemessen am Baum: von 70 Fundstellen der Einschaetzung „sicher" waren
#  **21** C5-Treffer, und ALLE 21 waren `strncpy`. `strncpy(dst, "lit", n)`
#  mit `n > strlen(lit)` ist korrekt und idiomatisch — der Standard fuellt
#  den Rest mit Nullbytes und liest die Quelle nur bis zu ihrer Null. Fuer
#  ein Feld fester Breite ist das genau der richtige Aufruf.
#
#  Das ist die eigene Klasse dieses Pruefers, auf ihn selbst angewandt:
#  eine Regel, die nicht unterscheidet, was sie ansieht.
NCMP_FNS = {'strncmp', 'strncasecmp', 'memcmp', 'bcmp'}

#: Funktionen, die mit einer Laenge KOPIEREN. Bei ihnen ist die
#  Bedeutung von „Laenge gegen Literal" umgekehrt:
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


@dataclass
class Finding:
    rule: str
    path: str
    line: int
    col: int
    severity: str      # 'sicher' | 'pruefen'
    what: str
    why: str
    snippet: str = ''


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
                    # A-027: bei einer KOPIERfunktion ist das der schaerfere
                    # Fall. `strncpy(dst, "SpartaDOS", 8)` speichert
                    # „SpartaDO" und schreibt KEIN Nullbyte — das Feld ist
                    # danach nur nullabgeschlossen, wenn der Aufrufer es
                    # selbst setzt. Gemessen an `uft_xfd_parser_v2.c` tut er
                    # das dort (`dos_name[8] = 0`), die Kuerzung bleibt.
                    out.append(Finding(
                        'C4', path, t.line, t.col, 'sicher',
                        f'{t.text}(..., "{lit}", {n}) — kopiert nur {n} von '
                        f'{len(lit)} Zeichen',
                        'Der Name wird still gekuerzt, und bei strncpy fehlt '
                        'dann der Nullabschluss. Entweder ist die Zahl '
                        'falsch oder das Literal zu lang — bei einem Feld '
                        'fester Breite gehoert sizeof(feld) - 1 hin.',
                        f'{t.text}({_text_of(other)}, "{lit}", {n})'))
                else:
                    out.append(Finding(
                        'C4', path, t.line, t.col, 'sicher',
                        f'{t.text}(..., "{lit}", {n}) — nur {n} von '
                        f'{len(lit)} Zeichen verglichen',
                        'Der Rest des Literals wird nie geprueft. Entweder '
                        'ist die Zahl falsch oder das Literal zu lang.',
                        f'{t.text}({_text_of(other)}, "{lit}", {n})'))
            elif n > len(lit) and not kopiert:
                # A-027: `not kopiert` ist der ganze Punkt. Bei `strncpy`
                # und `strncat` ist eine Laenge GROESSER als das Literal
                # korrekt und idiomatisch — der Standard fuellt den Rest mit
                # Nullbytes und liest die Quelle nur bis zu ihrer Null.
                # Vorher schlug die Regel dort 21-mal an, und jeder Treffer
                # war falsch.
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
    watched = SEARCH_FNS | NCMP_FNS | NCPY_FNS
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

        fn = m.group(1)

        # A-027, Defekt A: `re.fullmatch` ist BEIDSEITIG verankert. Es
        # kann per Definition nicht mitten in einen laengeren Namen
        # geraten — hier gibt es nichts zu melden.
        if fn == 'fullmatch':
            continue

        # Ein Muster ohne Wortzeichen kann nicht in ein Wort geraten.
        #
        # A-027, Defekt B: hier stand `\\[dDwWsSbBAZ]` — eine Aufzaehlung
        # von Escapes, und sie war unvollstaendig. `\n`, `\t`, `\r`, `\f`,
        # `\v` und der verdoppelte Backslash `\\` fehlten, also blieben
        # deren Buchstaben als „Wortzeichen" stehen. Gemessen:
        #
        #   Muster `\t\n\r`        -> core `\t\n\r`  -> „Wortzeichen" tnr
        #   Muster `\\\s*\n\s*`    -> core unveraendert -> „Wortzeichen" sns
        #
        # Das erste Muster enthaelt GAR KEIN Wortzeichen, und die Regel
        # meldete drei. Zwei Tore des Baums (`check_consistency.py:46`,
        # `verify_build_sources.py:203`) standen deshalb als Fundstelle
        # da, obwohl ihr Muster nur Zwischenraum zusammenfasst.
        #
        # `\\.` deckt JEDES Escape ab, auch die, die noch keiner
        # aufgeschrieben hat — das ist derselbe Grundsatz wie MF-636:
        # keine gepflegte Liste, wo eine Regel genuegt. Reihenfolge:
        # Escapes zuerst, sonst frisst die Zeichenklasse `[^\]]*` das
        # `\]` eines Escapes.
        core = re.sub(r'\\.', '', pat, flags=re.S)
        core = re.sub(r'\[[^\]]*\]|\(\?[^)]*\)', '', core)
        # A-027, Defekt F (beim Nachmessen von B gefunden): eine
        # WIEDERHOLUNGSANGABE ist kein Literal. `{4,}` lieferte die Ziffer
        # 4 als „Wortzeichen", und `audit_attribution_licence.py:132` stand
        # deshalb mit `\"[^\"]{4,}\"` als Fundstelle da — einem Muster, das
        # nach dem Abziehen der Escapes und der Zeichenklasse aus NICHTS
        # ausser `{4,}` besteht.
        core = re.sub(r'\{\d*(?:,\d*)?\}', '', core)
        if not re.search(r'[A-Za-z0-9_]', core):
            continue
        # Wortgrenze oder Ankerung vorhanden -> in Ordnung.
        if r'\b' in pat or pat.startswith('^') or pat.endswith('$') \
           or r'\A' in pat or r'\Z' in pat or '(?<' in pat:
            continue

        sev = 'sicher' if len(core) <= 6 else 'pruefen'

        # A-027, Defekt A: `re.match` ist am ANFANG verankert — das ist
        # keine Auslegung, sondern die Definition (`re.match('README',
        # 'XREADME') is None`). Die Meldung „kein Anker" war dort
        # sachlich falsch, und die Einschaetzung `sicher` zu hoch: was
        # fehlen KANN, ist der Anker am ENDE, und ob der gebraucht wird,
        # kann diese Regel nicht wissen. Gemessene Fundstelle:
        # `tools/uft-scout/scripts/vermessen.py:249` mit `re.match(
        # r"README")` — fuer „faengt der Name mit README an" richtig.
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

C_EXT = ('.c', '.h', '.cpp', '.hpp', '.cc', '.cxx')


def walk(roots, exts, wurzel=None):
    """Die Dateimenge kommt aus git, nicht aus einer gepflegten Liste.

    A-027, Defekt C: hier stand eine hartkodierte Ausschlussliste
    (`.git`, `build`, `obj`, `bin`, `__pycache__`). Das ist genau die
    Aufzaehlung bekannter Faelle, die `CLAUDE.md` §MF-636 verbietet, und
    der Preis war messbar: von **10** Python-Fundstellen der
    Einschaetzung „sicher" lagen **5** unter `tools/uft-scout/work/` —
    geklonten FREMD-Repos (fluxfox, hardsector_tool, greaseweazle). CI
    sieht diese Dateien nie; ein Befund darin ist richtig gesehen und
    vollkommen belanglos.

    `scripts/repo_scope.py` ist der Helfer dafuer, und seine eigene
    Zusage beschreibt DENSELBEN Fall (MF-633, nach dem nibtools-Klon).
    Ist git nicht befragbar, laesst er alles durch **und sagt es** — die
    Warnung wird weitergegeben, nicht geschluckt (MF-1171: eine
    grosszuegige Rueckfallebene ist eine Anforderung an den Parser
    dahinter, keine Nachsicht).
    """
    hier = os.path.dirname(os.path.abspath(__file__))
    from pathlib import Path
    # A-027: der Filter wird aus dem UNTERSUCHTEN Baum gebaut, nicht aus
    # dem Ort dieses Skripts.
    #
    # Der Unterschied ist kein Feinschliff, und `audit_selbsttest.py` hat
    # ihn gefunden: mit `Path(hier).parent` — also dem echten Baum — sind
    # die Dateien eines GEPFLANZTEN Prueflings dort nicht verzeichnet,
    # und `walk()` filterte sie restlos weg. Das Tor meldete auf drei
    # gepflanzten Defekten „blind — gepflanzter Defekt nicht gemeldet".
    #
    # Ein Tor, das im gepflanzten Baum nichts sieht, sieht auch sonst
    # nichts, sobald es auf einer anderen Wurzel laeuft — die Klasse
    # MF-1000/Tor 64: ein Pruefer, der nicht anschlagen KANN.
    basis = Path(wurzel) if wurzel is not None else Path(hier).parent
    try:
        sys.path.insert(0, hier)
        from repo_scope import make_filter          # noqa: PLC0415
        im_baum, warnung = make_filter(basis)
    except Exception as exc:                        # noqa: BLE001
        im_baum, warnung = (lambda p: True), (
            'repo_scope nicht ladbar (%s) — es wird der GANZE '
            'Verzeichnisbaum geprueft' % exc)
    if warnung:
        print(warnung, file=sys.stderr)

    for root in roots:
        if os.path.isfile(root):
            if root.endswith(exts) and im_baum(Path(root)):
                yield root
            continue
        for dirpath, dirnames, files in os.walk(root):
            dirnames[:] = [d for d in dirnames
                           if d not in ('.git', 'build', 'obj', 'bin',
                                        '__pycache__')]
            for f in sorted(files):
                if not f.endswith(exts):
                    continue
                p = os.path.join(dirpath, f)
                if im_baum(Path(p)):
                    yield p


# ── Grundlinie und Torschnittstelle ─────────────────────────────────────

#: Wo die bekannten Fundstellen stehen. Bauform Tor 57: die Zahl darf nur
#: SINKEN, und eine neue Fundstelle faellt sofort auf. Der Zweck ist der
#: Rand — eine neue Datei kann gar nicht mehr mit einer Teilstring-Falle
#: anfangen.
GRUNDLINIE_REL = os.path.join('docs', 'teilstring_baseline.txt')

#: Was `check()` ansieht. Bewusst dieselben Wurzeln wie die Vorgabe der
#: Kommandozeile, damit Tor und Handlauf dasselbe messen (MF-1177: die
#: Rechnung gehoert an EINE Stelle).
C_WURZELN = ('src', 'include')
PY_WURZELN = ('scripts', 'tools')


def _schluessel(f: 'Finding') -> str:
    """Die Kennung einer Fundstelle in der Grundlinie.

    Ohne Spalte und ohne Zeilennummer: eine Zeile, die sich um zwei
    Stellen verschiebt, ist nicht eine NEUE Falle. Sonst waere jede
    Umformatierung ein Befund, und ein Tor, das bei Umformatierungen
    anschlaegt, wird abgeschaltet.
    """
    return '%s:%s:%s' % (f.path.replace(os.sep, '/'), f.rule, f.snippet[:60])


def _grundlinie_lesen(wurzel) -> set:
    p = os.path.join(str(wurzel), GRUNDLINIE_REL)
    if not os.path.exists(p):
        return set()
    aus = set()
    with open(p, encoding='utf-8') as fh:
        for z in fh:
            z = z.strip()
            if z and not z.startswith('#'):
                aus.add(z)
    return aus


def _sammeln(wurzel) -> list:
    """Alle Fundstellen der Einschaetzung `sicher` ueber den ganzen Baum."""
    aus = []
    for wurzeln, py in ((C_WURZELN, False), (PY_WURZELN, True)):
        orte = [os.path.join(str(wurzel), w) for w in wurzeln]
        orte = [o for o in orte if os.path.exists(o)]
        if not orte:
            continue
        for p in walk(orte, ('.py',) if py else C_EXT, wurzel):
            try:
                src = open(p, encoding='utf-8', errors='replace').read()
            except OSError:
                continue
            rel = os.path.relpath(p, str(wurzel))
            found = audit_python(rel, src) if py else audit_c(rel, src)
            aus += [f for f in found if f.severity == 'sicher']
    return aus


def check(wurzel) -> list:
    """Torschnittstelle fuer `scripts/audit_selbsttest.py` (MF-735).

    Gemeldet wird, was NICHT in der Grundlinie steht — also jede neue
    Teilstring-Falle der Einschaetzung `sicher`.
    """
    bekannt = _grundlinie_lesen(wurzel)
    return ['%s:%s  %s' % (f.path.replace(os.sep, '/'), f.line, f.what)
            for f in _sammeln(wurzel) if _schluessel(f) not in bekannt]


# ── Selbsttest ───────────────────────────────────────────────────────────

def _regeln_von(quelle: str, pfad: str = 'src/formats/x.c') -> list:
    return audit_c(pfad, quelle)


def _py_regeln_von(quelle: str) -> list:
    return audit_python('scripts/x.py', quelle)


def selbsttest() -> int:
    """Prueft den PRUEFER — je ein Fall pro behobenem Defekt.

    Ohne diese Faelle waere keiner der sechs Fixes bewacht, und der
    naechste Umbau koennte sie lautlos zurueckdrehen. Der Baum hat die
    Lehre teuer bezahlt: `tuersucher.py` meldete einmal „Selbsttest 3/3"
    und lieferte gemessen 0/3.
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
    r = _regeln_von('void f(char*d){ strncpy(d, "abc", 16); }')
    zusage(not [x for x in r if x.rule == 'C5'],
           'strncpy(d, "abc", 16) ist KEIN C5 — der Standard fuellt den '
           'Rest mit Nullbytes (vorher: 21 Fehltreffer im Baum)')
    r = _regeln_von('int f(const char*a){ return strncmp(a, "abc", 16); }')
    zusage([x for x in r if x.rule == 'C5'],
           'strncmp(a, "abc", 16) IST C5 — dort liest es hinter das '
           'Literal')
    r = _regeln_von('void f(char*d){ strncpy(d, "SpartaDOS", 8); }')
    zusage([x for x in r if x.rule == 'C4'],
           'strncpy(d, "SpartaDOS", 8) IST C4 — Kuerzung auf "SpartaDO"')

    print('Defekt A — re.match ist verankert, re.fullmatch beidseitig')
    zusage(not _py_regeln_von('re.fullmatch(r"README", s)'),
           're.fullmatch(r"README") ergibt KEINEN Fund — beidseitig '
           'verankert')
    r = _py_regeln_von('re.match(r"README", s)')
    zusage(len(r) == 1 and r[0].severity == 'pruefen',
           're.match(r"README") ergibt EINEN Fund der Einschaetzung '
           '`pruefen` — am Anfang verankert, aber nicht am Ende')
    zusage(r and 'kein Anker' not in r[0].what,
           'und die Meldung sagt NICHT mehr „kein Anker" — das war '
           'sachlich falsch')

    print('Defekt B — Escapes tragen keine Wortzeichen')
    zusage(not _py_regeln_von(r'''re.search(r"\t\n\r", s)'''),
           r're.search(r"\t\n\r") ergibt KEINEN Fund — das Muster hat gar '
           'kein Wortzeichen (vorher meldete die Regel drei: t, n, r)')
    zusage(not _py_regeln_von(r'''re.sub(r"\\\s*\n\s*", "", s)'''),
           r're.sub(r"\\\s*\n\s*") ergibt KEINEN Fund — vorher standen '
           'zwei Tore des Baums deshalb als Fundstelle da')

    print('Defekt F — eine Wiederholungsangabe ist kein Literal')
    zusage(not _py_regeln_von(r'''re.search(r"[^x]{4,}", s)'''),
           r're.search(r"[^x]{4,}") ergibt KEINEN Fund — die 4 ist eine '
           'Wiederholungszahl')

    print('Defekt E — ein Absturz ist kein Urteil')
    _ausgabe_haerten()
    try:
        print('  (Zeichenprobe: ⁄ — ü)')
        zusage(True, 'ein Zeichen ausserhalb von cp1252 laesst die Ausgabe '
                     'nicht sterben (vorher: UnicodeEncodeError mitten im '
                     'Lauf, rc 1 wie „hat gefunden")')
    except UnicodeEncodeError:
        zusage(False, 'Ausgabe stirbt weiter an einem Zeichen')

    print('Defekt C — die Dateimenge kommt aus git (MF-636)')
    wurzel = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    fremd_ort = os.path.join(wurzel, 'tools', 'uft-scout', 'work')
    if os.path.isdir(fremd_ort):
        treffer = [p for p in walk([os.path.join(wurzel, 'tools')], ('.py',))
                   if 'uft-scout' in p and 'work' in p]
        zusage(not treffer,
               'kein Fund aus tools/uft-scout/work/ — geklonte Fremd-Repos '
               'sind gitignoriert, und CI sieht sie nie (%d Dateien '
               'geprueft)' % len(treffer))
    else:
        print('  [--- ] tools/uft-scout/work/ fehlt — Fall nicht pruefbar, '
              'und das wird gesagt statt uebersprungen')

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


def _ausgabe_haerten() -> None:
    """A-027, Defekt E: ein Absturz ist kein Urteil.

    Gemessen beim ersten Lauf ueber `src include`: das Werkzeug starb mit
    `UnicodeEncodeError: 'charmap' codec can't encode character '\\u2044'`
    beim Drucken eines Schnipsels aus
    `src/formats/reference/uft_floppy_reference.c:168` — die
    Vorgabekodierung der Windows-Konsole ist cp1252. Es hatte zu dem
    Zeitpunkt **vier von 14 Dateien** mit Funden gedruckt und den Rest
    des Baums nie gesehen; der Rueckgabewert war **1** und sah damit
    genau wie „hat etwas gefunden" aus.

    Das ist wortwoertlich die Klasse, die MF-1171 benennt: dort starb
    `enum_macro_conflicts.py` an `int('0170000', 0)` und
    `check_consistency.py` endete mit rc 1, **bevor die uebrigen 23
    Kategorien liefen**. Ein Tor, das stirbt, hat nicht geurteilt.

    Ein Pruefer fuer Quelltext MUSS mit Quelltext umgehen koennen, der
    Zeichen enthaelt, die die Konsole nicht darstellen kann. Nicht
    darstellbare Zeichen werden deshalb ersetzt — die FUNDSTELLE bleibt
    vollstaendig (Pfad, Zeile, Spalte, Regel), und nur der Schnipsel
    verliert ein Zeichen. Das ist die richtige Richtung: lieber ein
    unleserliches Zeichen im Zitat als ein unvollstaendiges Urteil.
    """
    for strom in (sys.stdout, sys.stderr):
        try:
            strom.reconfigure(errors='replace')     # Python >= 3.7
        except (AttributeError, OSError):
            pass


def main():
    _ausgabe_haerten()
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('roots', nargs='*', help='Dateien oder Verzeichnisse')
    ap.add_argument('--selbsttest', action='store_true',
                    help='den PRUEFER pruefen (Hausform, MF-735)')
    ap.add_argument('--tor', action='store_true',
                    help='wie das Tor laufen: nur was NICHT in der '
                         'Grundlinie steht')
    ap.add_argument('--schreibe-grundlinie', action='store_true',
                    help='die Grundlinie neu schreiben — sie darf nur '
                         'SINKEN')
    ap.add_argument('--python', action='store_true',
                    help='Python-Quellen pruefen statt C')
    ap.add_argument('--list-macros', action='store_true',
                    help='nur die Makroluecke auflisten')
    ap.add_argument('--json', action='store_true')
    ap.add_argument('--only', metavar='REGEL',
                    help='nur diese Regel, z. B. C3')
    ap.add_argument('--fail-on', default='sicher',
                    choices=('nichts', 'sicher', 'alles'),
                    help='Rueckgabewert 1 ab dieser Einschaetzung '
                         '(Vorgabe: sicher)')
    args = ap.parse_args()

    if args.selbsttest:
        return selbsttest()

    wurzel = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

    if args.tor or args.schreibe_grundlinie:
        alle = _sammeln(wurzel)
        if args.schreibe_grundlinie:
            alt = _grundlinie_lesen(wurzel)
            neu = {_schluessel(f) for f in alle}
            # A-027: die Grundlinie darf nur SINKEN. Waechst sie, ist das
            # eine neue Falle und keine Buchhaltung — dann bricht das
            # Schreiben ab und nennt die Zahl (Bauform Tor 57).
            dazu = neu - alt
            if alt and dazu:
                print('ABBRUCH: %d NEUE Fundstellen — die Grundlinie darf '
                      'nur sinken:' % len(dazu), file=sys.stderr)
                for k in sorted(dazu):
                    print('  + %s' % k, file=sys.stderr)
                return 2
            p = os.path.join(wurzel, GRUNDLINIE_REL)
            with open(p, 'w', encoding='utf-8', newline='\n') as fh:
                fh.write('# Bekannte Teilstring-Fallen der Einschaetzung '
                         '`sicher` (A-027).\n')
                fh.write('# Die Zahl darf nur SINKEN. Neu schreiben mit:\n')
                fh.write('# scripts/audit_teilstring.py '
                         '--schreibe-grundlinie\n')
                fh.write('# Schluessel: pfad:regel:schnipsel — ohne '
                         'Zeilennummer, damit eine\n')
                fh.write('# Umformatierung kein Befund ist.\n')
                for k in sorted(neu):
                    fh.write(k + '\n')
            print('-> %s (%d Fundstellen, vorher %d)'
                  % (GRUNDLINIE_REL, len(neu), len(alt)))
            return 0
        offen = check(wurzel)
        print('Teilstring-Fallen: %d in der Grundlinie, %d NEU'
              % (len(_grundlinie_lesen(wurzel)), len(offen)))
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

    if args.only:
        findings = [f for f in findings if f.rule == args.only]

    if args.json:
        # A-027: `ensure_ascii=True` (die Vorgabe von json), und zwar
        # ABSICHTLICH. Hier stand `False`, und zusammen mit der
        # Konsolen-Haertung aus `_ausgabe_haerten()` war die Folge
        # gemessen: die JSON-Datei kam in cp1252 heraus und
        # `json.load(..., encoding='utf-8')` starb mit
        # `UnicodeDecodeError: invalid start byte` an Position 17774.
        #
        # Das war mein eigener Fehler beim Beheben von Defekt E — die
        # Haertung der KONSOLE hatte die DATENausgabe mitgenommen. Gefangen
        # hat es die naechste Messung, nicht das Nachdenken. JSON ist ein
        # Austauschformat und muss unter jeder Konsolenkodierung lesbar
        # sein; `ensure_ascii` loest jedes Nicht-ASCII-Zeichen in `\uXXXX`
        # auf und macht die Ausgabe damit kodierungsunabhaengig.
        print(json.dumps([asdict(f) for f in findings], indent=2,
                         ensure_ascii=True))
    else:
        by_rule = {}
        for f in findings:
            by_rule.setdefault(f.rule, []).append(f)

        for rule in sorted(by_rule):
            group = by_rule[rule]
            sure = sum(1 for f in group if f.severity == 'sicher')
            print(f'\n=== {rule}: {len(group)} Funde '
                  f'({sure} sicher, {len(group) - sure} zu pruefen) ===')
            for f in group:
                print(f'\n{f.path}:{f.line}:{f.col}  [{f.severity}]')
                print(f'  {f.what}')
                print(f'  {f.why}')
                if f.snippet:
                    print(f'  > {f.snippet}')

        print(f'\n{files} Dateien geprueft, {len(findings)} Funde.')
        if not findings:
            print('Keine Teilstring-Falle gefunden. Das heisst NICHT, dass '
                  'keine da ist —\nnur, dass keine der sieben Regeln '
                  'angeschlagen hat. Makros sieht keine\nvon ihnen: '
                  '--list-macros zeigt, wo von Hand nachzusehen ist.')

    if args.fail_on == 'nichts':
        return 0
    if args.fail_on == 'alles':
        return 1 if findings else 0
    return 1 if any(f.severity == 'sicher' for f in findings) else 0


if __name__ == '__main__':
    sys.exit(main())
