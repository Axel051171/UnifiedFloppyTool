#pragma once
/*
 * mfm_ibm_encode.h — Minimal IBM-style MFM track synthesizer (sector -> MFM bitcells)
 *
 * Output representation matches cpc_mfm_decode_track_bits():
 *  - Bitstream contains raw MFM bitcells (clock+data), bit=1 indicates a flux
 *    transition at that bitcell boundary.
 *  - Bits are stored MSB-first within bytes.
 */

#include <stdint.h>
#include <stddef.h>

#include "flux_logical.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mfm_ibm_track_params {
    uint16_t cyl;
    uint16_t head;
    uint16_t spt;          /* sectors per track */
    uint32_t sec_size;     /* bytes */
    uint16_t bit_rate_kbps;/* 250, 300, 500 ... */
    uint16_t rpm;          /* 300 typical */
} mfm_ibm_track_params_t;

/* Estimate the nominal number of BYTES of raw bitcells per track for bitrate/rpm.
 * Example: 250kbps @300rpm -> 6250 bytes.
 */
size_t mfm_ibm_nominal_track_bytes(uint16_t bit_rate_kbps, uint16_t rpm);

/* Build an IBM MFM track bitstream from the logical sector map.
 *
 * Ownership:
 *  - On success, *out_bits points to a malloc() buffer of size ceil(out_bit_count/8).
 *  - Caller must free(*out_bits).
 */
int mfm_ibm_build_track_bits(
    const ufm_logical_image_t *li,
    const mfm_ibm_track_params_t *p,
    uint8_t **out_bits,
    size_t *out_bit_count);

#ifdef __cplusplus
}
#endif
