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

/* ============================================================================
 * Canonical PLL API (System B)
 *
 * Two PLL systems coexist in UFT:
 *   System A (C++): src/flux/fdc_bitstream/vfo_*.cpp (mfm_codec, VFO_TYPE_PID3)
 *   System B (C):   src/core/uft_pll_unified.c (preset-based, this API)
 *
 * The uft_pll_cfg_t / uft_flux_to_bits_pll above is a standalone helper.
 * The opaque context API below is for preset-based PLL processing.
 * ============================================================================ */

typedef enum {
    UFT_PLL_PRESET_AUTO = 0,    /**< Auto-detect from flux data */
    UFT_PLL_PRESET_IBM_DD,      /**< IBM PC DD: 250 kbps MFM */
    UFT_PLL_PRESET_IBM_HD,      /**< IBM PC HD: 500 kbps MFM */
    UFT_PLL_PRESET_AMIGA_DD,    /**< Amiga DD: 250 kbps MFM */
    UFT_PLL_PRESET_AMIGA_HD,    /**< Amiga HD: 500 kbps MFM */
    UFT_PLL_PRESET_C64,         /**< Commodore 64: ~250 kbps GCR */
    UFT_PLL_PRESET_APPLE2,      /**< Apple II: ~250 kbps GCR */
    UFT_PLL_PRESET_MAC_400K,    /**< Macintosh 400K: variable GCR */
    UFT_PLL_PRESET_MAC_800K,    /**< Macintosh 800K: variable GCR */
    UFT_PLL_PRESET_ATARI_ST,    /**< Atari ST: 250 kbps MFM */
    UFT_PLL_PRESET_FM_SD,       /**< FM Single Density: 125 kbps */
    UFT_PLL_PRESET_COUNT
} uft_pll_preset_t;

typedef enum {
    UFT_PLL_ALGO_PI = 0,       /**< PI controller (standard) */
    UFT_PLL_ALGO_ADAPTIVE,     /**< Adaptive gain control */
    UFT_PLL_ALGO_DPLL,         /**< Digital PLL */
    UFT_PLL_ALGO_COUNT
} uft_pll_algo_t;

/** Opaque PLL context — implementation in uft_pll_unified.c */
typedef struct uft_pll_context uft_pll_ctx_t;

uft_pll_ctx_t* uft_pll_create(uft_pll_preset_t preset);
void           uft_pll_destroy(uft_pll_ctx_t* ctx);
int            uft_pll_process_flux(uft_pll_ctx_t* ctx,
                                    uint32_t flux_ns,
                                    uint8_t* bits_out,
                                    int* bit_count_out);
void           uft_pll_reset(uft_pll_ctx_t* ctx);
double         uft_pll_get_cell_size_ns(const uft_pll_ctx_t* ctx);
bool           uft_pll_is_locked(const uft_pll_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLL_H */
