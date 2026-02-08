/**
 * @file uft_param_registry.h
 * @brief Centralized Parameter Registry for UFT GUI
 * 
 * Complete parameter definitions extracted from UFT GUI v1.4.0 diagnostics.
 * Provides:
 * - GUI mode definitions
 * - PLL parameter ranges
 * - DPLL phase tables (US Patent 4808884)
 * - FDC command definitions
 * - Hardware timing constants
 * 
 * @version 1.4.0
 */

#ifndef UFT_PARAM_REGISTRY_H
#define UFT_PARAM_REGISTRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include <stddef.h>

/*============================================================================
 * GUI Modes
 *============================================================================*/

/**
 * @brief GUI operation mode
 */
typedef enum {
    UFT_MODE_SIMPLE = 0,        /**< Quick copy/convert for beginners */
    UFT_MODE_FLUX,              /**< Raw flux capture with PLL tuning */
    UFT_MODE_RECOVERY,          /**< Data recovery from damaged media */
    UFT_MODE_PROTECTION         /**< Copy protection analysis */
} uft_gui_mode_t;

/**
 * @brief Mode configuration
 */
typedef struct {
    uft_gui_mode_t mode;
    const char *name;
    const char *description;
    const char *icon;
    uint32_t color;             /**< RGB color (0xRRGGBB) */
} uft_mode_config_t;

extern const uft_mode_config_t UFT_MODE_CONFIGS[4];

/*============================================================================
 * Parameter Categories
 *============================================================================*/

typedef enum {
    UFT_CAT_FORMAT = 0,
    UFT_CAT_PLL,
    UFT_CAT_CAPTURE,
    UFT_CAT_RECOVERY,
    UFT_CAT_PROTECTION,
    UFT_CAT_OUTPUT
} uft_param_category_t;

/*============================================================================
 * PLL Parameters
 *============================================================================*/

/**
 * @brief PLL configuration
 */
typedef struct {
    /* Basic PLL */
    double clock_gain;          /**< 0.01 - 0.50, default 0.05 */
    double phase_gain;          /**< 0.10 - 0.90, default 0.65 */
    double max_adjust;          /**< 0.05 - 0.30, default 0.20 */
    
    /* DPLL (Digital PLL from disktools) */
    uint16_t dpll_fast_divisor;  /**< 50-300, default 100 */
    uint16_t dpll_fast_count;    /**< 16-256, default 128 */
    double dpll_fast_tolerance;  /**< 1.0-20.0%, default 10.0 */
    uint16_t dpll_medium_divisor;/**< 100-500, default 300 */
    uint16_t dpll_slow_divisor;  /**< 200-800, default 400 */
    double dpll_slow_tolerance;  /**< 0.1-10.0%, default 2.5 */
} uft_pll_config_t;

/**
 * @brief PLL preset names
 */
typedef enum {
    UFT_PLL_DEFAULT = 0,
    UFT_PLL_AGGRESSIVE,
    UFT_PLL_CONSERVATIVE,
    UFT_PLL_WEAK_BITS,
    UFT_PLL_WIDE_TRAINING,
    UFT_PLL_NARROW_TRAINING,
    UFT_PLL_HARD_SECTOR,
    UFT_PLL_HARD_SECTOR_V2
} uft_pll_preset_t;

/**
 * @brief Get PLL preset configuration
 */
const uft_pll_config_t *uft_pll_get_preset(uft_pll_preset_t preset);

/**
 * @brief Default PLL configuration
 */
extern const uft_pll_config_t UFT_PLL_DEFAULT_CONFIG;

/*============================================================================
 * DPLL Phase Table (US Patent 4808884)
 *============================================================================*/

/**
 * @brief DPLL phase adjustment lookup table
 * 
 * From US Patent 4808884 / disktools
 * Used for phase correction during bit synchronization
 */
typedef struct {
    uint8_t c1_c2[16];          /**< Phase adjustment for condition 1/2 */
    uint8_t c3[16];             /**< Phase adjustment for condition 3 */
} uft_dpll_phase_table_t;

extern const uft_dpll_phase_table_t UFT_DPLL_PHASE_TABLE;

/*============================================================================
 * Hardware Timing
 *============================================================================*/

/**
 * @brief Drive timing constants
 */
typedef struct {
    uint32_t step_delay_us;         /**< Delay between step pulses */
    uint32_t head_settle_ms;        /**< Head settle time after seek */
    uint32_t motor_spinup_ms;       /**< Motor spinup timeout */
    uint32_t precomp_threshold_cyl; /**< Cylinder for precompensation */
    uint32_t precomp_value_ns;      /**< Precompensation amount */
} uft_drive_timing_t;

