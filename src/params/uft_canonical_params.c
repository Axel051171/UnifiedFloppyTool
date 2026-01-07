/**
 * @file uft_canonical_params.c
 * @brief Kanonisches Parameter-System Implementation
 * 
 * IMPLEMENTATION NOTES
 * ====================
 * 
 * 1. All setters mark params as dirty
 * 2. Validation clears dirty flag
 * 3. Auto-computed values are updated on recompute()
 * 4. Tool translation uses lookup tables
 */

#include "uft/params/uft_canonical_params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

// ============================================================================
// DEFAULTS
// ============================================================================

static const uft_geom_t GEOM_DEFAULTS = {
    .cylinders = 80,
    .heads = 2,
    .sectors_per_track = 18,
    .sector_size = 512,
    .cylinder_start = 0,
    .cylinder_end = -1,
    .head_mask = 0x03,
    .sector_base = 1,
    .interleave = 1,
    .skew = 0,
    .total_sectors = 0,  // Computed
    .total_bytes = 0,    // Computed
    .flags = UFT_PFLAG_NONE
};

static const uft_timing_t TIMING_DEFAULTS = {
    .cell_time_ns = 2000,       // DD MFM
    .rotation_ns = 200000000,   // 200ms = 300 RPM
    .datarate_bps = 250000,     // 250 kbps
    .rpm = 300.0,
    .pll_phase_adjust = 0.60,
    .pll_period_adjust = 0.05,
    .pll_period_min = 0.75,
    .pll_period_max = 1.25,
    .weak_threshold = 3.0,
    .flags = UFT_PFLAG_NONE
};

static const uft_format_params_t FORMAT_DEFAULTS = {
    .input_format = UFT_FORMAT_AUTO,
    .output_format = UFT_FORMAT_RAW,
    .encoding = UFT_ENC_MFM,
    .density = UFT_DENSITY_DD,
    .cbm = { .half_tracks = false, .error_map = false, .track_range = 35 },
    .amiga = { .filesystem = 0, .bootable = false },
    .ibm = { .gap0_bytes = 80, .gap1_bytes = 50, .gap2_bytes = 22, .gap3_bytes = 80 },
    .flags = UFT_PFLAG_NONE
};

static const uft_hardware_t HARDWARE_DEFAULTS = {
    .device_path = "",
    .device_index = -1,
    .drive_type = UFT_DRIVE_AUTO,
    .double_step = false,
    .tool = UFT_TOOL_INTERNAL,
    .hw = { .bus_type = 0, .drive_select = 0, .densel_polarity = 0 },
    .flags = UFT_PFLAG_NONE
};

static const uft_operation_t OPERATION_DEFAULTS = {
    .dry_run = false,
    .verify_after_write = true,
    .retries = 3,
    .revolutions = 3,
    .attempt_recovery = true,
    .preserve_errors = false,
    .verbose = false,
    .generate_audit = false,
    .audit_path = "",
    .flags = UFT_PFLAG_NONE
};

// ============================================================================
// PRESET DEFINITIONS
// ============================================================================

typedef struct {
    const char *name;
    const char *description;
    uft_format_e format;
    uft_encoding_e encoding;
    uft_density_e density;
    int32_t cylinders, heads, sectors, sector_size;
    uint32_t datarate_bps;
    double rpm;
} preset_def_t;

