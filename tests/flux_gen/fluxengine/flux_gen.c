/**
 * @file tests/flux_gen/fluxengine/flux_gen.c
 * @brief Implementation of the synthetic SCP-file generator (FluxEngine).
 *
 * The output decodes through the production parser
 * uft_scp_read_track_memory() (src/flux/uft_scp_parser.c); defect flags
 * map to that parser's error codes.
 */

#include "flux_gen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* SCP layout constants (mirror uft_scp_parser.h). */
#define SCP_HDR_SIZE    16u
#define SCP_MAX_TRACKS  168u
#define SCP_TABLE_SIZE  (SCP_MAX_TRACKS * 4u)
#define SCP_TRK_HDR     4u          /* "TRK" + track_number */

/* ─── RNG ───────────────────────────────────────────────────────────── */
static uint64_t rng_next(uint64_t *s) {
    uint64_t x = *s;
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27;
    *s = x;
    return x * 0x2545F4914F6CDD1DULL;
}

/* ─── LE / BE writers ───────────────────────────────────────────────── */
static void put_u32le(uint8_t *b, uint32_t v) {
    b[0] = (uint8_t)v; b[1] = (uint8_t)(v >> 8);
    b[2] = (uint8_t)(v >> 16); b[3] = (uint8_t)(v >> 24);
}
static void put_u16be(uint8_t *b, uint16_t v) {
    b[0] = (uint8_t)(v >> 8); b[1] = (uint8_t)v;
}

static uint32_t ns_to_ticks(uint32_t ns, uint32_t period_ns) {
    return (ns + period_ns / 2) / period_ns;   /* rounded */
}

/* ─── generator ─────────────────────────────────────────────────────── */

uft_fe_gen_err_t uft_fe_gen_scp(const uft_fe_gen_params_t *params,
                                uft_fe_gen_scp_t *out) {
    if (!params || !out) return UFT_FE_GEN_ERR_NULL;
    memset(out, 0, sizeof(*out));

    if (params->track < 0 || params->track > UFT_FE_MAX_TRACK ||
        params->revolutions < UFT_FE_MIN_REVS ||
        params->revolutions > UFT_FE_MAX_REVS)
        return UFT_FE_GEN_ERR_OUT_OF_SPEC;

    uint32_t cell_ns = params->cell_ns ? params->cell_ns : UFT_FE_CELL_DD_NS;
    if (cell_ns < UFT_FE_MIN_NS || cell_ns > UFT_FE_MAX_NS)
        return UFT_FE_GEN_ERR_OUT_OF_SPEC;

    uint8_t jitter = 0;
    if (params->defects & UFT_FE_DEFECT_WEAK_BITS) {
        jitter = params->weak_jitter_pct;
        if (jitter < 1 || jitter > 10) return UFT_FE_GEN_ERR_OUT_OF_SPEC;
    }

    const uint8_t resolution = 0;
    const uint32_t period_ns = UFT_FE_SCP_BASE_PERIOD_NS * (1u + resolution);
    const uint32_t rev_ns    = 200000000u;                 /* 200 ms, 300 RPM */
    const uint32_t index_ticks = rev_ns / period_ns;       /* 8,000,000 @ res 0 */

    /* Build ONE revolution of flux, reused for each rev (deterministic). */
    const uint32_t rev_cell_budget = rev_ns / cell_ns + 16u;
    /* flux samples are 16-bit; cap sample value to fit (large gaps split). */
    uint16_t *cells = (uint16_t *)malloc(rev_cell_budget * sizeof(uint16_t));
    if (!cells) return UFT_FE_GEN_ERR_NOMEM;

    uint64_t rng = params->seed ? params->seed : 0xF1U;
    uint32_t ncells = 0, accum_ns = 0;
    while (accum_ns < rev_ns && ncells < rev_cell_budget) {
        uint32_t mult_num = 1, mult_den = 1;
        uint64_t r = rng_next(&rng) % 100;
        if (r < 50)      { mult_num = 1; mult_den = 1; }
        else if (r < 80) { mult_num = 3; mult_den = 2; }
        else             { mult_num = 2; mult_den = 1; }
        uint32_t this_ns = cell_ns * mult_num / mult_den;
        if (jitter) {
            int32_t span = (int32_t)(this_ns * jitter / 100);
            int32_t d = (int32_t)(rng_next(&rng) % (uint32_t)(2 * span + 1)) - span;
            this_ns = (uint32_t)((int32_t)this_ns + d);
        }
        uint32_t ticks = ns_to_ticks(this_ns, period_ns);
        if (ticks == 0) ticks = 1;
        if (ticks > 0xFFFFu) ticks = 0xFFFFu;   /* clamp to a 16-bit sample */
        cells[ncells++] = (uint16_t)ticks;
        accum_ns += this_ns;
    }

    const int revs = params->revolutions;
    /* File size: header + table + TRK hdr + revs×12 rev-entries + flux. */
    size_t flux_bytes = (size_t)ncells * revs * 2u;
    size_t track_off  = SCP_HDR_SIZE + SCP_TABLE_SIZE;
    size_t file_len   = track_off + SCP_TRK_HDR + (size_t)revs * 12u + flux_bytes;

    uint8_t *buf = (uint8_t *)calloc(1, file_len);
    if (!buf) { free(cells); return UFT_FE_GEN_ERR_NOMEM; }

    /* Header. */
    memcpy(buf, "SCP", 3);
    buf[3]  = 0x22;                       /* version 2.2 (BCD-ish) */
    buf[4]  = 0x00;                       /* disk_type */
    buf[5]  = (uint8_t)revs;              /* revolutions */
    buf[6]  = (uint8_t)params->track;     /* start_track */
    buf[7]  = (uint8_t)params->track;     /* end_track */
    buf[8]  = 0x00;                       /* flags (no footer) */
    buf[9]  = 0x00;                       /* bit_cell_width 0 => 16-bit */
    buf[10] = 0x00;                       /* heads: both */
    buf[11] = resolution;                 /* resolution */
    put_u32le(&buf[12], 0);               /* checksum (parser does not verify) */

    /* Track offset table: point the target track at track_off. */
    if (!(params->defects & UFT_FE_DEFECT_ZERO_OFFSET)) {
        put_u32le(&buf[SCP_HDR_SIZE + (size_t)params->track * 4u],
                  (uint32_t)track_off);
    } /* else leave it 0 → parser returns ERR_TRACK */

    /* TRK header. */
    uint8_t *trk = buf + track_off;
    memcpy(trk, "TRK", 3);
    trk[3] = (uint8_t)params->track;

    /* Per-rev entries + flux. data_offset is relative to track_off. */
    size_t rev_entry = track_off + SCP_TRK_HDR;
    size_t flux_cursor = rev_entry + (size_t)revs * 12u;
    for (int r = 0; r < revs; r++) {
        uint32_t data_off_rel = (uint32_t)(flux_cursor - track_off);
        put_u32le(&buf[rev_entry + 0], index_ticks);   /* index_time */
        put_u32le(&buf[rev_entry + 4], ncells);         /* track_length (flux) */
        put_u32le(&buf[rev_entry + 8], data_off_rel);   /* data_offset */
        rev_entry += 12u;
        for (uint32_t i = 0; i < ncells; i++) {
            put_u16be(&buf[flux_cursor], cells[i]);
            flux_cursor += 2u;
        }
    }
    free(cells);

    /* Corrupt-signature defects. */
    if (params->defects & UFT_FE_DEFECT_BAD_SIG)     buf[0] = 'X';
    if (params->defects & UFT_FE_DEFECT_BAD_TRK_SIG) trk[0] = 'X';

    /* Truncation defect: drop the last few flux bytes so the parser's
     * bounds check (flux_pos + length*2 > size) fails → ERR_READ. */
    if (params->defects & UFT_FE_DEFECT_TRUNCATED) {
        if (file_len > flux_bytes / 2)
            file_len -= (flux_bytes / 2 + 1);
    }

    out->bytes         = buf;
    out->bytes_len     = file_len;
    out->flux_count    = ncells;
    out->index_time_ns = index_ticks * period_ns;
    out->rpm           = 300.0;
    out->target_track  = params->track;
    return UFT_FE_GEN_OK;
}

