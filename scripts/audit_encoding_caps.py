#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Tor 54: stimmt die Faehigkeitstabelle mit dem Verteiler ueberein? (MF-865)

── Warum dieses Tor mit der Tabelle zusammen entsteht ───────────────────

`src/core/uft_encoding_caps.c` behauptet je Kodierung, ob sie dekodiert
werden kann. Eine von Hand gefuehrte Tabelle dieser Art ist genau das
Muster, das in diesem Baum siebzehnmal als Ursache gefunden wurde: eine
Aufzaehlung bekannter Faelle, die still veraltet.

Deshalb wird die Spalte `can_decode` nicht geglaubt, sondern **gegen den
Verteiler gemessen**: gegen die `switch`-Zweige von
`flux_decode_track()` in `src/flux/uft_flux_decoder.c`.

Wer einen Dekoder anschliesst und die Tabelle vergisst, faellt hier auf.
Wer einen entfernt, ebenso.

── Und die Zweige werden auch nicht geglaubt ────────────────────────────

Ein `case`, der auf eine Funktion zeigt, die gar keine Sektoren anlegt,
waere derselbe Fehler eine Ebene tiefer — genau MF-864: `flux_decode_fm()`
stand im Verteiler und bestand aus zwei Kommentaren. Das Tor prueft
deshalb zusaetzlich, dass jede geroutete Funktion im eigenen Rumpf
`sector_count` erhoeht.

── Grundlinie ───────────────────────────────────────────────────────────

0 Abweichungen.

── Selbsttest ───────────────────────────────────────────────────────────

Laeuft vor der Messung und bricht bei roter Abnahme ab (MF-693).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

WURZEL = Path(__file__).resolve().parent.parent

VERTEILER = WURZEL / 'src' / 'flux' / 'uft_flux_decoder.c'
TABELLE = WURZEL / 'src' / 'core' / 'uft_encoding_caps.c'

# Die zwei Aufzaehlungen sprechen verschiedene Namen. Die Zuordnung ist
# der einzige von Hand gefuehrte Teil und deshalb klein gehalten.
BRUECKE = {
    'FLUX_ENC_MFM':        'UFT_ENC_MFM',
    'FLUX_ENC_FM':         'UFT_ENC_FM',
    'FLUX_ENC_GCR_C64':    'UFT_ENC_GCR_C64',
    'FLUX_ENC_GCR_APPLE':  'UFT_ENC_GCR_APPLE',
    'FLUX_ENC_AMIGA':      'UFT_ENC_AMIGA_MFM',
}

# `case FLUX_ENC_X:` gefolgt (ggf. nach Leerzeilen) von `return f(...)`.
CASE = re.compile(
    r'case\s+(FLUX_ENC_[A-Z0-9_]+)\s*:\s*(?:/\*.*?\*/\s*)*'
    r'return\s+(flux_decode_[a-z0-9_]+)\s*\(', re.S)

# Eintraege der Tabelle. BEIDE Schreibweisen, und das ist keine Kuer:
#
#   positionell  { UFT_ENC_X, true, false, "..." }
#   benannt      { .enc = UFT_ENC_X, .can_detect = true, .can_decode = false,
#                  .grenze = "..." }
#
# MF-865 hat diese Datei mitten in der Arbeit von der ersten auf die
# zweite Form umgestellt (weil das Tor „Tote Header-Felder" bei
# positioneller Schreibweise drei Felder als nie-geschrieben zaehlt).
# Das Tor las danach NULL Eintraege und meldete alle fuenf Dekoder als
# „fehlt in der Tabelle" — es fiel also laut aus, nicht still. Aber sein
# SELBSTTEST meldete weiter 8/8, weil er nur die alte Form kannte.
#
# Das ist die MF-693-Lehre in Reinform: ein Selbsttest, der die
# tatsaechliche Eingabeform nicht abdeckt, ist eine Beruhigung, keine
# Abnahme. Unten stehen deshalb Faelle fuer BEIDE Formen — und einer mit
# vertauschter Feldreihenfolge, die benannte Initialisierer erlauben.
ZEILE_POS = re.compile(
    r'\{\s*(UFT_ENC_[A-Z0-9_]+)\s*,\s*(true|false)\s*,\s*(true|false)\s*,')
