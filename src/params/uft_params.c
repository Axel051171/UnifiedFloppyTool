/**
 * @file uft_params.c
 * @brief Kanonische Parameter Implementation
 */

#include "uft/uft_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ============================================================================
// Default Values
// ============================================================================

uft_params_t uft_params_default(void) {
    uft_params_t p = {0};
    
    p.struct_size = sizeof(uft_params_t);
    p.version = 1;
    
    // Global defaults
    p.global.device_index = -1;
    p.global.drive_type = UFT_DRIVE_UNKNOWN;
    p.global.rpm = 0;  // Auto-detect
    p.global.global_retries = 3;
    p.global.verify_after_write = true;
    
    // Geometry defaults
    p.geometry.cylinder_start = 0;
    p.geometry.cylinder_end = -1;  // All
    p.geometry.head_start = 0;
    p.geometry.head_end = -1;      // Both
    p.geometry.sector_size = 512;
    p.geometry.sector_base = 1;
    p.geometry.interleave = 1;
    
    // Format defaults
    p.format.input_format = UFT_FORMAT_AUTO;
    p.format.protection.preserve_weak_bits = true;
    p.format.protection.preserve_timing = true;
    
    // Hardware defaults
    p.hardware.flux.revolutions = 3;
    p.hardware.flux.index_aligned = true;
    p.hardware.write.erase_empty_tracks = true;
    
    // Decoder defaults
    p.decoder.encoding = UFT_ENCODING_AUTO;
    p.decoder.pll.initial_period_us = 2.0;
    p.decoder.pll.tolerance = 0.25;
    p.decoder.pll.phase_adjust = 0.05;
    p.decoder.pll.freq_adjust = 0.01;
    p.decoder.pll.lock_threshold = 100;
    p.decoder.sync.sync_pattern = 0x4489;
    p.decoder.sync.sync_count = 3;
    p.decoder.errors.sector_retries = 3;
    p.decoder.errors.use_multiple_revs = true;
    
    // Output defaults
    p.output.log_level = 2;  // WARN
    
    p.is_valid = false;
    
    return p;
}

// ============================================================================
// Preset-based Initialization
// ============================================================================

uft_params_t uft_params_for_preset(uft_geometry_preset_t preset) {
    uft_params_t p = uft_params_default();
    
    switch (preset) {
        case UFT_GEO_PC_360K:
            p.geometry.total_cylinders = 40;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 9;
            p.geometry.sector_size = 512;
            p.global.rpm = 300;
            p.decoder.pll.initial_period_us = 4.0;  // DD
            break;
            
        case UFT_GEO_PC_720K:
            p.geometry.total_cylinders = 80;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 9;
            p.geometry.sector_size = 512;
            p.global.rpm = 300;
            p.decoder.pll.initial_period_us = 4.0;
            break;
            
        case UFT_GEO_PC_1200K:
            p.geometry.total_cylinders = 80;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 15;
            p.geometry.sector_size = 512;
            p.global.rpm = 360;
            p.decoder.pll.initial_period_us = 2.0;  // HD
            break;
            
        case UFT_GEO_PC_1440K:
            p.geometry.total_cylinders = 80;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 18;
            p.geometry.sector_size = 512;
            p.global.rpm = 300;
            p.decoder.pll.initial_period_us = 2.0;
            break;
            
        case UFT_GEO_AMIGA_DD:
            p.geometry.total_cylinders = 80;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 11;
            p.geometry.sector_size = 512;
            p.global.rpm = 300;
            p.decoder.pll.initial_period_us = 2.0;
            p.format.amiga.filesystem = 1;  // OFS
            break;
            
        case UFT_GEO_AMIGA_HD:
            p.geometry.total_cylinders = 80;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 22;
            p.geometry.sector_size = 512;
            p.global.rpm = 300;
            p.decoder.pll.initial_period_us = 1.0;
            p.format.amiga.allow_hd = true;
            break;
            
        case UFT_GEO_C64_1541:
            p.geometry.total_cylinders = 35;
            p.geometry.total_heads = 1;
            p.geometry.sectors_per_track = 0;  // Variable
            p.geometry.sector_size = 256;
            p.global.rpm = 300;
            p.decoder.encoding = UFT_ENCODING_GCR_CBM;
            p.decoder.pll.initial_period_us = 3.5;
            break;
            
        case UFT_GEO_C64_1571:
            p.geometry.total_cylinders = 35;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 0;
            p.geometry.sector_size = 256;
            p.global.rpm = 300;
            p.decoder.encoding = UFT_ENCODING_GCR_CBM;
            break;
            
        case UFT_GEO_APPLE_DOS33:
            p.geometry.total_cylinders = 35;
            p.geometry.total_heads = 1;
            p.geometry.sectors_per_track = 16;
            p.geometry.sector_size = 256;
            p.global.rpm = 300;
            p.decoder.encoding = UFT_ENCODING_GCR_APPLE;
            p.decoder.pll.initial_period_us = 4.0;
            p.format.apple.dos_version = 33;
            break;
            
        case UFT_GEO_ATARI_ST_DD:
            p.geometry.total_cylinders = 80;
            p.geometry.total_heads = 2;
            p.geometry.sectors_per_track = 9;
            p.geometry.sector_size = 512;
            p.global.rpm = 300;
            break;
            
        default:
            break;
    }
    
    // Compute totals
    if (p.geometry.sectors_per_track > 0) {
        p.geometry.total_sectors = p.geometry.total_cylinders * 
                                   p.geometry.total_heads * 
                                   p.geometry.sectors_per_track;
        p.geometry.total_bytes = (uint64_t)p.geometry.total_sectors * 
                                 p.geometry.sector_size;
    }
    
    return p;
}

