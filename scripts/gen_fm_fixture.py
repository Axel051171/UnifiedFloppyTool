# -*- coding: utf-8 -*-
"""Baut eine IBM-3740-FM-Spur und laesst sie vom Oracle abnehmen (MF-864).

Die Spur wird HIER gebaut und an vier Stellen von einer fremden,
unabhaengigen Umsetzung gegengeprueft — `fluxtoimd` (Eric Smith 2016,
GPL-3-only), geklont unter `tools/uft-scout/work/fluxtoimd`:

  1. jede Adressmarke gegen ihre vorberechneten Klassenwerte
     (`FM.id_address_mark` usw.) -- die Werte, mit denen das Werkzeug
     tatsaechlich arbeitet
  2. das ID-Feld zurueck durch `FM.decode()`
  3. das Datenfeld zurueck durch `FM.decode()`
  4. beide Pruefsummen gegen `crc.CRC` mit den Parametern, die die
     Vorlage fuer FM fuehrt (`crc_init = 0xffff`,
     `crc_includes_address_mark = True`)

Aus der Vorlage wird NICHTS uebernommen — sie wird ausgefuehrt. Das
Spurlayout stammt aus den benannten Normen, die ihr eigener Kopf nennt:
ECMA 54, ISO 5654, ANSI X3.73.

Ausgabe: `tests/fixtures/fm_ibm3740_track.h` — die Spur als (Daten,
Clock)-Bytepaare. Die Verschraenkung zu Kanalbits macht der C-Test
selbst; sie ist sechs Zeilen Arithmetik und oben unter (1) abgenommen.
"""
from __future__ import annotations

import sys
from pathlib import Path

BAUM = Path(r'C:\Users\Axel\Github\UnifiedFloppyTool-4.1.0')
ORACLE = BAUM / 'tools' / 'uft-scout' / 'work' / 'fluxtoimd'
sys.path.insert(0, str(ORACLE))

from modulation import FM          # noqa: E402  (Oracle, nicht Vorlage)
from crc import CRC                # noqa: E402

# Genau die Parameter, die fluxtoimd.py:208-215 fuer FM setzt.
CRC_PARAM = CRC.CRCParam(name='CRC-16-CCITT', order=16, poly=0x1021,
                         init=FM.crc_init, xorot=0x0000,
                         refin=FM.lsb_first, refot=False)

# ── Spurparameter ────────────────────────────────────────────────────
# Vier Sektoren statt der 26 einer echten 8-Zoll-Spur: der Dekoder
# zaehlt nicht, er sucht. Vier reichen fuer alle Faelle und halten das
# Fixture lesbar.
ZYLINDER      = 0
KOPF          = 0
SEKTOREN      = [1, 2, 3, 4]
SEKTORGROESSE = 128
GROESSENCODE  = 0            # 128 = 128 << 0

# Marken: (Daten, Clock). Aus den Normen; im Baum stehen drei davon
# bereits als 16-Bit-Woerter (uft_flux_decoder.h:44-45,
# uft_fuzzy_sync_v2.c:27-28).
IAM  = (0xFC, 0xD7)
IDAM = (0xFE, 0xC7)
DAM  = (0xFB, 0xC7)
DDAM = (0xF8, 0xC7)

NUTZ_CLOCK = 0xFF            # gewoehnliches Byte: alle Clockbits gesetzt

# Sektor 3 wird als GELOESCHT abgelegt — der Dekoder muss das melden,
# nicht verschweigen.
GELOESCHT = {3}


def verschraenken(daten: int, clock: int) -> str:
    """(Clock, Daten) paarweise, hoechstwertiges Bit zuerst."""
    s = []
    for i in range(8):
        s.append('01'[(clock >> (7 - i)) & 1])
        s.append('01'[(daten >> (7 - i)) & 1])
    return ''.join(s)


def crc_fm(bytes_: list[int]) -> int:
    """CRC-16/CCITT, Init 0xFFFF, einschliesslich der Adressmarke.

    Parameter aus der Vorlage: `FM.crc_init` und
    `FM.crc_includes_address_mark`. Gerechnet wird mit ihrer CRC-Klasse.
    """
    c = CRC(CRC_PARAM)
    c.make_table(8)
    c.reset()
    for b in bytes_:
        c.comp(b)
    return c.reg & 0xFFFF


# ── (1) Markenkodierung gegen das Oracle ─────────────────────────────
def marken_abnehmen() -> None:
    print('  (1) Adressmarken gegen die Klassenwerte des Oracles:')
    ORAKEL = {'Index': FM.index_address_mark,
              'ID': FM.id_address_mark,
              'Data': FM.data_address_mark,
              'Deleted': FM.deleted_data_address_mark}
    for name, (d, c) in (('Index', IAM), ('ID', IDAM),
                         ('Data', DAM), ('Deleted', DDAM)):
        meins = verschraenken(d, c)
        ihres = ORAKEL[name]
        wort = int(meins, 2)
        if meins != ihres:
            raise SystemExit(
                '  ABBRUCH: %s-Marke weicht ab\n    ich   %s\n    Oracle %s'
                % (name, meins, ihres))
        print('      %-8s Daten %02X Clock %02X -> 0x%04X  uebereinstimmend'
              % (name, d, c, wort))


