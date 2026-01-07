/**
 * @file uft_gw_official_params.h
 * 
 * 
 * This header contains the OFFICIAL parameter values from the reference
 */

#ifndef UFT_GW_OFFICIAL_PARAMS_H
#define UFT_GW_OFFICIAL_PARAMS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * PLL PRESETS (from track.py)
 *============================================================================*/

/**
 * PLL Configuration Types
 * 
 * AGGRESSIVE: Quick sync to extreme bit timings
 *             Good for: long tracks, variable-rate tracks
 * 
 * CONSERVATIVE: Ignores noise in well-behaved tracks
 *               Good for: high-frequency noise, dirt, mould
 */

typedef struct {
    const char *name;
    uint8_t period_adj_pct;     /* Period adjustment percentage */
    uint8_t phase_adj_pct;      /* Phase adjustment percentage */
    float lowpass_thresh_us;    /* Lowpass threshold in µs (0 = disabled) */
} uft_pll_preset_t;

static const uft_pll_preset_t UFT_PLL_PRESETS[] = {
    {
        .name = "Aggressive (Default)",
        .period_adj_pct = 5,
        .phase_adj_pct = 60,
        .lowpass_thresh_us = 0
    },
    {
        .name = "Conservative (Noisy Disks)",
        .period_adj_pct = 1,
        .phase_adj_pct = 10,
        .lowpass_thresh_us = 0
    },
    {
        .name = "Custom",
        .period_adj_pct = 0,
        .phase_adj_pct = 0,
        .lowpass_thresh_us = 0
    }
};

#define UFT_PLL_PRESET_COUNT (sizeof(UFT_PLL_PRESETS) / sizeof(UFT_PLL_PRESETS[0]))

/*============================================================================
 * PRECOMPENSATION (from track.py)
 *============================================================================*/

typedef enum {
    UFT_PRECOMP_MFM = 0,
    UFT_PRECOMP_FM  = 1,
    UFT_PRECOMP_GCR = 2
} uft_precomp_type_t;

static const char* const UFT_PRECOMP_TYPE_NAMES[] = {
    "MFM", "FM", "GCR"
};

/**
 * Precompensation patterns from track.py
 * 
 * MFM patterns:
 *   10100 -> shift bits 2,3 (reduce/increase)
 *   00101 -> shift bits 2,3 (increase/reduce)
 * 
 * GCR/FM patterns (adjacent 1s):
 *   110 -> shift bits 1,2
 *   011 -> shift bits 1,2
 */

typedef struct {
    uft_precomp_type_t type;
    float ns;                   /* Precompensation in nanoseconds */
    bool enabled;
} uft_precomp_config_t;

/* Default precompensation values */
#define UFT_PRECOMP_MFM_DEFAULT_NS      140.0f
#define UFT_PRECOMP_FM_DEFAULT_NS       0.0f
#define UFT_PRECOMP_GCR_DEFAULT_NS      0.0f

/*============================================================================
 * FM/MFM GAP PARAMETERS (from ibm.py)
 *============================================================================*/

/**
 * Gap parameters for IBM-compatible formats
 */
typedef struct {
    uint8_t gap1;       /* Post IAM gap */
    uint8_t gap2;       /* Post IDAM gap */
    uint8_t gap3[8];    /* Post DAM gap (indexed by sector size code) */
    uint8_t gap4a;      /* Post Index gap */
} uft_gap_params_t;

static const uft_gap_params_t UFT_FM_GAPS = {
    .gap1 = 26,
    .gap2 = 11,
    .gap3 = { 27, 42, 58, 138, 255, 255, 255, 255 },
    .gap4a = 40
};

static const uft_gap_params_t UFT_MFM_GAPS = {
    .gap1 = 50,
    .gap2 = 22,
    .gap3 = { 32, 54, 84, 116, 255, 255, 255, 255 },
    .gap4a = 80
};

/*============================================================================
 * SECTOR INTERLEAVE (from ibm.py)
 *============================================================================*/

