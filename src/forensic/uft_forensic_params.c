/**
 * @file uft_forensic_params.c
 * @brief GUI Parameter Definitions
 */

#include "uft/forensic/uft_forensic_params.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// XCOPY PARAMETER DEFINITIONS
// ============================================================================

static const char* XCOPY_MODES[] = {"Normal", "Raw", "Flux", "Nibble", "Verify", "Analyze", "Forensic"};
static const char* VERIFY_MODES[] = {"None", "Read", "Compare", "CRC", "Hash"};

const uft_param_def_t UFT_PARAM_XCOPY_MODE = {
    .id = "xcopy.mode", .name = "Copy Mode", .description = "Copy operation mode",
    .category = "xcopy", .type = UFT_PARAM_ENUM, .widget = UFT_WIDGET_COMBOBOX,
    .constraint.enum_values = {XCOPY_MODES, 7},
    .default_value.int_val = 0, .required = true, .advanced = false, .display_order = 1
};

const uft_param_def_t UFT_PARAM_XCOPY_START_TRACK = {
    .id = "xcopy.start_track", .name = "Start Track", .description = "First track to copy",
    .category = "xcopy", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {0, 84, 1},
    .default_value.int_val = 0, .required = true, .advanced = false, .display_order = 2
};

const uft_param_def_t UFT_PARAM_XCOPY_END_TRACK = {
    .id = "xcopy.end_track", .name = "End Track", .description = "Last track to copy",
    .category = "xcopy", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {0, 84, 1},
    .default_value.int_val = 79, .required = true, .advanced = false, .display_order = 3
};

const uft_param_def_t UFT_PARAM_XCOPY_SIDES = {
    .id = "xcopy.sides", .name = "Sides", .description = "Number of sides (1 or 2)",
    .category = "xcopy", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {1, 2, 1},
    .default_value.int_val = 2, .required = true, .advanced = false, .display_order = 4
};

const uft_param_def_t UFT_PARAM_XCOPY_RETRIES = {
    .id = "xcopy.retries", .name = "Retries", .description = "Read retries per track",
    .category = "xcopy", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {0, 20, 1},
    .default_value.int_val = 3, .required = false, .advanced = false, .display_order = 5
};

const uft_param_def_t UFT_PARAM_XCOPY_VERIFY = {
    .id = "xcopy.verify", .name = "Verify Mode", .description = "Post-copy verification",
    .category = "xcopy", .type = UFT_PARAM_ENUM, .widget = UFT_WIDGET_COMBOBOX,
    .constraint.enum_values = {VERIFY_MODES, 5},
    .default_value.int_val = 0, .required = false, .advanced = false, .display_order = 6
};

const uft_param_def_t UFT_PARAM_XCOPY_HALFTRACKS = {
    .id = "xcopy.halftracks", .name = "Include Halftracks", .description = "Copy halftrack data",
    .category = "xcopy", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = false, .required = false, .advanced = true, .display_order = 7
};

const uft_param_def_t UFT_PARAM_XCOPY_REVOLUTIONS = {
    .id = "xcopy.revolutions", .name = "Revolutions", .description = "Flux capture revolutions",
    .category = "xcopy", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {1, 10, 1},
    .default_value.int_val = 3, .required = false, .advanced = true, .display_order = 8
};

const uft_param_def_t UFT_PARAM_XCOPY_IGNORE_ERRORS = {
    .id = "xcopy.ignore_errors", .name = "Ignore Errors", .description = "Continue on read errors",
    .category = "xcopy", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 9
};

// ============================================================================
// RECOVERY PARAMETER DEFINITIONS
// ============================================================================

const uft_param_def_t UFT_PARAM_RECOV_MAX_RETRIES = {
    .id = "recovery.max_retries", .name = "Max Retries", .description = "Maximum recovery attempts",
    .category = "recovery", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {1, 20, 1},
    .default_value.int_val = 5, .required = false, .advanced = false, .display_order = 1
};

const uft_param_def_t UFT_PARAM_RECOV_MIN_CONFIDENCE = {
    .id = "recovery.min_confidence", .name = "Min Confidence", .description = "Minimum acceptance confidence",
    .category = "recovery", .type = UFT_PARAM_DOUBLE, .widget = UFT_WIDGET_SLIDER,
    .constraint.double_range = {0.5, 1.0, 0.05},
    .default_value.double_val = 0.90, .required = false, .advanced = true, .display_order = 2
};

const uft_param_def_t UFT_PARAM_RECOV_CRC_CORRECT = {
    .id = "recovery.crc_correction", .name = "CRC Correction", .description = "Enable CRC error correction",
    .category = "recovery", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 3
};

const uft_param_def_t UFT_PARAM_RECOV_MAX_CRC_BITS = {
    .id = "recovery.max_crc_bits", .name = "Max CRC Bits", .description = "Max bits to correct",
    .category = "recovery", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {1, 8, 1},
    .default_value.int_val = 2, .required = false, .advanced = true, .display_order = 4
};