# ── Spur bauen ───────────────────────────────────────────────────────
def spur_bauen() -> list[tuple[int, int]]:
    """Die Spur als Folge von (Daten, Clock)-Bytes.

    Aufbau nach ECMA 54 / ISO 5654 (8 Zoll, einseitig, einfache Dichte):
      GAP1  40x FF
      je Sektor:
        6x 00 Sync, IDAM, C, H, S, N, CRC(2)
        11x FF GAP2, 6x 00 Sync, DAM/DDAM, Daten, CRC(2)
        27x FF GAP3
    """
    spur: list[tuple[int, int]] = []

    def leg(daten: int, clock: int = NUTZ_CLOCK, n: int = 1) -> None:
        for _ in range(n):
            spur.append((daten, clock))

    leg(0xFF, n=40)                                   # GAP1
    leg(0x00, n=6)
    leg(*IAM)                                         # Index Address Mark
    leg(0xFF, n=26)

    for s in SEKTOREN:
        # ── ID-Feld ──
        leg(0x00, n=6)
        leg(*IDAM)
        kopf = [IDAM[0], ZYLINDER, KOPF, s, GROESSENCODE]
        for b in kopf[1:]:
            leg(b)
        c = crc_fm(kopf)
        leg((c >> 8) & 0xFF)
        leg(c & 0xFF)

        leg(0xFF, n=11)                               # GAP2

        # ── Datenfeld ──
        leg(0x00, n=6)
        marke = DDAM if s in GELOESCHT else DAM
        leg(*marke)
        nutz = [((s * 7 + i) & 0xFF) for i in range(SEKTORGROESSE)]
        for b in nutz:
            leg(b)
        c = crc_fm([marke[0]] + nutz)
        leg((c >> 8) & 0xFF)
        leg(c & 0xFF)

        leg(0xFF, n=27)                               # GAP3

    leg(0xFF, n=40)                                   # GAP4
    return spur


# ── (2)(3)(4) Rueckweg durch das Oracle ──────────────────────────────
def spur_abnehmen(spur: list[tuple[int, int]]) -> None:
    bits = ''.join(verschraenken(d, c) for d, c in spur)
    print('  Spur: %d Bytes, %d Kanalbits' % (len(spur), len(bits)))

    idam_bits = verschraenken(*IDAM)
    dam_bits = verschraenken(*DAM)
    ddam_bits = verschraenken(*DDAM)

    gefunden = 0
    pos = 0
    while True:
        p = bits.find(idam_bits, pos)
        if p < 0:
            break
        pos = p + 16

        # (2) ID-Feld: Marke + C,H,S,N + CRC(2) = 7 Bytes
        roh = FM.decode(bits[p:p + 16 * 7])
        if len(roh) != 7:
            raise SystemExit('  ABBRUCH: Oracle lieferte %d statt 7 Bytes'
                             % len(roh))
        marke, zyl, kopf, sek, code, ch, cl = roh
        if marke != IDAM[0]:
            raise SystemExit('  ABBRUCH: Oracle las Marke %02X' % marke)

        # (4) Pruefsumme
        soll = crc_fm([marke, zyl, kopf, sek, code])
        ist = (ch << 8) | cl
        if soll != ist:
            raise SystemExit('  ABBRUCH: ID-CRC %04X != %04X' % (ist, soll))

        # (3) Datenfeld
        d = bits.find(dam_bits, pos)
        dd = bits.find(ddam_bits, pos)
        kand = [x for x in (d, dd) if x >= 0]
        if not kand:
            raise SystemExit('  ABBRUCH: kein Datenfeld nach Sektor %d' % sek)
        dp = min(kand)
        geloescht = (dp == dd)
        droh = FM.decode(bits[dp:dp + 16 * (1 + SEKTORGROESSE + 2)])
        dmarke, nutz, dcrc = droh[0], droh[1:-2], droh[-2:]
        soll = crc_fm([dmarke] + list(nutz))
        ist = (dcrc[0] << 8) | dcrc[1]
        if soll != ist:
            raise SystemExit('  ABBRUCH: Daten-CRC Sektor %d' % sek)
        erwartet = [((sek * 7 + i) & 0xFF) for i in range(SEKTORGROESSE)]
        if list(nutz) != erwartet:
            raise SystemExit('  ABBRUCH: Nutzdaten Sektor %d weichen ab' % sek)

        print('      Sektor %d: C=%d H=%d N=%d, ID-CRC und Daten-CRC ok%s'
              % (sek, zyl, kopf, code, ', GELOESCHT' if geloescht else ''))
        gefunden += 1

    if gefunden != len(SEKTOREN):
        raise SystemExit('  ABBRUCH: Oracle fand %d von %d Sektoren'
                         % (gefunden, len(SEKTOREN)))
    print('  (2)(3)(4) Oracle liest alle %d Sektoren zurueck.' % gefunden)


