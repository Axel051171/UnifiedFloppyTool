#!/usr/bin/env python3
"""SCP-Ausschnitt fuer den freien Korpus — das Rezept zu MF-1471 (P3-630): behaelt nur die genannten Spuren, alle Umdrehungen,
Bytes der Spurbloecke unveraendert. Setzt start/end_track auf die
Spannweite, laesst die Tafel fuer fehlende Spuren auf 0 und rechnet die
Pruefsumme (Summe der Bytes ab 0x10) neu.

python tests/corpus_manifest/gen_scp_schnitt.py <ein.scp> <aus.scp> <spur> [<spur> ...]
"""
import struct
import sys


def main():
    ein, aus = sys.argv[1], sys.argv[2]
    spuren = sorted(int(x) for x in sys.argv[3:])
    d = open(ein, 'rb').read()
    assert d[:3] == b'SCP'
    revs = d[5]
    tafel = struct.unpack_from('<168I', d, 0x10)
    kopf = bytearray(d[:0x10])
    kopf[6] = spuren[0]
    kopf[7] = spuren[-1]
    neu_tafel = [0] * 168
    koerper = bytearray()
    basis = 0x10 + 168 * 4
    for t in spuren:
        off = tafel[t]
        assert off and d[off:off + 3] == b'TRK' and d[off + 3] == t, t
        # Blockende: groesstes (Datenversatz + 2*Laenge) ueber die Umdrehungen
        ende = 4 + 12 * revs
        for r in range(revs):
            _dauer, laenge, dvers = struct.unpack_from('<III', d, off + 4 + 12 * r)
            ende = max(ende, dvers + 2 * laenge)
        neu_tafel[t] = basis + len(koerper)
        koerper += d[off:off + ende]
    rest = struct.pack('<168I', *neu_tafel) + bytes(koerper)
    kopf[0x0C:0x10] = struct.pack('<I', sum(rest) & 0xFFFFFFFF)
    open(aus, 'wb').write(bytes(kopf) + rest)
    print(aus, len(kopf) + len(rest), 'Byte, Spuren', spuren, 'Umdrehungen', revs)


main()
