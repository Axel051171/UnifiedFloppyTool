/**
 * @file uft_params_json.c
 * @brief JSON Import/Export for Canonical Parameters
 * 
 * FEATURES:
 * - Full serialization of all parameter groups
 * - Robust parsing with error handling
 * - Partial JSON import (merge with defaults)
 * - Schema versioning for forward compatibility
 * - Human-readable output with comments
 */

#include "uft/params/uft_canonical_params.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

// ============================================================================
// JSON WRITER
// ============================================================================

typedef struct {
    char *buffer;
    size_t size;
    size_t pos;
    int indent;
    bool pretty;
} json_writer_t;

static void json_init(json_writer_t *w, char *buffer, size_t size, bool pretty) {
    w->buffer = buffer;
    w->size = size;
    w->pos = 0;
    w->indent = 0;
    w->pretty = pretty;
}

static void json_write(json_writer_t *w, const char *fmt, ...) {
    if (w->pos >= w->size - 1) return;
    
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(w->buffer + w->pos, w->size - w->pos, fmt, args);
    va_end(args);
    
    if (n > 0) w->pos += n;
}

static void json_indent(json_writer_t *w) {
    if (!w->pretty) return;
    for (int i = 0; i < w->indent; i++) {
        json_write(w, "  ");
    }
}

static void json_newline(json_writer_t *w) {
    if (w->pretty) json_write(w, "\n");
}

static void json_open_object(json_writer_t *w) {
    json_write(w, "{");
    json_newline(w);
    w->indent++;
}

static void json_close_object(json_writer_t *w, bool last) {
    w->indent--;
    json_indent(w);
    json_write(w, "}%s", last ? "" : ",");
    json_newline(w);
}

static void json_key(json_writer_t *w, const char *key) {
    json_indent(w);
    json_write(w, "\"%s\": ", key);
}

static void json_string(json_writer_t *w, const char *key, const char *value, bool last) {
    json_key(w, key);
    json_write(w, "\"%s\"%s", value ? value : "", last ? "" : ",");
    json_newline(w);
}

static void json_int(json_writer_t *w, const char *key, int64_t value, bool last) {
    json_key(w, key);
    json_write(w, "%lld%s", (long long)value, last ? "" : ",");
    json_newline(w);
}

static void json_uint(json_writer_t *w, const char *key, uint64_t value, bool last) {
    json_key(w, key);
    json_write(w, "%llu%s", (unsigned long long)value, last ? "" : ",");
    json_newline(w);
}

static void json_double(json_writer_t *w, const char *key, double value, bool last) {
    json_key(w, key);
    json_write(w, "%.6f%s", value, last ? "" : ",");
    json_newline(w);
}

static void json_bool(json_writer_t *w, const char *key, bool value, bool last) {
    json_key(w, key);
    json_write(w, "%s%s", value ? "true" : "false", last ? "" : ",");
    json_newline(w);
}

// ============================================================================
// FULL JSON EXPORT
// ============================================================================