# ── Fixture schreiben ────────────────────────────────────────────────
KOPFTEXT = '''/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file fm_ibm3740_track.h
 * @brief Eine FM-Spur nach IBM 3740, vom Oracle abgenommen (MF-864)
 *
 * ERZEUGT — nicht von Hand pflegen.
 * Erzeuger: scripts/gen_fm_fixture.py
 *
 * ── Herkunft ─────────────────────────────────────────────────────────
 *
 * Spurlayout nach den Normen, die die einfache Dichte festlegen:
 * **ECMA 54**, **ISO 5654**, **ANSI X3.73** (8 Zoll, einseitig).
 *
 * ── Vier Abnahmen durch eine fremde Hand ─────────────────────────────
 *
 * Der Baum hat KEINEN FM- oder MFM-Encoder (an vier Stellen als Blocker
 * vermerkt, u.a. `src/formats/kfx/uft_kfx.c:199`). Diese Spur ist also
 * hier gebaut worden — und deshalb von einer unabhaengigen Umsetzung
 * gegengeprueft: `fluxtoimd` (Eric Smith 2016, GPL-3-only), geklont
 * unter `tools/uft-scout/work/fluxtoimd`.
 *
 *   1. jede Adressmarke gegen ihre vorberechneten Klassenwerte
     (`FM.id_address_mark` usw.) -- die Werte, mit denen das Werkzeug
     tatsaechlich arbeitet
 *   2. das ID-Feld zurueck durch `FM.decode()`
 *   3. das Datenfeld zurueck durch `FM.decode()`
 *   4. beide Pruefsummen gegen ihre `crc.CRC` mit ihren FM-Parametern
 *
 * Aus der Vorlage ist NICHTS uebernommen — sie wurde ausgefuehrt. Das
 * ist der Kanal „Oracle" aus `docs/ORACLES.md`.
 *
 * ── Warum das noetig war ─────────────────────────────────────────────
 *
 * Ein Fixture, das ich baue, von einem Dekoder gelesen, den ich baue,
 * ist EINE Hand zweimal. Das ist die fuenfte Frage aus MF-644/760:
 * „ist dieses Oracle dieselbe Hand wie das, was es prueft?"
 */
'''


def fixture_schreiben(spur: list[tuple[int, int]]) -> Path:
    ziel = BAUM / 'tests' / 'fixtures' / 'fm_ibm3740_track.h'
    ziel.parent.mkdir(parents=True, exist_ok=True)

    z = [KOPFTEXT]
    z.append('#ifndef UFT_TEST_FM_IBM3740_TRACK_H\n')
    z.append('#define UFT_TEST_FM_IBM3740_TRACK_H\n\n')
    z.append('#include <stdint.h>\n\n')
    z.append('#define FM_FIXTURE_BYTES        %du\n' % len(spur))
    z.append('#define FM_FIXTURE_SEKTOREN     %du\n' % len(SEKTOREN))
    z.append('#define FM_FIXTURE_SEKTORGROESSE %du\n' % SEKTORGROESSE)
    z.append('#define FM_FIXTURE_GELOESCHT    %du  /* Sektornummer */\n\n'
             % sorted(GELOESCHT)[0])
    z.append('/* (Datenbyte, Clockbyte) je Spurbyte. Der Test verschraenkt\n'
             ' * sie zu Kanalbits: Clockbit zuerst, dann Datenbit, je Paar,\n'
             ' * hoechstwertiges Bit zuerst. */\n')
    z.append('static const uint8_t FM_FIXTURE[FM_FIXTURE_BYTES][2] = {\n')
    for i in range(0, len(spur), 8):
        teil = spur[i:i + 8]
        z.append('    ' + ' '.join('{0x%02X,0x%02X},' % (d, c)
                                   for d, c in teil) + '\n')
    z.append('};\n\n')
    z.append('#endif /* UFT_TEST_FM_IBM3740_TRACK_H */\n')

    ziel.write_text(''.join(z), encoding='utf-8')
    return ziel


def main() -> int:
    print('FM-Fixture nach IBM 3740, Abnahme durch fluxtoimd (MF-864)')
    print('=' * 68)
    if not (ORACLE / 'modulation.py').is_file():
        print('ABBRUCH: Oracle fehlt unter %s' % ORACLE)
        return 2
    marken_abnehmen()
    spur = spur_bauen()
    spur_abnehmen(spur)
    ziel = fixture_schreiben(spur)
    print('=' * 68)
    print('geschrieben: %s' % ziel.relative_to(BAUM))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