/**
 * Sector interleave configuration
 * 
 * interleave: Logical sector spacing
 * cskew: Cylinder skew (sectors to skip per cylinder)
 * hskew: Head skew (sectors to skip per head)
 */
typedef struct {
    uint8_t interleave;
    uint8_t cskew;
    uint8_t hskew;
} uft_interleave_config_t;

/* Default values */
#define UFT_INTERLEAVE_DEFAULT  1
#define UFT_CSKEW_DEFAULT       0
#define UFT_HSKEW_DEFAULT       0

/*============================================================================
 * TIMING PARAMETERS (from usb.py)
 *============================================================================*/

/**
 * Flux timing thresholds
 */
#define UFT_NFA_THRESH_US       150.0f  /* No-flux-area threshold in µs */
#define UFT_NFA_PERIOD_US       1.25f   /* No-flux-area period in µs */
#define UFT_DUMMY_FLUX_US       100.0f  /* Dummy flux for write end */

/**
 * Sample frequency handling
 */
static inline float uft_ticks_to_us(uint32_t ticks, uint32_t sample_freq) {
    return (float)ticks * 1e6f / sample_freq;
}

static inline uint32_t uft_us_to_ticks(float us, uint32_t sample_freq) {
    return (uint32_t)(us * sample_freq / 1e6f);
}

/*============================================================================
 * AMIGA PARAMETERS (from amigados.py)
 *============================================================================*/

typedef struct {
    uint8_t nsec;               /* Sectors per track */
    float clock;                /* Bit clock in seconds */
    float time_per_rev;         /* Time per revolution (0.2s = 300 RPM) */
    float verify_revs;          /* Revolutions for verification */
} uft_amiga_params_t;

/* AmigaDOS DD */
static const uft_amiga_params_t UFT_AMIGA_DD = {
    .nsec = 11,
    .clock = 2e-6f,             /* 2µs = 500kbit/s MFM = 250kbit/s data */
    .time_per_rev = 0.2f,       /* 200ms = 300 RPM */
    .verify_revs = 1.1f
};

/* AmigaDOS HD */
static const uft_amiga_params_t UFT_AMIGA_HD = {
    .nsec = 22,
    .clock = 1e-6f,             /* 1µs = 1Mbit/s MFM = 500kbit/s data */
    .time_per_rev = 0.2f,
    .verify_revs = 1.1f
};

/*============================================================================
 * IBM FORMAT PRESETS (from ibm.py)
 *============================================================================*/

typedef enum {
    UFT_IBM_MODE_FM,
    UFT_IBM_MODE_MFM,
    UFT_IBM_MODE_DEC_RX02
} uft_ibm_mode_t;

typedef struct {
    const char *name;
    uft_ibm_mode_t mode;
    uint8_t cyls;
    uint8_t heads;
    uint8_t secs;
    uint8_t sec_size;           /* Size code: 0=128, 1=256, 2=512, 3=1024 */
    uint16_t rpm;
    uint16_t data_rate;         /* kbit/s */
    float clock;                /* Bit clock in seconds */
} uft_ibm_format_t;

/* Common IBM formats */
static const uft_ibm_format_t UFT_IBM_FORMATS[] = {
    /* PC 3.5" formats */
    { "PC 720K DD",     UFT_IBM_MODE_MFM, 80, 2, 9,  2, 300, 250, 4e-6f },
    { "PC 1.44M HD",    UFT_IBM_MODE_MFM, 80, 2, 18, 2, 300, 500, 2e-6f },
    { "PC 2.88M ED",    UFT_IBM_MODE_MFM, 80, 2, 36, 2, 300, 1000, 1e-6f },
    
    /* PC 5.25" formats */
    { "PC 360K DD",     UFT_IBM_MODE_MFM, 40, 2, 9,  2, 300, 250, 4e-6f },
    { "PC 1.2M HD",     UFT_IBM_MODE_MFM, 80, 2, 15, 2, 360, 500, 2e-6f },
    
    /* Atari ST */
    { "Atari ST DD",    UFT_IBM_MODE_MFM, 80, 2, 9,  2, 300, 250, 4e-6f },
    { "Atari ST HD",    UFT_IBM_MODE_MFM, 80, 2, 18, 2, 300, 500, 2e-6f },
    
    /* FM formats */
    { "SD 8\" SSSD",    UFT_IBM_MODE_FM,  77, 1, 26, 0, 360, 250, 4e-6f },
    { "SD 5.25\" SSSD", UFT_IBM_MODE_FM,  40, 1, 10, 1, 300, 125, 8e-6f },
    
    /* DEC RX02 */
    { "DEC RX02",       UFT_IBM_MODE_DEC_RX02, 77, 1, 26, 1, 360, 250, 4e-6f },
};

