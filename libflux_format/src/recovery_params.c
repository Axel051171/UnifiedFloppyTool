// SPDX-License-Identifier: MIT
/*
 * recovery_params.c - Recovery Parameter Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-01
 */

#include "recovery_params.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/*============================================================================*
 * DEFAULT INITIALIZATION
 *============================================================================*/

void recovery_config_init(recovery_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    strncpy(config->name, "Default", sizeof(config->name) - 1);
    
    /* MFM Timing defaults */
    config->timing.timing_4us = MFM_TIMING_4US_DEFAULT;
    config->timing.timing_6us = MFM_TIMING_6US_DEFAULT;
    config->timing.timing_8us = MFM_TIMING_8US_DEFAULT;
    config->timing.threshold_offset = MFM_OFFSET_DEFAULT;
    config->timing.is_high_density = false;
    
    /* Adaptive defaults */
    config->adaptive.enabled = true;
    config->adaptive.rate_of_change = ADAPTIVE_RATE_DEFAULT;
    config->adaptive.lowpass_radius = ADAPTIVE_LOWPASS_DEFAULT;
    config->adaptive.warmup_samples = ADAPTIVE_WARMUP_DEFAULT;
    config->adaptive.max_drift = ADAPTIVE_DRIFT_DEFAULT;
    config->adaptive.lock_on_success = false;
    
    /* PLL defaults */
    config->pll.enabled = true;
    config->pll.gain = PLL_GAIN_DEFAULT;
    config->pll.phase_tolerance = PLL_PHASE_TOL_DEFAULT;
    config->pll.freq_tolerance = PLL_FREQ_TOL_DEFAULT;
    config->pll.reset_on_sync = true;
    config->pll.soft_pll = true;
    
    /* Error correction defaults */
    config->error_correction.enabled = true;
    config->error_correction.max_bit_flips = EC_MAX_FLIPS_DEFAULT;
    config->error_correction.search_region_size = EC_REGION_DEFAULT;
    config->error_correction.timeout_ms = EC_TIMEOUT_DEFAULT;
    config->error_correction.try_single_first = true;
    config->error_correction.use_multi_capture = true;
    config->error_correction.min_captures = EC_CAPTURES_DEFAULT;
    
    /* Retry defaults */
    config->retry.max_retries = RETRY_MAX_DEFAULT;
    config->retry.retry_delay_ms = RETRY_DELAY_DEFAULT;
    config->retry.seek_retry = true;
    config->retry.seek_distance = RETRY_SEEK_DEFAULT;
    config->retry.vary_speed = false;
    config->retry.speed_variation = RETRY_SPEED_VAR_DEFAULT;
    config->retry.progressive_relax = true;
    
    /* Analysis defaults */
    config->analysis.generate_histogram = true;
    config->analysis.generate_entropy = true;
    config->analysis.generate_scatter = false;
    config->analysis.scatter_start = 0;
    config->analysis.scatter_end = 10000;
    config->analysis.log_level = ANALYSIS_LOG_INFO;
    config->analysis.save_raw_flux = false;
    
    config->active_format = 0;
}

/*============================================================================*
 * PRESET CONFIGURATIONS
 *============================================================================*/

static const char *preset_names[] = {
    "Default",
    "Fast",
    "Thorough",
    "Aggressive",
    "Gentle",
    "Amiga Standard",
    "Amiga Damaged",
    "PC Standard",
    "PC Damaged",
    "Custom"
};

const char *recovery_preset_name(recovery_preset_t preset) {
    if (preset >= 0 && preset < PRESET_COUNT) {
        return preset_names[preset];
    }
    return "Unknown";
}

