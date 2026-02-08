/**
 * @file uft_recovery_params.c
 * @brief Recovery Parameter Management Implementation
 */

#include "uft_recovery_params.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* ============================================================================
 * Preset Table
 * ============================================================================ */

static const uft_recovery_params_t g_presets[] = {
    /* UFT_REC_PRESET_DEFAULT */
    UFT_RECOVERY_PARAMS_DEFAULT,
    
    /* UFT_REC_PRESET_QUICK */
    UFT_RECOVERY_PARAMS_QUICK,
    
    /* UFT_REC_PRESET_STANDARD */
    {
        .version = UFT_RECOVERY_PARAMS_VERSION,
        .flags = UFT_REC_FLAG_CRC_SINGLE | UFT_REC_FLAG_CRC_DOUBLE |
                 UFT_REC_FLAG_MULTI_READ | UFT_REC_FLAG_MAJORITY_VOTE,
        .max_retries_per_sector = 8,
        .max_retries_per_track = 4,
        .retry_delay_ms = 50,
        .min_reads_for_vote = 3,
        .max_reads_for_vote = 7,
        .vote_threshold = 0.6f,
        .enable_crc_single_fix = true,
        .enable_crc_double_fix = true,
        .crc_fix_max_bytes = 512,
        .weak_bit_threshold = 0.15f,
        .weak_bit_min_variance = 2,
        .stabilize_weak_bits = false,
        .alignment_tolerance = 0.05f,
        .auto_align_tracks = false,
        .pll_resync_on_error = true,
        .pll_resync_bits = 32,
        .enable_sector_splice = false,
        .enable_header_recovery = false,
        .mark_recovered_sectors = true,
        .generate_recovery_log = false,
        .preserve_original_on_fail = true,
        .name = "Standard",
        .description = "Standard recovery with CRC correction",
        .validated = true,
        .error_msg = ""
    },
    
    /* UFT_REC_PRESET_THOROUGH */
    {
        .version = UFT_RECOVERY_PARAMS_VERSION,
        .flags = UFT_REC_FLAG_CRC_SINGLE | UFT_REC_FLAG_CRC_DOUBLE |
                 UFT_REC_FLAG_MULTI_READ | UFT_REC_FLAG_MAJORITY_VOTE |
                 UFT_REC_FLAG_WEAK_BIT | UFT_REC_FLAG_PLL_RESYNC |
                 UFT_REC_FLAG_SECTOR_SPLICE,
        .max_retries_per_sector = 15,
        .max_retries_per_track = 7,
        .retry_delay_ms = 100,
        .min_reads_for_vote = 5,
        .max_reads_for_vote = 12,
        .vote_threshold = 0.55f,
        .enable_crc_single_fix = true,
        .enable_crc_double_fix = true,
        .crc_fix_max_bytes = 768,
        .weak_bit_threshold = 0.18f,
        .weak_bit_min_variance = 3,
        .stabilize_weak_bits = true,
        .alignment_tolerance = 0.06f,
        .auto_align_tracks = true,
        .pll_resync_on_error = true,
        .pll_resync_bits = 48,
        .enable_sector_splice = true,
        .enable_header_recovery = true,
        .mark_recovered_sectors = true,
        .generate_recovery_log = true,
        .preserve_original_on_fail = true,
        .name = "Thorough",
        .description = "Comprehensive recovery with sector splicing",
        .validated = true,
        .error_msg = ""
    },
    
    /* UFT_REC_PRESET_FORENSIC */
    UFT_RECOVERY_PARAMS_FORENSIC,
    
    /* UFT_REC_PRESET_WEAK_BIT */
    {
        .version = UFT_RECOVERY_PARAMS_VERSION,
        .flags = UFT_REC_FLAG_MULTI_READ | UFT_REC_FLAG_MAJORITY_VOTE |
                 UFT_REC_FLAG_WEAK_BIT,
        .max_retries_per_sector = 10,
        .max_retries_per_track = 5,
        .retry_delay_ms = 50,
        .min_reads_for_vote = 5,
        .max_reads_for_vote = 10,
        .vote_threshold = 0.5f,
        .enable_crc_single_fix = true,
        .enable_crc_double_fix = false,
        .crc_fix_max_bytes = 512,
        .weak_bit_threshold = 0.12f,
        .weak_bit_min_variance = 2,
        .stabilize_weak_bits = true,
        .alignment_tolerance = 0.05f,
        .auto_align_tracks = false,
        .pll_resync_on_error = true,
        .pll_resync_bits = 32,
        .enable_sector_splice = true,
        .enable_header_recovery = false,
        .mark_recovered_sectors = true,
        .generate_recovery_log = true,
        .preserve_original_on_fail = true,
        .name = "Weak Bit Focus",
        .description = "Optimized for weak bit recovery",
        .validated = true,
        .error_msg = ""
    },
    
    /* UFT_REC_PRESET_CRC_FOCUS */
    {
        .version = UFT_RECOVERY_PARAMS_VERSION,
        .flags = UFT_REC_FLAG_CRC_SINGLE | UFT_REC_FLAG_CRC_DOUBLE,
        .max_retries_per_sector = 3,
        .max_retries_per_track = 2,
        .retry_delay_ms = 0,
        .min_reads_for_vote = 2,
        .max_reads_for_vote = 3,
        .vote_threshold = 0.7f,
        .enable_crc_single_fix = true,
        .enable_crc_double_fix = true,
        .crc_fix_max_bytes = 1024,
        .weak_bit_threshold = 0.15f,
        .weak_bit_min_variance = 2,
        .stabilize_weak_bits = false,
        .alignment_tolerance = 0.05f,
        .auto_align_tracks = false,
        .pll_resync_on_error = false,
        .pll_resync_bits = 16,
        .enable_sector_splice = false,
        .enable_header_recovery = false,
        .mark_recovered_sectors = true,
        .generate_recovery_log = false,
        .preserve_original_on_fail = true,
        .name = "CRC Focus",
        .description = "Fast CRC error correction",
        .validated = true,
        .error_msg = ""
    }
};

