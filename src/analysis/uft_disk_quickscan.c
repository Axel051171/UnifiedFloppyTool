/**
 * @file uft_disk_quickscan.c
 * @brief CHECKDISK quick-scan analyser (M2 T6).
 *
 * Pure data-model + bitstream analysis. HAL-level track-sweep is
 * separate (planned for M3 when HAL API stabilises).
 */

#include "uft/analysis/uft_disk_quickscan.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

uft_qscan_track_status_t uft_qscan_classify_track(
    uint16_t syncs_found,
    uint16_t sectors_ok,
    uint16_t sectors_crc_fail,
    uint16_t sectors_missing)
{
    if (syncs_found == 0) return UFT_QSTAT_UNREADABLE;
    if (sectors_ok == 0 && sectors_crc_fail == 0 && sectors_missing == 0) {
        return UFT_QSTAT_BLANK;
    }
    if (sectors_crc_fail > 0 || sectors_missing > 0) {
        return UFT_QSTAT_DEGRADED;
    }
    return UFT_QSTAT_GOOD;
}

/*
 * Rolling-shift sync search. Matches the hot-path pattern from
 * PERFORMANCE_REVIEW finding #1: one bit advance per iteration,
 * window extracted by truncation. O(N).
 */
static size_t find_next_sync(const uint8_t *bits,
                              size_t bit_count,
                              size_t start_bit,
                              uint16_t sync_word)
{
    if (bit_count < 16) return bit_count;
    if (start_bit + 16 > bit_count) return bit_count;

    uint32_t window = 0;
    size_t pos = start_bit;
    /* Prime window with 16 bits */
    for (int i = 0; i < 16 && pos < bit_count; i++, pos++) {
        uint32_t b = (bits[pos >> 3] >> (7 - (pos & 7))) & 1u;
        window = (window << 1) | b;
    }

    while (pos <= bit_count) {
        if ((uint16_t)window == sync_word) return pos - 16;
        if (pos == bit_count) break;
        uint32_t b = (bits[pos >> 3] >> (7 - (pos & 7))) & 1u;
        window = (window << 1) | b;
        pos++;
    }
    return bit_count;
}

/*
 * Standard CRC-16/IBM-3740 (poly 0x1021, init 0xFFFF) of a byte range.
 * We keep this local rather than pulling in uft_crc16 to stay self-
 * contained; behaviour must match reveng verification.
 */
static uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int k = 0; k < 8; k++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                  : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

/* Extract N bytes starting at bit position `start_bit` (MSB-first). */
static void extract_bytes(const uint8_t *bits, size_t bit_count,
                           size_t start_bit, uint8_t *out, size_t count)
{
    memset(out, 0, count);
    size_t need = count * 8;
    if (start_bit + need > bit_count) need = bit_count - start_bit;
    for (size_t i = 0; i < need; i++) {
        size_t p = start_bit + i;
        uint32_t b = (bits[p >> 3] >> (7 - (p & 7))) & 1u;
        out[i >> 3] |= (uint8_t)(b << (7 - (i & 7)));
    }
}

uft_error_t uft_qscan_analyse_bitstream(
    const uint8_t *bits,
    size_t bit_count,
    uint16_t sync_word,
    uint16_t expected_secs,
    uft_qscan_track_t *out)
{
    if (bits == NULL || out == NULL) return UFT_ERR_INVALID_ARG;
    if (bit_count == 0) return UFT_ERR_INVALID_ARG;

    /* Preserve caller-supplied track/side, zero the stats. */
    uint8_t saved_track = out->track_num;
    uint8_t saved_side  = out->side;
    memset(out, 0, sizeof(*out));
    out->track_num = saved_track;
    out->side      = saved_side;

    size_t pos = 0;
    uint16_t syncs = 0, ok = 0, crcfail = 0;

    while (pos + 16 <= bit_count) {
        size_t found = find_next_sync(bits, bit_count, pos, sync_word);
        if (found >= bit_count) break;
        syncs++;

        /* Skip the sync and try to read an address field:
         * 4 bytes expected (track, side, sector, size_code) + 2 CRC.
         * We verify via CRC only — we don't decode the fields. */
        size_t after_sync = found + 16;
        uint8_t addr_plus_crc[6];
        extract_bytes(bits, bit_count, after_sync, addr_plus_crc,
                       sizeof(addr_plus_crc));
        uint16_t stored_crc = (uint16_t)((addr_plus_crc[4] << 8) |
                                           addr_plus_crc[5]);
        /* Preset 0xCDB4 for A1 A1 A1 sync; here we use the MFM-unaware
         * path so just compute CRC over the 4 address bytes from the
         * standard init. Real MFM decode would need cell-by-cell PLL. */
        uint16_t computed = crc16_ccitt(addr_plus_crc, 4);
        if (computed == stored_crc) {
            ok++;
        } else {
            crcfail++;
        }

        /* Advance past what we just read — avoid matching the same
         * sync twice. Each sector frame is ~600 bits (ID + gap + data
         * + gap) as a rule-of-thumb. We skip conservatively 128 bits. */
        pos = after_sync + 128;
    }

    out->syncs_found      = syncs;
    out->sectors_ok       = ok;
    out->sectors_crc_fail = crcfail;
    /* Missing = expected - found (clamped to non-negative). */
    if (expected_secs > (ok + crcfail)) {
        out->sectors_missing = (uint16_t)(expected_secs - ok - crcfail);
    } else {
        out->sectors_missing = 0;
    }

    out->status = uft_qscan_classify_track(
        out->syncs_found,
        out->sectors_ok,
        out->sectors_crc_fail,
        out->sectors_missing);

    return UFT_OK;
}

void uft_qscan_summarise(uft_qscan_result_t *result) {
    if (result == NULL) return;

    result->total_good       = 0;
    result->total_degraded   = 0;
    result->total_unreadable = 0;
    result->total_blank      = 0;
    result->total_elapsed_ms = 0;

    size_t scanned = 0;
    for (size_t i = 0; i < result->track_count; i++) {
        uft_qscan_track_t *t = &result->tracks[i];
        if (t->status == UFT_QSTAT_UNKNOWN) continue;
        scanned++;
        result->total_elapsed_ms += t->elapsed_ms;
        switch (t->status) {
        case UFT_QSTAT_GOOD:       result->total_good++; break;
        case UFT_QSTAT_DEGRADED:   result->total_degraded++; break;
        case UFT_QSTAT_UNREADABLE: result->total_unreadable++; break;
        case UFT_QSTAT_BLANK:      result->total_blank++; break;
        default: break;
        }
    }
    if (scanned > 0) {
        result->health_percent =
            (100.0f * (float)result->total_good) / (float)scanned;
    } else {
        result->health_percent = 0.0f;
    }
}