#define UFT_IBM_FORMAT_COUNT (sizeof(UFT_IBM_FORMATS) / sizeof(UFT_IBM_FORMATS[0]))

/*============================================================================
 * READ/WRITE PARAMETERS
 *============================================================================*/

/**
 * Read parameters
 */
typedef struct {
    uint8_t revs;               /* Revolutions to read */
    bool index_cued;            /* Cue reading at index */
    uint8_t retries;            /* Retry count */
    bool verify_writes;         /* Verify after write */
} uft_rw_params_t;

/* Defaults */
#define UFT_DEFAULT_REVS        2
#define UFT_AMIGA_DEFAULT_REVS  1.1f
#define UFT_DEFAULT_RETRIES     3

/*============================================================================
 * DRIVE DELAYS (from usb.py)
 *============================================================================*/

/**
 * Drive timing delays
 * 
 * These can be configured via "gw delays" command
 */
typedef struct {
    uint16_t step_delay_us;     /* Step delay in µs (default: 3000) */
    uint16_t settle_delay_ms;   /* Head settle delay in ms (default: 15) */
    uint16_t motor_delay_ms;    /* Motor spin-up delay in ms (default: 500) */
    uint8_t auto_off_secs;      /* Auto motor-off in seconds (default: 10) */
} uft_drive_delays_t;

static const uft_drive_delays_t UFT_DRIVE_DELAYS_DEFAULT = {
    .step_delay_us = 3000,      /* 3ms per step */
    .settle_delay_ms = 15,      /* 15ms head settle */
    .motor_delay_ms = 500,      /* 500ms motor spin-up */
    .auto_off_secs = 10         /* 10s auto-off */
};

/*============================================================================
 * MASTER TRACK PARAMETERS (from track.py)
 *============================================================================*/

typedef struct {
    uint32_t splice;            /* Splice position in bitcells */
    bool force_random_weak;     /* Randomize weak bits on write */
    /* Weak ranges: list of (start, length) pairs */
} uft_master_track_params_t;

/*============================================================================
 * GUI MAPPING STRUCTURE
 *============================================================================*/

/**
 * Complete GUI parameter structure for UFT
 * Maps to Qt widgets
 */
typedef struct {
    /* PLL Tab */
    uint8_t pll_preset_index;
    uint8_t pll_period_adj;
    uint8_t pll_phase_adj;
    float pll_lowpass_thresh;
    
    /* Precompensation Tab */
    bool precomp_enabled;
    uft_precomp_type_t precomp_type;
    float precomp_ns;
    
    /* Format Tab */
    uint8_t format_preset_index;
    uft_ibm_mode_t encoding_mode;
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint8_t sector_size_code;
    uint16_t rpm;
    uint16_t data_rate;
    
    /* Gaps Tab */
    uint8_t gap1;
    uint8_t gap2;
    uint8_t gap3;
    uint8_t gap4a;
    
    /* Interleave Tab */
    uint8_t interleave;
    uint8_t cskew;
    uint8_t hskew;
    
    /* Drive Tab */
    uint16_t step_delay;
    uint16_t settle_delay;
    uint16_t motor_delay;
    uint8_t auto_off;
    
    /* Read/Write Tab */
    uint8_t revs;
    bool index_cued;
    uint8_t retries;
    bool verify;
    
} uft_gui_params_complete_t;

