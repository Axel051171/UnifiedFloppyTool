/**
 * @file uft_param_validator.c
 * @brief Parameter Conflicts Validator implementation
 * 
 * P2-008: Cross-parameter validation
 */

#include "uft/core/uft_param_validator.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Parameter Set
 * ============================================================================ */

void param_set_init(param_set_t *ps) {
    if (!ps) return;
    memset(ps, 0, sizeof(*ps));
}

static param_def_t* find_param(param_set_t *ps, const char *name) {
    for (int i = 0; i < ps->param_count; i++) {
        if (strcmp(ps->params[i].name, name) == 0) {
            return &ps->params[i];
        }
    }
    return NULL;
}

static const param_def_t* find_param_const(const param_set_t *ps, 
                                           const char *name) {
    for (int i = 0; i < ps->param_count; i++) {
        if (strcmp(ps->params[i].name, name) == 0) {
            return &ps->params[i];
        }
    }
    return NULL;
}

int param_set_add(param_set_t *ps, const char *name, param_type_t type) {
    if (!ps || !name || ps->param_count >= PARAM_MAX_PARAMS) {
        return -1;
    }
    
    /* Check for duplicate */
    if (find_param(ps, name)) {
        return -1;
    }
    
    param_def_t *p = &ps->params[ps->param_count++];
    memset(p, 0, sizeof(*p));
    strncpy(p->name, name, PARAM_MAX_NAME_LEN - 1);
    p->type = type;
    
    return ps->param_count - 1;
}

int param_set_value_int(param_set_t *ps, const char *name, int64_t value) {
    param_def_t *p = find_param(ps, name);
    if (!p) {
        param_set_add(ps, name, PARAM_TYPE_INT);
        p = find_param(ps, name);
    }
    if (!p) return -1;
    
    p->value.i = value;
    p->is_set = true;
    return 0;
}

int param_set_value_float(param_set_t *ps, const char *name, double value) {
    param_def_t *p = find_param(ps, name);
    if (!p) {
        param_set_add(ps, name, PARAM_TYPE_FLOAT);
        p = find_param(ps, name);
    }
    if (!p) return -1;
    
    p->value.f = value;
    p->is_set = true;
    return 0;
}

int param_set_value_bool(param_set_t *ps, const char *name, bool value) {
    param_def_t *p = find_param(ps, name);
    if (!p) {
        param_set_add(ps, name, PARAM_TYPE_BOOL);
        p = find_param(ps, name);
    }
    if (!p) return -1;
    
    p->value.b = value;
    p->is_set = true;
    return 0;
}

int param_set_value_string(param_set_t *ps, const char *name, const char *value) {
    param_def_t *p = find_param(ps, name);
    if (!p) {
        param_set_add(ps, name, PARAM_TYPE_STRING);
        p = find_param(ps, name);
    }
    if (!p) return -1;
    
    p->value.s = value;
    p->is_set = true;
    return 0;
}

bool param_set_get_int(const param_set_t *ps, const char *name, int64_t *out) {
    const param_def_t *p = find_param_const(ps, name);
    if (!p || !p->is_set || p->type != PARAM_TYPE_INT) {
        return false;
    }
    if (out) *out = p->value.i;
    return true;
}

bool param_set_get_float(const param_set_t *ps, const char *name, double *out) {
    const param_def_t *p = find_param_const(ps, name);
    if (!p || !p->is_set || p->type != PARAM_TYPE_FLOAT) {
        return false;
    }
    if (out) *out = p->value.f;
    return true;
}

bool param_set_get_bool(const param_set_t *ps, const char *name, bool *out) {
    const param_def_t *p = find_param_const(ps, name);
    if (!p || !p->is_set || p->type != PARAM_TYPE_BOOL) {
        return false;
    }
    if (out) *out = p->value.b;
    return true;
}

/* ============================================================================
 * Validator
 * ============================================================================ */

void param_validator_init(param_validator_t *v) {
    if (!v) return;
    memset(v, 0, sizeof(*v));
    
    v->rule_capacity = 64;
    v->rules = calloc(v->rule_capacity, sizeof(conflict_rule_t));
}

