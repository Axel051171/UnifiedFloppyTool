/**
 * @file tests/flux_gen/xum1541/flux_gen.c
 * @brief Synthetic raw-GCR track generator (1541-class drives).
 *
 * See flux_gen.h for the spec references. Every byte this generator
 * emits is traceable: clean tracks follow the 1541 DOS format layout
 * exactly; defect bytes come from the seeded xorshift64* RNG and are
 * reproducible from (seed, params) alone.
 */

#include "flux_gen.h"

#include <stdlib.h>
#include <string.h>

/* ─── Deterministic RNG (xorshift64*) — identical to SCP/GW gens ────── */

static uint64_t rng_next(uint64_t *s)
{
    uint64_t x = *s;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1Dull;
}

static void rng_seed(uint64_t *s, uint64_t seed)
{
    *s = (seed == 0) ? 1 : seed;
}

/* ─── Zone tables ───────────────────────────────────────────────────── */

int uft_xum_gcr_speed_zone(int track)
{
    if (track < UFT_XUM_GCR_MIN_TRACK || track > UFT_XUM_GCR_MAX_TRACK) {
        return -1;
    }
    if (track <= 17) return 3;
    if (track <= 24) return 2;
    if (track <= 30) return 1;
    return 0;   /* 31..42 (36+ = extended tracks, same zone as 31-35) */
}

int uft_xum_gcr_sectors_for_zone(int zone)
{
    switch (zone) {
        case 3: return 21;
        case 2: return 19;
        case 1: return 18;
        case 0: return 17;
        default: return 0;
    }
}

size_t uft_xum_gcr_zone_budget(int zone)
{
    /* bytes per revolution at 300 rpm — bitrate * 0.2 s / 8.
     * Same values the G64 format uses as maximum track sizes. */
    switch (zone) {
        case 3: return 7692;
        case 2: return 7142;
        case 1: return 6666;
        case 0: return 6250;
        default: return 0;
    }
}

/* ─── GCR codec ─────────────────────────────────────────────────────── */

