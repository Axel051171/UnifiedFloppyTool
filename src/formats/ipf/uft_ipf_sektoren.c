/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_ipf_sektoren.c
 * @brief IPF: Sektorebene aus dem Zellstrom (MF-1373, P3-360 Teil 1)
 *
 * Eigene Umsetzung. Dekodiert selbst nichts, sondern waehlt zwischen den
 * zwei Bitstrom-Dekodern des Baums und uebertraegt ihr Ergebnis in die
 * Spur — Begruendung und Referenz im Kopf von uft_ipf_sektoren.h.
 */
#include "uft/formats/ipf/uft_ipf_sektoren.h"
#include "uft/uft_format_common.h"
#include "uft/flux/uft_mfm_sector_parser.h"
#include "uft/flux/uft_flux_decoder.h"

#include <stdlib.h>
#include <string.h>

/* Obergrenzen fuer den IBM-Lauf. 64 IDAMs sind mehr, als eine DD- oder
 * HD-Spur traegt (18); der Pool deckt N = 0..3, also bis 1024 Byte je
 * Sektor — mehr laesst `uft_mfm_decode_track()` ohne Optionen nicht zu. */
#define IPF_SEK_MAX_IBM   64u
#define IPF_SEK_POOL      ((size_t)IPF_SEK_MAX_IBM * 1024u)

/** Kennzeichnet den zuletzt angelegten Sektor mit dem, was der Dekoder
 *  gemessen hat. `uft_format_add_sector_with_id()` setzt unbedingt „gut". */
static void letzten_kennzeichnen(uft_track_t *track, bool kopf_ok,
                                 bool daten_ok, bool geloescht,
                                 uft_ipf_sektor_bericht_t *b)
{
    uft_sector_t *s = &track->sectors[track->sector_count - 1u];
    s->status |= (uint32_t)UFT_SECTOR_CRC_CHECKED;
    if (!kopf_ok) {
        uft_sector_set_id_crc(s, false);
        s->id.crc_ok = false;
        s->status |= (uint32_t)UFT_SECTOR_ID_CRC_ERROR;
        b->kopf_crc_falsch++;
    }
    if (!daten_ok) {
        uft_sector_set_crc(s, false);
        s->status |= (uint32_t)UFT_SECTOR_CRC_ERROR;
        b->daten_crc_falsch++;
    }
    if (geloescht) {
        s->deleted = true;
        s->status |= (uint32_t)UFT_SECTOR_DELETED;
    }
}

int uft_ipf_sektoren(const uint8_t *zellen, uint32_t bits,
                     uft_track_t *track,
                     uft_ipf_sektor_bericht_t *bericht)
{
    uft_ipf_sektor_bericht_t leer;
    uft_ipf_sektor_bericht_t *b = bericht ? bericht : &leer;
    memset(b, 0, sizeof(*b));
    if (!zellen || !track || bits == 0) return -1;

    /* ── IBM-MFM ─────────────────────────────────────────────────────── */
    uft_mfm_sector_t *recs = (uft_mfm_sector_t *)calloc(
        IPF_SEK_MAX_IBM, sizeof(uft_mfm_sector_t));
    uint8_t *pool = (uint8_t *)malloc(IPF_SEK_POOL);
    if (!recs || !pool) { free(recs); free(pool); return -1; }
    const size_t n_ibm = uft_mfm_decode_track(zellen, bits, pool,
                                              IPF_SEK_POOL, recs,
                                              IPF_SEK_MAX_IBM, NULL);
    for (size_t i = 0; i < n_ibm; i++)
        if (recs[i].id_crc_ok) b->ibm_koepfe_ok++;

    /* ── Amiga ───────────────────────────────────────────────────────── */
    flux_decoded_track_t *am = (flux_decoded_track_t *)calloc(
        1u, sizeof(flux_decoded_track_t));
    if (!am) { free(recs); free(pool); return -1; }
    (void)flux_decode_amiga_bits(zellen, bits, am, NULL);
    for (size_t i = 0; i < am->sector_count; i++)
        if (am->sectors[i].id_crc_ok) b->amiga_koepfe_ok++;

    /* ── Wahl nach dem Inhalt ────────────────────────────────────────── */
    if (b->ibm_koepfe_ok && b->amiga_koepfe_ok) {
        b->art = UFT_IPF_SEKTOR_MEHRDEUTIG;
    } else if (b->ibm_koepfe_ok) {
        b->art = UFT_IPF_SEKTOR_IBM;
        track->encoding = UFT_ENC_MFM;
        for (size_t i = 0; i < n_ibm; i++) {
            const uft_mfm_sector_t *r = &recs[i];
            if (!r->dam_present || r->data_len == 0) {
                b->ohne_daten++;
                continue;
            }
            if (uft_format_add_sector_with_id(track, r->sector,
                                              pool + r->data_offset,
                                              (uint16_t)r->data_len,
                                              r->cylinder,
                                              r->head) != UFT_OK) {
                continue;
            }
            b->angelegt++;
            letzten_kennzeichnen(track, r->id_crc_ok, r->data_crc_ok,
                                 r->deleted, b);
        }
    } else if (b->amiga_koepfe_ok) {
        b->art = UFT_IPF_SEKTOR_AMIGA;
        track->encoding = UFT_ENC_AMIGA_MFM;
        for (size_t i = 0; i < am->sector_count; i++) {
            const flux_decoded_sector_t *s = &am->sectors[i];
            if (!s->data || s->data_size == 0) continue;
            /* Amiga zaehlt seine Sektoren ab 0, und so stehen sie im
             * Kopf — die ID wird nicht umgerechnet. */
            if (uft_format_add_sector_with_id(track, s->sector, s->data,
                                              (uint16_t)s->data_size,
                                              s->cylinder,
                                              s->head) != UFT_OK) {
                continue;
            }
            b->angelegt++;
            letzten_kennzeichnen(track, s->id_crc_ok, s->data_crc_ok,
                                 false, b);
        }
    }

    flux_decoded_track_free(am);
    free(am);
    free(recs);
    free(pool);
    return 0;
}
