/**
 * @file uft_ipf_zellstrom.c
 * @brief IPF-Blockelemente -> MFM-Zellstrom (MF-1079, P3-360)
 *
 * Warum es diese Datei gibt, welche Referenz sie benennt und was sie
 * NICHT tut, steht im Kopf von
 * `include/uft/formats/ipf/uft_ipf_zellstrom.h`. Hier steht die
 * Umsetzung.
 *
 * **Eigenstaendig geschrieben.** Die Elementarten stammen aus MAMEs
 * `ipf_dsk.cpp` (BSD-3-Clause, Olivier Galibert), Z. 555-570 — gelesen,
 * nicht uebernommen; die MFM-Regel selbst ist die uebliche: je Datenbit
 * ein Taktbit davor, und das Taktbit ist genau dann 1, wenn das vorige
 * UND das jetzige Datenbit 0 sind.
 */
#include "uft/formats/ipf/uft_ipf_zellstrom.h"

#include <stdlib.h>
#include <string.h>

/* Elementarten laut Datei. Die Zahlen sind die des Formats, nicht
 * unsere Wahl — `uft_ipf_air.c` reicht sie unveraendert heraus. */
#define IPF_EL_SYNC   1u   /* schon Zellen */
#define IPF_EL_DATA   2u   /* dekodierte Bytes */
#define IPF_EL_GAP    3u   /* dekodierte Bytes */
#define IPF_EL_RAW    4u   /* schon Zellen */
#define IPF_EL_FUZZY  5u   /* ohne Wert */

/** Ein Bit in den Ausgabepuffer, MSB zuerst. */
static void bit_schreiben(uint8_t *puffer, uint32_t bitpos, int wert)
{
    const uint32_t byte = bitpos >> 3;
    const uint8_t  maske = (uint8_t)(0x80u >> (bitpos & 7u));
    if (wert) puffer[byte] |= maske;
    else      puffer[byte] = (uint8_t)(puffer[byte] & (uint8_t)~maske);
}

/** Ein Byte roher Zellen: unveraendert uebernehmen. */
static void zellen_roh(uint8_t *puffer, uint32_t *bitpos, uint8_t b)
{
    for (int i = 7; i >= 0; i--) {
        bit_schreiben(puffer, (*bitpos)++, (b >> i) & 1);
    }
}

/**
 * Ein Datenbit MFM-kodieren: erst das Taktbit, dann das Datenbit.
 * `vorher` traegt das zuletzt geschriebene DATENbit ueber Byte-,
 * Element- und Zwischenraumgrenzen hinweg — ohne diesen Kontext
 * stimmt das erste Taktbit hinter jeder Grenze nicht, und genau dort
 * sassen die 587 unmoeglichen Zellpaare (siehe unten).
 */
static void zellen_mfm_bit(uint8_t *puffer, uint32_t *bitpos, int d,
                           int *vorher)
{
    const int takt = (!*vorher && !d) ? 1 : 0;
    bit_schreiben(puffer, (*bitpos)++, takt);
    bit_schreiben(puffer, (*bitpos)++, d);
    *vorher = d;
}

/** Ein ganzes dekodiertes Byte, MSB zuerst. */
static void zellen_mfm(uint8_t *puffer, uint32_t *bitpos, uint8_t b,
                       int *vorher)
{
    for (int i = 7; i >= 0; i--) {
        zellen_mfm_bit(puffer, bitpos, (b >> i) & 1, vorher);
    }
}