/**
 * Initialize GUI parameters with defaults
 */
static inline void uft_gui_params_init(uft_gui_params_complete_t *params) {
    /* PLL */
    params->pll_preset_index = 0;
    params->pll_period_adj = UFT_PLL_PRESETS[0].period_adj_pct;
    params->pll_phase_adj = UFT_PLL_PRESETS[0].phase_adj_pct;
    params->pll_lowpass_thresh = 0;
    
    /* Precomp */
    params->precomp_enabled = true;
    params->precomp_type = UFT_PRECOMP_MFM;
    params->precomp_ns = UFT_PRECOMP_MFM_DEFAULT_NS;
    
    /* Format - PC 1.44M default */
    params->format_preset_index = 1;
    params->encoding_mode = UFT_IBM_MODE_MFM;
    params->cylinders = 80;
    params->heads = 2;
    params->sectors = 18;
    params->sector_size_code = 2;
    params->rpm = 300;
    params->data_rate = 500;
    
    /* Gaps - MFM defaults */
    params->gap1 = UFT_MFM_GAPS.gap1;
    params->gap2 = UFT_MFM_GAPS.gap2;
    params->gap3 = UFT_MFM_GAPS.gap3[2];  /* 512-byte sectors */
    params->gap4a = UFT_MFM_GAPS.gap4a;
    
    /* Interleave */
    params->interleave = UFT_INTERLEAVE_DEFAULT;
    params->cskew = UFT_CSKEW_DEFAULT;
    params->hskew = UFT_HSKEW_DEFAULT;
    
    /* Drive */
    params->step_delay = UFT_DRIVE_DELAYS_DEFAULT.step_delay_us;
    params->settle_delay = UFT_DRIVE_DELAYS_DEFAULT.settle_delay_ms;
    params->motor_delay = UFT_DRIVE_DELAYS_DEFAULT.motor_delay_ms;
    params->auto_off = UFT_DRIVE_DELAYS_DEFAULT.auto_off_secs;
    
    /* Read/Write */
    params->revs = UFT_DEFAULT_REVS;
    params->index_cued = true;
    params->retries = UFT_DEFAULT_RETRIES;
    params->verify = true;
}

/**
 * Apply format preset to parameters
 */
static inline void uft_gui_apply_format_preset(uft_gui_params_complete_t *params,
                                                uint8_t preset_index) {
    if (preset_index >= UFT_IBM_FORMAT_COUNT) return;
    
    const uft_ibm_format_t *fmt = &UFT_IBM_FORMATS[preset_index];
    params->format_preset_index = preset_index;
    params->encoding_mode = fmt->mode;
    params->cylinders = fmt->cyls;
    params->heads = fmt->heads;
    params->sectors = fmt->secs;
    params->sector_size_code = fmt->sec_size;
    params->rpm = fmt->rpm;
    params->data_rate = fmt->data_rate;
    
    /* Apply appropriate gaps */
    const uft_gap_params_t *gaps = (fmt->mode == UFT_IBM_MODE_FM) 
                                    ? &UFT_FM_GAPS : &UFT_MFM_GAPS;
    params->gap1 = gaps->gap1;
    params->gap2 = gaps->gap2;
    params->gap3 = gaps->gap3[fmt->sec_size];
    params->gap4a = gaps->gap4a;
}

/**
 * Apply PLL preset to parameters
 */
static inline void uft_gui_apply_pll_preset(uft_gui_params_complete_t *params,
                                             uint8_t preset_index) {
    if (preset_index >= UFT_PLL_PRESET_COUNT) return;
    
    const uft_pll_preset_t *pll = &UFT_PLL_PRESETS[preset_index];
    params->pll_preset_index = preset_index;
    params->pll_period_adj = pll->period_adj_pct;
    params->pll_phase_adj = pll->phase_adj_pct;
    params->pll_lowpass_thresh = pll->lowpass_thresh_us;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_GW_OFFICIAL_PARAMS_H */