void param_validator_free(param_validator_t *v) {
    if (!v) return;
    free(v->rules);
    v->rules = NULL;
    v->rule_count = 0;
}

int param_validator_add_rule(param_validator_t *v, const conflict_rule_t *rule) {
    if (!v || !rule) return -1;
    
    if (v->rule_count >= v->rule_capacity) {
        int new_cap = v->rule_capacity * 2;
        conflict_rule_t *new_rules = realloc(v->rules, 
                                             new_cap * sizeof(conflict_rule_t));
        if (!new_rules) return -1;
        v->rules = new_rules;
        v->rule_capacity = new_cap;
    }
    
    v->rules[v->rule_count++] = *rule;
    return v->rule_count - 1;
}

/* ============================================================================
 * Built-in Rules
 * ============================================================================ */

static const conflict_rule_t uft_floppy_rules[] = {
    /* Track range */
    {
        .param1 = "track_start",
        .param2 = "track_end",
        .type = CONFLICT_TYPE_RANGE,
        .severity = CONFLICT_ERROR,
        .message = "track_start must be <= track_end"
    },
    /* Sector size vs format */
    {
        .param1 = "sector_size",
        .param2 = "format",
        .type = CONFLICT_TYPE_FORMAT_MISMATCH,
        .severity = CONFLICT_ERROR,
        .message = "sector_size incompatible with format"
    },
    /* Sides vs heads */
    {
        .param1 = "sides",
        .param2 = "heads",
        .type = CONFLICT_TYPE_LOGICAL,
        .severity = CONFLICT_WARNING,
        .message = "sides and heads should match"
    },
    /* Bitrate vs density */
    {
        .param1 = "bitrate",
        .param2 = "density",
        .type = CONFLICT_TYPE_LOGICAL,
        .severity = CONFLICT_ERROR,
        .message = "bitrate incompatible with density"
    },
};

static const conflict_rule_t mfm_rules[] = {
    /* Gap lengths */
    {
        .param1 = "gap3_length",
        .param2 = "sector_size",
        .type = CONFLICT_TYPE_LOGICAL,
        .severity = CONFLICT_WARNING,
        .message = "gap3 may be too short for sector size"
    },
    /* Data rate */
    {
        .param1 = "data_rate",
        .param2 = "rpm",
        .type = CONFLICT_TYPE_HARDWARE_LIMIT,
        .severity = CONFLICT_ERROR,
        .message = "data_rate exceeds hardware limit for RPM"
    },
};

static const conflict_rule_t gcr_rules[] = {
    /* Track density zones */
    {
        .param1 = "speed_zone",
        .param2 = "track",
        .type = CONFLICT_TYPE_FORMAT_MISMATCH,
        .severity = CONFLICT_WARNING,
        .message = "speed_zone unusual for track number"
    },
};

static const conflict_rule_t flux_rules[] = {
    /* Sample rate */
    {
        .param1 = "sample_rate",
        .param2 = "bitrate",
        .type = CONFLICT_TYPE_LOGICAL,
        .severity = CONFLICT_ERROR,
        .message = "sample_rate should be >= 8x bitrate"
    },
    /* Index pulse */
    {
        .param1 = "use_index",
        .param2 = "revolutions",
        .type = CONFLICT_TYPE_DEPENDENCY,
        .severity = CONFLICT_WARNING,
        .message = "revolutions requires use_index=true"
    },
};

const conflict_rule_t* param_get_floppy_rules(int *count) {
    if (count) *count = sizeof(uft_floppy_rules) / sizeof(uft_floppy_rules[0]);
    return uft_floppy_rules;
}

const conflict_rule_t* param_get_mfm_rules(int *count) {
    if (count) *count = sizeof(mfm_rules) / sizeof(mfm_rules[0]);
    return mfm_rules;
}

const conflict_rule_t* param_get_gcr_rules(int *count) {
    if (count) *count = sizeof(gcr_rules) / sizeof(gcr_rules[0]);
    return gcr_rules;
}

const conflict_rule_t* param_get_flux_rules(int *count) {
    if (count) *count = sizeof(flux_rules) / sizeof(flux_rules[0]);
    return flux_rules;
}

