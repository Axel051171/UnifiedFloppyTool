#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
c_lex.py — ein kleiner C-Zerteiler, der weiss, wo er steht.

WARUM ER EXISTIERT
------------------
MF-1171: ein Muster, das die Sprache nicht kennt, findet 1387 Literale, von
denen 2 echt sind. Die drei Unterklassen aus deinem eigenen Befund:

  1. `print(` enthaelt `int(` als Teilstring
  2. Kommentare wurden als Code gezaehlt
  3. C++14-Trennzeichen `0b0'01010101` schlagen Lookbehind `(?<![\\w.])` aus

Alle drei verschwinden, sobald man in TOKEN denkt statt in Zeichen. Dieser
Zerteiler liefert Token mit Art und Zeilennummer; die Regeln in
substring_audit.py arbeiten ausschliesslich darauf.

Er ist absichtlich klein und absichtlich KEIN Praeprozessor: er wertet
`#if` nicht aus, ersetzt keine Makros und kennt keine Einbindungen. Er
muss nur zuverlaessig sagen, ob eine Stelle Code, Kommentar, Zeichenkette
oder Zahl ist — und das tut er.

WAS ER RICHTIG MACHT, WORAN REGEX SCHEITERT
-------------------------------------------
  Kommentare       /* … */ und // …, auch verschachtelt aussehende
  Zeichenketten    "…" mit \\" und \\\\ ; Rohzeichenketten R"tag(…)tag"
  Zeichenliterale  '…' inklusive '\\'' und '\\\\'
  Zahlen           0x1F, 0b0'0101'0101, 1'000'000, 1e-9, 0777, 12u, 3.4f
  Fortsetzung      eine Zeile, die auf \\ endet, gehoert zur naechsten
  Trigraph         NICHT behandelt — kommt in diesem Baum nicht vor, und
                   zu behaupten, man koenne es, waere schlimmer als es zu
                   lassen
