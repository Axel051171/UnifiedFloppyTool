/**
 * @file uft_pll_params.c
 * @brief PLL Parameter Management Implementation
 */

#include "uft_pll_params.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Preset Table
 * ============================================================================ */

static const uft_pll_params_t g_presets[] = {
    /* UFT_PLL_PRESET_DEFAULT */
    UFT_PLL_PARAMS_DEFAULT,
    
    /* UFT_PLL_PRESET_CLEAN_DISK */
    UFT_PLL_PARAMS_AGGRESSIVE,
    
    /* UFT_PLL_PRESET_DIRTY_DISK */
    UFT_PLL_PARAMS_CONSERVATIVE,
    
    /* UFT_PLL_PRESET_COPY_PROTECTED */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE | UFT_PLL_FLAG_WEAK_BIT_AWARE,
        .kp = 0.45, .ki = 0.00045, .kd = 0.0,
        .sync_tolerance = 0.30, .lock_tolerance = 0.12,
        .unlock_threshold = 0.42, .sync_bits_required = 20,
        .cell_adjust_rate = 0.045, .rpm_tolerance = 0.035,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.16, .weak_bit_min_count = 3,
        .name = "Copy Protected", .description = "For disks with copy protection",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_FORENSIC */
    UFT_PLL_PARAMS_FORENSIC,
    
    /* UFT_PLL_PRESET_IBM_PC_DD */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "IBM PC DD", .description = "IBM PC Double Density (360K/720K)",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_IBM_PC_HD */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 500000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "IBM PC HD", .description = "IBM PC High Density (1.2M/1.44M)",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_AMIGA_DD */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "Amiga DD", .description = "Amiga Double Density (880K)",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_AMIGA_HD */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 500000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "Amiga HD", .description = "Amiga High Density (1.76M)",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_ATARI_ST */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE | UFT_PLL_FLAG_WEAK_BIT_AWARE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "Atari ST", .description = "Atari ST (with fuzzy bit support)",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_C64 */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.45, .ki = 0.00045, .kd = 0.0,
        .sync_tolerance = 0.28, .lock_tolerance = 0.12,
        .unlock_threshold = 0.42, .sync_bits_required = 20,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.04,
        .encoding = UFT_ENC_CUSTOM, .data_rate = 250000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.18, .weak_bit_min_count = 3,
        .name = "C64 GCR", .description = "Commodore 64/1541 GCR encoding",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_APPLE_II */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.45, .ki = 0.00045, .kd = 0.0,
        .sync_tolerance = 0.28, .lock_tolerance = 0.12,
        .unlock_threshold = 0.42, .sync_bits_required = 20,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.04,
        .encoding = UFT_ENC_CUSTOM, .data_rate = 250000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.18, .weak_bit_min_count = 3,
        .name = "Apple II", .description = "Apple II GCR encoding",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_MAC_GCR */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.45, .ki = 0.00045, .kd = 0.0,
        .sync_tolerance = 0.28, .lock_tolerance = 0.12,
        .unlock_threshold = 0.42, .sync_bits_required = 20,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.04,
        .encoding = UFT_ENC_CUSTOM, .data_rate = 500000, .sample_rate = 24000000,
        .weak_bit_threshold = 0.18, .weak_bit_min_count = 3,
        .name = "Mac GCR", .description = "Macintosh GCR encoding",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_GREASEWEAZLE */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 24027429,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "Greaseweazle", .description = "Greaseweazle hardware",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_KRYOFLUX */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE | UFT_PLL_FLAG_MULTI_REV,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 24027429,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "KryoFlux", .description = "KryoFlux hardware",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_FLUXENGINE */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 72000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "FluxEngine", .description = "FluxEngine hardware",
        .validated = true, .error_msg = ""
    },
    
    /* UFT_PLL_PRESET_SCP */
    {
        .version = UFT_PLL_PARAMS_VERSION,
        .flags = UFT_PLL_FLAG_ADAPTIVE | UFT_PLL_FLAG_MULTI_REV,
        .kp = 0.5, .ki = 0.0005, .kd = 0.0,
        .sync_tolerance = 0.25, .lock_tolerance = 0.10,
        .unlock_threshold = 0.40, .sync_bits_required = 16,
        .cell_adjust_rate = 0.05, .rpm_tolerance = 0.03,
        .encoding = UFT_ENC_MFM, .data_rate = 250000, .sample_rate = 40000000,
        .weak_bit_threshold = 0.15, .weak_bit_min_count = 3,
        .name = "SuperCard Pro", .description = "SuperCard Pro hardware",
        .validated = true, .error_msg = ""
    }
};

static const char* g_preset_names[] = {
    "Default",
    "Clean Disk",
    "Dirty Disk",
    "Copy Protected",
    "Forensic",
    "IBM PC DD",
    "IBM PC HD",
    "Amiga DD",
    "Amiga HD",
    "Atari ST",
    "C64",
    "Apple II",
    "Mac GCR",
    "Greaseweazle",
    "KryoFlux",
    "FluxEngine",
    "SuperCard Pro"
};

/* ============================================================================
 * Implementation
 * ============================================================================ */

uft_pll_params_t uft_pll_params_default(void) {
    uft_pll_params_t params = UFT_PLL_PARAMS_DEFAULT;
    return params;
}

uft_pll_params_t uft_pll_params_preset(uft_pll_preset_id_t preset) {
    if (preset < 0 || preset >= UFT_PLL_PRESET_COUNT) {
        return uft_pll_params_default();
    }
    return g_presets[preset];
}