void recovery_config_load_preset(recovery_config_t *config, recovery_preset_t preset) {
    if (!config) return;
    
    /* Start with defaults */
    recovery_config_init(config);
    
    switch (preset) {
        case PRESET_FAST:
            strncpy(config->name, "Fast", sizeof(config->name) - 1);
            config->adaptive.enabled = false;
            config->error_correction.enabled = false;
            config->retry.max_retries = 2;
            config->analysis.generate_entropy = false;
            config->analysis.generate_scatter = false;
            break;
            
        case PRESET_THOROUGH:
            strncpy(config->name, "Thorough", sizeof(config->name) - 1);
            config->adaptive.lowpass_radius = 64;
            config->error_correction.max_bit_flips = 4;
            config->error_correction.timeout_ms = 10000;
            config->retry.max_retries = 10;
            config->retry.seek_retry = true;
            config->analysis.generate_scatter = true;
            break;
            
        case PRESET_AGGRESSIVE:
            strncpy(config->name, "Aggressive", sizeof(config->name) - 1);
            config->adaptive.rate_of_change = 2.0f;
            config->adaptive.max_drift = 15;
            config->pll.gain = 0.1f;
            config->pll.phase_tolerance = 0.6f;
            config->error_correction.max_bit_flips = 6;
            config->error_correction.search_region_size = 100;
            config->error_correction.timeout_ms = 30000;
            config->retry.max_retries = 20;
            config->retry.vary_speed = true;
            config->retry.speed_variation = 2.0f;
            break;
            
        case PRESET_GENTLE:
            strncpy(config->name, "Gentle", sizeof(config->name) - 1);
            config->adaptive.rate_of_change = 0.5f;
            config->adaptive.lowpass_radius = 128;
            config->pll.gain = 0.02f;
            config->retry.max_retries = 3;
            config->retry.retry_delay_ms = 500;
            config->retry.vary_speed = false;
            break;
            
        case PRESET_AMIGA_STANDARD:
            strncpy(config->name, "Amiga Standard", sizeof(config->name) - 1);
            config->timing.timing_4us = 20;
            config->timing.timing_6us = 30;
            config->timing.timing_8us = 40;
            config->active_format = 1;
            config->format_params.amiga.format = 1;  /* AmigaDOS */
            config->format_params.amiga.max_track = 79;
            break;
            
        case PRESET_AMIGA_DAMAGED:
            strncpy(config->name, "Amiga Damaged", sizeof(config->name) - 1);
            config->timing.timing_4us = 20;
            config->timing.timing_6us = 30;
            config->timing.timing_8us = 40;
            config->adaptive.rate_of_change = 1.5f;
            config->adaptive.max_drift = 12;
            config->error_correction.max_bit_flips = 5;
            config->error_correction.timeout_ms = 15000;
            config->retry.max_retries = 15;
            config->active_format = 1;
            config->format_params.amiga.format = 0;  /* Auto-detect */
            config->format_params.amiga.ignore_header_errors = true;
            config->format_params.amiga.read_extended_tracks = true;
            config->format_params.amiga.max_track = 82;
            break;
            
        case PRESET_PC_STANDARD:
            strncpy(config->name, "PC Standard", sizeof(config->name) - 1);
            config->active_format = 2;
            config->format_params.pc.format = 0;  /* Auto */
            config->format_params.pc.accept_deleted = true;
            break;
            
        case PRESET_PC_DAMAGED:
            strncpy(config->name, "PC Damaged", sizeof(config->name) - 1);
            config->adaptive.rate_of_change = 1.5f;
            config->error_correction.max_bit_flips = 4;
            config->retry.max_retries = 10;
            config->active_format = 2;
            config->format_params.pc.format = 0;
            config->format_params.pc.ignore_header_crc = true;
            config->format_params.pc.accept_deleted = true;
            break;
            
        case PRESET_DEFAULT:
        case PRESET_CUSTOM:
        default:
            /* Already initialized with defaults */
            break;
    }
}

/*============================================================================*
 * VALIDATION
 *============================================================================*/

int recovery_config_validate(const recovery_config_t *config) {
    if (!config) return -1;
    
    /* Check timing parameters */
    if (config->timing.timing_4us < MFM_TIMING_4US_MIN ||
        config->timing.timing_4us > MFM_TIMING_4US_MAX) return 1;
    if (config->timing.timing_6us < MFM_TIMING_6US_MIN ||
        config->timing.timing_6us > MFM_TIMING_6US_MAX) return 2;
    if (config->timing.timing_8us < MFM_TIMING_8US_MIN ||
        config->timing.timing_8us > MFM_TIMING_8US_MAX) return 3;
    
    /* Check timing order: 4us < 6us < 8us */
    if (config->timing.timing_4us >= config->timing.timing_6us) return 4;
    if (config->timing.timing_6us >= config->timing.timing_8us) return 5;
    
    /* Check adaptive parameters */
    if (config->adaptive.rate_of_change < ADAPTIVE_RATE_MIN ||
        config->adaptive.rate_of_change > ADAPTIVE_RATE_MAX) return 10;
    if (config->adaptive.lowpass_radius < ADAPTIVE_LOWPASS_MIN ||
        config->adaptive.lowpass_radius > ADAPTIVE_LOWPASS_MAX) return 11;
    
    /* Check PLL parameters */
    if (config->pll.gain < PLL_GAIN_MIN ||
        config->pll.gain > PLL_GAIN_MAX) return 20;
    if (config->pll.phase_tolerance < PLL_PHASE_TOL_MIN ||
        config->pll.phase_tolerance > PLL_PHASE_TOL_MAX) return 21;
    
    /* Check error correction parameters */
    if (config->error_correction.max_bit_flips < EC_MAX_FLIPS_MIN ||
        config->error_correction.max_bit_flips > EC_MAX_FLIPS_MAX) return 30;
    
    return 0;
}