"""

from __future__ import annotations

import re
from dataclasses import dataclass

__all__ = ['Tok', 'lex', 'lex_mit_makros',
           'KIND_ID', 'KIND_STR', 'KIND_CHAR', 'KIND_NUM',
           'KIND_PUNCT', 'KIND_COMMENT', 'KIND_PP']

KIND_ID      = 'id'
KIND_STR     = 'str'
KIND_CHAR    = 'char'
KIND_NUM     = 'num'
KIND_PUNCT   = 'punct'
KIND_COMMENT = 'comment'
KIND_PP      = 'pp'        # Praeprozessorzeile, als Ganzes


@dataclass(frozen=True)
class Tok:
    kind: str
    text: str          # der Rohtext, einschliesslich Anfuehrungszeichen
    line: int          # 1-basiert
    col: int           # 1-basiert
    value: str = ''    # bei Zeichenketten: der Inhalt, entschaerft

    def __repr__(self):                                   # pragma: no cover
        return f'Tok({self.kind}, {self.text!r}, L{self.line})'


_ID_START = re.compile(r'[A-Za-z_$]')
_ID_CONT  = re.compile(r'[A-Za-z0-9_$]')
# Zahlen: Praefix, Ziffern mit Trennzeichen, Exponent, Suffix. Das
# Trennzeichen ' ist der Grund, warum Lookbehind hier scheitert.
_NUM = re.compile(
    r"""
    (?: 0[xX][0-9a-fA-F']+ (?:\.[0-9a-fA-F']*)? (?:[pP][+-]?[0-9]+)?
      | 0[bB][01']+
      | (?:[0-9][0-9']*) (?:\.[0-9']*)? (?:[eE][+-]?[0-9]+)?
      | \. [0-9][0-9']* (?:[eE][+-]?[0-9]+)?
    )
    [uUlLfFzZ]*
    """, re.X)

_PUNCT3 = ('<<=', '>>=', '...', '->*')
_PUNCT2 = ('->', '++', '--', '<<', '>>', '<=', '>=', '==', '!=', '&&', '||',
           '+=', '-=', '*=', '/=', '%=', '&=', '|=', '^=', '::', '.*', '##')


def _unescape(raw: str) -> str:
    """Inhalt einer Zeichenkette, Fluchtzeichen aufgeloest, ohne die
    aeusseren Anfuehrungszeichen. Nur die Faelle, die in C-Quellen dieses
    Baums vorkommen — bei unbekannten Fluchten bleibt das Zeichen stehen."""
    if raw.startswith('R"') or raw.startswith('u8R"') or raw.startswith('LR"'):
        i = raw.index('"')
        j = raw.index('(', i)
        tag = raw[i + 1:j]
        end = raw.rindex(')' + tag + '"')
        return raw[j + 1:end]

    i = raw.index('"') + 1
    body = raw[i:raw.rindex('"')]
    out, k = [], 0
    simple = {'n': '\n', 't': '\t', 'r': '\r', '0': '\0', '\\': '\\',
              '"': '"', "'": "'", 'a': '\a', 'b': '\b', 'f': '\f',
              'v': '\v', '?': '?'}
    while k < len(body):
        c = body[k]
        if c != '\\':
            out.append(c); k += 1; continue
        k += 1
        if k >= len(body):
            out.append('\\'); break
        e = body[k]
        if e in simple:
            out.append(simple[e]); k += 1
        elif e == 'x':
            k += 1
            h = ''
            while k < len(body) and body[k] in '0123456789abcdefABCDEF':
                h += body[k]; k += 1
            out.append(chr(int(h, 16) & 0xFF) if h else 'x')
        elif e.isdigit():
            o = ''
            while k < len(body) and len(o) < 3 and body[k] in '01234567':
                o += body[k]; k += 1
            out.append(chr(int(o, 8) & 0xFF))
        else:
            out.append(e); k += 1
    return ''.join(out)


def lex(src: str, keep_comments: bool = False):
    """Zerteilt @p src in Token. Gibt einen Generator zurueck."""
    # Zeilenfortsetzungen zusammenziehen, aber die Zeilenzahl mitfuehren:
    # sonst stimmen alle Zeilennummern nach dem ersten \ am Zeilenende nicht.
    n = len(src)
    i = 0
    line, col = 1, 1

    def adv(k: int):
        nonlocal i, line, col
        for _ in range(k):
            if i < n and src[i] == '\n':
                line += 1; col = 1
            else:
                col += 1
            i += 1

    at_line_start = True

    while i < n:
        c = src[i]

        # Zeilenfortsetzung
        if c == '\\' and i + 1 < n and src[i + 1] == '\n':
            adv(2); continue

        if c in ' \t\r':
            adv(1); continue
        if c == '\n':
            adv(1); at_line_start = True; continue

        start_line, start_col = line, col

        # Kommentare
        if c == '/' and i + 1 < n and src[i + 1] == '/':
            j = src.find('\n', i)
            # Ein // -Kommentar kann per \ fortgesetzt werden.
            while j > 0 and src[j - 1] == '\\':
                j = src.find('\n', j + 1)
            if j < 0:
                j = n
            if keep_comments:
                yield Tok(KIND_COMMENT, src[i:j], start_line, start_col)
            adv(j - i); continue

        if c == '/' and i + 1 < n and src[i + 1] == '*':
            j = src.find('*/', i + 2)
            j = n if j < 0 else j + 2
            if keep_comments:
                yield Tok(KIND_COMMENT, src[i:j], start_line, start_col)
            adv(j - i); continue

        # Praeprozessorzeile als Ganzes — wir wollen nicht, dass
        # #include <string.h> als drei Token durchgeht.
        if at_line_start and c == '#':
            j = src.find('\n', i)
            while j > 0 and src[j - 1] == '\\':
                j = src.find('\n', j + 1)
            if j < 0:
                j = n
            text = src[i:j]
            yield Tok(KIND_PP, text, start_line, start_col)
            adv(j - i); continue

        at_line_start = False

        # Rohzeichenkette
        m = re.match(r'(?:u8|u|U|L)?R"([^\s()\\]{0,16})\(', src[i:])
        if m:
            tag = m.group(1)
            close = ')' + tag + '"'
            j = src.find(close, i + m.end())
            j = n if j < 0 else j + len(close)
            raw = src[i:j]
            yield Tok(KIND_STR, raw, start_line, start_col, _unescape(raw))
            adv(j - i); continue

        # Zeichenkette
        m = re.match(r'(?:u8|u|U|L)?"', src[i:])
        if m:
            j = i + m.end()
            while j < n:
                if src[j] == '\\':
                    j += 2; continue
                if src[j] == '"':
                    j += 1; break
                j += 1
            raw = src[i:j]
            yield Tok(KIND_STR, raw, start_line, start_col, _unescape(raw))
            adv(j - i); continue

        # Zeichenliteral
        m = re.match(r"(?:u8|u|U|L)?'", src[i:])
        if m:
            j = i + m.end()
            while j < n:
                if src[j] == '\\':
                    j += 2; continue
                if src[j] == "'":
                    j += 1; break
                j += 1
            yield Tok(KIND_CHAR, src[i:j], start_line, start_col)
            adv(j - i); continue

        # Zahl. MUSS vor dem Bezeichner stehen und MUSS die Trennzeichen
        # fressen — sonst zerfaellt 0b0'01010101 in drei Token, und genau
        # daran ist MF-1171 gescheitert.
        if c.isdigit() or (c == '.' and i + 1 < n and src[i + 1].isdigit()):
            m = _NUM.match(src, i)
            if m:
                yield Tok(KIND_NUM, m.group(0), start_line, start_col)
                adv(m.end() - i); continue

        # Bezeichner
        if _ID_START.match(c):
            j = i + 1
            while j < n and _ID_CONT.match(src[j]):
                j += 1
            yield Tok(KIND_ID, src[i:j], start_line, start_col)
            adv(j - i); continue

        # Satzzeichen, laengste zuerst
        for grp in (_PUNCT3, _PUNCT2):
            hit = next((p for p in grp if src.startswith(p, i)), None)
            if hit:
                yield Tok(KIND_PUNCT, hit, start_line, start_col)
                adv(len(hit)); break
        else:
            yield Tok(KIND_PUNCT, c, start_line, start_col)
            adv(1)


def lex_mit_makros(src: str, keep_comments: bool = False) -> list:
    """Wie `lex`, aber Praeprozessorzeilen werden NOCHMAL zerteilt.

    WARUM (MF-1247)
    ---------------
    `lex` gibt eine Praeprozessorzeile als EIN Token der Art `pp` aus —
    richtig fuer die Frage „ist das Code oder Text", falsch fuer die
    Frage „welche Bezeichner stehen in dieser Datei".

    Gemessen an `src/formats/dsk_generic/uft_dsk_generic.c`: die Datei
    definiert 49 Format-Plugins ueber ein einziges

        #define DSK_PLUGIN(INDEX, SUFFIX, NAME, DESC, EXT) ...

    und ALLE drei `UFT_FORMAT_CAP_*`-Flaggen stehen in dessen Rumpf.
    Ueber `lex` sind sie unsichtbar, ueber die Textsuche sichtbar —
    also haette ein Umstieg auf Token dort eine Flagge VERLOREN. Das
    ist derselbe Fehler wie der, gegen den MF-1247 antritt, nur in die
    andere Richtung.

    Der Rumpf wird gewonnen, indem das einleitende `#` faellt und der
    Rest erneut durch `lex` laeuft. Damit bleibt die Unterscheidung
    erhalten, auf die es ankommt: in `#error "UFT_FORMAT_CAP_WRITE"`
    ist der Name weiterhin eine ZEICHENKETTE und kein Bezeichner.

    @param src            Quelltext.
    @param keep_comments  Wie bei `lex`; wirkt auch im Makrorumpf.
    @return Liste statt Erzeuger, weil jeder bekannte Aufrufer die
            Nachbarschaft zweier Token braucht (`zip(t, t[1:], t[2:])`).
    """
    aus: list = []
    for tk in lex(src, keep_comments=keep_comments):
        if tk.kind != KIND_PP:
            aus.append(tk)
            continue
        rumpf = tk.text.lstrip()
        if rumpf.startswith('#'):
            rumpf = rumpf[1:]
        aus.extend(lex(rumpf, keep_comments=keep_comments))
    return aus


def _bezeichner(toks) -> set:
    return {t.text for t in toks if t.kind == KIND_ID}


def _selbsttest() -> int:
    """Belegt die drei Unterscheidungen, an denen MF-1247 haengt —
    jede mit der Gegenprobe, damit keine davon einfach immer gilt."""
    gruen = 0
    rot = 0

    def zusage(bedingung: bool, was: str) -> None:
        nonlocal gruen, rot
        if bedingung:
            gruen += 1
            print('   [ok ] %s' % was)
        else:
            rot += 1
            print('   [ROT] %s' % was)

    MARKE = 'UFT_FORMAT_CAP_WRITE'

    # 1 — der Fall aus dem Baum: der Name steht in einer ZEICHENKETTE,
    #     die ihn verneint. Woertlich aus `uft_woz_plugin.c`.
    verneint = ('static const char H[] =\n'
                '    "schreibt nicht: beansprucht kein '
                'UFT_FORMAT_CAP_WRITE.";\n'
                'const p_t p = { .capabilities = UFT_FORMAT_CAP_READ };\n')
    zusage(MARKE in verneint,
           'ROT-PROBE: eine TEXTSUCHE findet die Flagge im Satz, der '
           'sie verneint')
    zusage(MARKE not in _bezeichner(lex_mit_makros(verneint)),
           'als Bezeichner steht sie nicht da')
    zusage('UFT_FORMAT_CAP_READ' in _bezeichner(lex_mit_makros(verneint)),
           'GEGENPROBE: die wirklich gesetzte Flagge daneben steht da')

    # 2 — der Name im Kommentar (die aeltere Klasse MF-767).
    imkomm = ('/* MF-880: UFT_FORMAT_CAP_WRITE entfernt */\n'
              'const p_t p = { .capabilities = UFT_FORMAT_CAP_READ };\n')
    zusage(MARKE not in _bezeichner(lex_mit_makros(imkomm)),
           'im Blockkommentar steht sie nicht da')
    zusage(MARKE not in _bezeichner(
               lex_mit_makros('// ' + MARKE + ' war hier\nint x;\n')),
           'im Zeilenkommentar steht sie nicht da')

    # 3 — der Rumpf eines Makros. Ohne diese Lesart verschwaende
    #     `uft_dsk_generic.c` seine drei Flaggen, und genau das ist
    #     beim Umstieg auf Token gemessen passiert.
    makro = ('#define DSK_PLUGIN(S) \\\n'
             '    const p_t p_##S = { .capabilities = '
             'UFT_FORMAT_CAP_WRITE };\n'
             'DSK_PLUGIN(a)\n')
    zusage(MARKE not in _bezeichner(lex(makro)),
           'ROT-PROBE: `lex` allein VERLIERT die Flagge im Makrorumpf '
           '— eine Praeprozessorzeile ist dort EIN Token')
    zusage(MARKE in _bezeichner(lex_mit_makros(makro)),
           '`lex_mit_makros` findet sie')

    # 4 — und die Unterscheidung ueberlebt das tiefere Zerteilen:
    #     in einer Praeprozessor-ZEICHENKETTE bleibt der Name Text.
    pptext = '#error "braucht UFT_FORMAT_CAP_WRITE"\nint x;\n'
    zusage(MARKE not in _bezeichner(lex_mit_makros(pptext)),
           'GEGENPROBE: in `#error "..."` bleibt sie eine Zeichenkette')

    # 5 — die Merkmalstafel traegt ihre Aussage IN der Zeichenkette,
    #     und `value` liefert sie entschaerft.
    tafel = '{ "Write / encode", UFT_FEATURE_UNSUPPORTED, NULL },\n'
    toks = lex_mit_makros(tafel)
    paare = [(a.value, c.text) for a, b, c in zip(toks, toks[1:], toks[2:])
             if a.kind == KIND_STR and b.text == ','
             and c.kind == KIND_ID and c.text.startswith('UFT_FEATURE_')]
    zusage(paare == [('Write / encode', 'UFT_FEATURE_UNSUPPORTED')],
           'die Merkmalstafel wird als Paar (Zeichenkette, Bezeichner) '
           'gelesen')

    # 6 — Zeilennummern bleiben brauchbar (Tore melden sie).
    mehr = 'a;\n/* zwei\n   Zeilen */\nb;\nc;\n'
    letzte = [t for t in lex_mit_makros(mehr) if t.text == 'c']
    zusage(bool(letzte) and letzte[0].line == 5,
           'Zeilennummer nach einem mehrzeiligen Kommentar stimmt')

    print('\nSELBSTTEST %d/%d' % (gruen, gruen + rot))
    return 1 if rot else 0


if __name__ == '__main__':
    import sys
    sys.exit(_selbsttest())