void param_validator_load_format_rules(param_validator_t *v, 
                                       const char *format_name) {
    if (!v || !format_name) return;
    
    if (strcmp(format_name, "MFM") == 0 ||
        strcmp(format_name, "IBM") == 0) {
        int count;
        v->format_rules = param_get_mfm_rules(&count);
        v->format_rule_count = count;
    } else if (strcmp(format_name, "GCR") == 0 ||
               strcmp(format_name, "C64") == 0) {
        int count;
        v->format_rules = param_get_gcr_rules(&count);
        v->format_rule_count = count;
    } else if (strcmp(format_name, "FLUX") == 0) {
        int count;
        v->format_rules = param_get_flux_rules(&count);
        v->format_rule_count = count;
    }
}

/* ============================================================================
 * Validation
 * ============================================================================ */

static void check_rule(const conflict_rule_t *rule,
                       const param_set_t *ps,
                       param_validation_result_t *result) {
    const param_def_t *p1 = find_param_const(ps, rule->param1);
    const param_def_t *p2 = find_param_const(ps, rule->param2);
    
    /* Skip if parameters not set */
    if (!p1 || !p1->is_set) return;
    if (rule->param2[0] && (!p2 || !p2->is_set)) return;
    
    /* Check condition if present */
    if (rule->condition && !rule->condition(ps)) {
        return;  /* Condition not met, rule doesn't apply */
    }
    
    bool conflict = false;
    
    switch (rule->type) {
        case CONFLICT_TYPE_RANGE:
            if (p1->type == PARAM_TYPE_INT && p2->type == PARAM_TYPE_INT) {
                conflict = p1->value.i > p2->value.i;
            }
            break;
            
        case CONFLICT_TYPE_MUTUAL_EXCLUSIVE:
            /* Both set = conflict */
            conflict = (p1->is_set && p2 && p2->is_set);
            break;
            
        case CONFLICT_TYPE_DEPENDENCY:
            /* p1 set but p2 not set = conflict */
            conflict = (p1->is_set && p1->value.b && 
                       (!p2 || !p2->is_set || !p2->value.b));
            break;
            
        case CONFLICT_TYPE_LOGICAL:
            /* Format-specific logic would go here */
            /* For now, just check basic consistency */
            if (strcmp(rule->param1, "bitrate") == 0 &&
                strcmp(rule->param2, "density") == 0) {
                int64_t bitrate = p1->value.i;
                int64_t density = p2->value.i;
                
                /* DD = 250kbps, HD = 500kbps */
                if (density == 0 && bitrate > 300000) conflict = true;
                if (density == 1 && bitrate < 400000) conflict = true;
            }
            break;
            
        default:
            break;
    }
    
    if (conflict && result->conflict_count < PARAM_MAX_CONFLICTS) {
        param_conflict_t *c = &result->conflicts[result->conflict_count++];
        c->type = rule->type;
        c->severity = rule->severity;
        strncpy(c->param1, rule->param1, PARAM_MAX_NAME_LEN - 1);
        if (rule->param2[0]) {
            strncpy(c->param2, rule->param2, PARAM_MAX_NAME_LEN - 1);
        }
        strncpy(c->message, rule->message, sizeof(c->message) - 1);
        
        if (rule->severity >= CONFLICT_ERROR) {
            result->error_count++;
        } else {
            result->warning_count++;
        }
    }
}

param_validation_result_t param_validator_validate(const param_validator_t *v,
                                                   const param_set_t *ps) {
    param_validation_result_t result = {0};
    result.valid = true;
    
    if (!v || !ps) {
        return result;
    }
    
    /* Check built-in floppy rules */
    int uft_floppy_count;
    const conflict_rule_t *floppy = param_get_floppy_rules(&uft_floppy_count);
    for (int i = 0; i < uft_floppy_count; i++) {
        check_rule(&floppy[i], ps, &result);
    }
    
    /* Check validator rules */
    for (int i = 0; i < v->rule_count; i++) {
        check_rule(&v->rules[i], ps, &result);
    }
    
    /* Check format-specific rules */
    for (int i = 0; i < v->format_rule_count; i++) {
        check_rule(&v->format_rules[i], ps, &result);
    }
    
    result.valid = (result.error_count == 0);
    return result;
}

