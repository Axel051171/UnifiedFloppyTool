/**
 * @file uft_params_validation.c
 * @brief Format-Specific Parameter Validation Rules
 * 
 * VALIDATION CATEGORIES:
 * 1. Range validation (min/max)
 * 2. Consistency validation (inter-parameter)
 * 3. Format-specific validation
 * 4. Hardware constraints
 * 
 * LIABILITY NOTE:
 * These validation rules are designed to prevent data loss.
 * Every rule exists because of a known failure mode.
 */

#include "uft/params/uft_canonical_params.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// VALIDATION ERROR STRUCTURE
// ============================================================================

typedef struct {
    const char *param;          // Parameter name
    const char *message;        // Error message
    int severity;               // 0=warning, 1=error, 2=fatal
} validation_error_t;

#define MAX_ERRORS 32

typedef struct {
    validation_error_t errors[MAX_ERRORS];
    int count;
} validation_result_t;

static void add_error(validation_result_t *r, const char *param, 
                      const char *msg, int severity) {
    if (r->count < MAX_ERRORS) {
        r->errors[r->count].param = param;
        r->errors[r->count].message = msg;
        r->errors[r->count].severity = severity;
        r->count++;
    }
}

// ============================================================================
// GENERAL RANGE VALIDATION
// ============================================================================

static void validate_ranges(const uft_canonical_params_t *p, validation_result_t *r) {
    // Cylinders
    if (p->geometry.cylinders < 0 || p->geometry.cylinders > 255) {
        add_error(r, "geometry.cylinders", 
                  "Must be 0-255", 1);
    }
    
    // Heads
    if (p->geometry.heads < 1 || p->geometry.heads > 2) {
        add_error(r, "geometry.heads", 
                  "Must be 1 or 2", 1);
    }
    
    // Sectors per track
    if (p->geometry.sectors_per_track < 0 || p->geometry.sectors_per_track > 256) {
        add_error(r, "geometry.sectors_per_track", 
                  "Must be 0-256 (0 = variable)", 1);
    }
    
    // Sector size (power of 2, 128-8192)
    if (p->geometry.sector_size > 0) {
        int s = p->geometry.sector_size;
        if (s < 128 || s > 8192 || (s & (s - 1)) != 0) {
            add_error(r, "geometry.sector_size", 
                      "Must be power of 2, 128-8192", 1);
        }
    }
    
    // Cylinder range
    if (p->geometry.cylinder_start < 0) {
        add_error(r, "geometry.cylinder_start", 
                  "Must be >= 0", 1);
    }
    if (p->geometry.cylinder_end != -1 && 
        p->geometry.cylinder_end < p->geometry.cylinder_start) {
        add_error(r, "geometry.cylinder_end", 
                  "Must be >= cylinder_start", 1);
    }
    
    // Head mask
    if (p->geometry.head_mask == 0 || p->geometry.head_mask > 0x03) {
        add_error(r, "geometry.head_mask", 
                  "Must be 0x01, 0x02, or 0x03", 1);
    }
    
    // PLL parameters
    if (p->timing.pll_phase_adjust < 0.0 || p->timing.pll_phase_adjust > 1.0) {
        add_error(r, "timing.pll_phase_adjust", 
                  "Must be 0.0-1.0", 1);
    }
    if (p->timing.pll_period_adjust < 0.0 || p->timing.pll_period_adjust > 1.0) {
        add_error(r, "timing.pll_period_adjust", 
                  "Must be 0.0-1.0", 1);
    }
    if (p->timing.pll_period_min < 0.5 || p->timing.pll_period_min > 1.0) {
        add_error(r, "timing.pll_period_min", 
                  "Must be 0.5-1.0", 0);
    }
    if (p->timing.pll_period_max < 1.0 || p->timing.pll_period_max > 2.0) {
        add_error(r, "timing.pll_period_max", 
                  "Must be 1.0-2.0", 0);
    }
    if (p->timing.pll_period_min >= p->timing.pll_period_max) {
        add_error(r, "timing.pll_period_min/max", 
                  "min must be < max", 1);
    }
    
    // RPM
    if (p->timing.rpm < 250.0 || p->timing.rpm > 400.0) {
        add_error(r, "timing.rpm", 
                  "Unusual RPM (expected 270-370)", 0);
    }
    
    // Retries
    if (p->operation.retries < 0 || p->operation.retries > 100) {
        add_error(r, "operation.retries", 
                  "Must be 0-100", 0);
    }
    
    // Revolutions
    if (p->operation.revolutions < 1 || p->operation.revolutions > 20) {
        add_error(r, "operation.revolutions", 
                  "Must be 1-20", 0);
    }
}