extern const uft_drive_timing_t UFT_TIMING_DD;
extern const uft_drive_timing_t UFT_TIMING_HD;

/**
 * @brief Data transfer rates
 */
typedef struct {
    uint32_t rate_code;         /**< FDC rate code */
    uint32_t bps;               /**< Bits per second */
    const char *description;
} uft_data_rate_t;

extern const uft_data_rate_t UFT_DATA_RATES[4];

/*============================================================================
 * FDC Commands
 *============================================================================*/

/**
 * @brief FDC command flags
 */
#define UFT_FDC_FLAG_READ   0x01
#define UFT_FDC_FLAG_WRITE  0x02
#define UFT_FDC_FLAG_INTR   0x04

/**
 * @brief FDC command definition
 */
typedef struct {
    const char *name;
    uint8_t code;
    uint8_t flags;
} uft_fdc_command_t;

extern const uft_fdc_command_t UFT_FDC_COMMANDS[];
extern const size_t UFT_FDC_COMMAND_COUNT;

/*============================================================================
 * MFM Constants
 *============================================================================*/

#define UFT_MFM_DATA_MASK       0x55555555
#define UFT_MFM_CLOCK_MASK      0xAAAAAAAA
#define UFT_MFM_AMIGA_SYNC      0x44894489

/**
 * @brief MFM decode: data = ((odd & mask) << 1) | (even & mask)
 */
static inline uint32_t uft_mfm_decode_long(uint32_t odd, uint32_t even) {
    return ((odd & UFT_MFM_DATA_MASK) << 1) | (even & UFT_MFM_DATA_MASK);
}

/**
 * @brief MFM encode: split data into odd/even words
 */
static inline void uft_mfm_encode_long(uint32_t data, 
                                       uint32_t *odd, 
                                       uint32_t *even) {
    *even = data & UFT_MFM_DATA_MASK;
    *odd = (data >> 1) & UFT_MFM_DATA_MASK;
}

/*============================================================================
 * CRC Constants
 *============================================================================*/

/**
 * @brief CRC-16 CCITT parameters
 */
#define UFT_CRC16_POLYNOMIAL    0x1021
#define UFT_CRC16_INIT          0xFFFF

/*============================================================================
 * Amiga Drive IDs
 *============================================================================*/

/**
 * @brief Amiga drive type IDs
 */
typedef enum {
    UFT_DRT_AMIGA = 0x00000000,     /**< Standard Amiga (DS-DD 80T) */
    UFT_DRT_37422D2S = 0x55555555,  /**< 40-track drive (DS-DD 40T) */
    UFT_DRT_EMPTY = 0xFFFFFFFF,     /**< No drive present */
    UFT_DRT_150RPM = 0xAAAAAAAA     /**< HD drive or Gotek */
} uft_amiga_drive_type_t;

/*============================================================================
 * CMOS Drive Types
 *============================================================================*/

/**
 * @brief CMOS drive type entry
 */
typedef struct {
    uint8_t cmos_type;
    const char *name;
    uint32_t rotation_us;       /**< Rotation period in µs */
    uint16_t rpm;
    uint8_t rate_dd;            /**< Rate code for DD */
    uint8_t rate_hd;            /**< Rate code for HD */
    uint8_t rate_ed;            /**< Rate code for ED */
} uft_cmos_drive_t;

extern const uft_cmos_drive_t UFT_CMOS_DRIVES[7];

/*============================================================================
 * Format Definitions
 *============================================================================*/

/**
 * @brief Output format capabilities
 */
typedef struct {
    const char *id;
    const char *name;
    const char *extensions[4];
    bool raw;                   /**< Raw track format */
    bool has_timing;            /**< Preserves timing info */
    bool has_weak;              /**< Supports weak bits */
    bool preservation;          /**< Full preservation quality */
} uft_output_format_t;

extern const uft_output_format_t UFT_OUTPUT_FORMATS[];
extern const size_t UFT_OUTPUT_FORMAT_COUNT;

/*============================================================================
 * Validation
 *============================================================================*/

/**
 * @brief Validate PLL parameter
 */
bool uft_param_validate_pll_clock_gain(double value);
bool uft_param_validate_pll_phase_gain(double value);
bool uft_param_validate_pll_max_adjust(double value);

/**
 * @brief Clamp parameter to valid range
 */
double uft_param_clamp_pll_clock_gain(double value);
double uft_param_clamp_pll_phase_gain(double value);
double uft_param_clamp_pll_max_adjust(double value);

/*============================================================================
 * Initialization
 *============================================================================*/

/**
 * @brief Initialize parameter registry
 */
void uft_param_registry_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PARAM_REGISTRY_H */