// ============================================================================
// Format-based Initialization
// ============================================================================

uft_params_t uft_params_for_format(uft_format_t format) {
    uft_params_t p = uft_params_default();
    p.format.output_format = format;
    
    switch (format) {
        case UFT_FORMAT_D64:
            p = uft_params_for_preset(UFT_GEO_C64_1541);
            p.format.output_format = UFT_FORMAT_D64;
            break;
            
        case UFT_FORMAT_G64:
            p = uft_params_for_preset(UFT_GEO_C64_1541);
            p.format.output_format = UFT_FORMAT_G64;
            p.format.cbm.preserve_errors = true;
            break;
            
        case UFT_FORMAT_ADF:
            p = uft_params_for_preset(UFT_GEO_AMIGA_DD);
            p.format.output_format = UFT_FORMAT_ADF;
            break;
            
        case UFT_FORMAT_SCP:
            // Flux format - preserve everything
            p.format.output_format = UFT_FORMAT_SCP;
            p.hardware.flux.revolutions = 5;
            p.format.protection.preserve_weak_bits = true;
            p.format.protection.preserve_timing = true;
            break;
            
        case UFT_FORMAT_HFE:
            p.format.output_format = UFT_FORMAT_HFE;
            break;
            
        case UFT_FORMAT_IMG:
            p = uft_params_for_preset(UFT_GEO_PC_1440K);
            p.format.output_format = UFT_FORMAT_IMG;
            break;
            
        default:
            p.format.output_format = format;
            break;
    }
    
    return p;
}

// ============================================================================
// Validation
// ============================================================================

typedef struct {
    const char* condition;
    const char* error_msg;
} validation_rule_t;

static bool validate_geometry(const uft_params_t* p, char* error, size_t size) {
    // Cylinder range
    if (p->geometry.cylinder_start < 0) {
        snprintf(error, size, "cylinder_start must be >= 0");
        return false;
    }
    
    if (p->geometry.cylinder_end != -1 && 
        p->geometry.cylinder_end < p->geometry.cylinder_start) {
        snprintf(error, size, "cylinder_end must be >= cylinder_start");
        return false;
    }
    
    if (p->geometry.cylinder_end > 200) {
        snprintf(error, size, "cylinder_end exceeds maximum (200)");
        return false;
    }
    
    // Head range
    if (p->geometry.head_start < 0 || p->geometry.head_start > 1) {
        snprintf(error, size, "head_start must be 0 or 1");
        return false;
    }
    
    if (p->geometry.head_end != -1 && p->geometry.head_end > 1) {
        snprintf(error, size, "head_end must be 0, 1, or -1");
        return false;
    }
    
    // Sector size
    int valid_sizes[] = {128, 256, 512, 1024, 2048, 4096, 8192};
    bool size_valid = false;
    for (int i = 0; i < 7; i++) {
        if (p->geometry.sector_size == valid_sizes[i]) {
            size_valid = true;
            break;
        }
    }
    if (!size_valid && p->geometry.sector_size != 0) {
        snprintf(error, size, "sector_size must be power of 2 (128-8192)");
        return false;
    }
    
    return true;
}

static bool validate_pll(const uft_params_t* p, char* error, size_t size) {
    if (p->decoder.pll.initial_period_us < 0.5 || 
        p->decoder.pll.initial_period_us > 20.0) {
        snprintf(error, size, "PLL period must be 0.5-20.0 µs");
        return false;
    }
    
    if (p->decoder.pll.tolerance < 0.05 || p->decoder.pll.tolerance > 0.5) {
        snprintf(error, size, "PLL tolerance must be 5%%-50%%");
        return false;
    }
    
    if (p->decoder.pll.phase_adjust < 0.01 || p->decoder.pll.phase_adjust > 0.2) {
        snprintf(error, size, "PLL phase adjust must be 1%%-20%%");
        return false;
    }
    
    return true;
}

