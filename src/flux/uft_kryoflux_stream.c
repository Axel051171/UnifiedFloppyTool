/**
 * @file uft_kryoflux_stream.c
 * @brief KryoFlux stream-format decoder — implementation of uft_kf_decode().
 *
 * Decodes the KryoFlux raw stream container (the byte sequence DTC writes
 * to trackNN.S.raw) into flux transition intervals + index-pulse markers.
 *
 * STREAM FORMAT (public KryoFlux stream protocol, as documented by SPS /
 * Jean Louis-Guerin — the same encoding re-implemented in FluxEngine,
 * HxC, a2r tooling, etc.). Each byte of the cell stream is one of:
 *
 *   0x00..0x07  Flux2   — 2-byte flux value: (b0 << 8) | b1
 *   0x08        Nop1    — advance 1 byte, no flux
 *   0x09        Nop2    — advance 2 bytes, no flux
 *   0x0A        Nop3    — advance 3 bytes, no flux
 *   0x0B        Ovl16   — overflow: add 0x10000 to the *next* flux value
 *   0x0C        Flux3   — 3-byte block: flux value = (b1 << 8) | b2
 *   0x0D        OOB     — out-of-band block (not flux): type + LE16 size
 *                         + payload. Carries StreamInfo / Index /
 *                         StreamEnd / KFInfo / EOF.
 *   0x0E..0xFF  Flux1   — 1-byte flux value: b0
 *
 * Position counter: the "stream position" referenced by Index / StreamInfo
 * OOB blocks counts cell-stream bytes ONLY — OOB block bytes are *not*
 * counted (they are "out of band"). flux_positions[k] records the cell
 * position of flux value k so an Index OOB can be mapped to its cell.
 *
 * Forensic contract: this decoder NEVER fabricates a flux value. A
 * truncated or malformed stream is reported via the uft_kf_status_t
 * return; whatever flux WAS validly decoded before the fault is kept
 * (so a caller can still surface a marginal read), but nothing is
 * invented to fill the gap.
 *
 * Reference: KryoFlux stream protocol; uft_kryoflux.h (the public API
 * this file implements). License: GPL-2.0+ (matches uft_kryoflux.h).
 */
#include "uft/flux/uft_kryoflux.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Cell-stream opcode boundaries (the authoritative protocol values;
 *    see the file header). Kept local so the decoder is correct
 *    regardless of any naming drift in the public block-type enum. ── */
#define KF_OP_FLUX2_MAX   0x07u   /* 0x00..0x07 : 2-byte flux        */
#define KF_OP_NOP1        0x08u
#define KF_OP_NOP2        0x09u
#define KF_OP_NOP3        0x0Au
#define KF_OP_OVL16       0x0Bu
#define KF_OP_FLUX3       0x0Cu
#define KF_OP_OOB         0x0Du
#define KF_OP_FLUX1_MIN   0x0Eu   /* 0x0E..0xFF : 1-byte flux        */

#define KF_FLUX_INITIAL_CAP   8192u

/* ── lifecycle ────────────────────────────────────────────────────── */

uft_kf_status_t uft_kf_init(uft_kf_stream_t *stream)
{
    if (!stream) return UFT_UFT_KF_STATUS_READ_ERROR;

    memset(stream, 0, sizeof(*stream));

    stream->flux_values = (uint32_t *)malloc(KF_FLUX_INITIAL_CAP * sizeof(uint32_t));
    stream->flux_positions = (uint32_t *)malloc(KF_FLUX_INITIAL_CAP * sizeof(uint32_t));
    stream->indexes = (uft_kf_index_t *)calloc(UFT_UFT_KF_MAX_INDEX,
                                               sizeof(uft_kf_index_t));
    stream->index_internal = (uft_kf_index_internal_t *)calloc(
        UFT_UFT_KF_MAX_INDEX, sizeof(uft_kf_index_internal_t));

    if (!stream->flux_values || !stream->flux_positions ||
        !stream->indexes || !stream->index_internal) {
        uft_kf_free(stream);
        return UFT_UFT_KF_STATUS_READ_ERROR;
    }

    stream->flux_capacity = KF_FLUX_INITIAL_CAP;
    stream->sample_clock  = UFT_UFT_KF_SAMPLE_CLOCK;
    stream->index_clock   = UFT_UFT_KF_INDEX_CLOCK;
    return UFT_UFT_KF_STATUS_OK;
}