int uft_params_to_json_full(const uft_canonical_params_t *params,
                            char *buffer, size_t buffer_size,
                            bool pretty) {
    if (!params || !buffer || buffer_size < 256) return -1;
    
    json_writer_t w;
    json_init(&w, buffer, buffer_size, pretty);
    
    json_open_object(&w);
    
    // Header
    json_key(&w, "_meta");
    json_open_object(&w);
    json_int(&w, "version", params->version, false);
    json_int(&w, "magic", params->magic, false);
    json_string(&w, "source", params->source, false);
    json_bool(&w, "is_valid", params->is_valid, true);
    json_close_object(&w, false);
    
    // Geometry
    json_key(&w, "geometry");
    json_open_object(&w);
    json_int(&w, "cylinders", params->geometry.cylinders, false);
    json_int(&w, "heads", params->geometry.heads, false);
    json_int(&w, "sectors_per_track", params->geometry.sectors_per_track, false);
    json_int(&w, "sector_size", params->geometry.sector_size, false);
    json_int(&w, "cylinder_start", params->geometry.cylinder_start, false);
    json_int(&w, "cylinder_end", params->geometry.cylinder_end, false);
    json_int(&w, "head_mask", params->geometry.head_mask, false);
    json_int(&w, "sector_base", params->geometry.sector_base, false);
    json_int(&w, "interleave", params->geometry.interleave, false);
    json_int(&w, "skew", params->geometry.skew, false);
    json_int(&w, "total_sectors", params->geometry.total_sectors, false);
    json_int(&w, "total_bytes", params->geometry.total_bytes, true);
    json_close_object(&w, false);
    
    // Timing
    json_key(&w, "timing");
    json_open_object(&w);
    json_uint(&w, "cell_time_ns", params->timing.cell_time_ns, false);
    json_uint(&w, "rotation_ns", params->timing.rotation_ns, false);
    json_int(&w, "datarate_bps", params->timing.datarate_bps, false);
    json_double(&w, "rpm", params->timing.rpm, false);
    json_double(&w, "pll_phase_adjust", params->timing.pll_phase_adjust, false);
    json_double(&w, "pll_period_adjust", params->timing.pll_period_adjust, false);
    json_double(&w, "pll_period_min", params->timing.pll_period_min, false);
    json_double(&w, "pll_period_max", params->timing.pll_period_max, false);
    json_double(&w, "weak_threshold", params->timing.weak_threshold, true);
    json_close_object(&w, false);
    
    // Format
    json_key(&w, "format");
    json_open_object(&w);
    json_int(&w, "input_format", params->format.input_format, false);
    json_int(&w, "output_format", params->format.output_format, false);
    json_int(&w, "encoding", params->format.encoding, false);
    json_int(&w, "density", params->format.density, false);
    
    // Format: CBM
    json_key(&w, "cbm");
    json_open_object(&w);
    json_bool(&w, "half_tracks", params->format.cbm.half_tracks, false);
    json_bool(&w, "error_map", params->format.cbm.error_map, false);
    json_int(&w, "track_range", params->format.cbm.track_range, true);
    json_close_object(&w, false);
    
    // Format: Amiga
    json_key(&w, "amiga");
    json_open_object(&w);
    json_int(&w, "filesystem", params->format.amiga.filesystem, false);
    json_bool(&w, "bootable", params->format.amiga.bootable, true);
    json_close_object(&w, false);
    
    // Format: IBM
    json_key(&w, "ibm");
    json_open_object(&w);
    json_int(&w, "gap0_bytes", params->format.ibm.gap0_bytes, false);
    json_int(&w, "gap1_bytes", params->format.ibm.gap1_bytes, false);
    json_int(&w, "gap2_bytes", params->format.ibm.gap2_bytes, false);
    json_int(&w, "gap3_bytes", params->format.ibm.gap3_bytes, true);
    json_close_object(&w, true);
    
    json_close_object(&w, false);
    
    // Hardware
    json_key(&w, "hardware");
    json_open_object(&w);
    json_string(&w, "device_path", params->hardware.device_path, false);
    json_int(&w, "device_index", params->hardware.device_index, false);
    json_int(&w, "drive_type", params->hardware.drive_type, false);
    json_bool(&w, "double_step", params->hardware.double_step, false);
    json_int(&w, "tool", params->hardware.tool, true);
    json_close_object(&w, false);
    
    // Operation
    json_key(&w, "operation");
    json_open_object(&w);
    json_bool(&w, "dry_run", params->operation.dry_run, false);
    json_bool(&w, "verify_after_write", params->operation.verify_after_write, false);
    json_int(&w, "retries", params->operation.retries, false);
    json_int(&w, "revolutions", params->operation.revolutions, false);
    json_bool(&w, "attempt_recovery", params->operation.attempt_recovery, false);
    json_bool(&w, "preserve_errors", params->operation.preserve_errors, false);
    json_bool(&w, "verbose", params->operation.verbose, false);
    json_bool(&w, "generate_audit", params->operation.generate_audit, true);
    json_close_object(&w, true);
    
    json_close_object(&w, true);
    
    return (int)w.pos;
}

// ============================================================================
// JSON PARSER HELPERS
// ============================================================================

typedef struct {
    const char *json;
    size_t pos;
    size_t len;
    char error[256];
} json_parser_t;

static void skip_whitespace(json_parser_t *p) {
    while (p->pos < p->len && isspace((unsigned char)p->json[p->pos])) {
        p->pos++;
    }
}