void uft_fe_gen_free(uft_fe_gen_scp_t *s) {
    if (!s) return;
    free(s->bytes);
    s->bytes = NULL;
    s->bytes_len = 0;
}

size_t uft_fe_gen_count_unsafe(const uft_fe_gen_scp_t *s) {
    if (!s || !s->bytes) return 0;
    /* Re-read the target track's flux and range-check each interval. */
    size_t track_off = SCP_HDR_SIZE + SCP_TABLE_SIZE;
    if (s->bytes_len < track_off + SCP_TRK_HDR + 12u) return 0;
    const uint8_t resolution = s->bytes[11];
    const uint32_t period_ns = UFT_FE_SCP_BASE_PERIOD_NS * (1u + resolution);
    /* first rev entry */
    size_t rev = track_off + SCP_TRK_HDR;
    uint32_t track_length = (uint32_t)(s->bytes[rev + 4] | (s->bytes[rev + 5] << 8) |
                            (s->bytes[rev + 6] << 16) | (s->bytes[rev + 7] << 24));
    uint32_t data_off = (uint32_t)(s->bytes[rev + 8] | (s->bytes[rev + 9] << 8) |
                        (s->bytes[rev + 10] << 16) | (s->bytes[rev + 11] << 24));
    size_t flux_pos = track_off + data_off;
    size_t unsafe = 0;
    const double min_t = (double)UFT_FE_MIN_NS;
    const double max_t = (double)UFT_FE_MAX_NS;
    for (uint32_t i = 0; i < track_length; i++) {
        if (flux_pos + 2u > s->bytes_len) break;
        uint16_t raw = (uint16_t)((s->bytes[flux_pos] << 8) | s->bytes[flux_pos + 1]);
        flux_pos += 2u;
        double t_ns = (double)raw * period_ns;
        if (t_ns < min_t || t_ns > max_t) unsafe++;
    }
    return unsafe;
}

/* ─── fluxengine rpm stdout shapes ──────────────────────────────────── */

void uft_fe_format_rpm_line(double rpm, int variant, char *out, size_t cap) {
    if (!out || cap == 0) return;
    /* Whole vs fractional: keep it simple and deterministic. */
    switch (variant) {
        case 1:  snprintf(out, cap, "RPM: %.1f", rpm); break;
        case 2:  snprintf(out, cap, "rotational speed: %.0f rpm", rpm); break;
        case 0:
        default: snprintf(out, cap, "%.1f rpm", rpm); break;
    }
}