/*============================================================================*
 * COPY
 *============================================================================*/

void recovery_config_copy(recovery_config_t *dst, const recovery_config_t *src) {
    if (!dst || !src) return;
    memcpy(dst, src, sizeof(*dst));
}

/*============================================================================*
 * FILE I/O (Simple Key=Value format)
 *============================================================================*/

int recovery_config_save(const recovery_config_t *config, const char *filename) {
    if (!config || !filename) return -1;
    
    FILE *f = fopen(filename, "w");
    if (!f) return -2;
    
    fprintf(f, "# UnifiedFloppyTool Recovery Configuration\n");
    fprintf(f, "# Version 1.0\n\n");
    
    fprintf(f, "[General]\n");
    fprintf(f, "name=%s\n\n", config->name);
    
    fprintf(f, "[Timing]\n");
    fprintf(f, "timing_4us=%d\n", config->timing.timing_4us);
    fprintf(f, "timing_6us=%d\n", config->timing.timing_6us);
    fprintf(f, "timing_8us=%d\n", config->timing.timing_8us);
    fprintf(f, "threshold_offset=%d\n", config->timing.threshold_offset);
    fprintf(f, "is_high_density=%d\n\n", config->timing.is_high_density ? 1 : 0);
    
    fprintf(f, "[Adaptive]\n");
    fprintf(f, "enabled=%d\n", config->adaptive.enabled ? 1 : 0);
    fprintf(f, "rate_of_change=%.2f\n", config->adaptive.rate_of_change);
    fprintf(f, "lowpass_radius=%d\n", config->adaptive.lowpass_radius);
    fprintf(f, "warmup_samples=%d\n", config->adaptive.warmup_samples);
    fprintf(f, "max_drift=%d\n", config->adaptive.max_drift);
    fprintf(f, "lock_on_success=%d\n\n", config->adaptive.lock_on_success ? 1 : 0);
    
    fprintf(f, "[PLL]\n");
    fprintf(f, "enabled=%d\n", config->pll.enabled ? 1 : 0);
    fprintf(f, "gain=%.3f\n", config->pll.gain);
    fprintf(f, "phase_tolerance=%.2f\n", config->pll.phase_tolerance);
    fprintf(f, "freq_tolerance=%.1f\n", config->pll.freq_tolerance);
    fprintf(f, "reset_on_sync=%d\n", config->pll.reset_on_sync ? 1 : 0);
    fprintf(f, "soft_pll=%d\n\n", config->pll.soft_pll ? 1 : 0);
    
    fprintf(f, "[ErrorCorrection]\n");
    fprintf(f, "enabled=%d\n", config->error_correction.enabled ? 1 : 0);
    fprintf(f, "max_bit_flips=%d\n", config->error_correction.max_bit_flips);
    fprintf(f, "search_region_size=%d\n", config->error_correction.search_region_size);
    fprintf(f, "timeout_ms=%d\n", config->error_correction.timeout_ms);
    fprintf(f, "try_single_first=%d\n", config->error_correction.try_single_first ? 1 : 0);
    fprintf(f, "use_multi_capture=%d\n", config->error_correction.use_multi_capture ? 1 : 0);
    fprintf(f, "min_captures=%d\n\n", config->error_correction.min_captures);
    
    fprintf(f, "[Retry]\n");
    fprintf(f, "max_retries=%d\n", config->retry.max_retries);
    fprintf(f, "retry_delay_ms=%d\n", config->retry.retry_delay_ms);
    fprintf(f, "seek_retry=%d\n", config->retry.seek_retry ? 1 : 0);
    fprintf(f, "seek_distance=%d\n", config->retry.seek_distance);
    fprintf(f, "vary_speed=%d\n", config->retry.vary_speed ? 1 : 0);
    fprintf(f, "speed_variation=%.1f\n", config->retry.speed_variation);
    fprintf(f, "progressive_relax=%d\n\n", config->retry.progressive_relax ? 1 : 0);
    
    fprintf(f, "[Analysis]\n");
    fprintf(f, "generate_histogram=%d\n", config->analysis.generate_histogram ? 1 : 0);
    fprintf(f, "generate_entropy=%d\n", config->analysis.generate_entropy ? 1 : 0);
    fprintf(f, "generate_scatter=%d\n", config->analysis.generate_scatter ? 1 : 0);
    fprintf(f, "log_level=%d\n", config->analysis.log_level);
    fprintf(f, "save_raw_flux=%d\n", config->analysis.save_raw_flux ? 1 : 0);
    
    fclose(f);
    return 0;
}