static bool validate_flux(const uft_params_t* p, char* error, size_t size) {
    if (p->hardware.flux.revolutions < 1 || p->hardware.flux.revolutions > 20) {
        snprintf(error, size, "Revolutions must be 1-20");
        return false;
    }
    
    return true;
}

static bool validate_format_dependencies(const uft_params_t* p, 
                                          char* error, size_t size) {
    // D64 requires 1541 geometry
    if (p->format.output_format == UFT_FORMAT_D64) {
        if (p->geometry.sector_size != 256 && p->geometry.sector_size != 0) {
            snprintf(error, size, "D64 format requires 256-byte sectors");
            return false;
        }
    }
    
    // ADF requires Amiga geometry
    if (p->format.output_format == UFT_FORMAT_ADF) {
        if (p->geometry.sector_size != 512 && p->geometry.sector_size != 0) {
            snprintf(error, size, "ADF format requires 512-byte sectors");
            return false;
        }
    }
    
    // Flux formats need revolutions
    if (p->format.output_format == UFT_FORMAT_SCP ||
        p->format.output_format == UFT_FORMAT_KRYOFLUX) {
        if (p->hardware.flux.revolutions < 1) {
            snprintf(error, size, "Flux formats require at least 1 revolution");
            return false;
        }
    }
    
    return true;
}

uft_error_t uft_params_validate(uft_params_t* params) {
    if (!params) return UFT_ERROR_NULL_POINTER;
    
    params->is_valid = false;
    params->validation_error[0] = '\0';
    
    // Run all validators
    if (!validate_geometry(params, params->validation_error, 
                           sizeof(params->validation_error))) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (!validate_pll(params, params->validation_error,
                      sizeof(params->validation_error))) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (!validate_flux(params, params->validation_error,
                       sizeof(params->validation_error))) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (!validate_format_dependencies(params, params->validation_error,
                                      sizeof(params->validation_error))) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    params->is_valid = true;
    return UFT_OK;
}

// ============================================================================
// Parameter Schema Definition
// ============================================================================

#define PARAM_OFFSET(field) offsetof(uft_params_t, field)