void uft_kf_free(uft_kf_stream_t *stream)
{
    if (!stream) return;
    free(stream->flux_values);
    free(stream->flux_positions);
    free(stream->indexes);
    free(stream->index_internal);
    stream->flux_values    = NULL;
    stream->flux_positions = NULL;
    stream->indexes        = NULL;
    stream->index_internal = NULL;
    stream->flux_capacity  = 0;
    stream->flux_count     = 0;
    stream->index_count    = 0;
}

void uft_kf_reset(uft_kf_stream_t *stream)
{
    if (!stream) return;
    stream->flux_count  = 0;
    stream->index_count = 0;
    stream->data_count  = 0;
    stream->data_time   = 0;
    stream->info_string[0] = '\0';
    stream->sample_clock = UFT_UFT_KF_SAMPLE_CLOCK;
    stream->index_clock  = UFT_UFT_KF_INDEX_CLOCK;
    memset(&stream->stats, 0, sizeof(stream->stats));
    if (stream->indexes)
        memset(stream->indexes, 0,
               UFT_UFT_KF_MAX_INDEX * sizeof(uft_kf_index_t));
    if (stream->index_internal)
        memset(stream->index_internal, 0,
               UFT_UFT_KF_MAX_INDEX * sizeof(uft_kf_index_internal_t));
}

/* ── flux buffer growth ───────────────────────────────────────────── */

static int kf_push_flux(uft_kf_stream_t *s, uint32_t value, uint32_t pos)
{
    if (s->flux_count >= s->flux_capacity) {
        uint32_t new_cap = s->flux_capacity ? s->flux_capacity * 2u
                                            : KF_FLUX_INITIAL_CAP;
        uint32_t *nv = (uint32_t *)realloc(s->flux_values,
                                           new_cap * sizeof(uint32_t));
        if (!nv) return -1;
        s->flux_values = nv;
        uint32_t *np = (uint32_t *)realloc(s->flux_positions,
                                           new_cap * sizeof(uint32_t));
        if (!np) return -1;
        s->flux_positions = np;
        s->flux_capacity = new_cap;
    }
    s->flux_values[s->flux_count]    = value;
    s->flux_positions[s->flux_count] = pos;
    s->flux_count++;
    return 0;
}

/* ── KFInfo string parsing (sck= / ick= override the defaults) ────── */

static void kf_parse_info(uft_kf_stream_t *s, const char *info)
{
    /* Append to the running info string (multiple KFInfo blocks appear). */
    size_t cur = strlen(s->info_string);
    size_t room = sizeof(s->info_string) - 1 - cur;
    if (room > 0) {
        strncat(s->info_string, info, room);
    }

    const char *p = strstr(info, "sck=");
    if (p) {
        double v = strtod(p + 4, NULL);
        if (v > 0.0) s->sample_clock = v;
    }
    p = strstr(info, "ick=");
    if (p) {
        double v = strtod(p + 4, NULL);
        if (v > 0.0) s->index_clock = v;
    }
}

/* ── post-decode: map Index OOB stream positions onto flux cells ──── */