/*============================================================================*
 * GUI WIDGET DESCRIPTIONS
 *============================================================================*/

static const char *log_level_options[] = {"None", "Errors", "Info", "Debug", NULL};
static const char *amiga_format_options[] = {"Auto", "AmigaDOS", "DiskSpare", "PFS", NULL};
static const char *pc_format_options[] = {"Auto", "DD 720K", "HD 1.44M", "DD 360K", "HD 1.2M", NULL};

static const param_widget_desc_t widget_descriptions[] = {
    /* MFM Timing Group */
    {
        .name = "timing_4us",
        .label = "4µs Threshold",
        .tooltip = "Timing threshold for short (4µs) pulses. Lower for slower motors.",
        .group = "MFM Timing",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = MFM_TIMING_4US_MIN,
        .max_val = MFM_TIMING_4US_MAX,
        .default_val = MFM_TIMING_4US_DEFAULT,
        .step = 1,
        .unit = "ticks"
    },
    {
        .name = "timing_6us",
        .label = "6µs Threshold",
        .tooltip = "Timing threshold for medium (6µs) pulses.",
        .group = "MFM Timing",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = MFM_TIMING_6US_MIN,
        .max_val = MFM_TIMING_6US_MAX,
        .default_val = MFM_TIMING_6US_DEFAULT,
        .step = 1,
        .unit = "ticks"
    },
    {
        .name = "timing_8us",
        .label = "8µs Threshold",
        .tooltip = "Timing threshold for long (8µs) pulses. Higher for faster motors.",
        .group = "MFM Timing",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = MFM_TIMING_8US_MIN,
        .max_val = MFM_TIMING_8US_MAX,
        .default_val = MFM_TIMING_8US_DEFAULT,
        .step = 1,
        .unit = "ticks"
    },
    {
        .name = "threshold_offset",
        .label = "Threshold Offset",
        .tooltip = "Global offset applied to all thresholds. Adjust for disk speed.",
        .group = "MFM Timing",
        .widget_type = WIDGET_SLIDER_INT,
        .min_val = MFM_OFFSET_MIN,
        .max_val = MFM_OFFSET_MAX,
        .default_val = MFM_OFFSET_DEFAULT,
        .step = 1,
        .unit = "ticks"
    },
    {
        .name = "is_high_density",
        .label = "High Density",
        .tooltip = "Enable for HD (1.44MB/1.2MB) disks.",
        .group = "MFM Timing",
        .widget_type = WIDGET_CHECKBOX
    },
    
    /* Adaptive Group */
    {
        .name = "adaptive_enabled",
        .label = "Enable Adaptive",
        .tooltip = "Automatically adjust thresholds during read.",
        .group = "Adaptive Processing",
        .widget_type = WIDGET_CHECKBOX
    },
    {
        .name = "rate_of_change",
        .label = "Adaptation Rate",
        .tooltip = "How quickly thresholds adapt. Higher = faster but less stable.",
        .group = "Adaptive Processing",
        .widget_type = WIDGET_SLIDER_FLOAT,
        .min_val = ADAPTIVE_RATE_MIN,
        .max_val = ADAPTIVE_RATE_MAX,
        .default_val = ADAPTIVE_RATE_DEFAULT,
        .step = ADAPTIVE_RATE_STEP,
        .unit = "x"
    },
    {
        .name = "lowpass_radius",
        .label = "Filter Window",
        .tooltip = "Number of samples for low-pass averaging. Higher = smoother.",
        .group = "Adaptive Processing",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = ADAPTIVE_LOWPASS_MIN,
        .max_val = ADAPTIVE_LOWPASS_MAX,
        .default_val = ADAPTIVE_LOWPASS_DEFAULT,
        .step = 1,
        .unit = "samples"
    },
    {
        .name = "max_drift",
        .label = "Max Drift",
        .tooltip = "Maximum threshold drift allowed from initial values.",
        .group = "Adaptive Processing",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = 1,
        .max_val = 20,
        .default_val = ADAPTIVE_DRIFT_DEFAULT,
        .step = 1,
        .unit = "ticks"
    },
    
    /* PLL Group */
    {
        .name = "pll_enabled",
        .label = "Enable PLL",
        .tooltip = "Use phase-locked loop for bit synchronization.",
        .group = "PLL",
        .widget_type = WIDGET_CHECKBOX
    },
    {
        .name = "pll_gain",
        .label = "PLL Gain",
        .tooltip = "How aggressively PLL tracks phase errors. Higher = faster lock.",
        .group = "PLL",
        .widget_type = WIDGET_SLIDER_FLOAT,
        .min_val = PLL_GAIN_MIN,
        .max_val = PLL_GAIN_MAX,
        .default_val = PLL_GAIN_DEFAULT,
        .step = PLL_GAIN_STEP,
        .unit = ""
    },
    {
        .name = "phase_tolerance",
        .label = "Phase Tolerance",
        .tooltip = "How much phase error before resync. Higher = more forgiving.",
        .group = "PLL",
        .widget_type = WIDGET_SLIDER_FLOAT,
        .min_val = PLL_PHASE_TOL_MIN,
        .max_val = PLL_PHASE_TOL_MAX,
        .default_val = PLL_PHASE_TOL_DEFAULT,
        .step = 0.05,
        .unit = "bits"
    },
    
    /* Error Correction Group */
    {
        .name = "ec_enabled",
        .label = "Enable Error Correction",
        .tooltip = "Try to correct bad sectors by flipping bits.",
        .group = "Error Correction",
        .widget_type = WIDGET_CHECKBOX
    },
    {
        .name = "max_bit_flips",
        .label = "Max Bit Flips",
        .tooltip = "Maximum bits to try flipping. WARNING: >4 is very slow!",
        .group = "Error Correction",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = EC_MAX_FLIPS_MIN,
        .max_val = EC_MAX_FLIPS_MAX,
        .default_val = EC_MAX_FLIPS_DEFAULT,
        .step = 1,
        .unit = "bits"
    },
    {
        .name = "search_region",
        .label = "Search Region",
        .tooltip = "Size of region to search for errors.",
        .group = "Error Correction",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = EC_REGION_MIN,
        .max_val = EC_REGION_MAX,
        .default_val = EC_REGION_DEFAULT,
        .step = 10,
        .unit = "bits"
    },
    {
        .name = "ec_timeout",
        .label = "Timeout",
        .tooltip = "Maximum time for error correction attempt.",
        .group = "Error Correction",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = EC_TIMEOUT_MIN,
        .max_val = EC_TIMEOUT_MAX,
        .default_val = EC_TIMEOUT_DEFAULT,
        .step = 1000,
        .unit = "ms"
    },
    
    /* Retry Group */
    {
        .name = "max_retries",
        .label = "Max Retries",
        .tooltip = "Number of read attempts per sector.",
        .group = "Retry",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = RETRY_MAX_MIN,
        .max_val = RETRY_MAX_MAX,
        .default_val = RETRY_MAX_DEFAULT,
        .step = 1,
        .unit = ""
    },
    {
        .name = "retry_delay",
        .label = "Retry Delay",
        .tooltip = "Wait time between retries.",
        .group = "Retry",
        .widget_type = WIDGET_SPINBOX_INT,
        .min_val = RETRY_DELAY_MIN,
        .max_val = RETRY_DELAY_MAX,
        .default_val = RETRY_DELAY_DEFAULT,
        .step = 50,
        .unit = "ms"
    },
    {
        .name = "seek_retry",
        .label = "Seek on Retry",
        .tooltip = "Seek away and back to reposition head on retry.",
        .group = "Retry",
        .widget_type = WIDGET_CHECKBOX
    },
    
    /* Analysis Group */
    {
        .name = "generate_histogram",
        .label = "Generate Histogram",
        .tooltip = "Create timing histogram for analysis.",
        .group = "Analysis",
        .widget_type = WIDGET_CHECKBOX
    },
    {
        .name = "generate_entropy",
        .label = "Generate Entropy Graph",
        .tooltip = "Track timing variations across track.",
        .group = "Analysis",
        .widget_type = WIDGET_CHECKBOX
    },
    {
        .name = "log_level",
        .label = "Log Level",
        .tooltip = "Verbosity of log output.",
        .group = "Analysis",
        .widget_type = WIDGET_COMBOBOX,
        .options = log_level_options,
        .option_count = 4
    },
    
    /* End marker */
    {.name = NULL}
};

const param_widget_desc_t *recovery_get_widget_descriptions(void) {
    return widget_descriptions;
}

int recovery_get_param_count(void) {
    int count = 0;
    while (widget_descriptions[count].name != NULL) {
        count++;
    }
    return count;
}