static const uft_param_schema_t g_param_schema[] = {
    // === GLOBAL ===
    {
        .name = "global.device_index",
        .display_name = "Device",
        .description = "Selected hardware device",
        .group = "Device",
        .type = UFT_PARAM_TYPE_INT,
        .category = UFT_PARAM_CAT_GLOBAL,
        .flags = 0,
        .int_range = { .min = -1, .max = 15, .step = 1, .def = -1 },
        .offset = PARAM_OFFSET(global.device_index),
        .size = sizeof(int),
    },
    {
        .name = "global.rpm",
        .display_name = "RPM",
        .description = "Disk rotation speed (0=auto)",
        .group = "Device",
        .type = UFT_PARAM_TYPE_DOUBLE,
        .category = UFT_PARAM_CAT_GLOBAL,
        .flags = 0,
        .double_range = { .min = 0, .max = 400, .step = 1, .def = 0 },
        .offset = PARAM_OFFSET(global.rpm),
        .size = sizeof(double),
    },
    {
        .name = "global.verify_after_write",
        .display_name = "Verify After Write",
        .description = "Read back and verify written data",
        .group = "Device",
        .type = UFT_PARAM_TYPE_BOOL,
        .category = UFT_PARAM_CAT_GLOBAL,
        .flags = 0,
        .bool_val = { .def = true },
        .offset = PARAM_OFFSET(global.verify_after_write),
        .size = sizeof(bool),
    },
    
    // === GEOMETRY ===
    {
        .name = "geometry.cylinder_start",
        .display_name = "Start Cylinder",
        .description = "First cylinder to process (0-based)",
        .group = "Geometry",
        .type = UFT_PARAM_TYPE_INT,
        .category = UFT_PARAM_CAT_GEOMETRY,
        .flags = 0,
        .int_range = { .min = 0, .max = 200, .step = 1, .def = 0 },
        .offset = PARAM_OFFSET(geometry.cylinder_start),
        .size = sizeof(int),
    },
    {
        .name = "geometry.cylinder_end",
        .display_name = "End Cylinder",
        .description = "Last cylinder (-1 = all)",
        .group = "Geometry",
        .type = UFT_PARAM_TYPE_INT,
        .category = UFT_PARAM_CAT_GEOMETRY,
        .flags = 0,
        .int_range = { .min = -1, .max = 200, .step = 1, .def = -1 },
        .offset = PARAM_OFFSET(geometry.cylinder_end),
        .size = sizeof(int),
    },
    {
        .name = "geometry.head_start",
        .display_name = "Start Head",
        .description = "First head (0 or 1)",
        .group = "Geometry",
        .type = UFT_PARAM_TYPE_INT,
        .category = UFT_PARAM_CAT_GEOMETRY,
        .flags = 0,
        .int_range = { .min = 0, .max = 1, .step = 1, .def = 0 },
        .offset = PARAM_OFFSET(geometry.head_start),
        .size = sizeof(int),
    },
    {
        .name = "geometry.head_end",
        .display_name = "End Head",
        .description = "Last head (-1 = both)",
        .group = "Geometry",
        .type = UFT_PARAM_TYPE_INT,
        .category = UFT_PARAM_CAT_GEOMETRY,
        .flags = 0,
        .int_range = { .min = -1, .max = 1, .step = 1, .def = -1 },
        .offset = PARAM_OFFSET(geometry.head_end),
        .size = sizeof(int),
    },
    {
        .name = "geometry.sector_size",
        .display_name = "Sector Size",
        .description = "Bytes per sector",
        .group = "Geometry",
        .type = UFT_PARAM_TYPE_ENUM,
        .category = UFT_PARAM_CAT_GEOMETRY,
        .flags = 0,
        .offset = PARAM_OFFSET(geometry.sector_size),
        .size = sizeof(int),
    },
    
    // === DECODER/PLL ===
    {
        .name = "decoder.pll.initial_period_us",
        .display_name = "Bit Cell Time (µs)",
        .description = "Initial PLL bit cell period",
        .group = "PLL",
        .type = UFT_PARAM_TYPE_DOUBLE,
        .category = UFT_PARAM_CAT_DECODER,
        .flags = UFT_PARAM_ADVANCED,
        .double_range = { .min = 0.5, .max = 20.0, .step = 0.1, .def = 2.0 },
        .offset = PARAM_OFFSET(decoder.pll.initial_period_us),
        .size = sizeof(double),
    },
    {
        .name = "decoder.pll.tolerance",
        .display_name = "PLL Tolerance",
        .description = "Allowed deviation from nominal (0.25 = ±25%)",
        .group = "PLL",
        .type = UFT_PARAM_TYPE_DOUBLE,
        .category = UFT_PARAM_CAT_DECODER,
        .flags = UFT_PARAM_ADVANCED,
        .double_range = { .min = 0.05, .max = 0.50, .step = 0.01, .def = 0.25 },
        .offset = PARAM_OFFSET(decoder.pll.tolerance),
        .size = sizeof(double),
    },
    {
        .name = "decoder.pll.phase_adjust",
        .display_name = "Phase Adjust",
        .description = "PLL phase correction rate",
        .group = "PLL",
        .type = UFT_PARAM_TYPE_DOUBLE,
        .category = UFT_PARAM_CAT_DECODER,
        .flags = UFT_PARAM_ADVANCED | UFT_PARAM_EXPERT,
        .double_range = { .min = 0.01, .max = 0.20, .step = 0.01, .def = 0.05 },
        .offset = PARAM_OFFSET(decoder.pll.phase_adjust),
        .size = sizeof(double),
    },
    
    // === HARDWARE/FLUX ===
    {
        .name = "hardware.flux.revolutions",
        .display_name = "Revolutions",
        .description = "Number of disk revolutions to capture",
        .group = "Capture",
        .type = UFT_PARAM_TYPE_INT,
        .category = UFT_PARAM_CAT_HARDWARE,
        .flags = 0,
        .int_range = { .min = 1, .max = 20, .step = 1, .def = 3 },
        .offset = PARAM_OFFSET(hardware.flux.revolutions),
        .size = sizeof(int),
    },
    {
        .name = "hardware.flux.index_aligned",
        .display_name = "Index Aligned",
        .description = "Align capture to index pulse",
        .group = "Capture",
        .type = UFT_PARAM_TYPE_BOOL,
        .category = UFT_PARAM_CAT_HARDWARE,
        .flags = 0,
        .bool_val = { .def = true },
        .offset = PARAM_OFFSET(hardware.flux.index_aligned),
        .size = sizeof(bool),
    },
    
    // Terminator
    { .name = NULL }
};

const uft_param_schema_t* uft_params_get_schema(size_t* count) {
    if (count) {
        size_t n = 0;
        while (g_param_schema[n].name) n++;
        *count = n;
    }
    return g_param_schema;
}

const uft_param_schema_t* uft_params_get_schema_by_name(const char* name) {
    if (!name) return NULL;
    
    for (size_t i = 0; g_param_schema[i].name; i++) {
        if (strcmp(g_param_schema[i].name, name) == 0) {
            return &g_param_schema[i];
        }
    }
    return NULL;
}

int uft_params_get_by_category(uft_param_category_t cat,
                                const uft_param_schema_t** schemas,
                                int max_count) {
    int count = 0;
    for (size_t i = 0; g_param_schema[i].name && count < max_count; i++) {
        if (g_param_schema[i].category == cat) {
            if (schemas) schemas[count] = &g_param_schema[i];
            count++;
        }
    }
    return count;
}