static void kf_resolve_indexes(uft_kf_stream_t *s)
{
    /* Running cumulative tick sum keyed by flux cell, so we can derive
     * pre_index_time without a second allocation. */
    for (uint32_t n = 0; n < s->index_count; ++n) {
        uint32_t target_pos = s->index_internal[n].stream_pos;

        /* First flux cell whose stream position is at or after the
         * index point — that is the cell the index pulse falls in. */
        uint32_t fi = s->flux_count;          /* default: past the end */
        for (uint32_t k = 0; k < s->flux_count; ++k) {
            if (s->flux_positions[k] >= target_pos) { fi = k; break; }
        }
        s->indexes[n].flux_position = fi;

        /* rotation_time = sample-counter delta vs the previous index.
         * The first index has no predecessor — leave it 0 (the public
         * uft_kf_revolution_time_ms() helper only uses indexes >= 1). */
        if (n == 0) {
            s->indexes[n].rotation_time = 0;
        } else {
            uint32_t prev = s->index_internal[n - 1].sample_counter;
            uint32_t cur  = s->index_internal[n].sample_counter;
            s->indexes[n].rotation_time = (cur >= prev) ? (cur - prev) : 0u;
        }

        /* pre_index_time = ticks from the start of the containing flux
         * cell up to the index point. Sum flux up to (not including) fi
         * to get the cell start, then subtract from the index sample
         * counter. If anything is inconsistent, report 0 rather than a
         * fabricated value. */
        uint64_t cell_start = 0;
        for (uint32_t k = 0; k < fi && k < s->flux_count; ++k)
            cell_start += s->flux_values[k];
        uint32_t sc = s->index_internal[n].sample_counter;
        s->indexes[n].pre_index_time =
            ((uint64_t)sc >= cell_start) ? (uint32_t)((uint64_t)sc - cell_start)
                                         : 0u;
    }
}

/* ── core decode ──────────────────────────────────────────────────── */

