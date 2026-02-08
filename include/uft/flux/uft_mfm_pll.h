/**
 * @file uft_mfm_pll.h
 * @brief MFM Phase-Locked Loop Decoder
 * 
 * Based on Glasgow project (software MFM decoder)
 * Implements digital PLL for flux timing recovery
 * 
 * Reference: INTEL 82077AA CHMOS Single-Chip Floppy Disk Controller
 */

#ifndef UFT_MFM_PLL_H
#define UFT_MFM_PLL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * PLL Configuration
 *===========================================================================*/

/**
 * @brief PLL parameters
 */
typedef struct {
    uint32_t nco_init_period;   /**< Initial NCO period (0 = auto-detect) */
    uint32_t nco_min_period;    /**< Minimum NCO period */
    uint32_t nco_max_period;    /**< Maximum NCO period */
    uint8_t  nco_frac_bits;     /**< Fractional bits for NCO (precision) */
    uint8_t  pll_kp_exp;        /**< Proportional gain exponent */
    uint8_t  pll_gph_exp;       /**< Phase gain exponent */
} uft_pll_params_t;

/** Default PLL parameters (from Glasgow) */
#define UFT_PLL_DEFAULT_NCO_INIT     0
#define UFT_PLL_DEFAULT_NCO_MIN      16
#define UFT_PLL_DEFAULT_NCO_MAX      256
#define UFT_PLL_DEFAULT_FRAC_BITS    8
#define UFT_PLL_DEFAULT_KP_EXP       2
#define UFT_PLL_DEFAULT_GPH_EXP      1

/**
 * @brief Initialize default PLL parameters
 */
static inline void uft_pll_params_default(uft_pll_params_t *params)
{
    params->nco_init_period = UFT_PLL_DEFAULT_NCO_INIT;
    params->nco_min_period  = UFT_PLL_DEFAULT_NCO_MIN;
    params->nco_max_period  = UFT_PLL_DEFAULT_NCO_MAX;
    params->nco_frac_bits   = UFT_PLL_DEFAULT_FRAC_BITS;
    params->pll_kp_exp      = UFT_PLL_DEFAULT_KP_EXP;
    params->pll_gph_exp     = UFT_PLL_DEFAULT_GPH_EXP;
}

/*===========================================================================
 * PLL State
 *===========================================================================*/

/**
 * @brief PLL state machine
 */
typedef struct {
    /* NCO (Numerically Controlled Oscillator) */
    int32_t  nco_period;        /**< Current NCO period (fractional) */
    int32_t  nco_phase;         /**< Current NCO phase */
    int32_t  nco_step;          /**< NCO step size (1 << frac_bits) */
    
    /* PLL feedback */
    int32_t  pll_error;         /**< Phase error */
    int32_t  pll_feedback;      /**< Feedback value */
    
    /* Current bit state */
    uint8_t  bit_current;       /**< Current output bit */
    
    /* Configuration */
    uft_pll_params_t params;
} uft_pll_state_t;

/**
 * @brief Initialize PLL state
 */
void uft_pll_init(uft_pll_state_t *pll, const uft_pll_params_t *params);

/**
 * @brief Process one edge through PLL
 * @param pll PLL state
 * @param has_edge true if flux edge detected
 * @param bit_out Output: recovered bit (if valid)
 * @return true if a bit was output
 */
bool uft_pll_process_edge(uft_pll_state_t *pll, bool has_edge, uint8_t *bit_out);

/**
 * @brief Reset PLL to initial state
 */
void uft_pll_reset(uft_pll_state_t *pll);

/*===========================================================================
 * Edge Detection from Bytestream
 *===========================================================================*/

/**
 * @brief Edge bytestream context
 * 
 * Glasgow format: each byte represents delay until next edge
 * Value 0xFD indicates continuation (add to next byte)
 */
typedef struct {
    const uint8_t *data;        /**< Input bytestream */
    size_t length;              /**< Bytestream length */
    size_t position;            /**< Current position */
    uint16_t edge_len;          /**< Accumulated edge length */
} uft_edge_stream_t;

/**
 * @brief Initialize edge stream
 */
static inline void uft_edge_stream_init(uft_edge_stream_t *stream,
                                         const uint8_t *data, size_t length)
{
    stream->data = data;
    stream->length = length;
    stream->position = 0;
    stream->edge_len = 0;
}

/**
 * @brief Get next edge length
 * @return Edge length, or 0 if end of stream
 */
uint16_t uft_edge_stream_next(uft_edge_stream_t *stream);

