/**
 * @file uft_pattern_generator.h
 * @brief Test Pattern Generation for Flux Analysis
 * 
 * 
 * Generates various flux test patterns for:
 * - Media tolerance testing
 * - Head alignment verification
 * - PLL stress testing
 * - Encoder/decoder validation
 * 
 * Pattern types:
 * - Random: Uniformly distributed intervals
 * - PRBS7/PRBS15: Pseudo-random bit sequences (LFSR-based)
 * - Alternating: Regular toggle patterns
 * - Run-length: Variable run patterns
 * - Chirp: Frequency sweep
 * - DC Bias: Phase-modulated patterns
 * - Burst: Periodic noise bursts
 */

#ifndef UFT_PATTERN_GENERATOR_H
#define UFT_PATTERN_GENERATOR_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

/** Revolution time constants (nanoseconds) */
#define UFT_PATTERN_REV_NS_300      200000000   /**< 300 RPM */
#define UFT_PATTERN_REV_NS_360      166666667   /**< 360 RPM */

/** Standard bit cell times */
#define UFT_PATTERN_CELL_DD_NS      4000    /**< DD: 4µs (250 kbps) */
#define UFT_PATTERN_CELL_HD_NS      2000    /**< HD: 2µs (500 kbps) */
#define UFT_PATTERN_CELL_ED_NS      1000    /**< ED: 1µs (1 Mbps) */

/** LFSR polynomial taps */
#define UFT_LFSR_TAPS_7     {7, 6}      /**< x^7 + x^6 + 1 */
#define UFT_LFSR_TAPS_15    {15, 14}    /**< x^15 + x^14 + 1 */
#define UFT_LFSR_TAPS_23    {23, 18}    /**< x^23 + x^18 + 1 */
#define UFT_LFSR_TAPS_31    {31, 28}    /**< x^31 + x^28 + 1 */

/*===========================================================================
 * Pattern Types
 *===========================================================================*/

/**
 * @brief Pattern type enumeration
 */
typedef enum {
    UFT_PATTERN_RANDOM,         /**< Uniformly random intervals */
    UFT_PATTERN_PRBS7,          /**< 7-bit LFSR pseudo-random */
    UFT_PATTERN_PRBS15,         /**< 15-bit LFSR pseudo-random */
    UFT_PATTERN_PRBS23,         /**< 23-bit LFSR pseudo-random */
    UFT_PATTERN_PRBS31,         /**< 31-bit LFSR pseudo-random */
    UFT_PATTERN_ALT,            /**< Alternating short/long */
    UFT_PATTERN_RUNLEN,         /**< Variable run lengths */
    UFT_PATTERN_CHIRP,          /**< Frequency chirp/sweep */
    UFT_PATTERN_DC_BIAS,        /**< DC bias modulation */
    UFT_PATTERN_BURST,          /**< Periodic noise bursts */
    UFT_PATTERN_MFM_CLOCK,      /**< MFM clock pattern (0x4E) */
    UFT_PATTERN_MFM_SYNC,       /**< MFM sync pattern (A1A1A1) */
    UFT_PATTERN_GCR_SYNC,       /**< GCR sync pattern */
    UFT_PATTERN_CUSTOM          /**< User-defined pattern */
} uft_pattern_type_t;

/*===========================================================================
 * Data Structures
 *===========================================================================*/

/**
 * @brief LFSR state for PRBS generation
 */
typedef struct {
    uint32_t state;             /**< Current LFSR state */
    uint8_t order;              /**< LFSR order (7, 15, 23, 31) */
    uint8_t tap1;               /**< First tap position */
    uint8_t tap2;               /**< Second tap position */
} uft_lfsr_t;

/**
 * @brief Pattern generation configuration
 */
typedef struct {
    uft_pattern_type_t type;    /**< Pattern type */
    uint32_t base_cell_ns;      /**< Base bit cell time in ns */
    double rpm;                 /**< Drive RPM (300 or 360) */
    uint32_t seed;              /**< Random seed (0 = use time) */
    
    /* Pattern-specific parameters */
    union {
        struct {
            uint8_t runlen;     /**< ALT: toggle every N bits */
        } alt;
        
        struct {
            uint8_t max_len;    /**< RUNLEN: maximum run length */
        } runlen;
        
        struct {
            uint32_t start_ns;  /**< CHIRP: start cell time */
            uint32_t end_ns;    /**< CHIRP: end cell time */
        } chirp;
        
        struct {
            double bias;        /**< DC_BIAS: -0.5 to +0.5 */
        } dc_bias;
        
        struct {
            uint16_t period;    /**< BURST: period in cells */
            double duty;        /**< BURST: duty cycle 0-1 */
            double noise;       /**< BURST: noise level 0-1 */
        } burst;
    } params;
} uft_pattern_config_t;

/**
 * @brief Generated pattern data
 */
typedef struct {
    uint32_t *intervals;        /**< Flux intervals in ns */
    size_t count;               /**< Number of intervals */
    size_t capacity;            /**< Allocated capacity */
    uint16_t revolutions;       /**< Number of revolutions */
    double actual_density;      /**< Achieved bits/revolution */
} uft_pattern_data_t;

/*===========================================================================
 * LFSR Functions
 *===========================================================================*/

/**
 * @brief Initialize LFSR for PRBS generation
 * @param lfsr LFSR state
 * @param order LFSR order (7, 15, 23, or 31)
 * @param seed Initial seed (0 = default)
 */
void uft_lfsr_init(uft_lfsr_t *lfsr, uint8_t order, uint32_t seed);

/**
 * @brief Get next LFSR bit
 * @param lfsr LFSR state
 * @return Next pseudo-random bit (0 or 1)
 */