static const char* g_preset_names[] = {
    "Default",
    "Quick",
    "Standard",
    "Thorough",
    "Forensic",
    "Weak Bit Focus",
    "CRC Focus"
};

/* ============================================================================
 * Implementation
 * ============================================================================ */

uft_recovery_params_t uft_recovery_params_default(void) {
    uft_recovery_params_t params = UFT_RECOVERY_PARAMS_DEFAULT;
    return params;
}

uft_recovery_params_t uft_recovery_params_preset(uft_recovery_preset_id_t preset) {
    if (preset < 0 || preset >= UFT_REC_PRESET_COUNT) {
        return uft_recovery_params_default();
    }
    return g_presets[preset];
}

const char* uft_recovery_preset_name(uft_recovery_preset_id_t preset) {
    if (preset < 0 || preset >= UFT_REC_PRESET_COUNT) {
        return "Unknown";
    }
    return g_preset_names[preset];
}

bool uft_recovery_params_validate(uft_recovery_params_t* params) {
    if (!params) return false;
    
    params->validated = false;
    params->error_msg[0] = '\0';
    
    if (params->max_retries_per_sector < 1 || params->max_retries_per_sector > 50) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "max_retries_per_sector out of range (1-50): %d", 
                 params->max_retries_per_sector);
        return false;
    }
    
    if (params->vote_threshold < 0.3f || params->vote_threshold > 1.0f) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "vote_threshold out of range (0.3-1.0): %.2f", 
                 params->vote_threshold);
        return false;
    }
    
    if (params->min_reads_for_vote > params->max_reads_for_vote) {
        snprintf(params->error_msg, sizeof(params->error_msg),
                 "min_reads_for_vote > max_reads_for_vote");
        return false;
    }
    
    params->validated = true;
    return true;
}