static const preset_def_t PRESETS[] = {
    // PC/DOS
    {"pc_360k", "PC 360K (5.25\" DD)", UFT_FORMAT_IMG, UFT_ENC_MFM, UFT_DENSITY_DD,
     40, 2, 9, 512, 250000, 300.0},
    {"pc_720k", "PC 720K (3.5\" DD)", UFT_FORMAT_IMG, UFT_ENC_MFM, UFT_DENSITY_DD,
     80, 2, 9, 512, 250000, 300.0},
    {"pc_1200k", "PC 1.2M (5.25\" HD)", UFT_FORMAT_IMG, UFT_ENC_MFM, UFT_DENSITY_HD,
     80, 2, 15, 512, 500000, 360.0},
    {"pc_1440k", "PC 1.44M (3.5\" HD)", UFT_FORMAT_IMG, UFT_ENC_MFM, UFT_DENSITY_HD,
     80, 2, 18, 512, 500000, 300.0},
    
    // Commodore
    {"c64_d64", "C64 D64 (35 Track)", UFT_FORMAT_D64, UFT_ENC_GCR_CBM, UFT_DENSITY_DD,
     35, 1, 21, 256, 0, 300.0},  // GCR doesn't use standard datarate
    {"c64_d64_40", "C64 D64 (40 Track)", UFT_FORMAT_D64, UFT_ENC_GCR_CBM, UFT_DENSITY_DD,
     40, 1, 21, 256, 0, 300.0},
    {"c64_g64", "C64 G64 (GCR)", UFT_FORMAT_G64, UFT_ENC_GCR_CBM, UFT_DENSITY_DD,
     42, 1, 21, 256, 0, 300.0},
    
    // Amiga
    {"amiga_dd", "Amiga DD (880K)", UFT_FORMAT_ADF, UFT_ENC_AMIGA_MFM, UFT_DENSITY_DD,
     80, 2, 11, 512, 250000, 300.0},
    {"amiga_hd", "Amiga HD (1.76M)", UFT_FORMAT_ADF, UFT_ENC_AMIGA_MFM, UFT_DENSITY_HD,
     80, 2, 22, 512, 500000, 300.0},
    
    // Atari ST
    {"atari_st_ss", "Atari ST SS (360K)", UFT_FORMAT_ST, UFT_ENC_MFM, UFT_DENSITY_DD,
     80, 1, 9, 512, 250000, 300.0},
    {"atari_st_ds", "Atari ST DS (720K)", UFT_FORMAT_ST, UFT_ENC_MFM, UFT_DENSITY_DD,
     80, 2, 9, 512, 250000, 300.0},
    
    // Flux preservation
    {"preservation", "Preservation Mode (SCP)", UFT_FORMAT_SCP, UFT_ENC_AUTO, UFT_DENSITY_AUTO,
     84, 2, 0, 0, 0, 300.0},
    
    // Sentinel
    {NULL, NULL, 0, 0, 0, 0, 0, 0, 0, 0, 0.0}
};

// ============================================================================
// VALIDATION RULES
// ============================================================================

static bool validate_cylinder_range(const uft_canonical_params_t *p, const char **err) {
    if (p->geometry.cylinder_start < 0) {
        *err = "cylinder_start must be >= 0";
        return false;
    }
    if (p->geometry.cylinder_end != -1 && 
        p->geometry.cylinder_end < p->geometry.cylinder_start) {
        *err = "cylinder_end must be >= cylinder_start";
        return false;
    }
    if (p->geometry.cylinders > 0 && p->geometry.cylinder_start >= p->geometry.cylinders) {
        *err = "cylinder_start exceeds total cylinders";
        return false;
    }
    return true;
}

static bool validate_head_mask(const uft_canonical_params_t *p, const char **err) {
    if (p->geometry.head_mask == 0) {
        *err = "head_mask must not be 0";
        return false;
    }
    if (p->geometry.head_mask > 0x03) {
        *err = "head_mask must be 0x01, 0x02, or 0x03";
        return false;
    }
    if (p->geometry.heads == 1 && p->geometry.head_mask == 0x02) {
        *err = "head_mask 0x02 invalid for single-sided disk";
        return false;
    }
    return true;
}

static bool validate_sector_size(const uft_canonical_params_t *p, const char **err) {
    int32_t size = p->geometry.sector_size;
    // Must be power of 2, 128-8192
    if (size < 128 || size > 8192) {
        *err = "sector_size must be 128-8192";
        return false;
    }
    if ((size & (size - 1)) != 0) {
        *err = "sector_size must be power of 2";
        return false;
    }
    return true;
}

