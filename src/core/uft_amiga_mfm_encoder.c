/**
 * @file uft_amiga_mfm_encoder.c
 * @brief AmigaDOS-Trackdisk-Encoder (MF-1081, hebt MF-539 auf)
 *
 * Warum es diese Datei gibt, welche Referenzen sie benennt und was sie
 * NICHT tut, steht im Kopf von `include/uft/uft_amiga_mfm_encoder.h`.
 * Hier steht die Umsetzung.
 *
 * **Eigenstaendig geschrieben** als exakte Umkehrung von
 * `decode_amiga_sector()` (`src/flux/uft_flux_decoder.c`). Zweite
 * benannte Referenz: `mfmdisk`, Serge Vakulenko, **GPL-2.0**
 * (`tools/uft-scout/work/mfmdisk/src/amiga.c`, Klon `42f560badf`) —
 * gelesen, nicht uebernommen.
 *
 * ── Eine Zirkularitaet, die keine ist ───────────────────────────────────
 *
 * Die Datenpruefsumme steht auf der Spur VOR den Daten, wird aber ueber
 * sie gebildet. Der erste Entwurf hier rechnete sie deshalb voraus und
 * versuchte, den Takt-Kontext zwischen beiden Feldern mitzufuehren " ein
 * Aufwand, der auf einem Denkfehler beruhte.
 *
 * Der Dekoder vergleicht `hchk_stored == (hdr_csum & 0x55555555)`: die
 * Maske faellt am ENDE. Da XOR bitweise ist, tragen die Taktbits an den
 * `0xAA`-Positionen zum Ergebnis nichts bei " sie werden weggemaskt. Die
 * Pruefsumme haengt also **allein an den Datenbits** und ist vom
 * Takt-Kontext unabhaengig. Damit ist die Reihenfolge frei, und der
 * Vorausrechnungs-Block entfaellt ersatzlos.
 */
#include "uft/uft_amiga_mfm_encoder.h"

#include <string.h>

#define AMIGA_CKSUM_MASK   0x55555555u
#define AMIGA_SECTOR_BYTES 512u
#define AMIGA_MAX_SECTOR   21u
#define AMIGA_MAX_TRACK    167u

/**
 * Ein rohes MFM-Byte bilden und dabei die TAKTBITS fuellen.
 *
 * `vorher` traegt das zuletzt geschriebene ZELLbit ueber Byte- und
 * Feldgrenzen hinweg. Die Regel ist die uebliche: ein Taktbit ist genau
 * dann 1, wenn das Datenbit davor UND das Datenbit danach 0 sind.
 *
 * Die Datenbits stehen hier schon an den `0x55`-Positionen des Bytes
 * (das ist die Amiga-odd/even-Aufteilung); gefuellt werden nur die
 * `0xAA`-Positionen. Genau diese Bits sieht der Dekoder NICHT an " sie
 * entscheiden aber, ob ein echtes Laufwerk die Spur lesen kann. Deshalb
 * misst die Abnahme die Zellregel eigens (MF-1079).
 */
static uint8_t takt_fuellen(uint8_t daten, int *vorher)
{
    uint8_t aus = 0;
    int i;
    for (i = 3; i >= 0; i--) {
        const int d = (daten >> (i * 2)) & 1;          /* Datenbit */
        const int takt = (!*vorher && !d) ? 1 : 0;
        aus = (uint8_t)((aus << 2) | (unsigned)((takt << 1) | d));
        *vorher = d;
    }
    return aus;
}

/** Ein Byte roher Zellen unveraendert anhaengen (Sync, Luecke). */
static void roh(uint8_t *out, size_t *n, uint8_t b, int *vorher)
{
    out[(*n)++] = b;
    *vorher = b & 1;
}

/** Die odd- bzw. even-Haelfte eines Bytes an den 0x55-Positionen. */
static uint8_t haelfte(uint8_t b, int even)
{
    return even ? (uint8_t)(b & 0x55u) : (uint8_t)((b >> 1) & 0x55u);
}

/**
 * Ein odd/even-Feld schreiben: erst `nbytes` Rohbytes mit den ODD-Bits,
 * dann `nbytes` mit den EVEN-Bits. Umkehrung von `amiga_read_field()`.
 *
 * `csum` sammelt XOR der big-endian Langworte " gerechnet ueber die
 * DATENBITS, weil die Maske am Ende die Taktbits ohnehin entfernt
 * (siehe Dateikopf).
 */