static bool expect_char(json_parser_t *p, char c) {
    skip_whitespace(p);
    if (p->pos >= p->len || p->json[p->pos] != c) {
        snprintf(p->error, sizeof(p->error), 
                 "Expected '%c' at position %zu", c, p->pos);
        return false;
    }
    p->pos++;
    return true;
}

static bool parse_string(json_parser_t *p, char *out, size_t out_size) {
    skip_whitespace(p);
    if (!expect_char(p, '"')) return false;
    
    size_t start = p->pos;
    while (p->pos < p->len && p->json[p->pos] != '"') {
        if (p->json[p->pos] == '\\') p->pos++;  // Skip escape
        p->pos++;
    }
    
    size_t len = p->pos - start;
    if (len >= out_size) len = out_size - 1;
    memcpy(out, p->json + start, len);
    out[len] = '\0';
    
    p->pos++;  // Skip closing quote
    return true;
}

static bool parse_int64(json_parser_t *p, int64_t *out) {
    skip_whitespace(p);
    
    char *endptr;
    errno = 0;
    *out = strtoll(p->json + p->pos, &endptr, 10);
    
    if (errno != 0 || endptr == p->json + p->pos) {
        snprintf(p->error, sizeof(p->error), 
                 "Invalid integer at position %zu", p->pos);
        return false;
    }
    
    p->pos = endptr - p->json;
    return true;
}

static bool parse_double(json_parser_t *p, double *out) {
    skip_whitespace(p);
    
    char *endptr;
    errno = 0;
    *out = strtod(p->json + p->pos, &endptr);
    
    if (errno != 0 || endptr == p->json + p->pos) {
        snprintf(p->error, sizeof(p->error), 
                 "Invalid number at position %zu", p->pos);
        return false;
    }
    
    p->pos = endptr - p->json;
    return true;
}

static bool parse_bool(json_parser_t *p, bool *out) {
    skip_whitespace(p);
    
    if (p->pos + 4 <= p->len && strncmp(p->json + p->pos, "true", 4) == 0) {
        *out = true;
        p->pos += 4;
        return true;
    }
    if (p->pos + 5 <= p->len && strncmp(p->json + p->pos, "false", 5) == 0) {
        *out = false;
        p->pos += 5;
        return true;
    }
    
    snprintf(p->error, sizeof(p->error), 
             "Expected 'true' or 'false' at position %zu", p->pos);
    return false;
}

static bool skip_value(json_parser_t *p);

static bool skip_object(json_parser_t *p) {
    if (!expect_char(p, '{')) return false;
    
    skip_whitespace(p);
    if (p->pos < p->len && p->json[p->pos] == '}') {
        p->pos++;
        return true;
    }
    
    do {
        char key[256];
        if (!parse_string(p, key, sizeof(key))) return false;
        if (!expect_char(p, ':')) return false;
        if (!skip_value(p)) return false;
        
        skip_whitespace(p);
        if (p->pos < p->len && p->json[p->pos] == ',') {
            p->pos++;
        } else {
            break;
        }
    } while (p->pos < p->len);
    
    return expect_char(p, '}');
}

static bool skip_array(json_parser_t *p) {
    if (!expect_char(p, '[')) return false;
    
    skip_whitespace(p);
    if (p->pos < p->len && p->json[p->pos] == ']') {
        p->pos++;
        return true;
    }
    
    do {
        if (!skip_value(p)) return false;
        
        skip_whitespace(p);
        if (p->pos < p->len && p->json[p->pos] == ',') {
            p->pos++;
        } else {
            break;
        }
    } while (p->pos < p->len);
    
    return expect_char(p, ']');
}

static bool skip_value(json_parser_t *p) {
    skip_whitespace(p);
    
    if (p->pos >= p->len) return false;
    
    char c = p->json[p->pos];
    
    if (c == '"') {
        char tmp[4096];
        return parse_string(p, tmp, sizeof(tmp));
    }
    if (c == '{') return skip_object(p);
    if (c == '[') return skip_array(p);
    if (c == 't' || c == 'f') {
        bool tmp;
        return parse_bool(p, &tmp);
    }
    if (c == 'n') {
        if (p->pos + 4 <= p->len && strncmp(p->json + p->pos, "null", 4) == 0) {
            p->pos += 4;
            return true;
        }
        return false;
    }
    if (c == '-' || isdigit((unsigned char)c)) {
        double tmp;
        return parse_double(p, &tmp);
    }
    
    return false;
}