static bool validate_pll_params(const uft_canonical_params_t *p, const char **err) {
    if (p->timing.pll_phase_adjust < 0.0 || p->timing.pll_phase_adjust > 1.0) {
        *err = "pll_phase_adjust must be 0.0-1.0";
        return false;
    }
    if (p->timing.pll_period_adjust < 0.0 || p->timing.pll_period_adjust > 1.0) {
        *err = "pll_period_adjust must be 0.0-1.0";
        return false;
    }
    if (p->timing.pll_period_min >= p->timing.pll_period_max) {
        *err = "pll_period_min must be < pll_period_max";
        return false;
    }
    return true;
}

static bool validate_timing_consistency(const uft_canonical_params_t *p, const char **err) {
    // Check cell_time vs datarate consistency
    if (p->timing.datarate_bps > 0 && p->timing.cell_time_ns > 0) {
        uint64_t expected_cell = uft_params_compute_cell_time(
            p->timing.datarate_bps, p->format.encoding);
        int64_t diff = (int64_t)p->timing.cell_time_ns - (int64_t)expected_cell;
        if (diff < -100 || diff > 100) {
            *err = "cell_time_ns inconsistent with datarate_bps";
            return false;
        }
    }
    return true;
}

// ============================================================================
// TOOL MAPPING TABLES
// ============================================================================

typedef struct {
    uft_tool_e tool;
    const char *canonical;
    const char *flag;
    const char *format;
} tool_flag_t;

static const tool_flag_t TOOL_FLAGS[] = {
    {UFT_TOOL_GW, "geometry.cylinder_start", "--cyls", "%d:"},
    {UFT_TOOL_GW, "geometry.cylinder_end", "--cyls", ":%d"},
    {UFT_TOOL_GW, "geometry.head_mask", "--heads", "%s"},  // "0", "1", "0:1"
    {UFT_TOOL_GW, "operation.revolutions", "--revs", "%d"},
    {UFT_TOOL_GW, "operation.retries", "--retries", "%d"},
    
    {UFT_TOOL_FE, "geometry.cylinder_start", "-c", "%d-"},
    {UFT_TOOL_FE, "geometry.cylinder_end", "-c", "-%d"},
    {UFT_TOOL_FE, "geometry.head_mask", "-h", "%s"},
    {UFT_TOOL_FE, "operation.revolutions", "--revolutions", "%d"},
    
    // HxC
    {UFT_TOOL_LIBFLUX, "geometry.cylinder_start", "-c", "%d"},
    {UFT_TOOL_LIBFLUX, "geometry.heads", "-h", "%d"},
    
    // Sentinel
    {0, NULL, NULL, NULL}
};

// ============================================================================
// INITIALIZATION
// ============================================================================

uft_canonical_params_t uft_params_init(void) {
    uft_canonical_params_t params;
    memset(&params, 0, sizeof(params));
    
    params.magic = UFT_PARAMS_MAGIC;
    params.version = UFT_PARAMS_VERSION;
    params.struct_size = sizeof(uft_canonical_params_t);
    
    params.geometry = GEOM_DEFAULTS;
    params.timing = TIMING_DEFAULTS;
    params.format = FORMAT_DEFAULTS;
    params.hardware = HARDWARE_DEFAULTS;
    params.operation = OPERATION_DEFAULTS;
    
    params.is_valid = false;
    params.is_dirty = true;
    params.error_count = 0;
    strncpy(params.source, "default", sizeof(params.source) - 1); params.source[sizeof(params.source) - 1] = '\0';
    
    // Compute derived values
    uft_params_recompute(&params);
    
    return params;
}

uft_canonical_params_t uft_params_for_format(uft_format_e format) {
    uft_canonical_params_t params = uft_params_init();
    
    // Find preset for format
    for (int i = 0; PRESETS[i].name != NULL; i++) {
        if (PRESETS[i].format == format) {
            params.format.input_format = format;
            params.format.output_format = format;
            params.format.encoding = PRESETS[i].encoding;
            params.format.density = PRESETS[i].density;
            
            params.geometry.cylinders = PRESETS[i].cylinders;
            params.geometry.heads = PRESETS[i].heads;
            params.geometry.sectors_per_track = PRESETS[i].sectors;
            params.geometry.sector_size = PRESETS[i].sector_size;
            
            params.timing.datarate_bps = PRESETS[i].datarate_bps;
            params.timing.rpm = PRESETS[i].rpm;
            
            uft_params_recompute(&params);
            break;
        }
    }
    
    return params;
}

