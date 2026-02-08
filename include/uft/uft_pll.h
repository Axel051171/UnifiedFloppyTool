/**
 * @file uft_pll.h
 * @brief Simple PLL-based flux-to-bit conversion.
 *
 * This is intentionally pragmatic: it stabilizes cell timing over a track and
 * turns transition intervals into a bitstream robustly enough for recovery work.
 */

#ifndef UFT_PLL_H
#define UFT_PLL_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t cell_ns;        /**< current estimated cell time */
    uint32_t cell_ns_min;    /**< clamp lower */
    uint32_t cell_ns_max;    /**< clamp upper */
    uint32_t alpha_q16;      /**< loop gain (Q16), e.g. 0.05 => 3277 */
    uint32_t max_run_cells;  /**< clamp run length to avoid bursts */
} uft_pll_cfg_t;

/**
 * @brief Conservative default PLL config for DD MFM.
 */
uft_pll_cfg_t uft_pll_cfg_default_mfm_dd(void);

/**
 * @brief Conservative default PLL config for HD MFM.
 */
uft_pll_cfg_t uft_pll_cfg_default_mfm_hd(void);

/**
 * @brief Decode flux transitions into a raw bitstream using a simple PLL.
 *
 * Input is an array of monotonically increasing timestamps (ns).
 * Output is packed bits (MSB-first per byte). The function returns the number
 * of bits written.
 */
size_t uft_flux_to_bits_pll(
    const uint64_t *timestamps_ns,
    size_t count,
    const uft_pll_cfg_t *cfg,
    uint8_t *out_bits,
    size_t out_bits_capacity_bits,
    uint32_t *out_final_cell_ns,
    size_t *out_dropped_transitions
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLL_H */