// ============================================================================
// CONSISTENCY VALIDATION
// ============================================================================

static void validate_consistency(const uft_canonical_params_t *p, validation_result_t *r) {
    // Head mask vs heads
    if (p->geometry.heads == 1 && p->geometry.head_mask == 0x02) {
        add_error(r, "geometry.head_mask", 
                  "Cannot select head 1 on single-sided disk", 1);
    }
    if (p->geometry.heads == 1 && p->geometry.head_mask == 0x03) {
        add_error(r, "geometry.head_mask", 
                  "Both heads selected but disk is single-sided", 0);
    }
    
    // Cell time vs datarate consistency
    if (p->timing.datarate_bps > 0 && p->timing.cell_time_ns > 0) {
        uint64_t expected = uft_params_compute_cell_time(
            p->timing.datarate_bps, p->format.encoding);
        int64_t diff = (int64_t)p->timing.cell_time_ns - (int64_t)expected;
        if (diff < -200 || diff > 200) {
            add_error(r, "timing.cell_time_ns", 
                      "Inconsistent with datarate_bps", 0);
        }
    }
    
    // Total bytes vs geometry
    if (p->geometry.cylinders > 0 && p->geometry.heads > 0 &&
        p->geometry.sectors_per_track > 0 && p->geometry.sector_size > 0) {
        int64_t expected = (int64_t)p->geometry.cylinders *
                           p->geometry.heads *
                           p->geometry.sectors_per_track *
                           p->geometry.sector_size;
        if (p->geometry.total_bytes > 0 && p->geometry.total_bytes != expected) {
            add_error(r, "geometry.total_bytes", 
                      "Inconsistent with geometry", 0);
        }
    }
    
    // Cylinder range vs total
    if (p->geometry.cylinders > 0) {
        if (p->geometry.cylinder_start >= p->geometry.cylinders) {
            add_error(r, "geometry.cylinder_start", 
                      "Exceeds total cylinders", 1);
        }
        if (p->geometry.cylinder_end != -1 && 
            p->geometry.cylinder_end >= p->geometry.cylinders) {
            add_error(r, "geometry.cylinder_end", 
                      "Exceeds total cylinders", 0);
        }
    }
}

// ============================================================================
// FORMAT-SPECIFIC VALIDATION: D64 (Commodore)
// ============================================================================

static void validate_d64(const uft_canonical_params_t *p, validation_result_t *r) {
    // D64 tracks: 35, 40, or 42
    int tracks = p->geometry.cylinders;
    if (tracks != 35 && tracks != 40 && tracks != 42) {
        add_error(r, "geometry.cylinders", 
                  "D64 requires 35, 40, or 42 tracks", 1);
    }
    
    // D64 is single-sided
    if (p->geometry.heads != 1) {
        add_error(r, "geometry.heads", 
                  "D64 is single-sided (heads=1)", 1);
    }
    
    // D64 uses GCR
    if (p->format.encoding != UFT_ENC_GCR_CBM) {
        add_error(r, "format.encoding", 
                  "D64 requires GCR_CBM encoding", 1);
    }
    
    // D64 sector size is 256
    if (p->geometry.sector_size != 256 && p->geometry.sector_size != 0) {
        add_error(r, "geometry.sector_size", 
                  "D64 uses 256-byte sectors", 0);
    }
    
    // Sectors per track should be 0 (variable) for GCR
    if (p->geometry.sectors_per_track > 0) {
        add_error(r, "geometry.sectors_per_track", 
                  "D64/GCR has variable sectors (should be 0)", 0);
    }
    
    // Expected file sizes
    uint32_t expected_sizes[] = {
        174848,     // 35 track, no errors
        175531,     // 35 track + errors
        196608,     // 40 track
        197376,     // 40 track + errors
        205312,     // 42 track
        206114      // 42 track + errors
    };
    
    if (p->geometry.total_bytes > 0) {
        bool valid_size = false;
        for (int i = 0; i < 6; i++) {
            if ((uint32_t)p->geometry.total_bytes == expected_sizes[i]) {
                valid_size = true;
                break;
            }
        }
        if (!valid_size) {
            add_error(r, "geometry.total_bytes", 
                      "Unusual D64 size - may be corrupted", 0);
        }
    }
}