int uft_params_from_preset(const char *preset_name, uft_canonical_params_t *params) {
    if (!preset_name || !params) return -1;
    
    for (int i = 0; PRESETS[i].name != NULL; i++) {
        if (strcmp(PRESETS[i].name, preset_name) == 0) {
            *params = uft_params_init();
            
            params->format.input_format = PRESETS[i].format;
            params->format.output_format = PRESETS[i].format;
            params->format.encoding = PRESETS[i].encoding;
            params->format.density = PRESETS[i].density;
            
            params->geometry.cylinders = PRESETS[i].cylinders;
            params->geometry.heads = PRESETS[i].heads;
            params->geometry.sectors_per_track = PRESETS[i].sectors;
            params->geometry.sector_size = PRESETS[i].sector_size;
            params->geometry.head_mask = (PRESETS[i].heads == 2) ? 0x03 : 0x01;
            
            params->timing.datarate_bps = PRESETS[i].datarate_bps;
            params->timing.rpm = PRESETS[i].rpm;
            
            snprintf(params->source, sizeof(params->source), "preset:%s", preset_name);
            
            uft_params_recompute(params);
            return 0;
        }
    }
    
    return -1;  // Preset not found
}

// ============================================================================
// COMPUTED VALUES
// ============================================================================

uint64_t uft_params_compute_cell_time(uint32_t datarate_bps, uft_encoding_e enc) {
    if (datarate_bps == 0) return 0;
    
    // Cell time = 1 / (2 * datarate) for MFM
    // Cell time = 1 / datarate for FM
    double divisor = (enc == UFT_ENC_FM) ? 1.0 : 2.0;
    return (uint64_t)(1e9 / (datarate_bps * divisor));
}

uint32_t uft_params_compute_datarate(uint64_t cell_time_ns, uft_encoding_e enc) {
    if (cell_time_ns == 0) return 0;
    
    double divisor = (enc == UFT_ENC_FM) ? 1.0 : 2.0;
    return (uint32_t)(1e9 / (cell_time_ns * divisor));
}

uint64_t uft_params_compute_rotation_ns(double rpm) {
    if (rpm <= 0.0) return 200000000;  // Default 200ms
    return (uint64_t)(60e9 / rpm);
}

void uft_params_recompute(uft_canonical_params_t *params) {
    if (!params) return;
    
    // Compute cell_time from datarate
    if (params->timing.datarate_bps > 0) {
        params->timing.cell_time_ns = uft_params_compute_cell_time(
            params->timing.datarate_bps, params->format.encoding);
    }
    
    // Compute rotation time from RPM
    params->timing.rotation_ns = uft_params_compute_rotation_ns(params->timing.rpm);
    
    // Compute total sectors and bytes
    if (params->geometry.cylinders > 0 && 
        params->geometry.heads > 0 &&
        params->geometry.sectors_per_track > 0) {
        
        params->geometry.total_sectors = 
            (int64_t)params->geometry.cylinders *
            (int64_t)params->geometry.heads *
            (int64_t)params->geometry.sectors_per_track;
        
        params->geometry.total_bytes = 
            params->geometry.total_sectors * params->geometry.sector_size;
    }
    
    // Mark computed fields as auto
    params->geometry.flags |= UFT_PFLAG_AUTO;
    params->timing.flags |= UFT_PFLAG_AUTO;
}

// ============================================================================
// VALIDATION
// ============================================================================

