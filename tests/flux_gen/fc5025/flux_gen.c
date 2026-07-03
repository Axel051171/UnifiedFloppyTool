/**
 * @file tests/flux_gen/fc5025/flux_gen.c
 * @brief Implementation of the synthetic FC5025 sector generator.
 *
 * See flux_gen.h. FC5025 is a sector-only device; this produces decoded
 * sector payloads with per-sector CRC status and models divergent
 * re-reads of a marginal sector (provider Rule F-3).
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* ─── geometry table ────────────────────────────────────────────────── */

bool uft_fc_gen_geometry(uft_fc_gen_format_t fmt, uft_fc_geometry_t *out) {
    if (!out) return false;
    switch (fmt) {
        case UFT_FC_GEN_APPLE_DOS33:
            *out = (uft_fc_geometry_t){ 35, 16, 256, 300.0 }; return true;
        case UFT_FC_GEN_APPLE_DOS32:
            *out = (uft_fc_geometry_t){ 35, 13, 256, 300.0 }; return true;
        case UFT_FC_GEN_IBM_FM_8:
            *out = (uft_fc_geometry_t){ 77, 26, 128, 360.0 }; return true;
        case UFT_FC_GEN_TRS80_SSSD:
            *out = (uft_fc_geometry_t){ 35, 10, 256, 300.0 }; return true;
        default:
            return false;
    }
}

/* ─── track build ───────────────────────────────────────────────────── */

uft_fc_gen_err_t uft_fc_gen_track(const uft_fc_gen_params_t *params,
                                  uft_fc_gen_track_t *out) {
    if (!params || !out) return UFT_FC_GEN_ERR_NULL;
    memset(out, 0, sizeof(*out));

    uft_fc_geometry_t g;
    if (!uft_fc_gen_geometry(params->format, &g))
        return UFT_FC_GEN_ERR_OUT_OF_SPEC;
    if (params->track < 0 || params->track >= g.tracks ||
        params->side < 0 || params->side > 1)
        return UFT_FC_GEN_ERR_OUT_OF_SPEC;

    int nsec = g.sectors_per_track;
    uint16_t ssz = g.sector_size;

    uint8_t *data = (uint8_t *)malloc((size_t)nsec * ssz);
    uint8_t *stat = (uint8_t *)calloc((size_t)nsec, 1);
    if (!data || !stat) { free(data); free(stat); return UFT_FC_GEN_ERR_NOMEM; }

    uint64_t rng = params->seed
                 ? params->seed ^ ((uint64_t)params->track << 16)
                                ^ ((uint64_t)params->side << 8)
                 : 0xFC5025ULL;

    /* Fill each sector with deterministic pseudo-random payload. */
    for (int s = 0; s < nsec; s++) {
        uint8_t *sec = data + (size_t)s * ssz;
        for (uint16_t b = 0; b < ssz; b++)
            sec[b] = (uint8_t)(rng_next(&rng) & 0xFF);
    }

    /* Apply defects to the chosen sector. */
    int ds = params->defect_sector;
    if (ds >= 0 && ds < nsec) {
        if (params->defects & UFT_FC_DEFECT_CRC_SECTOR)
            stat[ds] = UFT_FC_SEC_CRC_ERROR;
        else if (params->defects & UFT_FC_DEFECT_MISSING_SEC)
            stat[ds] = UFT_FC_SEC_MISSING;
        /* WEAK_SECTOR keeps status OK but diverges on re-read (see
         * uft_fc_gen_read_sector). */
    }

    out->format        = params->format;
    out->track         = params->track;
    out->side          = params->side;
    out->sector_count  = nsec;
    out->sector_size   = ssz;
    out->data          = data;
    out->status        = stat;
    out->base_seed     = params->seed ? params->seed : 0xFC5025ULL;
    out->defects       = params->defects;
    out->defect_sector = ds;
    return UFT_FC_GEN_OK;
}

void uft_fc_gen_free_track(uft_fc_gen_track_t *t) {
    if (!t) return;
    free(t->data);
    free(t->status);
    t->data = NULL; t->status = NULL;
    t->sector_count = 0;
}

/* ─── sector read (with divergence for marginal sectors) ────────────── */

uft_fc_sec_status_t uft_fc_gen_read_sector(const uft_fc_gen_track_t *t,
                                           int sector, int attempt,
                                           uint8_t *buf, size_t cap) {
    if (!t || !buf || sector < 0 || sector >= t->sector_count)
        return UFT_FC_SEC_MISSING;
    if (cap < t->sector_size) return UFT_FC_SEC_MISSING;

    uft_fc_sec_status_t st = (uft_fc_sec_status_t)t->status[sector];
    const uint8_t *base = t->data + (size_t)sector * t->sector_size;
    memcpy(buf, base, t->sector_size);

    /* A CRC-error or weak sector diverges on each read attempt: flip a
     * few bytes deterministically keyed by (sector, attempt). Clean
     * sectors return the identical bytes on every attempt. */
    bool diverges =
        (st == UFT_FC_SEC_CRC_ERROR) ||
        (t->defect_sector == sector &&
         (t->defects & UFT_FC_DEFECT_WEAK_SECTOR));
    if (diverges) {
        uint64_t r = t->base_seed
                   ^ ((uint64_t)sector << 24)
                   ^ ((uint64_t)attempt << 40)
                   ^ 0x9E3779B97F4A7C15ULL;
        /* Perturb ~4 bytes so re-reads differ but the sector is still
         * "mostly" the same (marginal, not garbage). */
        for (int k = 0; k < 4; k++) {
            uint32_t pos = (uint32_t)(rng_next(&r) % t->sector_size);
            buf[pos] ^= (uint8_t)(rng_next(&r) & 0xFF);
        }
    }
    return st;
}