// ============================================================================
// FORMAT-SPECIFIC VALIDATION: ADF (Amiga)
// ============================================================================

static void validate_adf(const uft_canonical_params_t *p, validation_result_t *r) {
    // Amiga: 80 cylinders, 2 heads
    if (p->geometry.cylinders != 80) {
        add_error(r, "geometry.cylinders", 
                  "ADF requires 80 cylinders", 1);
    }
    if (p->geometry.heads != 2) {
        add_error(r, "geometry.heads", 
                  "ADF requires 2 heads", 1);
    }
    
    // Amiga DD: 11 sectors, HD: 22 sectors
    if (p->geometry.sectors_per_track != 11 && 
        p->geometry.sectors_per_track != 22) {
        add_error(r, "geometry.sectors_per_track", 
                  "ADF requires 11 (DD) or 22 (HD) sectors", 1);
    }
    
    // Sector size: 512
    if (p->geometry.sector_size != 512) {
        add_error(r, "geometry.sector_size", 
                  "ADF uses 512-byte sectors", 1);
    }
    
    // Encoding
    if (p->format.encoding != UFT_ENC_AMIGA_MFM && 
        p->format.encoding != UFT_ENC_MFM) {
        add_error(r, "format.encoding", 
                  "ADF requires AMIGA_MFM or MFM encoding", 1);
    }
    
    // Expected sizes
    if (p->geometry.total_bytes > 0) {
        if (p->geometry.total_bytes != 901120 &&    // DD
            p->geometry.total_bytes != 1802240) {   // HD
            add_error(r, "geometry.total_bytes", 
                      "ADF should be 901120 (DD) or 1802240 (HD) bytes", 0);
        }
    }
    
    // Sector base: Amiga uses 0
    if (p->geometry.sector_base != 0) {
        add_error(r, "geometry.sector_base", 
                  "Amiga uses 0-based sectors", 0);
    }
}

// ============================================================================
// FORMAT-SPECIFIC VALIDATION: IMG (PC)
// ============================================================================

static void validate_img(const uft_canonical_params_t *p, validation_result_t *r) {
    // PC standard geometries
    typedef struct {
        int cylinders;
        int heads;
        int sectors;
        int sector_size;
        uint32_t total;
        const char *name;
    } pc_geometry_t;
    
    static const pc_geometry_t pc_geoms[] = {
        {40, 1, 8, 512, 163840, "160K"},
        {40, 1, 9, 512, 184320, "180K"},
        {40, 2, 8, 512, 327680, "320K"},
        {40, 2, 9, 512, 368640, "360K"},
        {80, 2, 9, 512, 737280, "720K"},
        {80, 2, 15, 512, 1228800, "1.2M"},
        {80, 2, 18, 512, 1474560, "1.44M"},
        {80, 2, 36, 512, 2949120, "2.88M"},
        {0, 0, 0, 0, 0, NULL}
    };
    
    bool match = false;
    for (int i = 0; pc_geoms[i].name != NULL; i++) {
        if (p->geometry.cylinders == pc_geoms[i].cylinders &&
            p->geometry.heads == pc_geoms[i].heads &&
            p->geometry.sectors_per_track == pc_geoms[i].sectors) {
            match = true;
            break;
        }
    }
    
    if (!match && p->geometry.cylinders > 0) {
        add_error(r, "geometry", 
                  "Non-standard PC geometry - verify carefully", 0);
    }
    
    // PC uses MFM
    if (p->format.encoding != UFT_ENC_MFM && 
        p->format.encoding != UFT_ENC_FM) {
        add_error(r, "format.encoding", 
                  "PC IMG requires MFM or FM encoding", 1);
    }
    
    // Sector base: PC uses 1
    if (p->geometry.sector_base != 1 && p->geometry.sector_base != 0) {
        add_error(r, "geometry.sector_base", 
                  "PC uses 1-based sectors", 0);
    }
}