int uft_params_validate(uft_canonical_params_t *params) {
    if (!params) return -1;
    
    params->error_count = 0;
    params->error_message[0] = '\0';
    
    const char *err = NULL;
    
    // Geometry validation
    if (!validate_cylinder_range(params, &err)) {
        strncpy(params->error_message, err, sizeof(params->error_message) - 1);
        params->error_count++;
    }
    
    if (!validate_head_mask(params, &err)) {
        if (params->error_count == 0) {
            strncpy(params->error_message, err, sizeof(params->error_message) - 1);
        }
        params->error_count++;
    }
    
    if (params->geometry.sector_size > 0 && !validate_sector_size(params, &err)) {
        if (params->error_count == 0) {
            strncpy(params->error_message, err, sizeof(params->error_message) - 1);
        }
        params->error_count++;
    }
    
    // Timing validation
    if (!validate_pll_params(params, &err)) {
        if (params->error_count == 0) {
            strncpy(params->error_message, err, sizeof(params->error_message) - 1);
        }
        params->error_count++;
    }
    
    if (!validate_timing_consistency(params, &err)) {
        if (params->error_count == 0) {
            strncpy(params->error_message, err, sizeof(params->error_message) - 1);
        }
        params->error_count++;
    }
    
    params->is_valid = (params->error_count == 0);
    params->is_dirty = false;
    
    return params->error_count;
}

bool uft_params_is_valid(const uft_canonical_params_t *params) {
    return params && params->is_valid && !params->is_dirty;
}

int uft_params_get_errors(const uft_canonical_params_t *params,
                          char **errors, int max_errors) {
    if (!params || !errors || max_errors < 1) return 0;
    
    if (params->error_count > 0 && params->error_message[0]) {
        errors[0] = (char*)params->error_message;
        return 1;
    }
    
    return 0;
}

// ============================================================================
// ALIAS RESOLUTION
// ============================================================================

const char *uft_params_resolve_alias(const char *alias, uft_tool_e tool) {
    if (!alias) return NULL;
    
    for (int i = 0; UFT_PARAM_ALIASES[i].canonical != NULL; i++) {
        if (strcmp(UFT_PARAM_ALIASES[i].alias, alias) == 0) {
            // Check tool match
            if (UFT_PARAM_ALIASES[i].tool == NULL ||
                (tool != UFT_TOOL_INTERNAL && 
                 strcmp(UFT_PARAM_ALIASES[i].tool, 
                        tool == UFT_TOOL_GW ? "gw" : 
                        tool == UFT_TOOL_FE ? "fe" : "") == 0)) {
                return UFT_PARAM_ALIASES[i].canonical;
            }
        }
    }
    
    return alias;  // Return original if no alias found
}

int uft_params_get_aliases(const char *canonical, 
                           const char **aliases, int max_count) {
    if (!canonical || !aliases || max_count < 1) return 0;
    
    int count = 0;
    for (int i = 0; UFT_PARAM_ALIASES[i].canonical != NULL && count < max_count; i++) {
        if (strcmp(UFT_PARAM_ALIASES[i].canonical, canonical) == 0) {
            aliases[count++] = UFT_PARAM_ALIASES[i].alias;
        }
    }
    
    return count;
}

// ============================================================================
// SETTERS
// ============================================================================