/* CBM 4-bit -> 5-bit group code (1541 DOS ROM $F77F table). */
static const uint8_t GCR_ENC[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

void uft_xum_gcr_encode4(const uint8_t in[4], uint8_t out[5])
{
    uint64_t bits = 0;
    for (int i = 0; i < 4; i++) {
        bits = (bits << 5) | GCR_ENC[in[i] >> 4];
        bits = (bits << 5) | GCR_ENC[in[i] & 0x0F];
    }
    for (int i = 4; i >= 0; i--) {
        out[i] = (uint8_t)(bits & 0xFF);
        bits >>= 8;
    }
}

int uft_xum_gcr_decode5(const uint8_t in[5], uint8_t out[4])
{
    /* Inverse table: -1 marks the 16 illegal 5-bit codes. */
    static int8_t dec[32];
    static int    dec_ready = 0;
    if (!dec_ready) {
        memset(dec, -1, sizeof(dec));
        for (int i = 0; i < 16; i++) dec[GCR_ENC[i]] = (int8_t)i;
        dec_ready = 1;
    }
    uint64_t bits = 0;
    for (int i = 0; i < 5; i++) bits = (bits << 8) | in[i];
    for (int i = 0; i < 4; i++) {
        int hi = dec[(bits >> (35 - i * 10)) & 0x1F];
        int lo = dec[(bits >> (30 - i * 10)) & 0x1F];
        if (hi < 0 || lo < 0) return -1;
        out[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

/* Encode `plain_len` bytes (must be a multiple of 4) into GCR at dst. */
static void gcr_encode_block(const uint8_t *plain, size_t plain_len,
                             uint8_t *dst)
{
    for (size_t i = 0; i < plain_len; i += 4) {
        uft_xum_gcr_encode4(plain + i, dst + (i / 4) * 5);
    }
}

/* ─── Track generation ──────────────────────────────────────────────── */

uft_xum_gcr_err_t uft_xum_gcr_gen_track(const uft_xum_gcr_params_t *params,
                                        uft_xum_gcr_track_t *out)
{
    if (!out) return UFT_XUM_GCR_ERR_NULL;
    memset(out, 0, sizeof(*out));
    if (!params) return UFT_XUM_GCR_ERR_NULL;

    /* Forensic-medium-safety: stepper bounds. Track 42 is the physical
     * limit; a half-track read at 42+0.5 would step past the stop. */
    if (params->track < UFT_XUM_GCR_MIN_TRACK ||
        params->track > UFT_XUM_GCR_MAX_TRACK) {
        return UFT_XUM_GCR_ERR_OUT_OF_SPEC;
    }
    if ((params->defects & UFT_XUM_DEFECT_HALF_TRACK) &&
        params->track >= UFT_XUM_GCR_MAX_TRACK) {
        return UFT_XUM_GCR_ERR_OUT_OF_SPEC;
    }

    /* DENSITY_MISMATCH: the protection writes zone-3 bit density on a
     * track whose nominal zone is lower (classic "more data than DOS
     * expects" scheme). Budget follows the density actually written,
     * because bytes/rev = bitrate x rev-time, independent of radius. */
    int zone = (params->defects & UFT_XUM_DEFECT_DENSITY_MISMATCH)
                   ? 3 : uft_xum_gcr_speed_zone(params->track);
    int    sectors = uft_xum_gcr_sectors_for_zone(zone);
    size_t budget  = uft_xum_gcr_zone_budget(zone);

    uint8_t *buf = (uint8_t *)malloc(budget);
    if (!buf) return UFT_XUM_GCR_ERR_NULL;

    /* KILLER_TRACK: constant 0x55, zero sync marks. A 1541 read head
     * waiting for sync on such a track hangs forever (RapidLok et al).
     * Overrides all other defects — there is no structure to defect. */
    if (params->defects & UFT_XUM_DEFECT_KILLER_TRACK) {
        memset(buf, 0x55, budget);
        out->bytes = buf;
        out->len = budget;
        out->sector_count = 0;
        out->speed_zone = zone;
        out->budget = budget;
        return UFT_XUM_GCR_OK;
    }

    uint64_t rng;
    rng_seed(&rng, params->seed);

    size_t pos = 0;
    size_t sec0_data_off = 0;  /* GCR offset of sector 0's data block */

    for (int s = 0; s < sectors; s++) {
        /* Header sync — SYNC_LONG stretches sector 0's to 40 bytes. */
        size_t hsync = UFT_XUM_GCR_SYNC_LEN;
        if ((params->defects & UFT_XUM_DEFECT_SYNC_LONG) && s == 0) {
            hsync = UFT_XUM_GCR_LONG_SYNC_LEN;
        }
        if (pos + hsync > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
        memset(buf + pos, 0xFF, hsync);
        pos += hsync;

        /* Header block: 8 plain -> 10 GCR. */
        uint8_t hdr[8];
        hdr[0] = 0x08;
        hdr[2] = (uint8_t)s;
        hdr[3] = (uint8_t)params->track;
        hdr[4] = params->id2;
        hdr[5] = params->id1;
        hdr[1] = (uint8_t)(hdr[2] ^ hdr[3] ^ hdr[4] ^ hdr[5]);
        hdr[6] = 0x0F;
        hdr[7] = 0x0F;
        if (pos + UFT_XUM_GCR_HEADER_GCR_LEN > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
        gcr_encode_block(hdr, 8, buf + pos);
        pos += UFT_XUM_GCR_HEADER_GCR_LEN;

        /* Header gap. */
        if (pos + UFT_XUM_GCR_GAP_LEN > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
        memset(buf + pos, 0x55, UFT_XUM_GCR_GAP_LEN);
        pos += UFT_XUM_GCR_GAP_LEN;

        /* Data sync — SYNC_SHORT cuts sector 1's to a single 0xFF
         * (8 bits < the 10-bit sync threshold; drive never syncs). */
        size_t dsync = UFT_XUM_GCR_SYNC_LEN;
        if ((params->defects & UFT_XUM_DEFECT_SYNC_SHORT) && s == 1) {
            dsync = 1;
        }
        if (pos + dsync > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
        memset(buf + pos, 0xFF, dsync);
        pos += dsync;

        /* Data block: 260 plain -> 325 GCR. */
        uint8_t data[260];
        data[0] = 0x07;
        uint8_t cks = 0;
        for (int i = 0; i < 256; i++) {
            uint8_t b = (uint8_t)(rng_next(&rng) & 0xFF);
            data[1 + i] = b;
            cks ^= b;
        }
        if ((params->defects & UFT_XUM_DEFECT_CHECKSUM_ERROR) && s == 0) {
            cks ^= 0xFF;   /* 1541 DOS would report error 23 */
        }
        data[257] = cks;
        data[258] = 0x00;
        data[259] = 0x00;
        if (pos + UFT_XUM_GCR_DATA_GCR_LEN > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
        if (s == 0) sec0_data_off = pos;
        gcr_encode_block(data, 260, buf + pos);
        pos += UFT_XUM_GCR_DATA_GCR_LEN;

        /* Inter-sector gap. */
        if (pos + UFT_XUM_GCR_GAP_LEN > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
        memset(buf + pos, 0x55, UFT_XUM_GCR_GAP_LEN);
        pos += UFT_XUM_GCR_GAP_LEN;
    }

    /* Tail pad: fill to the exact zone budget (the physical revolution
     * always contains budget bytes — the formatter's tail gap). */
    if (pos > budget) { free(buf); return UFT_XUM_GCR_ERR_OUT_OF_SPEC; }
    memset(buf + pos, 0x55, budget - pos);

    /* ── Post-pass defects ─────────────────────────────────────────── */

    if (params->defects & UFT_XUM_DEFECT_WEAK_BITS) {
        /* Unstable magnetisation region inside sector 0's data block.
         * Real weak bits read differently on every pass; this generator
         * emits ONE deterministic snapshot (documented divergence).
         * 0xFF replaced by 0x7F so no fake sync run can appear. */
        size_t n = params->weak_region_bytes ? params->weak_region_bytes : 32;
        size_t off = sec0_data_off + 100;   /* mid-block, past the 0x07 ID */
        for (size_t i = 0; i < n && off + i < budget; i++) {
            uint8_t b = (uint8_t)(rng_next(&rng) & 0xFF);
            if (b == 0xFF) b = 0x7F;
            buf[off + i] = b;
        }
    }

    if (params->defects & UFT_XUM_DEFECT_HALF_TRACK) {
        /* Head positioned between track N and N+1: both tracks' fields
         * superimpose. Approximation (documented divergence): corrupt
         * ~30% of bytes deterministically, then wipe every second sync
         * run — crosstalk destroys roughly half the sync coherence. */
        for (size_t i = 0; i < budget; i++) {
            if ((rng_next(&rng) % 100u) < 30u) {
                uint8_t b = (uint8_t)(buf[i] ^ (uint8_t)(rng_next(&rng) & 0xFF));
                if (b == 0xFF) b = 0x7E;
                buf[i] = b;
            }
        }
        size_t run = 0, sync_idx = 0;
        for (size_t i = 0; i <= budget; i++) {
            if (i < budget && buf[i] == 0xFF) {
                run++;
                continue;
            }
            if (run >= 2) {
                if ((sync_idx & 1u) == 1u) {          /* odd-numbered sync */
                    memset(buf + i - run, 0x55, run);
                }
                sync_idx++;
            }
            run = 0;
        }
    }

    out->bytes = buf;
    out->len = budget;
    out->sector_count = sectors;
    out->speed_zone = zone;
    out->budget = budget;
    return UFT_XUM_GCR_OK;
}

void uft_xum_gcr_free(uft_xum_gcr_track_t *track)
{
    if (!track) return;
    free(track->bytes);
    track->bytes = NULL;
    track->len = 0;
    track->sector_count = 0;
}

/* ─── Analysis helpers ──────────────────────────────────────────────── */

size_t uft_xum_gcr_count_syncs(const uint8_t *bytes, size_t len)
{
    if (!bytes) return 0;
    size_t count = 0, run = 0;
    for (size_t i = 0; i <= len; i++) {
        if (i < len && bytes[i] == 0xFF) {
            run++;
            continue;
        }
        if (run >= 2) count++;
        run = 0;
    }
    return count;
}

size_t uft_xum_gcr_max_sync_run(const uint8_t *bytes, size_t len)
{
    if (!bytes) return 0;
    size_t best = 0, run = 0;
    for (size_t i = 0; i < len; i++) {
        if (bytes[i] == 0xFF) {
            run++;
            if (run > best) best = run;
        } else {
            run = 0;
        }
    }
    return best;
}

int uft_xum_gcr_parse_headers(const uint8_t *bytes, size_t len,
                              uft_xum_gcr_header_t *out, int max_out)
{
    if (!bytes || !out || max_out <= 0) return 0;
    int found = 0;
    size_t i = 0;
    while (i < len && found < max_out) {
        if (bytes[i] != 0xFF) { i++; continue; }
        size_t j = i;
        while (j < len && bytes[j] == 0xFF) j++;
        if (j - i >= 2 && j + UFT_XUM_GCR_HEADER_GCR_LEN <= len) {
            uint8_t plain[8];
            if (uft_xum_gcr_decode5(bytes + j, plain) == 0 &&
                uft_xum_gcr_decode5(bytes + j + 5, plain + 4) == 0 &&
                plain[0] == 0x08) {
                out[found].sector = plain[2];
                out[found].track  = plain[3];
                out[found].id2    = plain[4];
                out[found].id1    = plain[5];
                out[found].checksum_ok =
                    (plain[1] == (uint8_t)(plain[2] ^ plain[3] ^
                                           plain[4] ^ plain[5]));
                found++;
            }
        }
        i = j;
    }
    return found;
}

int uft_xum_gcr_parse_data_blocks(const uint8_t *bytes, size_t len,
                                  uint8_t *ok_flags, int max_out)
{
    if (!bytes || !ok_flags || max_out <= 0) return 0;
    int found = 0;
    size_t i = 0;
    while (i < len && found < max_out) {
        if (bytes[i] != 0xFF) { i++; continue; }
        size_t j = i;
        while (j < len && bytes[j] == 0xFF) j++;
        if (j - i >= 2 && j + 5 <= len) {
            uint8_t first[4];
            if (uft_xum_gcr_decode5(bytes + j, first) == 0 &&
                first[0] == 0x07 &&
                j + UFT_XUM_GCR_DATA_GCR_LEN <= len) {
                /* Decode the full 260-byte block; any illegal GCR group
                 * inside (e.g. weak-bit corruption) => ok = 0. */
                uint8_t plain[260];
                int ok = 1;
                for (int q = 0; q < 65; q++) {
                    if (uft_xum_gcr_decode5(bytes + j + (size_t)q * 5,
                                            plain + q * 4) != 0) {
                        ok = 0;
                        break;
                    }
                }
                if (ok) {
                    uint8_t cks = 0;
                    for (int k = 1; k <= 256; k++) cks ^= plain[k];
                    ok = (cks == plain[257]);
                }
                ok_flags[found++] = (uint8_t)ok;
            }
        }
        i = j;
    }
    return found;
}

size_t uft_xum_gcr_count_unsafe(const uft_xum_gcr_track_t *track)
{
    if (!track || !track->bytes) return 0;
    size_t violations = 0;
    /* Budget must be one of the four known zone budgets. */
    if (track->budget != uft_xum_gcr_zone_budget(0) &&
        track->budget != uft_xum_gcr_zone_budget(1) &&
        track->budget != uft_xum_gcr_zone_budget(2) &&
        track->budget != uft_xum_gcr_zone_budget(3)) {
        violations++;
    }
    /* Overlong track would overwrite its own start if written back. */
    if (track->len > track->budget) violations++;
    return violations;
}
