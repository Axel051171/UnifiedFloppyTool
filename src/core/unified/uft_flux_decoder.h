/**
 * @file uft_flux_decoder.h
 * @brief Advanced Flux Decoder with FluxEngine-style PLL
 * 
 * Based on FluxEngine's proven flux decoding algorithms:
 * - Adaptive PLL with configurable parameters
 * - Multi-format sync detection
 * - Bit error tolerance handling
 * - Clock recovery from noisy data
 */

#ifndef UFT_FLUX_DECODER_H
#define UFT_FLUX_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef _WIN32
  typedef long ssize_t;
#else
  #include <sys/types.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_FLUX_NS_PER_TICK    83.333  /* 12 MHz sample rate */
#define UFT_FLUX_MAX_REVS       10
#define UFT_FLUX_PULSE          0x80
#define UFT_FLUX_INDEX          0x40

/* ═══════════════════════════════════════════════════════════════════════════════
 * PLL CONFIGURATION
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    double clock_period_ns;      /* Nominal bit cell time (ns) */
    double clock_min_ns;         /* Minimum clock period */
    double clock_max_ns;         /* Maximum clock period */
    double pll_phase;            /* Phase adjustment (0.0-1.0), default 0.65 */
    double pll_adjust;           /* Frequency adjustment (0.0-0.5), default 0.04 */
    double bit_error_threshold;  /* Bit error tolerance (0.0-0.5), default 0.2 */
    double pulse_debounce_ns;    /* Minimum pulse separation */
    double clock_interval_bias;  /* Bias for interval measurement */
    bool auto_clock;             /* Auto-detect clock from sync */
} uft_pll_config_t;

/* Default configs for common formats */
extern const uft_pll_config_t UFT_PLL_MFM_DD;   /* MFM Double Density */
extern const uft_pll_config_t UFT_PLL_MFM_HD;   /* MFM High Density */
extern const uft_pll_config_t UFT_PLL_FM;        /* FM Single Density */
extern const uft_pll_config_t UFT_PLL_GCR_C64;   /* Commodore GCR */
extern const uft_pll_config_t UFT_PLL_GCR_APPLE; /* Apple GCR */
extern const uft_pll_config_t UFT_PLL_GCR_MAC;   /* Macintosh GCR */

/* ═══════════════════════════════════════════════════════════════════════════════
 * DECODER STATE
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Configuration */
    uft_pll_config_t config;
    
    /* PLL state */
    double clock;               /* Current clock period (ns) */
    double flux_accumulator;    /* Accumulated flux time */
    uint32_t clocked_zeros;     /* Consecutive zero bits */
    uint32_t good_bits;         /* Good bits since sync loss */
    bool sync_lost;             /* Sync status */
    
    /* Statistics */
    uint64_t total_bits;
    uint64_t bad_bits;
    uint64_t sync_losses;
    double min_clock_seen;
    double max_clock_seen;
    
    /* Output buffer */
    uint8_t *output;
    size_t output_size;
    size_t output_pos;
    int bit_pos;
    uint8_t current_byte;
} uft_flux_decoder_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * DECODER RESULTS
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint8_t *data;              /* Decoded bytes */
    size_t length;              /* Number of bytes */
    double clock_ns;            /* Detected clock period */
    uint64_t total_bits;        /* Total bits processed */
    uint64_t bad_bits;          /* Bits with errors */
    double error_rate;          /* bad_bits / total_bits */
    bool valid;                 /* Decode successful */
} uft_decode_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize decoder with configuration
 */
void uft_flux_decoder_init(uft_flux_decoder_t *dec, const uft_pll_config_t *config);

/**
 * @brief Reset decoder state (keep config)
 */
void uft_flux_decoder_reset(uft_flux_decoder_t *dec);

/**
 * @brief Free decoder resources
 */
void uft_flux_decoder_free(uft_flux_decoder_t *dec);

/**
 * @brief Decode flux transitions to bits
 * @param dec Decoder state
 * @param flux Raw flux data (FluxEngine/Greaseweazle format)
 * @param flux_len Length of flux data
 * @param result Output result structure
 * @return true if decode successful
 */
bool uft_flux_decode(uft_flux_decoder_t *dec,
                     const uint8_t *flux, size_t flux_len,
                     uft_decode_result_t *result);

/**
 * @brief Process single flux interval
 * @param dec Decoder state  
 * @param interval_ns Time since last transition (nanoseconds)
 * @return Number of bits output (0-8)
 */
int uft_flux_process_interval(uft_flux_decoder_t *dec, double interval_ns);

/**
 * @brief Detect clock from sync pattern
 * @param flux Raw flux data
 * @param flux_len Length
 * @param sync_pattern Expected sync (e.g., 0x4489 for MFM)
 * @param sync_bits Number of bits in sync
 * @return Detected clock period in ns, or 0 if not found
 */
double uft_flux_detect_clock(const uint8_t *flux, size_t flux_len,
                             uint32_t sync_pattern, int sync_bits);

/**
 * @brief Find sync pattern in flux stream
 * @param dec Decoder state
 * @param flux Raw flux data
 * @param flux_len Length
 * @param sync_pattern Pattern to find
 * @param sync_bits Pattern length
 * @return Offset to sync, or -1 if not found
 */
ssize_t uft_flux_find_sync(uft_flux_decoder_t *dec,
                           const uint8_t *flux, size_t flux_len,
                           uint32_t sync_pattern, int sync_bits);

/**
 * @brief Decode MFM data (removes clock bits)
 */
size_t uft_decode_mfm(const uint8_t *encoded, size_t len, uint8_t *decoded);

/**
 * @brief Decode FM data
 */
size_t uft_decode_fm(const uint8_t *encoded, size_t len, uint8_t *decoded);

/**
 * @brief Decode GCR data (Commodore style)
 */
size_t uft_decode_gcr_c64(const uint8_t *encoded, size_t len, uint8_t *decoded);

/**
 * @brief Decode GCR data (Apple 6-and-2)
 */
size_t uft_decode_gcr_apple(const uint8_t *encoded, size_t len, uint8_t *decoded);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_DECODER_H */