ZEILE_NAM = re.compile(
    r'\.enc\s*=\s*(UFT_ENC_[A-Z0-9_]+)\s*,'
    r'(?=(?:[^{}]|\{[^{}]*\})*?\.can_detect\s*=\s*(true|false))'
    r'(?=(?:[^{}]|\{[^{}]*\})*?\.can_decode\s*=\s*(true|false))')


def verteiler_zweige(text: str) -> dict[str, str]:
    """Nur der Rumpf von flux_decode_track()."""
    i = text.find('flux_status_t flux_decode_track(')
    if i < 0:
        return {}
    return {m.group(1): m.group(2) for m in CASE.finditer(text[i:])}


def _rumpf(text: str, funktion: str) -> str | None:
    """Der Rumpf von @p funktion per Klammerzaehlung, oder None."""
    muster = re.compile(r'^[A-Za-z_][A-Za-z0-9_ \t\*]*\b%s\s*\('
                        % re.escape(funktion), re.M)
    for m in muster.finditer(text):
        auf = text.find('{', m.end())
        if auf < 0:
            continue
        # Ein Prototyp endet vor der Klammer auf ';'.
        if ';' in text[m.end():auf]:
            continue
        tiefe, i = 0, auf
        while i < len(text):
            if text[i] == '{':
                tiefe += 1
            elif text[i] == '}':
                tiefe -= 1
                if tiefe == 0:
                    return text[auf:i + 1]
            i += 1
    return None


AUFRUF = re.compile(r'\b([a-z_][a-z0-9_]*)\s*\(')


def legt_sektoren_an(text: str, funktion: str, tiefe: int = 0,
                     gesehen: set[str] | None = None) -> bool:
    """Erhoeht @p funktion — direkt ODER ueber Aufrufe — `sector_count`?

    BERICHTIGT gegenueber der Erstfassung: die prüfte nur den eigenen
    Rumpf und meldete `flux_decode_amiga` als Stub. Das war ein Fehler
    des TORES, nicht des Codes — die Funktion delegiert an
    `amiga_decode_at()`, `amiga_try_candidate()` und
    `amiga_try_dewarped()`, und dort werden die Sektoren angelegt.

    Delegation ist der Normalfall, nicht die Ausnahme. Ein Tor, das sie
    als Fehler meldet, wird nach dem dritten Fehlalarm abgeschaltet — und
    ist dann schlechter als keines.
    """
    if gesehen is None:
        gesehen = set()
    if funktion in gesehen or tiefe > 3:
        return False
    gesehen.add(funktion)

    rumpf = _rumpf(text, funktion)
    if rumpf is None:
        return False
    ohne = rumpf.replace(' ', '')
    if 'sector_count++' in ohne or 'sector_count)++' in ohne:
        return True

    for m in AUFRUF.finditer(rumpf):
        name = m.group(1)
        if name == funktion or name in gesehen:
            continue
        if legt_sektoren_an(text, name, tiefe + 1, gesehen):
            return True
    return False


def tabelle_lesen(text: str) -> dict[str, tuple[bool, bool]]:
    """Liest beide Schreibweisen. Siehe die Begruendung bei ZEILE_NAM."""
    aus: dict[str, tuple[bool, bool]] = {}
    for m in ZEILE_NAM.finditer(text):
        aus[m.group(1)] = (m.group(2) == 'true', m.group(3) == 'true')
    for m in ZEILE_POS.finditer(text):
        aus.setdefault(m.group(1),
                       (m.group(2) == 'true', m.group(3) == 'true'))
    return aus