static void *get_param_ptr(uft_canonical_params_t *params, const char *name) {
    if (!params || !name) return NULL;
    
    // Geometry
    if (strcmp(name, "geometry.cylinders") == 0) return &params->geometry.cylinders;
    if (strcmp(name, "geometry.heads") == 0) return &params->geometry.heads;
    if (strcmp(name, "geometry.sectors_per_track") == 0) return &params->geometry.sectors_per_track;
    if (strcmp(name, "geometry.sector_size") == 0) return &params->geometry.sector_size;
    if (strcmp(name, "geometry.cylinder_start") == 0) return &params->geometry.cylinder_start;
    if (strcmp(name, "geometry.cylinder_end") == 0) return &params->geometry.cylinder_end;
    if (strcmp(name, "geometry.head_mask") == 0) return &params->geometry.head_mask;
    if (strcmp(name, "geometry.sector_base") == 0) return &params->geometry.sector_base;
    if (strcmp(name, "geometry.interleave") == 0) return &params->geometry.interleave;
    if (strcmp(name, "geometry.skew") == 0) return &params->geometry.skew;
    
    // Timing
    if (strcmp(name, "timing.cell_time_ns") == 0) return &params->timing.cell_time_ns;
    if (strcmp(name, "timing.datarate_bps") == 0) return &params->timing.datarate_bps;
    if (strcmp(name, "timing.rpm") == 0) return &params->timing.rpm;
    if (strcmp(name, "timing.pll_phase_adjust") == 0) return &params->timing.pll_phase_adjust;
    if (strcmp(name, "timing.pll_period_adjust") == 0) return &params->timing.pll_period_adjust;
    if (strcmp(name, "timing.pll_period_min") == 0) return &params->timing.pll_period_min;
    if (strcmp(name, "timing.pll_period_max") == 0) return &params->timing.pll_period_max;
    if (strcmp(name, "timing.weak_threshold") == 0) return &params->timing.weak_threshold;
    
    // Operation
    if (strcmp(name, "operation.retries") == 0) return &params->operation.retries;
    if (strcmp(name, "operation.revolutions") == 0) return &params->operation.revolutions;
    if (strcmp(name, "operation.dry_run") == 0) return &params->operation.dry_run;
    if (strcmp(name, "operation.verify_after_write") == 0) return &params->operation.verify_after_write;
    if (strcmp(name, "operation.attempt_recovery") == 0) return &params->operation.attempt_recovery;
    if (strcmp(name, "operation.verbose") == 0) return &params->operation.verbose;
    
    return NULL;
}

int uft_params_set_int(uft_canonical_params_t *params, 
                       const char *name, int64_t value) {
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    void *ptr = get_param_ptr(params, canonical);
    
    if (!ptr) return -1;
    
    // Determine type and set
    if (strstr(canonical, "geometry.")) {
        *(int32_t*)ptr = (int32_t)value;
    } else if (strstr(canonical, "timing.cell_time") || strstr(canonical, "timing.rotation")) {
        *(uint64_t*)ptr = (uint64_t)value;
    } else if (strstr(canonical, "timing.datarate")) {
        *(uint32_t*)ptr = (uint32_t)value;
    } else {
        *(int32_t*)ptr = (int32_t)value;
    }
    
    params->is_dirty = true;
    uft_params_recompute(params);
    
    return 0;
}

int uft_params_set_double(uft_canonical_params_t *params,
                          const char *name, double value) {
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    void *ptr = get_param_ptr(params, canonical);
    
    if (!ptr) return -1;
    
    *(double*)ptr = value;
    params->is_dirty = true;
    uft_params_recompute(params);
    
    return 0;
}

int uft_params_set_bool(uft_canonical_params_t *params,
                        const char *name, bool value) {
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    void *ptr = get_param_ptr(params, canonical);
    
    if (!ptr) return -1;
    
    *(bool*)ptr = value;
    params->is_dirty = true;
    
    return 0;
}

// ============================================================================
// GETTERS
// ============================================================================

int64_t uft_params_get_int(const uft_canonical_params_t *params, 
                           const char *name) {
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    void *ptr = get_param_ptr((uft_canonical_params_t*)params, canonical);
    
    if (!ptr) return 0;
    
    if (strstr(canonical, "geometry.")) {
        return *(int32_t*)ptr;
    } else if (strstr(canonical, "timing.cell_time") || strstr(canonical, "timing.rotation")) {
        return (int64_t)*(uint64_t*)ptr;
    } else if (strstr(canonical, "timing.datarate")) {
        return *(uint32_t*)ptr;
    }
    
    return *(int32_t*)ptr;
}

double uft_params_get_double(const uft_canonical_params_t *params,
                             const char *name) {
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    void *ptr = get_param_ptr((uft_canonical_params_t*)params, canonical);
    
    if (!ptr) return 0.0;
    
    return *(double*)ptr;
}