static inline uint8_t uft_lfsr_next_bit(uft_lfsr_t *lfsr)
{
    uint8_t bit = lfsr->state & 1;
    uint32_t fb = ((lfsr->state >> (lfsr->tap1 - 1)) ^
                   (lfsr->state >> (lfsr->tap2 - 1))) & 1;
    lfsr->state = (lfsr->state >> 1) | (fb << (lfsr->order - 1));
    return bit;
}

/**
 * @brief Get next LFSR byte
 * @param lfsr LFSR state
 * @return 8 pseudo-random bits
 */
static inline uint8_t uft_lfsr_next_byte(uft_lfsr_t *lfsr)
{
    uint8_t byte = 0;
    for (int i = 0; i < 8; i++) {
        byte = (byte << 1) | uft_lfsr_next_bit(lfsr);
    }
    return byte;
}

/*===========================================================================
 * Pattern Generation Functions
 *===========================================================================*/

/**
 * @brief Initialize pattern configuration with defaults
 * @param config Configuration to initialize
 * @param type Pattern type
 */
void uft_pattern_config_init(uft_pattern_config_t *config,
                             uft_pattern_type_t type);

/**
 * @brief Allocate pattern data
 * @param revolutions Number of revolutions
 * @param rpm Drive RPM
 * @param base_cell_ns Base cell time
 * @return Allocated pattern data or NULL
 */
uft_pattern_data_t *uft_pattern_alloc(uint16_t revolutions, double rpm,
                                       uint32_t base_cell_ns);

/**
 * @brief Free pattern data
 * @param data Pattern data to free
 */
void uft_pattern_free(uft_pattern_data_t *data);

/**
 * @brief Generate pattern
 * @param config Pattern configuration
 * @param revolutions Number of revolutions
 * @param data Output pattern data (pre-allocated)
 * @return 0 on success
 */
int uft_pattern_generate(const uft_pattern_config_t *config,
                         uint16_t revolutions, uft_pattern_data_t *data);

/**
 * @brief Generate random pattern
 * @param data Output pattern data
 * @param base_cell_ns Base cell time
 * @param rpm Drive RPM
 * @param seed Random seed
 */
void uft_pattern_gen_random(uft_pattern_data_t *data, uint32_t base_cell_ns,
                            double rpm, uint32_t seed);

/**
 * @brief Generate PRBS pattern
 * @param data Output pattern data
 * @param order LFSR order (7, 15, 23, 31)
 * @param base_cell_ns Base cell time
 * @param rpm Drive RPM
 * @param seed Random seed
 */
void uft_pattern_gen_prbs(uft_pattern_data_t *data, uint8_t order,
                          uint32_t base_cell_ns, double rpm, uint32_t seed);

/**
 * @brief Generate alternating pattern
 * @param data Output pattern data
 * @param base_cell_ns Base cell time
 * @param rpm Drive RPM
 * @param runlen Toggle every N bits
 */
void uft_pattern_gen_alt(uft_pattern_data_t *data, uint32_t base_cell_ns,
                         double rpm, uint8_t runlen);

/**
 * @brief Generate chirp pattern (frequency sweep)
 * @param data Output pattern data
 * @param start_ns Starting cell time
 * @param end_ns Ending cell time
 * @param rpm Drive RPM
 */
void uft_pattern_gen_chirp(uft_pattern_data_t *data, uint32_t start_ns,
                           uint32_t end_ns, double rpm);

/**
 * @brief Generate burst pattern
 * @param data Output pattern data
 * @param base_cell_ns Base cell time
 * @param rpm Drive RPM
 * @param period Burst period in cells
 * @param duty Duty cycle (0-1)
 * @param noise Noise level (0-1)
 */
void uft_pattern_gen_burst(uft_pattern_data_t *data, uint32_t base_cell_ns,
                           double rpm, uint16_t period, double duty,
                           double noise);

/*===========================================================================
 * Normalization Functions
 *===========================================================================*/

/**
 * @brief Calculate bits per revolution
 * @param base_cell_ns Base cell time
 * @param rpm Drive RPM
 * @return Bits per revolution
 */
static inline uint32_t uft_pattern_bits_per_rev(uint32_t base_cell_ns, double rpm)
{
    uint64_t rev_time = (rpm == 300.0) ? UFT_PATTERN_REV_NS_300 
                                       : UFT_PATTERN_REV_NS_360;
    return (uint32_t)(rev_time / (base_cell_ns > 0 ? base_cell_ns : 1));
}

/**
 * @brief Normalize revolution to target time
 * @param intervals Interval array (modified in place)
 * @param count Current count
 * @param capacity Maximum capacity
 * @param rpm Drive RPM
 * @param base_cell_ns Base cell time for padding
 * @return New count after normalization
 * 
 * Pads or trims to approximately one revolution time.
 */
size_t uft_pattern_normalize_rev(uint32_t *intervals, size_t count,
                                 size_t capacity, double rpm,
                                 uint32_t base_cell_ns);

/**
 * @brief Add jitter/noise to pattern
 * @param intervals Interval array (modified in place)
 * @param count Number of intervals
 * @param jitter_percent Jitter as percentage of mean (0-100)
 * @param seed Random seed
 */
void uft_pattern_add_jitter(uint32_t *intervals, size_t count,
                            double jitter_percent, uint32_t seed);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get pattern type name
 * @param type Pattern type
 * @return Human-readable name
 */
const char *uft_pattern_type_name(uft_pattern_type_t type);

/**
 * @brief Verify pattern quality
 * @param data Pattern data
 * @param config Original configuration
 * @return Quality score 0-100
 */
int uft_pattern_verify(const uft_pattern_data_t *data,
                       const uft_pattern_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PATTERN_GENERATOR_H */