/*===========================================================================
 * Bit Stream from Bytestream
 *===========================================================================*/

/**
 * @brief Bit stream context
 * 
 * Converts edge bytestream to raw bit stream
 */
typedef struct {
    const uint8_t *data;        /**< Input bytestream */
    size_t length;              /**< Bytestream length */
    size_t position;            /**< Current position */
    uint8_t prev_byte;          /**< Previous byte value */
    uint8_t zeros_remaining;    /**< Zeros to emit before next edge */
} uft_bit_stream_t;

/**
 * @brief Initialize bit stream
 */
void uft_bit_stream_init(uft_bit_stream_t *stream, const uint8_t *data, size_t length);

/**
 * @brief Get next bit
 * @param stream Bit stream context
 * @param bit_out Output bit
 * @return true if bit available, false if end of stream
 */
bool uft_bit_stream_next(uft_bit_stream_t *stream, uint8_t *bit_out);

/*===========================================================================
 * MFM Sync Detection
 *===========================================================================*/

/**
 * A1 sync pattern (with missing clock):
 * Bit pattern: 10100001
 * MFM pattern: 01 00 01 00 10 00 10 01
 * 
 * The "10 00" at position 4-5 is a clock violation used for sync
 */
#define UFT_MFM_SYNC_A1_PATTERN     0x4489  /**< MFM encoded A1 (16 bits) */
#define UFT_MFM_SYNC_A1_MASK        0xFFFF

/**
 * C2 sync pattern (for Index Address Mark):
 * Bit pattern: 11000010
 * MFM pattern: 01 01 00 10 00 10 01 00
 */
#define UFT_MFM_SYNC_C2_PATTERN     0x5224  /**< MFM encoded C2 (16 bits) */
#define UFT_MFM_SYNC_C2_MASK        0xFFFF

/*===========================================================================
 * MFM Demodulator
 *===========================================================================*/

/** Demodulator sync state */
typedef enum {
    UFT_DEMOD_IDLE,             /**< Searching for sync */
    UFT_DEMOD_SYNCED            /**< Synchronized, decoding data */
} uft_demod_state_t;

/**
 * @brief MFM demodulator context
 */
typedef struct {
    /* Shift register for sync detection */
    uint16_t shreg;             /**< 16-bit shift register */
    uint8_t  shreg_count;       /**< Bits in shift register */
    
    /* State */
    uft_demod_state_t state;
    uint8_t  prev_bit;          /**< Previous decoded bit (for MFM rules) */
    
    /* Current byte assembly */
    uint8_t  current_byte;
    uint8_t  bit_count;
    
    /* Statistics */
    uint32_t offset;            /**< Current chip offset */
    uint32_t sync_count;        /**< Number of syncs detected */
    uint32_t desync_count;      /**< Number of desyncs */
} uft_demod_ctx_t;

/**
 * @brief Demodulator output
 */
typedef struct {
    uint8_t  data;              /**< Decoded byte */
    bool     is_sync;           /**< true if this is a sync byte (K.A1) */
    bool     valid;             /**< true if output is valid */
} uft_demod_output_t;

/**
 * @brief Initialize demodulator
 */
void uft_demod_init(uft_demod_ctx_t *ctx);

/**
 * @brief Process one chip (bit cell) through demodulator
 * @param ctx Demodulator context
 * @param chip Input chip (0 or 1)
 * @param out Output structure
 * @return true if output is valid
 */
bool uft_demod_process_chip(uft_demod_ctx_t *ctx, uint8_t chip, uft_demod_output_t *out);

/**
 * @brief Reset demodulator to idle state
 */
void uft_demod_reset(uft_demod_ctx_t *ctx);

/*===========================================================================
 * High-Level API
 *===========================================================================*/

/**
 * @brief Decode MFM track from raw flux data
 * 
 * Complete pipeline: edges -> bits -> PLL -> demod -> sectors
 * 
 * @param flux_data Raw flux bytestream (Glasgow format)
 * @param flux_len Length of flux data
 * @param params PLL parameters (NULL for defaults)
 * @param output Output buffer for decoded bytes
 * @param output_len Output buffer size
 * @param sync_positions Output: positions of sync bytes (optional)
 * @param max_syncs Maximum syncs to record
 * @return Number of bytes decoded
 */
size_t uft_mfm_decode_track(const uint8_t *flux_data, size_t flux_len,
                            const uft_pll_params_t *params,
                            uint8_t *output, size_t output_len,
                            uint32_t *sync_positions, size_t max_syncs);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_PLL_H */