static void feld(uint8_t *out, size_t *n, const uint8_t *in, size_t nbytes,
                 uint32_t *csum, int *vorher)
{
    int half;
    for (half = 0; half < 2; half++) {                 /* 0 = odd, 1 = even */
        uint32_t acc = 0;
        int acc_n = 0;
        size_t j;
        for (j = 0; j < nbytes; j++) {
            const uint8_t daten = haelfte(in[j], half);
            out[(*n)++] = takt_fuellen(daten, vorher);
            if (csum) {
                acc = (acc << 8) | daten;
                if (++acc_n == 4) { *csum ^= acc; acc = 0; acc_n = 0; }
            }
        }
        if (csum && acc_n) {                           /* nbytes nicht /4 */
            acc <<= 8 * (4 - acc_n);
            *csum ^= acc;
        }
    }
}

/** Dieselbe Rechnung wie `feld()`, aber ohne zu schreiben. */
static uint32_t feld_summe(const uint8_t *in, size_t nbytes)
{
    uint32_t csum = 0;
    int half;
    for (half = 0; half < 2; half++) {
        uint32_t acc = 0;
        int acc_n = 0;
        size_t j;
        for (j = 0; j < nbytes; j++) {
            acc = (acc << 8) | haelfte(in[j], half);
            if (++acc_n == 4) { csum ^= acc; acc = 0; acc_n = 0; }
        }
        if (acc_n) {
            acc <<= 8 * (4 - acc_n);
            csum ^= acc;
        }
    }
    return csum;
}

size_t uft_amiga_mfm_encode_track(const uint8_t *data, unsigned sectors,
                                  unsigned track, const uint8_t *labels,
                                  uint8_t *out, size_t out_cap)
{
    size_t n = 0;
    unsigned s;
    int vorher = 0;

    if (!data || !out || sectors == 0u) return 0;
    if (sectors - 1u > AMIGA_MAX_SECTOR) return 0;
    if (track > AMIGA_MAX_TRACK) return 0;
    if (out_cap < (size_t)sectors * UFT_AMIGA_SECTOR_MFM_BYTES) return 0;

    for (s = 0; s < sectors; s++) {
        uint8_t info[4], label[16], hchk[4], dchk[4];
        uint32_t hdr_csum = 0, data_csum, ist = 0;
        const uint8_t *sek = data + (size_t)s * AMIGA_SECTOR_BYTES;

        /* Luecke: zwei Byte 0x00, MFM-kodiert. */
        out[n++] = takt_fuellen(0x00u, &vorher);
        out[n++] = takt_fuellen(0x00u, &vorher);

        /* Zwei Synchronworte 0x4489, ROH. Sie tragen absichtlich ein
         * FEHLENDES Taktbit " das ist die Marke, die ein Laufwerk sucht,
         * und sie darf deshalb nicht durch die Regel oben laufen. */
        roh(out, &n, 0x44u, &vorher);
        roh(out, &n, 0x89u, &vorher);
        roh(out, &n, 0x44u, &vorher);
        roh(out, &n, 0x89u, &vorher);

        /* info = [0xFF][track][sector][Sektoren bis zur Luecke] */
        info[0] = 0xFFu;
        info[1] = (uint8_t)track;
        info[2] = (uint8_t)s;
        info[3] = (uint8_t)(sectors - s);

        if (labels) memcpy(label, labels + (size_t)s * 16u, 16u);
        else        memset(label, 0, sizeof label);

        feld(out, &n, info, 4u, &hdr_csum, &vorher);
        feld(out, &n, label, 16u, &hdr_csum, &vorher);
        hdr_csum &= AMIGA_CKSUM_MASK;
        hchk[0] = (uint8_t)(hdr_csum >> 24);
        hchk[1] = (uint8_t)(hdr_csum >> 16);
        hchk[2] = (uint8_t)(hdr_csum >>  8);
        hchk[3] = (uint8_t)(hdr_csum);

        data_csum = feld_summe(sek, AMIGA_SECTOR_BYTES) & AMIGA_CKSUM_MASK;
        dchk[0] = (uint8_t)(data_csum >> 24);
        dchk[1] = (uint8_t)(data_csum >> 16);
        dchk[2] = (uint8_t)(data_csum >>  8);
        dchk[3] = (uint8_t)(data_csum);

        /* Die Pruefsummenfelder selbst zaehlen nicht mit (csum = NULL) "
         * eine Pruefsumme pruefsummt sich nicht selbst. */
        feld(out, &n, hchk, 4u, NULL, &vorher);
        feld(out, &n, dchk, 4u, NULL, &vorher);

        feld(out, &n, sek, AMIGA_SECTOR_BYTES, &ist, &vorher);

        /* Sicherung, keine Zierde: geht die Summe hier nicht auf, stimmt
         * die Rechnung nicht, und ein Sektor mit falscher Pruefsumme
         * waere eine stille Luege. Dann wird NICHTS ausgegeben. */
        if ((ist & AMIGA_CKSUM_MASK) != data_csum) return 0;
    }
    return n;
}