// ============================================================================
// FORMAT-SPECIFIC VALIDATION: Apple II
// ============================================================================

static void validate_apple(const uft_canonical_params_t *p, validation_result_t *r) {
    // Apple II: 35 tracks (some have 40)
    if (p->geometry.cylinders != 35 && p->geometry.cylinders != 40) {
        add_error(r, "geometry.cylinders", 
                  "Apple II requires 35 or 40 tracks", 1);
    }
    
    // Single-sided
    if (p->geometry.heads != 1) {
        add_error(r, "geometry.heads", 
                  "Apple II is single-sided", 1);
    }
    
    // Sectors: 13 (DOS 3.2) or 16 (DOS 3.3/ProDOS)
    if (p->geometry.sectors_per_track != 13 && 
        p->geometry.sectors_per_track != 16 &&
        p->geometry.sectors_per_track != 0) {
        add_error(r, "geometry.sectors_per_track", 
                  "Apple II uses 13 or 16 sectors per track", 1);
    }
    
    // Sector size: 256
    if (p->geometry.sector_size != 256 && p->geometry.sector_size != 0) {
        add_error(r, "geometry.sector_size", 
                  "Apple II uses 256-byte sectors", 0);
    }
    
    // GCR encoding
    if (p->format.encoding != UFT_ENC_GCR_APPLE) {
        add_error(r, "format.encoding", 
                  "Apple II requires GCR_APPLE encoding", 1);
    }
    
    // Expected sizes
    if (p->geometry.total_bytes > 0) {
        if (p->geometry.total_bytes != 116480 &&    // 13-sector
            p->geometry.total_bytes != 143360) {    // 16-sector
            add_error(r, "geometry.total_bytes", 
                      "Unusual Apple II size", 0);
        }
    }
}

// ============================================================================
// FORMAT-SPECIFIC VALIDATION: SCP (Flux)
// ============================================================================

static void validate_scp(const uft_canonical_params_t *p, validation_result_t *r) {
    // SCP can have many tracks (up to 168 for half-tracks)
    if (p->geometry.cylinders > 168) {
        add_error(r, "geometry.cylinders", 
                  "SCP maximum is 168 (84 tracks * 2 sides)", 0);
    }
    
    // Revolutions: SCP typically stores 3-5
    if (p->operation.revolutions < 1) {
        add_error(r, "operation.revolutions", 
                  "SCP requires at least 1 revolution", 1);
    }
    
    // Sectors should be 0 for flux
    if (p->geometry.sectors_per_track != 0) {
        add_error(r, "geometry.sectors_per_track", 
                  "SCP is flux format (sectors_per_track should be 0)", 0);
    }
}

// ============================================================================
// FORMAT-SPECIFIC VALIDATION: BBC Micro
// ============================================================================

static void validate_bbc(const uft_canonical_params_t *p, validation_result_t *r) {
    // BBC DFS: 40 or 80 tracks
    if (p->geometry.cylinders != 40 && p->geometry.cylinders != 80) {
        add_error(r, "geometry.cylinders", 
                  "BBC DFS requires 40 or 80 tracks", 1);
    }
    
    // 10 sectors per track
    if (p->geometry.sectors_per_track != 10) {
        add_error(r, "geometry.sectors_per_track", 
                  "BBC DFS uses 10 sectors per track", 1);
    }
    
    // 256-byte sectors
    if (p->geometry.sector_size != 256) {
        add_error(r, "geometry.sector_size", 
                  "BBC DFS uses 256-byte sectors", 1);
    }
    
    // FM encoding
    if (p->format.encoding != UFT_ENC_FM) {
        add_error(r, "format.encoding", 
                  "BBC DFS uses FM encoding", 1);
    }
    
    // Sector base: 0
    if (p->geometry.sector_base != 0) {
        add_error(r, "geometry.sector_base", 
                  "BBC DFS uses 0-based sectors", 0);
    }
}

// ============================================================================
// MAIN VALIDATION ENTRY POINT
// ============================================================================

