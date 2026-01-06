/**
 * @file uft_protection_params.c
 * @brief Protection Parameter Management Implementation
 */

#include "uft_protection_params.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* Preset table */
static const uft_protection_params_t g_presets[] = {
    UFT_PROTECTION_PARAMS_DEFAULT,
    /* Quick */ {
        .version = UFT_PROTECTION_PARAMS_VERSION,
        .flags = 0,
        .detect_types = UFT_PROT_FUZZY_BITS | UFT_PROT_LONG_TRACK | UFT_PROT_INVALID_CRC,
        .fuzzy_timing_min_us = 4.3f, .fuzzy_timing_max_us = 5.7f,
        .fuzzy_min_reads = 2, .fuzzy_variance_threshold = 0.18f,
        .long_track_threshold = 1.03f, .short_track_threshold = 0.97f,
        .min_crc_errors_for_protect = 5, .ignore_random_crc_errors = true,
        .confidence_threshold = 60,
        .name = "Quick", .description = "Fast protection scan",
        .validated = true
    },
    /* Thorough */ {
        .version = UFT_PROTECTION_PARAMS_VERSION,
        .flags = UFT_PROT_FLAG_DETECT_ALL | UFT_PROT_FLAG_MULTI_REV | UFT_PROT_FLAG_REPORT,
        .detect_types = 0xFFFF,
        .fuzzy_timing_min_us = 4.2f, .fuzzy_timing_max_us = 5.8f,
        .fuzzy_min_reads = 7, .fuzzy_variance_threshold = 0.10f,
        .long_track_threshold = 1.01f, .short_track_threshold = 0.99f,
        .min_crc_errors_for_protect = 2, .ignore_random_crc_errors = false,
        .detect_duplicate_ids = true, .detect_half_tracks = true,
        .enable_atari_st_checks = true, .enable_amiga_checks = true,
        .enable_c64_checks = true, .enable_apple_checks = true,
        .confidence_threshold = 30,
        .name = "Thorough", .description = "Complete protection analysis",
        .validated = true
    },
    UFT_PROTECTION_PARAMS_ATARI_ST,
    /* Amiga */ {
        .version = UFT_PROTECTION_PARAMS_VERSION,
        .flags = UFT_PROT_FLAG_DETECT_ALL,
        .detect_types = UFT_PROT_LONG_TRACK | UFT_PROT_TIMING_BASED | UFT_PROT_NON_STANDARD_GAP,
        .long_track_threshold = 1.005f, .short_track_threshold = 0.995f,
        .enable_amiga_checks = true,
        .confidence_threshold = 45,
        .name = "Amiga", .description = "Amiga protection focus",
        .validated = true
    },
    /* C64 */ {
        .version = UFT_PROTECTION_PARAMS_VERSION,
        .flags = UFT_PROT_FLAG_DETECT_ALL,
        .detect_types = UFT_PROT_DENSITY_VAR | UFT_PROT_HALF_TRACK | UFT_PROT_NON_STANDARD_GAP,
        .density_variance_threshold = 0.08f,
        .detect_half_tracks = true, .half_track_signal_threshold = 0.25f,
        .enable_c64_checks = true,
        .confidence_threshold = 45,
        .name = "C64", .description = "C64/1541 protection focus",
        .validated = true
    },
    /* Apple */ {
        .version = UFT_PROTECTION_PARAMS_VERSION,
        .flags = UFT_PROT_FLAG_DETECT_ALL,
        .detect_types = UFT_PROT_HALF_TRACK | UFT_PROT_TIMING_BASED | UFT_PROT_NON_STANDARD_GAP,
        .detect_half_tracks = true, .half_track_signal_threshold = 0.20f,
        .enable_apple_checks = true,
        .confidence_threshold = 45,
        .name = "Apple", .description = "Apple II protection focus",
        .validated = true
    }
};

static const char* g_preset_names[] = {
    "Default", "Quick", "Thorough", "Atari ST", "Amiga", "C64", "Apple"
};

static const char* g_type_names[] = {
    "None", "Fuzzy Bits", "Long Track", "Short Track", "Invalid CRC",
    "Duplicate Sector", "Missing Sector", "Half Track", "Density Variation",
    "Non-Standard Gap", "Timing Based", "Sector 247", "Copylock", "Speedlock"
};

uft_protection_params_t uft_protection_params_default(void) {
    uft_protection_params_t params = UFT_PROTECTION_PARAMS_DEFAULT;
    return params;
}

uft_protection_params_t uft_protection_params_preset(uft_protection_preset_id_t preset) {
    if (preset < 0 || preset >= UFT_PROT_PRESET_COUNT) {
        return uft_protection_params_default();
    }
    return g_presets[preset];
}

const char* uft_protection_preset_name(uft_protection_preset_id_t preset) {
    if (preset < 0 || preset >= UFT_PROT_PRESET_COUNT) return "Unknown";
    return g_preset_names[preset];
}

const char* uft_protection_type_name(uft_protection_type_t type) {
    int idx = 0;
    uint32_t t = (uint32_t)type;
    while (t > 1 && idx < 14) { t >>= 1; idx++; }
    return g_type_names[idx];
}

bool uft_protection_params_validate(uft_protection_params_t* params) {
    if (!params) return false;
    params->validated = true;
    return true;
}

void uft_protection_result_init(uft_protection_result_t* result) {
    if (result) memset(result, 0, sizeof(*result));
}

char* uft_protection_params_to_json(const uft_protection_params_t* params) {
    if (!params) return NULL;
    char* json = (char*)malloc(2048);
    if (!json) return NULL;
    snprintf(json, 2048,
        "{\n  \"name\": \"%s\",\n  \"detect_types\": %u,\n"
        "  \"fuzzy\": { \"min_us\": %.2f, \"max_us\": %.2f },\n"
        "  \"confidence_threshold\": %d\n}\n",
        params->name, params->detect_types,
        params->fuzzy_timing_min_us, params->fuzzy_timing_max_us,
        params->confidence_threshold);
    return json;
}