int param_validator_auto_resolve(param_set_t *ps,
                                 const param_validation_result_t *result) {
    if (!ps || !result) return 0;
    
    int resolved = 0;
    
    for (int i = 0; i < result->conflict_count; i++) {
        const param_conflict_t *c = &result->conflicts[i];
        
        if (!c->can_auto_resolve) continue;
        
        param_def_t *p = find_param(ps, c->resolution_param);
        if (p) {
            p->value = c->resolution_value;
            resolved++;
        }
    }
    
    return resolved;
}

/* ============================================================================
 * Common Validation Functions
 * ============================================================================ */

bool param_validate_track_range(int start, int end, int max) {
    return start >= 0 && end >= start && end < max;
}

bool param_validate_sector_count(int sectors, const char *format) {
    if (!format) return false;
    
    if (strcmp(format, "D64") == 0 || strcmp(format, "C64") == 0) {
        return sectors >= 17 && sectors <= 21;
    }
    if (strcmp(format, "ADF") == 0 || strcmp(format, "AMIGA") == 0) {
        return sectors == 11 || sectors == 22;
    }
    if (strcmp(format, "IBM") == 0) {
        return sectors >= 8 && sectors <= 36;
    }
    
    return sectors > 0 && sectors <= 64;
}

bool param_validate_bitrate(int bitrate, const char *encoding) {
    if (!encoding) return false;
    
    if (strcmp(encoding, "MFM") == 0) {
        return bitrate >= 125000 && bitrate <= 1000000;
    }
    if (strcmp(encoding, "FM") == 0) {
        return bitrate >= 62500 && bitrate <= 500000;
    }
    if (strcmp(encoding, "GCR") == 0) {
        return bitrate >= 200000 && bitrate <= 400000;
    }
    
    return bitrate > 0;
}

bool param_validate_rpm(int rpm, const char *drive_type) {
    if (!drive_type) return false;
    
    if (strcmp(drive_type, "5.25") == 0) {
        return rpm == 300 || rpm == 360;
    }
    if (strcmp(drive_type, "3.5") == 0) {
        return rpm == 300;
    }
    if (strcmp(drive_type, "8") == 0) {
        return rpm == 360;
    }
    if (strcmp(drive_type, "1541") == 0) {
        return rpm >= 280 && rpm <= 320;
    }
    
    return rpm >= 200 && rpm <= 400;
}

/* ============================================================================
 * Formatting
 * ============================================================================ */

int param_conflict_to_string(const param_conflict_t *c, 
                             char *buffer, size_t size) {
    if (!c || !buffer || size == 0) return -1;
    
    const char *severity_str;
    switch (c->severity) {
        case CONFLICT_WARNING: severity_str = "WARNING"; break;
        case CONFLICT_ERROR: severity_str = "ERROR"; break;
        case CONFLICT_CRITICAL: severity_str = "CRITICAL"; break;
        default: severity_str = "INFO"; break;
    }
    
    return snprintf(buffer, size, "[%s] %s vs %s: %s",
                    severity_str, c->param1, c->param2, c->message);
}

int param_result_to_json(const param_validation_result_t *r,
                         char *buffer, size_t size) {
    if (!r || !buffer || size == 0) return -1;
    
    int pos = 0;
    
    pos += snprintf(buffer + pos, size - pos, 
                    "{\"valid\":%s,\"errors\":%d,\"warnings\":%d,\"conflicts\":[",
                    r->valid ? "true" : "false",
                    r->error_count, r->warning_count);
    
    for (int i = 0; i < r->conflict_count && pos < (int)size - 100; i++) {
        const param_conflict_t *c = &r->conflicts[i];
        
        if (i > 0) {
            buffer[pos++] = ',';
        }
        
        pos += snprintf(buffer + pos, size - pos,
                        "{\"param1\":\"%s\",\"param2\":\"%s\","
                        "\"severity\":%d,\"message\":\"%s\"}",
                        c->param1, c->param2, c->severity, c->message);
    }
    
    pos += snprintf(buffer + pos, size - pos, "]}");
    
    return pos;
}