bool uft_params_get_bool(const uft_canonical_params_t *params,
                         const char *name) {
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    void *ptr = get_param_ptr((uft_canonical_params_t*)params, canonical);
    
    if (!ptr) return false;
    
    return *(bool*)ptr;
}

// ============================================================================
// TOOL TRANSLATION
// ============================================================================

int uft_params_to_cli(const uft_canonical_params_t *params,
                      uft_tool_e tool,
                      char *buffer, size_t buffer_size) {
    if (!params || !buffer || buffer_size < 1) return -1;
    
    buffer[0] = '\0';
    size_t pos = 0;
    
    switch (tool) {
        case UFT_TOOL_GW: {
            int cyl_start = params->geometry.cylinder_start;
            int cyl_end = params->geometry.cylinder_end;
            if (cyl_end < 0) cyl_end = params->geometry.cylinders - 1;
            
            pos += snprintf(buffer + pos, buffer_size - pos,
                           "--cyls %d:%d ", cyl_start, cyl_end);
            
            // Heads
            if (params->geometry.head_mask == 0x01) {
                pos += snprintf(buffer + pos, buffer_size - pos, "--heads 0 ");
            } else if (params->geometry.head_mask == 0x02) {
                pos += snprintf(buffer + pos, buffer_size - pos, "--heads 1 ");
            }
            // 0x03 = both heads, no flag needed
            
            // Revolutions
            if (params->operation.revolutions != 3) {
                pos += snprintf(buffer + pos, buffer_size - pos,
                               "--revs %d ", params->operation.revolutions);
            }
            
            // Retries
            if (params->operation.retries != 3) {
                pos += snprintf(buffer + pos, buffer_size - pos,
                               "--retries %d ", params->operation.retries);
            }
            break;
        }
        
        case UFT_TOOL_FE: {
            int cyl_start = params->geometry.cylinder_start;
            int cyl_end = params->geometry.cylinder_end;
            if (cyl_end < 0) cyl_end = params->geometry.cylinders - 1;
            
            pos += snprintf(buffer + pos, buffer_size - pos,
                           "-c %d-%d ", cyl_start, cyl_end);
            
            // Heads
            if (params->geometry.head_mask == 0x01) {
                pos += snprintf(buffer + pos, buffer_size - pos, "-h 0 ");
            } else if (params->geometry.head_mask == 0x02) {
                pos += snprintf(buffer + pos, buffer_size - pos, "-h 1 ");
            }
            
            // Revolutions
            if (params->operation.revolutions != 3) {
                pos += snprintf(buffer + pos, buffer_size - pos,
                               "--revolutions %d ", params->operation.revolutions);
            }
            break;
        }
        
        default:
            // Unknown tool
            break;
    }
    
    return (int)pos;
}

const char *uft_params_get_tool_flag(const char *canonical, uft_tool_e tool) {
    for (int i = 0; TOOL_FLAGS[i].canonical != NULL; i++) {
        if (TOOL_FLAGS[i].tool == tool &&
            strcmp(TOOL_FLAGS[i].canonical, canonical) == 0) {
            return TOOL_FLAGS[i].flag;
        }
    }
    return NULL;
}

// ============================================================================
// GUI FORMATTING
// ============================================================================

