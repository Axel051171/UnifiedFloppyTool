/**
 * @file tests/flux_gen/amigados/flux_gen.c
 * @brief Implementierung des synthetischen AmigaDOS-MFM-Generators.
 *
 * Herausgezogen aus tests/test_convert_scp_adf_multirev.c (MF-478), weil ein
 * zweiter Test ihn braucht (MF-480). Unveraendert uebernommen — der
 * Selbsttest gegen `flux_decode_amiga()` gilt weiter und ist die einzige
 * Begruendung, warum man diesem Kodierer glauben darf.
 *
 * Aufbau eines Sektors, exakt die Umkehrung von `decode_amiga_sector()`
 * (src/flux/uft_flux_decoder.c:1204):
 *
 *   Vorspann | 0x4489 | 0x4489 | info(4) | label(16) | hchk(4) | dchk(4)
 *            | data(512)
 *
 * Jedes Feld liegt odd/even gespalten vor: erst @p nbytes Rohbytes mit den
 * ungeraden Datenbits, dann @p nbytes mit den geraden.
 */

#include "flux_gen.h"

#include <string.h>

/* ─── Zellen ─────────────────────────────────────────────────────────── */

static void cell_put(uft_amigados_cells_t *c, int bit)
{
    if (c->n >= c->cap) return;
    if (bit) c->cells[c->n / 8] |= (uint8_t)(0x80u >> (c->n % 8));
    c->n++;
}

/* Ein Datenbit anhaengen: Taktbit nach MFM-Regel (1 nur zwischen zwei
 * Null-Datenbits), dann das Datenbit selbst. */
static void mfm_put_data_bit(uft_amigados_cells_t *c, int d)
{
    cell_put(c, (!c->prev && !d) ? 1 : 0);
    cell_put(c, d);
    c->prev = d;
}

/* Ein rohes 16-Zellen-Wort anhaengen — fuer die Sync-Marke 0x4489, deren
 * fehlendes Taktbit sie ueberhaupt erst zur Marke macht. */
static void cell_put_word(uft_amigados_cells_t *c, uint16_t w)
{
    for (int i = 15; i >= 0; i--) cell_put(c, (w >> i) & 1);
    c->prev = w & 1;                       /* letzte Zelle ist ein Datenbit */
}

/* Fuellmuster 0xAA: abwechselnd, also lauter Datennullen mit Takt. */
static void cell_put_gap(uft_amigados_cells_t *c, size_t cells)
{
    for (size_t i = 0; i < cells; i++) mfm_put_data_bit(c, 0);
}

/* ─── Felder ─────────────────────────────────────────────────────────── */

/**
 * Ein Odd/Even-Feld schreiben und in die Pruefsumme falten.
 *
 * Der Dekoder setzt ein Byte als `((odd & 0x55) << 1) | (even & 0x55)`
 * zusammen. Also ist das Rohbyte der ungeraden Haelfte `(D >> 1) & 0x55`
 * und das der geraden `D & 0x55`.
 *
 * Die Pruefsumme ist das XOR der big-endian Rohbyte-Longs, am Ende mit
 * 0x55555555 maskiert. Weil die Maske genau die Taktbits wegnimmt, darf hier
 * mit taktfreien Rohbytes gerechnet werden: (a^b) & M == (a&M) ^ (b&M).
 */
static void amiga_put_field(uft_amigados_cells_t *c, const uint8_t *data,
                            size_t nbytes, uint32_t *csum)
{
    for (int half = 0; half < 2; half++) {
        uint32_t acc = 0;
        int      acc_n = 0;
        for (size_t j = 0; j < nbytes; j++) {
            uint8_t rb = (half == 0) ? (uint8_t)((data[j] >> 1) & 0x55u)
                                     : (uint8_t)(data[j] & 0x55u);
            for (int b = 3; b >= 0; b--)
                mfm_put_data_bit(c, (rb >> (2 * b)) & 1);
            if (csum) {
                acc = (acc << 8) | rb;
                if (++acc_n == 4) { *csum ^= acc; acc = 0; acc_n = 0; }
            }
        }
        if (csum && acc_n) {                 /* nbytes nicht durch 4 teilbar */
            acc <<= 8 * (4 - acc_n);
            *csum ^= acc;
        }
    }
}