const uft_param_def_t UFT_PARAM_RECOV_WEAK_BIT = {
    .id = "recovery.weak_bit", .name = "Weak Bit Recovery", .description = "Enable weak bit consensus",
    .category = "recovery", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 5
};

const uft_param_def_t UFT_PARAM_RECOV_MULTI_REV = {
    .id = "recovery.multi_rev", .name = "Multi-Revolution", .description = "Use multiple reads",
    .category = "recovery", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 6
};

const uft_param_def_t UFT_PARAM_RECOV_REPAIR_BAM = {
    .id = "recovery.repair_bam", .name = "Repair BAM", .description = "Auto-repair BAM/allocation",
    .category = "recovery", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = false, .required = false, .advanced = false, .display_order = 7
};

const uft_param_def_t UFT_PARAM_RECOV_REPAIR_DIR = {
    .id = "recovery.repair_dir", .name = "Repair Directory", .description = "Auto-repair directory",
    .category = "recovery", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = false, .required = false, .advanced = false, .display_order = 8
};

const uft_param_def_t UFT_PARAM_RECOV_FILL_PATTERN = {
    .id = "recovery.fill_pattern", .name = "Fill Pattern", .description = "Byte for unreadable sectors (hex)",
    .category = "recovery", .type = UFT_PARAM_INT, .widget = UFT_WIDGET_SPINBOX,
    .constraint.int_range = {0, 255, 1},
    .default_value.int_val = 0x00, .required = false, .advanced = true, .display_order = 9
};

const uft_param_def_t UFT_PARAM_RECOV_AUDIT_LOG = {
    .id = "recovery.audit_log", .name = "Audit Log", .description = "Enable full audit logging",
    .category = "recovery", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = false, .required = false, .advanced = true, .display_order = 10
};

// ============================================================================
// PROTECTION PARAMETER DEFINITIONS
// ============================================================================

const uft_param_def_t UFT_PARAM_PROT_DETECT = {
    .id = "protection.detect", .name = "Detect Protection", .description = "Enable protection detection",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 1
};

const uft_param_def_t UFT_PARAM_PROT_DEEP_SCAN = {
    .id = "protection.deep_scan", .name = "Deep Scan", .description = "Full protection analysis",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = false, .required = false, .advanced = true, .display_order = 2
};

const uft_param_def_t UFT_PARAM_PROT_WEAK_BITS = {
    .id = "protection.weak_bits", .name = "Weak Bit Detection", .description = "Multi-rev weak bit analysis",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 3
};

const uft_param_def_t UFT_PARAM_PROT_SYNC_ANALYSIS = {
    .id = "protection.sync_analysis", .name = "Sync Analysis", .description = "Analyze sync patterns",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 4
};

const uft_param_def_t UFT_PARAM_PROT_HALFTRACK = {
    .id = "protection.halftrack", .name = "Halftrack Check", .description = "Check halftrack data",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = false, .required = false, .advanced = true, .display_order = 5
};

const uft_param_def_t UFT_PARAM_PROT_TRACK_LENGTH = {
    .id = "protection.track_length", .name = "Track Length Analysis", .description = "Check track length variance",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 6
};

const uft_param_def_t UFT_PARAM_PROT_FINGERPRINT = {
    .id = "protection.fingerprint", .name = "Fingerprint Match", .description = "Match against database",
    .category = "protection", .type = UFT_PARAM_BOOL, .widget = UFT_WIDGET_CHECKBOX,
    .default_value.bool_val = true, .required = false, .advanced = false, .display_order = 7
};

// ============================================================================
// PARAMETER REGISTRY
// ============================================================================

static const uft_param_def_t* ALL_PARAMS[] = {
    // XCopy
    &UFT_PARAM_XCOPY_MODE, &UFT_PARAM_XCOPY_START_TRACK, &UFT_PARAM_XCOPY_END_TRACK,
    &UFT_PARAM_XCOPY_SIDES, &UFT_PARAM_XCOPY_RETRIES, &UFT_PARAM_XCOPY_VERIFY,
    &UFT_PARAM_XCOPY_HALFTRACKS, &UFT_PARAM_XCOPY_REVOLUTIONS, &UFT_PARAM_XCOPY_IGNORE_ERRORS,
    // Recovery
    &UFT_PARAM_RECOV_MAX_RETRIES, &UFT_PARAM_RECOV_MIN_CONFIDENCE, &UFT_PARAM_RECOV_CRC_CORRECT,
    &UFT_PARAM_RECOV_MAX_CRC_BITS, &UFT_PARAM_RECOV_WEAK_BIT, &UFT_PARAM_RECOV_MULTI_REV,
    &UFT_PARAM_RECOV_REPAIR_BAM, &UFT_PARAM_RECOV_REPAIR_DIR, &UFT_PARAM_RECOV_FILL_PATTERN,
    &UFT_PARAM_RECOV_AUDIT_LOG,
    // Protection
    &UFT_PARAM_PROT_DETECT, &UFT_PARAM_PROT_DEEP_SCAN, &UFT_PARAM_PROT_WEAK_BITS,
    &UFT_PARAM_PROT_SYNC_ANALYSIS, &UFT_PARAM_PROT_HALFTRACK, &UFT_PARAM_PROT_TRACK_LENGTH,
    &UFT_PARAM_PROT_FINGERPRINT
};