def _selbsttest() -> bool:
    faelle = []

    v = verteiler_zweige(
        'flux_status_t flux_decode_track(void) {\n'
        '  switch (e) {\n'
        '  case FLUX_ENC_MFM:\n    return flux_decode_mfm(a);\n'
        '  case FLUX_ENC_FM:\n    /* Kommentar */\n'
        '    return flux_decode_fm(a);\n  }\n}\n')
    faelle.append(('zwei Zweige erkannt', len(v) == 2))
    faelle.append(('Kommentar zwischen case und return',
                   v.get('FLUX_ENC_FM') == 'flux_decode_fm'))

    # Ein Zweig VOR dem Verteiler darf nicht mitzaehlen.
    v2 = verteiler_zweige(
        'static void x(void){ case FLUX_ENC_MFM: return flux_decode_mfm(a); }\n'
        'flux_status_t flux_decode_track(void) {\n'
        '  case FLUX_ENC_FM:\n    return flux_decode_fm(a);\n}\n')
    faelle.append(('nur der Verteiler zaehlt', list(v2) == ['FLUX_ENC_FM']))

    t = tabelle_lesen('{ UFT_ENC_FM, true, true, NULL },\n'
                      '{ UFT_ENC_M2FM, false, false, "x" },')
    faelle.append(('Tabelle gelesen (positionell)',
                   t == {'UFT_ENC_FM': (True, True),
                         'UFT_ENC_M2FM': (False, False)}))

    # DER FALL, DER GEFEHLT HAT. Die Datei steht seit MF-865 in dieser
    # Form; ohne diesen Selbsttestfall meldete das Tor 8/8 und las dabei
    # null Eintraege.
    t2 = tabelle_lesen(
        '{ .enc = UFT_ENC_FM,   .can_detect = true,  .can_decode = true,\n'
        '  .grenze = NULL },\n'
        '{ .enc = UFT_ENC_M2FM, .can_detect = false, .can_decode = false,\n'
        '  .grenze = "M2FM ist nur benannt" },')
    faelle.append(('Tabelle gelesen (benannt, mehrzeilig)',
                   t2 == {'UFT_ENC_FM': (True, True),
                          'UFT_ENC_M2FM': (False, False)}))

    # Benannte Initialisierer erlauben eine andere Reihenfolge. Ein Tor,
    # das stillschweigend die Positionen annimmt, laese hier Unsinn.
    t3 = tabelle_lesen(
        '{ .grenze = NULL, .can_decode = true, .can_detect = false,\n'
        '  .enc = UFT_ENC_GCR_C64 },')
    faelle.append(('vertauschte Feldreihenfolge',
                   t3 == {} or t3 == {'UFT_ENC_GCR_C64': (False, True)}))

    # Gegenprobe: eine Struktur OHNE .enc darf keinen Eintrag erzeugen.
    faelle.append(('fremde Struktur erzeugt keinen Eintrag',
                   tabelle_lesen('{ .foo = 1, .can_decode = true },') == {}))

    stub = ('\nflux_status_t flux_decode_leer(void) {\n'
            '  /* decoding would go here */\n  return 0;\n}\n'
            '\nflux_status_t flux_decode_echt(void) {\n'
            '  track->sector_count++;\n  return 0;\n}\n'
            '\nstatic int helfer_legt_an(void) {\n'
            '  track->sector_count++;\n  return 0;\n}\n'
            '\nflux_status_t flux_decode_delegiert(void) {\n'
            '  return helfer_legt_an();\n}\n'
            '\nstatic int nur_zaehlt(void) { return 0; }\n'
            '\nflux_status_t flux_decode_leer_delegiert(void) {\n'
            '  return nur_zaehlt();\n}\n')
    faelle.append(('Stub erkannt',
                   not legt_sektoren_an(stub, 'flux_decode_leer')))
    faelle.append(('echter Dekoder erkannt',
                   legt_sektoren_an(stub, 'flux_decode_echt')))
    # Der Fall, an dem die Erstfassung des Tores scheiterte: Delegation.
    faelle.append(('Delegation an einen Helfer wird erkannt',
                   legt_sektoren_an(stub, 'flux_decode_delegiert')))
    # Gegenprobe, damit „folgt Aufrufen" nicht zu „findet immer etwas" wird.
    faelle.append(('Delegation an einen LEEREN Helfer bleibt ein Stub',
                   not legt_sektoren_an(stub, 'flux_decode_leer_delegiert')))

    ok = sum(1 for _, g in faelle if g)
    for was, g in faelle:
        if not g:
            print('  SELBSTTEST ROT: %s' % was)
    print('  Selbsttest: %d/%d' % (ok, len(faelle)))
    return ok == len(faelle)