static void be32(uint8_t out[4], uint32_t v)
{
    out[0] = (uint8_t)(v >> 24); out[1] = (uint8_t)(v >> 16);
    out[2] = (uint8_t)(v >>  8); out[3] = (uint8_t)v;
}

/**
 * Einen AmigaDOS-Sektor anhaengen.
 *
 * @param force_bad_dchk  Datenpruefsumme absichtlich falsch schreiben
 */
static void amiga_put_sector(uft_amigados_cells_t *c, uint8_t track,
                             uint8_t sec, const uint8_t *data,
                             bool force_bad_dchk)
{
    cell_put_gap(c, 32);                     /* Vorspann */
    cell_put_word(c, 0x4489);                /* Amiga schreibt zwei Marken */
    cell_put_word(c, 0x4489);

    uint8_t info[4]  = { 0xFF, track, sec, (uint8_t)(UFT_AMIGADOS_SPT - sec) };
    uint8_t label[16]; memset(label, 0, sizeof(label));

    /* Kopf- und Datenpruefsumme muessen VOR dem Schreiben feststehen, weil
     * sie zwischen Label und Daten im Strom liegen. Also erst trocken
     * rechnen (cap 0 schluckt die Zellen), dann schreiben. */
    uft_amigados_cells_t dry = { NULL, 0, 0, 0 };
    uint32_t hdr = 0, dat = 0;
    amiga_put_field(&dry, info,  4,  &hdr);
    amiga_put_field(&dry, label, 16, &hdr);
    amiga_put_field(&dry, data,  UFT_AMIGADOS_SECSZ, &dat);

    uint8_t hchk[4], dchk[4];
    be32(hchk, hdr & 0x55555555u);
    be32(dchk, (dat ^ (force_bad_dchk ? 0x00540000u : 0u)) & 0x55555555u);

    amiga_put_field(c, info,  4,  NULL);
    amiga_put_field(c, label, 16, NULL);
    amiga_put_field(c, hchk,  4,  NULL);
    amiga_put_field(c, dchk,  4,  NULL);
    amiga_put_field(c, data,  UFT_AMIGADOS_SECSZ, NULL);
}

/* ─── API ────────────────────────────────────────────────────────────── */

void uft_amigados_build_track(uft_amigados_cells_t *c, const uint8_t *adf,
                              uint8_t track,
                              const uft_amigados_defect_t *defect)
{
    if (!c || !c->cells || !adf || c->cap == 0) return;

    memset(c->cells, 0, (c->cap + 7) / 8);
    c->n = 0; c->prev = 0;

    for (int s = 0; s < UFT_AMIGADOS_SPT; s++) {
        const uint8_t *src = adf + ((size_t)track * UFT_AMIGADOS_SPT
                                    + (size_t)s) * UFT_AMIGADOS_SECSZ;
        uint8_t buf[UFT_AMIGADOS_SECSZ];
        memcpy(buf, src, UFT_AMIGADOS_SECSZ);

        bool bad = false;
        if (defect && defect->sector == s) {
            bad = defect->bad_checksum;
            if (defect->overwrite) buf[0] = defect->overwrite;
        }
        amiga_put_sector(c, track, (uint8_t)s, buf, bad);
    }

    /* Auf die volle Umdrehung auffuellen. */
    while (c->n + 1 < c->cap) mfm_put_data_bit(c, 0);
}

size_t uft_amigados_cells_to_intervals(const uft_amigados_cells_t *c,
                                       unsigned cell_ns,
                                       uint32_t *out, size_t cap)
{
    if (!c || !c->cells || !out || cell_ns == 0) return 0;

    size_t n = 0, last = 0;
    for (size_t i = 0; i < c->n && n < cap; i++) {
        if (!((c->cells[i / 8] >> (7 - (i % 8))) & 1)) continue;
        out[n++] = (uint32_t)((i - last) * (size_t)cell_ns);
        last = i;
    }
    /* Zelle 0 gesetzt: ein Intervall von 0 gibt es nicht. */
    if (n > 0 && out[0] == 0) out[0] = cell_ns;
    return n;
}

void uft_amigados_fill_pattern(uint8_t *adf, size_t bytes)
{
    if (!adf) return;
    for (size_t i = 0; i < bytes; i++)
        adf[i] = (uint8_t)(0x40u + ((i * 7u + (i >> 9)) & 0x3Fu));
}