const char* uft_pll_preset_name(uft_pll_preset_id_t preset) {
    if (preset < 0 || preset >= UFT_PLL_PRESET_COUNT) {
        return "Unknown";
    }
    return g_preset_names[preset];
}

bool uft_pll_params_validate(uft_pll_params_t* params) {
    if (!params) return false;
    
    params->validated = false;
    params->error_msg[0] = '\0';
    
    /* Check ranges */
    if (params->kp < 0.01 || params->kp > 2.0) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "kp out of range (0.01-2.0): %.4f", params->kp);
        return false;
    }
    
    if (params->ki < 0.0 || params->ki > 0.1) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "ki out of range (0.0-0.1): %.6f", params->ki);
        return false;
    }
    
    if (params->sync_tolerance < 0.05 || params->sync_tolerance > 0.8) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "sync_tolerance out of range (0.05-0.8): %.2f", 
                 params->sync_tolerance);
        return false;
    }
    
    if (params->data_rate < 100000 || params->data_rate > 20000000) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "data_rate out of range: %u", params->data_rate);
        return false;
    }
    
    params->validated = true;
    return true;
}

void uft_pll_params_copy(uft_pll_params_t* dst, const uft_pll_params_t* src) {
    if (dst && src) {
        memcpy(dst, src, sizeof(uft_pll_params_t));
    }
}

uft_pll_config_t uft_pll_params_to_config(const uft_pll_params_t* params) {
    uft_pll_config_t config = {0};
    if (params) {
        config.kp = params->kp;
        config.ki = params->ki;
        config.sync_tolerance = params->sync_tolerance;
        config.lock_threshold = params->lock_tolerance;
        config.encoding = params->encoding;
        config.data_rate = params->data_rate;
    }
    return config;
}

uft_pll_params_t uft_pll_params_from_config(const uft_pll_config_t* config) {
    uft_pll_params_t params = uft_pll_params_default();
    if (config) {
        params.kp = config->kp;
        params.ki = config->ki;
        params.sync_tolerance = config->sync_tolerance;
        params.lock_tolerance = config->lock_threshold;
        params.encoding = config->encoding;
        params.data_rate = config->data_rate;
    }
    return params;
}

char* uft_pll_params_to_json(const uft_pll_params_t* params) {
    if (!params) return NULL;
    
    char* json = (char*)malloc(2048);
    if (!json) return NULL;
    
    snprintf(json, 2048,
        "{\n"
        "  \"version\": %u,\n"
        "  \"flags\": %u,\n"
        "  \"name\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"pi\": {\n"
        "    \"kp\": %.6f,\n"
        "    \"ki\": %.8f,\n"
        "    \"kd\": %.8f\n"
        "  },\n"
        "  \"sync\": {\n"
        "    \"tolerance\": %.4f,\n"
        "    \"lock_tolerance\": %.4f,\n"
        "    \"unlock_threshold\": %.4f,\n"
        "    \"bits_required\": %d\n"
        "  },\n"
        "  \"timing\": {\n"
        "    \"cell_adjust_rate\": %.4f,\n"
        "    \"rpm_tolerance\": %.4f,\n"
        "    \"data_rate\": %u,\n"
        "    \"sample_rate\": %u\n"
        "  },\n"
        "  \"weak_bits\": {\n"
        "    \"threshold\": %.4f,\n"
        "    \"min_count\": %d\n"
        "  }\n"
        "}\n",
        params->version, params->flags,
        params->name, params->description,
        params->kp, params->ki, params->kd,
        params->sync_tolerance, params->lock_tolerance,
        params->unlock_threshold, params->sync_bits_required,
        params->cell_adjust_rate, params->rpm_tolerance,
        params->data_rate, params->sample_rate,
        params->weak_bit_threshold, params->weak_bit_min_count
    );
    
    return json;
}

void uft_pll_params_adjust_for_platform(uft_pll_params_t* params, 
                                         const char* platform) {
    if (!params || !platform) return;
    
    if (strcmp(platform, "amiga") == 0) {
        params->encoding = UFT_ENC_MFM;
        params->data_rate = 250000;
    } else if (strcmp(platform, "c64") == 0) {
        params->encoding = UFT_ENC_CUSTOM;
        params->rpm_tolerance = 0.04;
    } else if (strcmp(platform, "apple2") == 0) {
        params->encoding = UFT_ENC_CUSTOM;
        params->rpm_tolerance = 0.04;
    } else if (strcmp(platform, "atari_st") == 0) {
        params->flags |= UFT_PLL_FLAG_WEAK_BIT_AWARE;
    }
}

void uft_pll_params_adjust_for_hardware(uft_pll_params_t* params,
                                         const char* hardware) {
    if (!params || !hardware) return;
    
    if (strcmp(hardware, "greaseweazle") == 0) {
        params->sample_rate = 24027429;
    } else if (strcmp(hardware, "kryoflux") == 0) {
        params->sample_rate = 24027429;
        params->flags |= UFT_PLL_FLAG_MULTI_REV;
    } else if (strcmp(hardware, "fluxengine") == 0) {
        params->sample_rate = 72000000;
    } else if (strcmp(hardware, "scp") == 0) {
        params->sample_rate = 40000000;
        params->flags |= UFT_PLL_FLAG_MULTI_REV;
    }
}