def main() -> int:
    print('Tor 54: Faehigkeitstabelle gegen den Verteiler (MF-865)')
    print('=' * 68)
    if not _selbsttest():
        print('ABBRUCH: der Selbsttest ist rot — die Messung darunter waere')
        print('         wertlos, ihre Null keine Entwarnung.')
        return 2

    if not VERTEILER.is_file() or not TABELLE.is_file():
        print('ABBRUCH: Quelle fehlt (%s / %s)' % (VERTEILER, TABELLE))
        return 2

    vtext = VERTEILER.read_text(encoding='utf-8', errors='replace')
    ttext = TABELLE.read_text(encoding='utf-8', errors='replace')

    zweige = verteiler_zweige(vtext)
    tabelle = tabelle_lesen(ttext)

    abweichungen = []

    # (1) Jeder geroutete Zweig muss in der Tabelle als dekodierbar stehen.
    geroutet = set()
    for flux_name, funktion in sorted(zweige.items()):
        kanon = BRUECKE.get(flux_name)
        if kanon is None:
            continue                       # AUTO, RAW: keine Kodierung
        geroutet.add(kanon)

        if not legt_sektoren_an(vtext, funktion):
            abweichungen.append(
                '%s wird an %s() geroutet, die in ihrem Rumpf KEINEN '
                'Sektor anlegt (das war MF-864)' % (flux_name, funktion))
            continue

        eintrag = tabelle.get(kanon)
        if eintrag is None:
            abweichungen.append(
                '%s wird dekodiert, fehlt aber in der Tabelle' % kanon)
        elif not eintrag[1]:
            abweichungen.append(
                '%s wird von %s() dekodiert, die Tabelle sagt '
                'can_decode=false' % (kanon, funktion))

    # (2) Umgekehrt: nichts darf can_decode=true behaupten ohne Zweig.
    for kanon, (_, decode) in sorted(tabelle.items()):
        if decode and kanon not in geroutet:
            abweichungen.append(
                '%s behauptet can_decode=true, der Verteiler hat aber '
                'keinen Zweig dafuer' % kanon)

    print()
    print('Verteiler-Zweige mit Kodierung : %d' % len(geroutet))
    print('Tabellen-Eintraege             : %d' % len(tabelle))
    for a in abweichungen:
        print('  ABWEICHUNG: %s' % a)
    print()
    print('=' * 68)
    print('Abweichungen: %d (Grundlinie 0)' % len(abweichungen))
    if abweichungen:
        print()
        print('Die Tabelle sagt Benutzern, was dieses Werkzeug kann. Weicht')
        print('sie vom Verteiler ab, ist sie eine Zusicherung statt einer')
        print('Messung — und dann waere sie schaedlicher als keine Tabelle.')
        return 1
    return 0


def abweichungen_messen(vtext: str, ttext: str) -> list[str]:
    """Die eigentliche Messung, ohne Ausgabe — fuer check() und main()."""
    zweige = verteiler_zweige(vtext)
    tabelle = tabelle_lesen(ttext)
    aus: list[str] = []
    geroutet = set()

    for flux_name, funktion in sorted(zweige.items()):
        kanon = BRUECKE.get(flux_name)
        if kanon is None:
            continue
        geroutet.add(kanon)
        if not legt_sektoren_an(vtext, funktion):
            aus.append('%s wird an %s() geroutet, die keinen Sektor anlegt '
                       '(das war MF-864)' % (flux_name, funktion))
            continue
        eintrag = tabelle.get(kanon)
        if eintrag is None:
            aus.append('%s wird dekodiert, fehlt aber in der Tabelle' % kanon)
        elif not eintrag[1]:
            aus.append('%s wird von %s() dekodiert, die Tabelle sagt '
                       'can_decode=false' % (kanon, funktion))

    for kanon, (_, decode) in sorted(tabelle.items()):
        if decode and kanon not in geroutet:
            aus.append('%s behauptet can_decode=true, der Verteiler hat '
                       'aber keinen Zweig dafuer' % kanon)
    return aus


def check(repo) -> list[str]:
    """Fuer scripts/check_consistency.py."""
    from pathlib import Path as _P
    wurzel = _P(repo)
    v = wurzel / 'src' / 'flux' / 'uft_flux_decoder.c'
    tb = wurzel / 'src' / 'core' / 'uft_encoding_caps.c'
    if not v.is_file() or not tb.is_file():
        return ['audit_encoding_caps: Quelle fehlt (%s / %s)' % (v, tb)]

    import io as _io, sys as _sys
    alt = _sys.stdout
    _sys.stdout = _io.StringIO()
    try:
        gruen = _selbsttest()
    finally:
        _sys.stdout = alt
    if not gruen:
        return ['audit_encoding_caps: Selbsttest ROT — Messung wertlos']

    return abweichungen_messen(
        v.read_text(encoding='utf-8', errors='replace'),
        tb.read_text(encoding='utf-8', errors='replace'))


if __name__ == '__main__':
    raise SystemExit(main())