int uft_params_validate_full(uft_canonical_params_t *params, 
                             char *error_buffer, size_t error_size) {
    if (!params) return -1;
    
    validation_result_t result = {0};
    
    // General validation
    validate_ranges(params, &result);
    validate_consistency(params, &result);
    
    // Format-specific validation
    switch (params->format.input_format) {
        case UFT_FORMAT_D64:
        case UFT_FORMAT_D71:
        case UFT_FORMAT_G64:
            validate_d64(params, &result);
            break;
            
        case UFT_FORMAT_ADF:
            validate_adf(params, &result);
            break;
            
        case UFT_FORMAT_IMG:
        case UFT_FORMAT_IMA:
        case UFT_FORMAT_DSK:
            validate_img(params, &result);
            break;
            
        case UFT_FORMAT_DO:
        case UFT_FORMAT_PO:
        case UFT_FORMAT_NIB:
        case UFT_FORMAT_WOZ:
            validate_apple(params, &result);
            break;
            
        case UFT_FORMAT_SCP:
        case UFT_FORMAT_HFE:
        case UFT_FORMAT_IPF:
            validate_scp(params, &result);
            break;
            
        case UFT_FORMAT_SSD:
        case UFT_FORMAT_DSD:
            validate_bbc(params, &result);
            break;
            
        default:
            // No specific validation
            break;
    }
    
    // Count errors (not warnings)
    int error_count = 0;
    for (int i = 0; i < result.count; i++) {
        if (result.errors[i].severity > 0) {
            error_count++;
        }
    }
    
    // Build error message
    if (error_buffer && error_size > 0) {
        error_buffer[0] = '\0';
        size_t pos = 0;
        
        for (int i = 0; i < result.count && pos < error_size - 1; i++) {
            const char *prefix = result.errors[i].severity == 0 ? "WARN" :
                                 result.errors[i].severity == 1 ? "ERROR" : "FATAL";
            int n = snprintf(error_buffer + pos, error_size - pos,
                            "%s: %s: %s\n",
                            prefix,
                            result.errors[i].param,
                            result.errors[i].message);
            if (n > 0) pos += n;
        }
        
        // Copy to params
        strncpy(params->error_message, error_buffer, 
                sizeof(params->error_message) - 1);
    }
    
    params->error_count = result.count;
    params->is_valid = (error_count == 0);
    params->is_dirty = false;
    
    return result.count;
}

// ============================================================================
// HELPER: Get validation rules for a format
// ============================================================================

typedef struct {
    const char *rule;
    const char *description;
} format_rule_t;

int uft_params_get_format_rules(uft_format_e format, 
                                const char **rules, int max_rules) {
    static const format_rule_t d64_rules[] = {
        {"cylinders=35|40|42", "D64 track count"},
        {"heads=1", "D64 is single-sided"},
        {"encoding=GCR_CBM", "D64 uses Commodore GCR"},
        {"sector_size=256", "D64 256-byte sectors"},
        {NULL, NULL}
    };
    
    static const format_rule_t adf_rules[] = {
        {"cylinders=80", "ADF 80 cylinders"},
        {"heads=2", "ADF double-sided"},
        {"sectors=11|22", "ADF 11 (DD) or 22 (HD) sectors"},
        {"sector_size=512", "ADF 512-byte sectors"},
        {"sector_base=0", "ADF 0-based sectors"},
        {NULL, NULL}
    };
    
    static const format_rule_t apple_rules[] = {
        {"cylinders=35|40", "Apple II 35 or 40 tracks"},
        {"heads=1", "Apple II single-sided"},
        {"sectors=13|16", "Apple II 13 or 16 sectors"},
        {"sector_size=256", "Apple II 256-byte sectors"},
        {"encoding=GCR_APPLE", "Apple II GCR"},
        {NULL, NULL}
    };
    
    const format_rule_t *rule_set = NULL;
    
    switch (format) {
        case UFT_FORMAT_D64:
        case UFT_FORMAT_G64:
            rule_set = d64_rules;
            break;
        case UFT_FORMAT_ADF:
            rule_set = adf_rules;
            break;
        case UFT_FORMAT_DO:
        case UFT_FORMAT_PO:
            rule_set = apple_rules;
            break;
        default:
            return 0;
    }
    
    int count = 0;
    for (int i = 0; rule_set[i].rule != NULL && count < max_rules; i++) {
        rules[count++] = rule_set[i].rule;
    }
    
    return count;
}
