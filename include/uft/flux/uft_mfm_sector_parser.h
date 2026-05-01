/**
 * @file uft_mfm_sector_parser.h
 * @brief Standalone MFM IDAM/DAM sector extractor (MF-141 / AUD-002).
 *
 * The audit identified this as the largest functional gap in UFT v4.1.3:
 * the SCP/HFE/KryoFlux -> sector-image conversion pipeline existed in
 * skeleton form but had no clean MFM sector parser to call. The
 * convert_flux pipeline open-coded a partial parser inline that:
 *   - did not pair IDAM (0xFE) with the matching DAM/DDAM in the gap,
 *   - did not validate either CRC,
 *   - placed sectors in scan order, not by IBM sector ID R.
 *
 * This API provides a forensically honest, single-call sector decoder
 * over an arbitrary MFM-encoded bitstream. Callers feed in a packed
 * bitstream (MSB-first, the format produced by the existing
 * uft_pll_process_flux_mfm() PLL output) plus a caller-owned buffer
 * pool, and receive back an array of decoded sectors with explicit
 * CRC-validity flags.
 *
 * Forensic contract:
 *   - Sectors with bad CRC are still returned (with the corresponding
 *     flag set false), never silently dropped or rewritten. The caller
 *     decides whether to use, retry, or report them.
 *   - Sectors are NOT placed by R-position into a backing image — the
 *     caller does that. This API stays at "what was on the track"
 *     level; image-layout decisions are the conversion-pipeline's
 *     concern.
 *   - No data is fabricated to fill gaps. If a DAM is missing after a
 *     successful IDAM, the entry is returned with `data_len == 0`.
 *
 * Encoding contract:
 *   - The input bitstream is the raw MFM bitstream (i.e. the
 *     interleaved clock+data stream the PLL produces). The decoder
 *     is responsible for skipping the clock bits and re-assembling
 *     bytes from the data bits.
 *   - The 0xA1 sync mark with the missing-clock pattern is detected
 *     by its bit-level signature 0x4489 (clock+data interleaved).
 *
 * Not in scope (future work):
 *   - FM (single-density) decoding — separate API needed.
 *   - GCR (CBM/Apple) — separate API needed.
 *   - Track-write back-encoding — this is decoder-only.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef UFT_MFM_SECTOR_PARSER_H
#define UFT_MFM_SECTOR_PARSER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Decoded MFM sector record.
 *
 * One entry per IDAM seen on the track, regardless of CRC validity.
 * The caller inspects the `*_crc_ok` flags to decide what to trust.
 */
typedef struct {
    uint8_t  cylinder;     /**< CHRN.C — track number from IDAM */
    uint8_t  head;         /**< CHRN.H — head number from IDAM */
    uint8_t  sector;       /**< CHRN.R — sector ID (1-based, IBM) */
    uint8_t  size_code;    /**< CHRN.N — 0=128, 1=256, 2=512, 3=1024 */
    bool     id_crc_ok;    /**< true if IDAM CRC matched */
    bool     data_crc_ok;  /**< true if DAM/DDAM CRC matched (false if no DAM) */
    bool     deleted;      /**< true if DAM was 0xF8 (Deleted DAM) */
    bool     dam_present;  /**< false if no DAM was found within the gap window */
    size_t   data_offset;  /**< byte offset into caller's data pool */
    size_t   data_len;     /**< actual data length: 1 << (7 + size_code), or 0 */
} uft_mfm_sector_t;

/**
 * @brief Decoder configuration. Pass NULL to use defaults.
 */
typedef struct {
    /** Maximum bytes between IDAM and matching DAM. IBM/PC default = 43,
     *  Amiga formats can be tighter, weird-protected disks may have
     *  larger. 0 means "use default 43". */
    uint16_t dam_search_window_bytes;
    /** When true, accept N=0..6 (128..8192 byte sectors). When false,
     *  reject anything not in {0,1,2,3} (i.e. 128..1024). Defaults to
     *  strict (false) since N>3 is almost always a CRC-corrupted IDAM
     *  pretending to be a giant sector — accepting it would make the
     *  decoder OOM on bad data. */
    bool     accept_extended_size;
} uft_mfm_decoder_opts_t;

/**
 * @brief Decode all MFM sectors from a track bitstream.
 *
 * @param bitstream     Packed MFM bits, MSB-first. Output of the PLL.
 * @param bit_count     Total valid bits in @p bitstream (not bytes).
 * @param data_pool     Caller-owned buffer of size @p data_pool_size,
 *                      receives sector payloads back-to-back.
 * @param data_pool_size Size of @p data_pool in bytes. The decoder
 *                      stops adding sectors when the pool is full;
 *                      already-found IDAMs whose data wouldn't fit
 *                      are returned with `dam_present = false` and
 *                      `data_len = 0`.
 * @param sectors       Caller-owned array of size @p max_sectors,
 *                      receives the decoded sector records.
 * @param max_sectors   Capacity of @p sectors.
 * @param opts          Decoder options (NULL = defaults).
 * @return Number of sectors written to @p sectors (i.e. number of
 *         IDAMs successfully parsed). Note: this is NOT the number
 *         of CRC-good sectors — caller must inspect flags.
 */
size_t uft_mfm_decode_track(
    const uint8_t *bitstream,
    size_t bit_count,
    uint8_t *data_pool,
    size_t data_pool_size,
    uft_mfm_sector_t *sectors,
    size_t max_sectors,
    const uft_mfm_decoder_opts_t *opts);

/**
 * @brief Convert IBM size code N (0..7) to byte length.
 *
 * size_code N -> 128 << N bytes. N=3 = 512 (most common).
 * Returns 0 for N > 7 (defensive).
 */
size_t uft_mfm_sector_size_from_code(uint8_t size_code);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_SECTOR_PARSER_H */