const char *uft_params_format_for_gui(const uft_canonical_params_t *params,
                                      const char *name,
                                      char *buffer, size_t buffer_size) {
    if (!params || !name || !buffer || buffer_size < 1) return "";
    
    const char *canonical = uft_params_resolve_alias(name, UFT_TOOL_INTERNAL);
    
    // Time: ns -> µs for GUI
    if (strcmp(canonical, "timing.cell_time_ns") == 0) {
        double us = params->timing.cell_time_ns / 1000.0;
        snprintf(buffer, buffer_size, "%.2f µs", us);
        return buffer;
    }
    
    // Ratios: 0.0-1.0 -> 0-100% for GUI
    if (strcmp(canonical, "timing.pll_phase_adjust") == 0) {
        snprintf(buffer, buffer_size, "%.0f%%", params->timing.pll_phase_adjust * 100);
        return buffer;
    }
    if (strcmp(canonical, "timing.pll_period_adjust") == 0) {
        snprintf(buffer, buffer_size, "%.0f%%", params->timing.pll_period_adjust * 100);
        return buffer;
    }
    if (strcmp(canonical, "timing.pll_period_min") == 0) {
        snprintf(buffer, buffer_size, "%.0f%%", params->timing.pll_period_min * 100);
        return buffer;
    }
    if (strcmp(canonical, "timing.pll_period_max") == 0) {
        snprintf(buffer, buffer_size, "%.0f%%", params->timing.pll_period_max * 100);
        return buffer;
    }
    
    // Datarate: bps -> kbps for GUI
    if (strcmp(canonical, "timing.datarate_bps") == 0) {
        snprintf(buffer, buffer_size, "%.0f kbps", params->timing.datarate_bps / 1000.0);
        return buffer;
    }
    
    // Bytes: show KB/MB for large values
    if (strcmp(canonical, "geometry.total_bytes") == 0) {
        if (params->geometry.total_bytes >= 1024 * 1024) {
            snprintf(buffer, buffer_size, "%.2f MB", 
                     params->geometry.total_bytes / (1024.0 * 1024.0));
        } else if (params->geometry.total_bytes >= 1024) {
            snprintf(buffer, buffer_size, "%.2f KB",
                     params->geometry.total_bytes / 1024.0);
        } else {
            snprintf(buffer, buffer_size, "%lld bytes",
                     (long long)params->geometry.total_bytes);
        }
        return buffer;
    }
    
    // Default: return raw value
    int64_t val = uft_params_get_int(params, canonical);
    snprintf(buffer, buffer_size, "%lld", (long long)val);
    return buffer;
}

// ============================================================================
// PRESETS
// ============================================================================

int uft_params_list_presets(const char **names, int max_count) {
    int count = 0;
    for (int i = 0; PRESETS[i].name != NULL && count < max_count; i++) {
        names[count++] = PRESETS[i].name;
    }
    return count;
}

// ============================================================================
// SERIALIZATION
// ============================================================================

int uft_params_to_json(const uft_canonical_params_t *params,
                       char *buffer, size_t buffer_size) {
    if (!params || !buffer || buffer_size < 100) return -1;
    
    int n = snprintf(buffer, buffer_size,
        "{\n"
        "  \"version\": %u,\n"
        "  \"source\": \"%s\",\n"
        "  \"geometry\": {\n"
        "    \"cylinders\": %d,\n"
        "    \"heads\": %d,\n"
        "    \"sectors_per_track\": %d,\n"
        "    \"sector_size\": %d,\n"
        "    \"cylinder_start\": %d,\n"
        "    \"cylinder_end\": %d,\n"
        "    \"total_bytes\": %lld\n"
        "  },\n"
        "  \"timing\": {\n"
        "    \"cell_time_ns\": %llu,\n"
        "    \"datarate_bps\": %u,\n"
        "    \"rpm\": %.1f,\n"
        "    \"pll_phase_adjust\": %.3f,\n"
        "    \"pll_period_adjust\": %.3f\n"
        "  },\n"
        "  \"operation\": {\n"
        "    \"retries\": %d,\n"
        "    \"revolutions\": %d,\n"
        "    \"verify_after_write\": %s\n"
        "  }\n"
        "}\n",
        params->version,
        params->source,
        params->geometry.cylinders,
        params->geometry.heads,
        params->geometry.sectors_per_track,
        params->geometry.sector_size,
        params->geometry.cylinder_start,
        params->geometry.cylinder_end,
        (long long)params->geometry.total_bytes,
        (unsigned long long)params->timing.cell_time_ns,
        params->timing.datarate_bps,
        params->timing.rpm,
        params->timing.pll_phase_adjust,
        params->timing.pll_period_adjust,
        params->operation.retries,
        params->operation.revolutions,
        params->operation.verify_after_write ? "true" : "false"
    );
    
    return n;
}