uft_kf_status_t uft_kf_decode(uft_kf_stream_t *stream,
                              const uint8_t *data, size_t len)
{
    if (!stream || !stream->flux_values) return UFT_UFT_KF_STATUS_READ_ERROR;
    if (!data && len > 0)                return UFT_UFT_KF_STATUS_READ_ERROR;

    uft_kf_reset(stream);

    uint32_t overflow   = 0;   /* accumulated Ovl16, applied to next flux */
    uint32_t stream_pos = 0;   /* cell-stream byte position (OOB excluded) */
    uft_kf_status_t status = UFT_UFT_KF_STATUS_OK;
    int saw_stream_end = 0;

    size_t i = 0;
    while (i < len) {
        uint8_t op = data[i];

        if (op <= KF_OP_FLUX2_MAX) {
            /* Flux2: 2-byte value (op, op+1). */
            if (i + 1 >= len) { status = UFT_UFT_KF_STATUS_MISSING_DATA; break; }
            uint32_t v = ((uint32_t)op << 8) | data[i + 1];
            if (kf_push_flux(stream, v + overflow, stream_pos) != 0)
                return UFT_UFT_KF_STATUS_READ_ERROR;
            overflow = 0;
            i += 2; stream_pos += 2;

        } else if (op == KF_OP_NOP1) {
            i += 1; stream_pos += 1;

        } else if (op == KF_OP_NOP2) {
            if (i + 2 > len) { status = UFT_UFT_KF_STATUS_MISSING_DATA; break; }
            i += 2; stream_pos += 2;

        } else if (op == KF_OP_NOP3) {
            if (i + 3 > len) { status = UFT_UFT_KF_STATUS_MISSING_DATA; break; }
            i += 3; stream_pos += 3;

        } else if (op == KF_OP_OVL16) {
            overflow += 0x10000u;
            i += 1; stream_pos += 1;

        } else if (op == KF_OP_FLUX3) {
            /* Flux3: 3-byte block, flux value in bytes [i+1], [i+2]. */
            if (i + 2 >= len) { status = UFT_UFT_KF_STATUS_MISSING_DATA; break; }
            uint32_t v = ((uint32_t)data[i + 1] << 8) | data[i + 2];
            if (kf_push_flux(stream, v + overflow, stream_pos) != 0)
                return UFT_UFT_KF_STATUS_READ_ERROR;
            overflow = 0;
            i += 3; stream_pos += 3;

        } else if (op == KF_OP_OOB) {
            /* OOB: 0x0D, type, LE16 size, payload[size].
             * OOB bytes do NOT advance stream_pos. */
            if (i + 4 > len) { status = UFT_UFT_KF_STATUS_MISSING_DATA; break; }
            uint8_t  oob_type = data[i + 1];
            uint16_t oob_size = uft_kf_read_u16(&data[i + 2]);

            /* EOF marker: type 0x0D, size sentinel 0x0D0D — stream ends.
             * No payload follows. */
            if (oob_type == UFT_UFT_KF_OOB_EOF) {
                saw_stream_end = 1;
                i += 4;
                break;
            }

            if (i + 4 + (size_t)oob_size > len) {
                status = UFT_UFT_KF_STATUS_MISSING_DATA;
                break;
            }
            const uint8_t *payload = &data[i + 4];

            switch (oob_type) {
            case UFT_UFT_KF_OOB_STREAM_INFO:
                if (oob_size >= 8) {
                    stream->data_count = uft_kf_read_u32(&payload[0]);
                    stream->data_time  = uft_kf_read_u32(&payload[4]);
                }
                break;

            case UFT_UFT_KF_OOB_INDEX:
                if (oob_size >= 12 &&
                    stream->index_count < UFT_UFT_KF_MAX_INDEX) {
                    uft_kf_index_internal_t *ix =
                        &stream->index_internal[stream->index_count];
                    ix->stream_pos     = uft_kf_read_u32(&payload[0]);
                    ix->sample_counter = uft_kf_read_u32(&payload[4]);
                    ix->index_counter  = uft_kf_read_u32(&payload[8]);
                    stream->index_count++;
                }
                break;

            case UFT_UFT_KF_OOB_STREAM_END:
                saw_stream_end = 1;
                if (oob_size >= 8) {
                    uint32_t result = uft_kf_read_u32(&payload[4]);
                    if (result == UFT_UFT_KF_RESULT_BUFFERING)
                        status = UFT_UFT_KF_STATUS_DEV_BUFFER;
                    else if (result == UFT_UFT_KF_RESULT_NO_INDEX)
                        status = UFT_UFT_KF_STATUS_DEV_INDEX;
                }
                break;

            case UFT_UFT_KF_OOB_UFT_KF_INFO: {
                /* NUL-terminated key=value info string. */
                char buf[256];
                size_t n = oob_size < sizeof(buf) - 1 ? oob_size
                                                      : sizeof(buf) - 1;
                memcpy(buf, payload, n);
                buf[n] = '\0';
                kf_parse_info(stream, buf);
                break;
            }

            case UFT_UFT_KF_OOB_INVALID:
            default:
                /* Unknown OOB sub-type: skip its payload, do not abort —
                 * forward compatibility with newer DTC stream revisions. */
                break;
            }

            i += 4 + (size_t)oob_size;

        } else {
            /* 0x0E..0xFF : Flux1, single-byte value. */
            uint32_t v = op;
            if (kf_push_flux(stream, v + overflow, stream_pos) != 0)
                return UFT_UFT_KF_STATUS_READ_ERROR;
            overflow = 0;
            i += 1; stream_pos += 1;
        }
    }

    kf_resolve_indexes(stream);
    uft_uft_kf_calc_stats(stream);

    /* Clean decode that never saw a StreamEnd/EOF block is suspicious —
     * the container was likely truncated. Report MISSING_END so the
     * caller can treat the (still valid) flux it got as marginal, but
     * keep the flux: nothing was fabricated. */
    if (status == UFT_UFT_KF_STATUS_OK && !saw_stream_end)
        status = UFT_UFT_KF_STATUS_MISSING_END;

    return status;
}