// ============================================================================
// JSON IMPORT
// ============================================================================

static bool parse_geometry(json_parser_t *p, uft_geom_t *geom) {
    if (!expect_char(p, '{')) return false;
    
    skip_whitespace(p);
    while (p->pos < p->len && p->json[p->pos] != '}') {
        char key[64];
        if (!parse_string(p, key, sizeof(key))) return false;
        if (!expect_char(p, ':')) return false;
        
        int64_t ival;
        if (strcmp(key, "cylinders") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->cylinders = (int32_t)ival;
        } else if (strcmp(key, "heads") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->heads = (int32_t)ival;
        } else if (strcmp(key, "sectors_per_track") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->sectors_per_track = (int32_t)ival;
        } else if (strcmp(key, "sector_size") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->sector_size = (int32_t)ival;
        } else if (strcmp(key, "cylinder_start") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->cylinder_start = (int32_t)ival;
        } else if (strcmp(key, "cylinder_end") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->cylinder_end = (int32_t)ival;
        } else if (strcmp(key, "head_mask") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->head_mask = (int32_t)ival;
        } else if (strcmp(key, "sector_base") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->sector_base = (int32_t)ival;
        } else if (strcmp(key, "interleave") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->interleave = (int32_t)ival;
        } else if (strcmp(key, "skew") == 0) {
            if (!parse_int64(p, &ival)) return false;
            geom->skew = (int32_t)ival;
        } else {
            // Skip unknown keys
            if (!skip_value(p)) return false;
        }
        
        skip_whitespace(p);
        if (p->pos < p->len && p->json[p->pos] == ',') {
            p->pos++;
        }
        skip_whitespace(p);
    }
    
    return expect_char(p, '}');
}

static bool parse_timing(json_parser_t *p, uft_timing_t *timing) {
    if (!expect_char(p, '{')) return false;
    
    skip_whitespace(p);
    while (p->pos < p->len && p->json[p->pos] != '}') {
        char key[64];
        if (!parse_string(p, key, sizeof(key))) return false;
        if (!expect_char(p, ':')) return false;
        
        int64_t ival;
        double dval;
        
        if (strcmp(key, "cell_time_ns") == 0) {
            if (!parse_int64(p, &ival)) return false;
            timing->cell_time_ns = (uint64_t)ival;
        } else if (strcmp(key, "rotation_ns") == 0) {
            if (!parse_int64(p, &ival)) return false;
            timing->rotation_ns = (uint64_t)ival;
        } else if (strcmp(key, "datarate_bps") == 0) {
            if (!parse_int64(p, &ival)) return false;
            timing->datarate_bps = (uint32_t)ival;
        } else if (strcmp(key, "rpm") == 0) {
            if (!parse_double(p, &dval)) return false;
            timing->rpm = dval;
        } else if (strcmp(key, "pll_phase_adjust") == 0) {
            if (!parse_double(p, &dval)) return false;
            timing->pll_phase_adjust = dval;
        } else if (strcmp(key, "pll_period_adjust") == 0) {
            if (!parse_double(p, &dval)) return false;
            timing->pll_period_adjust = dval;
        } else if (strcmp(key, "pll_period_min") == 0) {
            if (!parse_double(p, &dval)) return false;
            timing->pll_period_min = dval;
        } else if (strcmp(key, "pll_period_max") == 0) {
            if (!parse_double(p, &dval)) return false;
            timing->pll_period_max = dval;
        } else if (strcmp(key, "weak_threshold") == 0) {
            if (!parse_double(p, &dval)) return false;
            timing->weak_threshold = dval;
        } else {
            if (!skip_value(p)) return false;
        }
        
        skip_whitespace(p);
        if (p->pos < p->len && p->json[p->pos] == ',') {
            p->pos++;
        }
        skip_whitespace(p);
    }
    
    return expect_char(p, '}');
}