int uft_ipf_zellstrom(const ipf_air_disk_t *disk, int cyl, int head,
                      uint8_t **out_buf, uint32_t *out_bits)
{
    if (out_buf)  *out_buf  = NULL;
    if (out_bits) *out_bits = 0;
    if (!disk || !out_buf || !out_bits) return -1;

    uint32_t track_bits = 0, dichte = 0, flaggen = 0;
    bool fuzzy = false;
    if (ipf_air_get_track_meta(disk, cyl, head, &track_bits, &dichte,
                               &flaggen, &fuzzy) != 0) {
        return -1;
    }
    const int bloecke = ipf_air_get_block_count(disk, cyl, head);
    if (bloecke <= 0 || track_bits == 0) return -1;

    /* MF-830 hat den Verlust schon gemessen und festgehalten; hier
     * wird er zum ersten Mal an einem echten Abbild SICHTBAR.
     * `IPF_MAX_BLOCKS` ist 16, und Spur 79/0 von
     * `sps_lethalxcess_a.ipf` sagt **35** Bloecke an — 19
     * haelt dieser Leser nicht. Ein Zellstrom aus 16 von 35
     * Bloecken waere kuerzer als `track_bits` und damit falsch.
     * Also eigener Code statt der allgemeinen Absage, damit der
     * Aufrufer den Grund NENNEN kann statt ihn zu raten. */
    bool gekappt = false;
    if (ipf_air_get_track_loss(disk, cyl, head, NULL, NULL,
                               &gekappt) == 0 && gekappt) {
        return -3;
    }

    /* Der Puffer wird auf die von der DATEI angesagte Groesse gelegt,
     * nicht auf eine selbst gerechnete. Passt das Ergebnis nicht hinein
     * oder fuellt es den Puffer nicht, ist die Rechnung falsch und wir
     * sagen ab (-2) statt etwas Halbes auszugeben. */
    const size_t bytes = ((size_t)track_bits + 7u) / 8u;
    uint8_t *puffer = (uint8_t *)calloc(1u, bytes ? bytes : 1u);
    if (!puffer) return -1;

    uint32_t bitpos = 0;
    int vorher = 0;   /* zuletzt geschriebenes Datenbit */

    for (uint32_t b = 0; b < (uint32_t)bloecke; b++) {
        uint32_t data_bits = 0, gap_bits = 0;
        if (ipf_air_get_block_sizes(disk, cyl, head, b,
                                    &data_bits, &gap_bits) != 0) {
            free(puffer);
            return -2;
        }
        const uint32_t block_start = bitpos;

        const int elemente = ipf_air_get_elem_count(disk, cyl, head, b);
        if (elemente < 0) { free(puffer); return -2; }

        for (uint32_t e = 0; e < (uint32_t)elemente; e++) {
            uint32_t typ = 0, bits = 0, len = 0;
            const uint8_t *wert = NULL;
            if (ipf_air_get_elem(disk, cyl, head, b, e, &typ, &bits,
                                 &wert, &len) != 0) {
                free(puffer);
                return -2;
            }

            uint32_t braucht = 0;
            if (typ == IPF_EL_SYNC || typ == IPF_EL_RAW) {
                braucht = len * 8u;
            } else if (typ == IPF_EL_DATA || typ == IPF_EL_GAP) {
                braucht = len * 16u;
            } else if (typ == IPF_EL_FUZZY) {
                braucht = bits;
            }
            if ((size_t)bitpos + braucht > (size_t)track_bits) {
                free(puffer);
                return -2;
            }

            if (typ == IPF_EL_SYNC || typ == IPF_EL_RAW) {
                /* Schon Zellen. Der MFM-Kontext haengt danach am letzten
                 * ZELLbit, und das ist hier das niederwertigste. */
                for (uint32_t i = 0; wert && i < len; i++) {
                    zellen_roh(puffer, &bitpos, wert[i]);
                }
                if (wert && len) vorher = wert[len - 1u] & 1;
            } else if (typ == IPF_EL_DATA || typ == IPF_EL_GAP) {
                for (uint32_t i = 0; wert && i < len; i++) {
                    zellen_mfm(puffer, &bitpos, wert[i], &vorher);
                }
            } else if (typ == IPF_EL_FUZZY) {
                /* Ohne Wert. Die Zellen bleiben null, und die Spur ist
                 * ohnehin als FUZZY gekennzeichnet (MF-823) — erfunden
                 * wird hier nichts, nur Platz gehalten. */
                bitpos += bits;
            } else {
                free(puffer);
                return -2;
            }
        }

        if (bitpos - block_start != data_bits) {
            /* Die Gleichung, die ueber 1618 Bloecke gemessen aufgeht.
             * Geht sie hier nicht auf, stimmt unsere Lesart nicht. */
            free(puffer);
            return -2;
        }

        if (gap_bits) {
            if ((size_t)bitpos + gap_bits > (size_t)track_bits) {
                free(puffer);
                return -2;
            }
            /* Der Zwischenraum ist MFM fuer lauter 0x00 — also die
             * Folge 1 0 1 0 ... Aber NUR ab dem zweiten Taktbit.
             *
             * Hier stand `(i & 1u) ? 0 : 1`, und das schrieb die erste
             * Zelle unbedingt als 1. Genau das ist falsch, wenn das
             * letzte Datenbit davor eine 1 war: dann verlangt die
             * MFM-Regel ein Taktbit 0, und die feste 1 erzeugt zwei
             * benachbarte Einsen — eine Folge, die auf einer
             * MFM-Diskette nicht vorkommt.
             *
             * Gemessen am Vorzustand: **587** solche Paare auf
             * Diskette A und **760** auf B, alle 587 bzw. 760 in
             * MFM-Bereichen und **keine** in den Rohzellen — also
             * genau dort, wo sie unmoeglich sind. Deshalb laeuft der
             * Zwischenraum jetzt durch dieselbe Regel wie die Daten,
             * mit demselben `vorher`-Kontext.
             *
             * Dass das aufgeht, ist gemessen und nicht angenommen:
             * **alle** 1573 bzw. 1600 Zwischenraeume der beiden
             * Abbilder haben eine GERADE Zellzahl, sind also glatt
             * `gap_bits / 2` Datenbits. Eine ungerade Zahl waere kein
             * MFM mehr; dann wird abgesagt statt gerundet.
             *
             * Die Grenze aus dem Kopf bleibt: ausdrueckliche
             * Gap-Beschreibungen wertet diese Fassung nicht aus. */
            if (gap_bits & 1u) {
                free(puffer);
                return -2;
            }
            for (uint32_t i = 0; i < gap_bits / 2u; i++) {
                zellen_mfm_bit(puffer, &bitpos, 0, &vorher);
            }
        }
    }

    if (bitpos != track_bits) {
        free(puffer);
        return -2;
    }

    *out_buf  = puffer;
    *out_bits = track_bits;
    return 0;
}