uft_kf_status_t uft_kf_decode_file(uft_kf_stream_t *stream,
                                   const char *filename)
{
    if (!stream || !filename) return UFT_UFT_KF_STATUS_READ_ERROR;

    FILE *f = fopen(filename, "rb");
    if (!f) return UFT_UFT_KF_STATUS_READ_ERROR;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_UFT_KF_STATUS_READ_ERROR; }
    long size = ftell(f);
    if (size < 0) { fclose(f); return UFT_UFT_KF_STATUS_READ_ERROR; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_UFT_KF_STATUS_READ_ERROR; }

    uint8_t *buf = (uint8_t *)malloc((size_t)size ? (size_t)size : 1);
    if (!buf) { fclose(f); return UFT_UFT_KF_STATUS_READ_ERROR; }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    if (got != (size_t)size) { free(buf); return UFT_UFT_KF_STATUS_READ_ERROR; }

    uft_kf_status_t st = uft_kf_decode(stream, buf, (size_t)size);
    free(buf);
    return st;
}

/* ── statistics ───────────────────────────────────────────────────── */

void uft_uft_kf_calc_stats(uft_kf_stream_t *stream)
{
    if (!stream) return;

    uft_kf_stats_t *st = &stream->stats;
    memset(st, 0, sizeof(*st));

    if (stream->flux_count > 0) {
        uint32_t mn = stream->flux_values[0];
        uint32_t mx = stream->flux_values[0];
        for (uint32_t k = 1; k < stream->flux_count; ++k) {
            uint32_t v = stream->flux_values[k];
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        st->min_flux = mn;
        st->max_flux = mx;
    }

    /* RPM from consecutive index rotation times. */
    double sum_rpm = 0.0;
    uint32_t rev_samples = 0, rev_count = 0;
    for (uint32_t n = 1; n < stream->index_count; ++n) {
        uint32_t rt = stream->indexes[n].rotation_time;
        if (rt == 0) continue;
        double rpm = 60.0 * stream->sample_clock / (double)rt;
        if (rev_count == 0) {
            st->min_rpm = st->max_rpm = rpm;
        } else {
            if (rpm < st->min_rpm) st->min_rpm = rpm;
            if (rpm > st->max_rpm) st->max_rpm = rpm;
        }
        sum_rpm += rpm;
        rev_samples += rt;
        rev_count++;
    }
    if (rev_count > 0) {
        st->avg_rpm = sum_rpm / rev_count;
        st->flux_per_rev = stream->flux_count / rev_count;
    }
    if (stream->data_time > 0)
        st->avg_bps = (double)stream->data_count * 1000.0
                      / (double)stream->data_time;
}

/* ── histogram helpers ────────────────────────────────────────────── */

void uft_uft_kf_build_histogram(const uft_kf_stream_t *stream,
                                uint32_t *histogram, uint32_t max_value)
{
    if (!stream || !histogram || max_value == 0) return;
    memset(histogram, 0, ((size_t)max_value + 1) * sizeof(uint32_t));
    for (uint32_t k = 0; k < stream->flux_count; ++k) {
        uint32_t v = stream->flux_values[k];
        if (v <= max_value) histogram[v]++;
    }
}

int uft_kf_find_histogram_peaks(const uint32_t *histogram, size_t len,
                                uint32_t *peaks, int max_peaks)
{
    if (!histogram || !peaks || max_peaks <= 0 || len < 3) return 0;

    int found = 0;
    for (size_t v = 1; v + 1 < len && found < max_peaks; ++v) {
        /* Local maximum, and meaningfully above noise (require the peak
         * to be strictly greater than both neighbours and non-trivial). */
        if (histogram[v] > histogram[v - 1] &&
            histogram[v] >= histogram[v + 1] &&
            histogram[v] > 0) {
            peaks[found++] = (uint32_t)v;
        }
    }
    return found;
}