void uft_recovery_params_copy(uft_recovery_params_t* dst, 
                               const uft_recovery_params_t* src) {
    if (dst && src) {
        memcpy(dst, src, sizeof(uft_recovery_params_t));
    }
}

void uft_recovery_stats_init(uft_recovery_stats_t* stats) {
    if (stats) {
        memset(stats, 0, sizeof(uft_recovery_stats_t));
    }
}

char* uft_recovery_params_to_json(const uft_recovery_params_t* params) {
    if (!params) return NULL;
    
    char* json = (char*)malloc(2048);
    if (!json) return NULL;
    
    snprintf(json, 2048,
        "{\n"
        "  \"version\": %u,\n"
        "  \"flags\": %u,\n"
        "  \"name\": \"%s\",\n"
        "  \"description\": \"%s\",\n"
        "  \"retry\": {\n"
        "    \"max_per_sector\": %d,\n"
        "    \"max_per_track\": %d,\n"
        "    \"delay_ms\": %d\n"
        "  },\n"
        "  \"multi_read\": {\n"
        "    \"min_reads\": %d,\n"
        "    \"max_reads\": %d,\n"
        "    \"vote_threshold\": %.2f\n"
        "  },\n"
        "  \"crc\": {\n"
        "    \"single_fix\": %s,\n"
        "    \"double_fix\": %s,\n"
        "    \"max_bytes\": %d\n"
        "  },\n"
        "  \"weak_bit\": {\n"
        "    \"threshold\": %.2f,\n"
        "    \"min_variance\": %d,\n"
        "    \"stabilize\": %s\n"
        "  }\n"
        "}\n",
        params->version, params->flags,
        params->name, params->description,
        params->max_retries_per_sector, params->max_retries_per_track,
        params->retry_delay_ms,
        params->min_reads_for_vote, params->max_reads_for_vote,
        params->vote_threshold,
        params->enable_crc_single_fix ? "true" : "false",
        params->enable_crc_double_fix ? "true" : "false",
        params->crc_fix_max_bytes,
        params->weak_bit_threshold, params->weak_bit_min_variance,
        params->stabilize_weak_bits ? "true" : "false"
    );
    
    return json;
}

char* uft_recovery_generate_report(const uft_recovery_stats_t* stats,
                                    const uft_recovery_params_t* params) {
    if (!stats) return NULL;
    
    char* report = (char*)malloc(4096);
    if (!report) return NULL;
    
    float success_rate = 0.0f;
    if (stats->sectors_read > 0) {
        success_rate = (float)(stats->sectors_ok + stats->sectors_recovered) / 
                       (float)stats->sectors_read * 100.0f;
    }
    
    snprintf(report, 4096,
        "═══════════════════════════════════════════════════════════\n"
        "              UFT RECOVERY REPORT\n"
        "═══════════════════════════════════════════════════════════\n"
        "\n"
        "SUMMARY\n"
        "───────────────────────────────────────────────────────────\n"
        "  Sectors Read:      %u\n"
        "  Sectors OK:        %u\n"
        "  Sectors Recovered: %u\n"
        "  Sectors Failed:    %u\n"
        "  Success Rate:      %.1f%%\n"
        "\n"
        "RECOVERY DETAILS\n"
        "───────────────────────────────────────────────────────────\n"
        "  CRC Errors Fixed:  %u\n"
        "  Weak Bits Fixed:   %u\n"
        "  Total Retries:     %u\n"
        "\n"
        "SETTINGS USED: %s\n"
        "═══════════════════════════════════════════════════════════\n",
        stats->sectors_read,
        stats->sectors_ok,
        stats->sectors_recovered,
        stats->sectors_failed,
        success_rate,
        stats->crc_errors_fixed,
        stats->weak_bits_fixed,
        stats->retries,
        params ? params->name : "Default"
    );
    
    return report;
}
