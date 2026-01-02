#pragma once
/*
 * cpc_mfm.h — CPC MFM sector decode (IBM MFM layout)
 *
 * This module turns an MFM-encoded bitcell stream into logical sectors.
 *
 * Input representation:
 *   - "mfm_bits" is a bitstream where bit=1 means a flux transition at that
 *     bitcell boundary (i.e., the classic MFM "encoded bits" representation).
 *   - Bits are ordered MSB-first within bytes.
 *
 * Decoder strategy:
 *   - Scan the raw MFM stream for the special sync word 0x4489.
 *   - Expect 3x 0x4489, then an address mark byte (0xFE, 0xFB, 0xF8, ...).
 *   - Parse IDAM (0xFE) -> remember CHRN and N (size code).
 *   - Parse DAM (0xFB/0xF8) -> read sector payload, CRC, and emit sector.
 *
 * Notes:
 *   - Phase-1: "best effort" parsing with strict bounds checks.
 *   - Multiple revolutions / weak-bit correlation comes later.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "flux_logical.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cpc_mfm_decode_stats {
    uint32_t sync_hits;
    uint32_t idams;
    uint32_t dams;
    uint32_t sectors_emitted;
    uint32_t bad_crc_idam;
    uint32_t bad_crc_dam;
    uint32_t truncated_fields;
} cpc_mfm_decode_stats_t;

/* Decode a single track's MFM bitstream and append decoded sectors to 'li'.
 *
 * Parameters:
 *  - cyl/head: physical track position used when the stream's IDAM fields are
 *              unreliable; we still prefer CHRN from the IDAM.
 *  - want_head_match: if true, only accept IDAMs whose H matches 'head'.
 */
int cpc_mfm_decode_track_bits(
    const uint8_t *mfm_bits, size_t mfm_bit_count,
    uint16_t cyl, uint16_t head,
    bool want_head_match,
    ufm_logical_image_t *li,
    cpc_mfm_decode_stats_t *stats_out);

#ifdef __cplusplus
}
#endif
