/**
 * @file uft_decode_pipeline.h
 * @brief UFT Decode Pipeline — public API
 *
 * Connects: Flux → PLL → Bits → Sectors → OTDR
 * with retry logic and quality feedback.
 */

#ifndef UFT_DECODE_PIPELINE_H
#define UFT_DECODE_PIPELINE_H

#include "uft/uft_decode_session.h"
#include "uft/uft_error.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run full pipeline: Flux → PLL → Bits → Sectors → OTDR
 *
 * Session must be initialized with uft_session_init() and
 * flux_ns/flux_count set. Performs up to UFT_SESSION_MAX_RETRIES
 * attempts if format parser requests retry.
 */
uft_error_t uft_pipeline_run(uft_decode_session_t* s);

/**
 * @brief PLL stage only: Flux → Bits
 *
 * Fills s->bitstream, s->bit_count, s->pll.
 */
uft_error_t uft_pipeline_run_pll(uft_decode_session_t* s);

/**
 * @brief CRC check with OTDR reporting
 *
 * @param s         Session (NULL = standalone check, no OTDR)
 * @param data      Sector data
 * @param len       Data length
 * @param expected  Expected CRC
 * @param bit_pos   Position in bitstream (for OTDR anchoring)
 * @return true if CRC matches
 */
bool uft_pipeline_check_crc(uft_decode_session_t* s,
                             const uint8_t* data, size_t len,
                             uint16_t expected, uint32_t bit_pos);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODE_PIPELINE_H */