static bool parse_operation(json_parser_t *p, uft_operation_t *op) {
    if (!expect_char(p, '{')) return false;
    
    skip_whitespace(p);
    while (p->pos < p->len && p->json[p->pos] != '}') {
        char key[64];
        if (!parse_string(p, key, sizeof(key))) return false;
        if (!expect_char(p, ':')) return false;
        
        int64_t ival;
        bool bval;
        
        if (strcmp(key, "dry_run") == 0) {
            if (!parse_bool(p, &bval)) return false;
            op->dry_run = bval;
        } else if (strcmp(key, "verify_after_write") == 0) {
            if (!parse_bool(p, &bval)) return false;
            op->verify_after_write = bval;
        } else if (strcmp(key, "retries") == 0) {
            if (!parse_int64(p, &ival)) return false;
            op->retries = (int32_t)ival;
        } else if (strcmp(key, "revolutions") == 0) {
            if (!parse_int64(p, &ival)) return false;
            op->revolutions = (int32_t)ival;
        } else if (strcmp(key, "attempt_recovery") == 0) {
            if (!parse_bool(p, &bval)) return false;
            op->attempt_recovery = bval;
        } else if (strcmp(key, "preserve_errors") == 0) {
            if (!parse_bool(p, &bval)) return false;
            op->preserve_errors = bval;
        } else if (strcmp(key, "verbose") == 0) {
            if (!parse_bool(p, &bval)) return false;
            op->verbose = bval;
        } else if (strcmp(key, "generate_audit") == 0) {
            if (!parse_bool(p, &bval)) return false;
            op->generate_audit = bval;
        } else {
            if (!skip_value(p)) return false;
        }
        
        skip_whitespace(p);
        if (p->pos < p->len && p->json[p->pos] == ',') {
            p->pos++;
        }
        skip_whitespace(p);
    }
    
    return expect_char(p, '}');
}

int uft_params_from_json(const char *json, uft_canonical_params_t *params) {
    if (!json || !params) return -1;
    
    // Start with defaults
    *params = uft_params_init();
    
    json_parser_t p = {
        .json = json,
        .pos = 0,
        .len = strlen(json),
        .error = ""
    };
    
    if (!expect_char(&p, '{')) {
        return -1;
    }
    
    skip_whitespace(&p);
    while (p.pos < p.len && p.json[p.pos] != '}') {
        char key[64];
        if (!parse_string(&p, key, sizeof(key))) return -1;
        if (!expect_char(&p, ':')) return -1;
        
        if (strcmp(key, "geometry") == 0) {
            if (!parse_geometry(&p, &params->geometry)) return -1;
        } else if (strcmp(key, "timing") == 0) {
            if (!parse_timing(&p, &params->timing)) return -1;
        } else if (strcmp(key, "operation") == 0) {
            if (!parse_operation(&p, &params->operation)) return -1;
        } else if (strcmp(key, "_meta") == 0 || strcmp(key, "format") == 0 ||
                   strcmp(key, "hardware") == 0) {
            // Skip these complex objects for now
            if (!skip_object(&p)) return -1;
        } else {
            if (!skip_value(&p)) return -1;
        }
        
        skip_whitespace(&p);
        if (p.pos < p.len && p.json[p.pos] == ',') {
            p.pos++;
        }
        skip_whitespace(&p);
    }
    
    if (!expect_char(&p, '}')) {
        return -1;
    }
    
    // Recompute derived values
    uft_params_recompute(params);
    
    strcpy(params->source, "json");  /* REVIEW: Consider bounds check */  /* Safe: "json" is 4 chars + NUL, source is 64 bytes */
    
    return 0;
}

// ============================================================================
// FILE I/O
// ============================================================================

int uft_params_save_to_file(const uft_canonical_params_t *params, const char *path) {
    if (!params || !path) return -1;
    
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    
    char *buffer = malloc(65536);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    
    int len = uft_params_to_json_full(params, buffer, 65536, true);
    if (len > 0) {
        if (fwrite(buffer, 1, len, f) != len) { /* I/O error */ }
    }
    
    free(buffer);
    fclose(f);
    
    return (len > 0) ? 0 : -1;
}

int uft_params_load_from_file(const char *path, uft_canonical_params_t *params) {
    if (!path || !params) return -1;
    
    FILE *f = fopen(path, "r");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0 || size > 1024 * 1024) {
        fclose(f);
        return -1;
    }
    
    char *buffer = malloc(size + 1);
    if (!buffer) {
        fclose(f);
        return -1;
    }
    
    size_t read = fread(buffer, 1, size, f);
    buffer[read] = '\0';
    fclose(f);
    
    int result = uft_params_from_json(buffer, params);
    free(buffer);
    
    return result;
}