static const int PARAM_COUNT = sizeof(ALL_PARAMS) / sizeof(ALL_PARAMS[0]);

// ============================================================================
// API IMPLEMENTATION
// ============================================================================

int uft_param_get_definitions(const uft_param_def_t** defs, int* count) {
    if (defs) *defs = (const uft_param_def_t*)ALL_PARAMS;
    if (count) *count = PARAM_COUNT;
    return 0;
}

const uft_param_def_t* uft_param_get_def(const char* id) {
    if (!id) return NULL;
    for (int i = 0; i < PARAM_COUNT; i++) {
        if (strcmp(ALL_PARAMS[i]->id, id) == 0) {
            return ALL_PARAMS[i];
        }
    }
    return NULL;
}

int uft_param_get_category(const char* category, const uft_param_def_t** defs, int* count) {
    if (!category || !defs || !count) return -1;
    
    static const uft_param_def_t* filtered[32];
    int n = 0;
    
    for (int i = 0; i < PARAM_COUNT && n < 32; i++) {
        if (strcmp(ALL_PARAMS[i]->category, category) == 0) {
            filtered[n++] = ALL_PARAMS[i];
        }
    }
    
    *defs = (const uft_param_def_t*)filtered;
    *count = n;
    return 0;
}

uft_param_set_t* uft_param_set_create(void) {
    uft_param_set_t* set = calloc(1, sizeof(uft_param_set_t));
    if (set) {
        set->capacity = 32;
        set->params = calloc(set->capacity, sizeof(uft_param_value_t));
    }
    return set;
}

void uft_param_set_destroy(uft_param_set_t* set) {
    if (!set) return;
    for (int i = 0; i < set->count; i++) {
        if (set->params[i].type == UFT_PARAM_STRING && set->params[i].value.string_val) {
            free(set->params[i].value.string_val);
        }
    }
    free(set->params);
    free(set);
}

int uft_param_set_int(uft_param_set_t* set, const char* id, int value) {
    if (!set || !id || set->count >= set->capacity) return -1;
    
    uft_param_value_t* p = &set->params[set->count++];
    p->id = id;
    p->type = UFT_PARAM_INT;
    p->value.int_val = value;
    p->is_set = true;
    return 0;
}

int uft_param_set_bool(uft_param_set_t* set, const char* id, bool value) {
    if (!set || !id || set->count >= set->capacity) return -1;
    
    uft_param_value_t* p = &set->params[set->count++];
    p->id = id;
    p->type = UFT_PARAM_BOOL;
    p->value.bool_val = value;
    p->is_set = true;
    return 0;
}

int uft_param_set_double(uft_param_set_t* set, const char* id, double value) {
    if (!set || !id || set->count >= set->capacity) return -1;
    
    uft_param_value_t* p = &set->params[set->count++];
    p->id = id;
    p->type = UFT_PARAM_DOUBLE;
    p->value.double_val = value;
    p->is_set = true;
    return 0;
}

bool uft_param_get_int(const uft_param_set_t* set, const char* id, int* value) {
    if (!set || !id || !value) return false;
    
    for (int i = 0; i < set->count; i++) {
        if (strcmp(set->params[i].id, id) == 0) {
            *value = set->params[i].value.int_val;
            return true;
        }
    }
    return false;
}

int uft_param_set_to_json(const uft_param_set_t* set, char* buffer, size_t size) {
    if (!set || !buffer || size == 0) return -1;
    
    int pos = snprintf(buffer, size, "{\n");
    
    for (int i = 0; i < set->count && pos < (int)size - 10; i++) {
        const uft_param_value_t* p = &set->params[i];
        
        switch (p->type) {
            case UFT_PARAM_BOOL:
                pos += snprintf(buffer + pos, size - pos, "  \"%s\": %s%s\n",
                               p->id, p->value.bool_val ? "true" : "false",
                               i < set->count - 1 ? "," : "");
                break;
            case UFT_PARAM_INT:
                pos += snprintf(buffer + pos, size - pos, "  \"%s\": %d%s\n",
                               p->id, p->value.int_val,
                               i < set->count - 1 ? "," : "");
                break;
            case UFT_PARAM_DOUBLE:
                pos += snprintf(buffer + pos, size - pos, "  \"%s\": %.3f%s\n",
                               p->id, p->value.double_val,
                               i < set->count - 1 ? "," : "");
                break;
            default:
                break;
        }
    }
    
    pos += snprintf(buffer + pos, size - pos, "}\n");
    return pos;
}
