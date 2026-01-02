/**
 * @file uft_pll_params.h
 * @brief PLL (Phase-Locked Loop) Parameters for flux decoding
 * 
 * Extracted from SAMdisk FluxDecoder and FluxEngine
 */

#ifndef UFT_PLL_PARAMS_H
#define UFT_PLL_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// PLL CONFIGURATION
// ============================================================================

typedef struct {
    // Clock parameters
    int bitcell_ns;           // Nominal bit cell time in nanoseconds
    int clock_min_ns;         // Minimum clock period (after adjustment)
    int clock_max_ns;         // Maximum clock period (after adjustment)
    int clock_centre_ns;      // Centre clock period
    
    // PLL tuning
    int pll_adjust_percent;   // PLL adjustment range (default: 15%)
    int pll_phase_percent;    // Phase adjustment (default: 60%)
    int flux_scale_percent;   // Flux timing scale (default: 100%)
    
    // Sync parameters
    int sync_bits_required;   // Bits required before sync (default: 256)
    int jitter_percent;       // Allowed jitter percentage (default: 2%)
    
    // Encoding-specific
    bool use_fm;              // FM encoding (double clock rate)
    bool use_gcr;             // GCR encoding
} uft_pll_params_t;

// Default PLL parameters for common encodings
static const uft_pll_params_t UFT_PLL_MFM_250K = {
    .bitcell_ns = 4000,       // 4µs = 250kbps
    .clock_min_ns = 3400,
    .clock_max_ns = 4600,
    .clock_centre_ns = 4000,
    .pll_adjust_percent = 15,
    .pll_phase_percent = 60,
    .flux_scale_percent = 100,
    .sync_bits_required = 256,
    .jitter_percent = 2,
    .use_fm = false,
    .use_gcr = false
};

static const uft_pll_params_t UFT_PLL_MFM_500K = {
    .bitcell_ns = 2000,       // 2µs = 500kbps
    .clock_min_ns = 1700,
    .clock_max_ns = 2300,
    .clock_centre_ns = 2000,
    .pll_adjust_percent = 15,
    .pll_phase_percent = 60,
    .flux_scale_percent = 100,
    .sync_bits_required = 256,
    .jitter_percent = 2,
    .use_fm = false,
    .use_gcr = false
};

static const uft_pll_params_t UFT_PLL_FM_125K = {
    .bitcell_ns = 8000,       // 8µs = 125kbps (FM)
    .clock_min_ns = 6800,
    .clock_max_ns = 9200,
    .clock_centre_ns = 8000,
    .pll_adjust_percent = 15,
    .pll_phase_percent = 60,
    .flux_scale_percent = 100,
    .sync_bits_required = 256,
    .jitter_percent = 2,
    .use_fm = true,
    .use_gcr = false
};

static const uft_pll_params_t UFT_PLL_GCR_C64 = {
    .bitcell_ns = 3200,       // Variable: 3.2-4.0µs depending on zone
    .clock_min_ns = 2700,
    .clock_max_ns = 4600,
    .clock_centre_ns = 3600,
    .pll_adjust_percent = 15,
    .pll_phase_percent = 60,
    .flux_scale_percent = 100,
    .sync_bits_required = 256,
    .jitter_percent = 2,
    .use_fm = false,
    .use_gcr = true
};

static const uft_pll_params_t UFT_PLL_APPLE_GCR = {
    .bitcell_ns = 4000,       // 4µs base
    .clock_min_ns = 3400,
    .clock_max_ns = 4600,
    .clock_centre_ns = 4000,
    .pll_adjust_percent = 15,
    .pll_phase_percent = 60,
    .flux_scale_percent = 100,
    .sync_bits_required = 256,
    .jitter_percent = 2,
    .use_fm = false,
    .use_gcr = true
};

static const uft_pll_params_t UFT_PLL_AMIGA = {
    .bitcell_ns = 2000,       // 2µs = 500kbps MFM
    .clock_min_ns = 1700,
    .clock_max_ns = 2300,
    .clock_centre_ns = 2000,
    .pll_adjust_percent = 15,
    .pll_phase_percent = 60,
    .flux_scale_percent = 100,
    .sync_bits_required = 256,
    .jitter_percent = 2,
    .use_fm = false,
    .use_gcr = false
};

// ============================================================================
// BITCELL TIMING BY FORMAT
// ============================================================================

// Common bitcell times in nanoseconds
#define UFT_BITCELL_250K    4000    // DD disks
#define UFT_BITCELL_300K    3333    // 5.25" HD in 360K drive
#define UFT_BITCELL_500K    2000    // HD disks
#define UFT_BITCELL_1M      1000    // ED disks
#define UFT_BITCELL_FM      8000    // FM single density

// ============================================================================
// PLL STATE
// ============================================================================

typedef struct {
    int clock;                // Current clock period
    int flux;                 // Accumulated flux time
    int clocked_zeros;        // Count of zero bits since last one
    int goodbits;             // Count of good bits since sync loss
    bool sync_lost;           // Sync lost flag
    bool index;               // Index pulse seen
} uft_pll_state_t;

// ============================================================================
// PLL FUNCTIONS
// ============================================================================

/**
 * @brief Initialize PLL state
 */
void uft_pll_init(uft_pll_state_t *state, const uft_pll_params_t *params);

/**
 * @brief Reset PLL to centre frequency
 */
void uft_pll_reset(uft_pll_state_t *state, const uft_pll_params_t *params);

/**
 * @brief Process next flux transition
 * @return Decoded bit (-1 if no bit, 0 or 1 otherwise)
 */
int uft_pll_process_flux(uft_pll_state_t *state, const uft_pll_params_t *params, int flux_ns);

/**
 * @brief Check if sync was lost
 */
bool uft_pll_sync_lost(uft_pll_state_t *state);

#ifdef __cplusplus
}
#endif

#endif // UFT_PLL_PARAMS_H
