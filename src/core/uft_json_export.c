/**
 * @file uft_json_export.c
 * @brief JSON Diagnostic Export Implementation
 */

#include "uft/core/uft_json_export.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static int json_write(uft_json_writer_t *writer, const char *str, size_t len) {
    if (writer->error) return -1;
    
    switch (writer->output_type) {
        case UFT_JSON_TO_BUFFER:
            if (writer->buffer && writer->buffer_pos + len < writer->buffer_size) {
                memcpy(writer->buffer + writer->buffer_pos, str, len);
                writer->buffer_pos += len;
                writer->buffer[writer->buffer_pos] = '\0';
            } else {
                writer->buffer_pos += len;  /* Track even if buffer full */
            }
            break;
            
        case UFT_JSON_TO_FILE:
            if (writer->file) {
                if (fwrite(str, 1, len, writer->file) != len) {
                    writer->error = 1;
                    return -1;
                }
            }
            break;
            
        case UFT_JSON_TO_CALLBACK:
            if (writer->callback) {
                if (writer->callback(str, len, writer->callback_data) < 0) {
                    writer->error = 1;
                    return -1;
                }
            }
            break;
    }
    
    return 0;
}

static int json_printf(uft_json_writer_t *writer, const char *fmt, ...) {
    char buf[1024];
    va_list args;
    
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len > 0) {
        return json_write(writer, buf, len);
    }
    return -1;
}

static void json_indent(uft_json_writer_t *writer) {
    if (!writer->pretty_print) return;
    
    for (int i = 0; i < writer->indent_level; i++) {
        json_write(writer, "  ", 2);
    }
}

static void json_newline(uft_json_writer_t *writer) {
    if (writer->pretty_print) {
        json_write(writer, "\n", 1);
    }
}

static void json_separator(uft_json_writer_t *writer) {
    if (!writer->first_element) {
        json_write(writer, ",", 1);
        json_newline(writer);
    }
    writer->first_element = false;
}

static int json_escape_string(uft_json_writer_t *writer, const char *str) {
    if (!str) {
        return json_write(writer, "null", 4);
    }
    
    json_write(writer, "\"", 1);
    
    while (*str) {
        switch (*str) {
            case '\\': json_write(writer, "\\\\", 2); break;
            case '"':  json_write(writer, "\\\"", 2); break;
            case '\n': json_write(writer, "\\n", 2); break;
            case '\r': json_write(writer, "\\r", 2); break;
            case '\t': json_write(writer, "\\t", 2); break;
            default:
                if ((unsigned char)*str < 0x20) {
                    json_printf(writer, "\\u%04x", (unsigned char)*str);
                } else {
                    json_write(writer, str, 1);
                }
        }
        str++;
    }
    
    json_write(writer, "\"", 1);
    return 0;
}

/* ============================================================================
 * JSON Writer Functions
 * ============================================================================ */

void uft_json_init_buffer(
    uft_json_writer_t *writer,
    char *buffer,
    size_t size
) {
    memset(writer, 0, sizeof(*writer));
    writer->output_type = UFT_JSON_TO_BUFFER;
    writer->buffer = buffer;
    writer->buffer_size = size;
    writer->first_element = true;
    writer->pretty_print = true;
    
    if (buffer && size > 0) {
        buffer[0] = '\0';
    }
}

void uft_json_init_file(
    uft_json_writer_t *writer,
    FILE *file
) {
    memset(writer, 0, sizeof(*writer));
    writer->output_type = UFT_JSON_TO_FILE;
    writer->file = file;
    writer->first_element = true;
    writer->pretty_print = true;
}

void uft_json_init_callback(
    uft_json_writer_t *writer,
    uft_json_write_fn callback,
    void *user_data
) {
    memset(writer, 0, sizeof(*writer));
    writer->output_type = UFT_JSON_TO_CALLBACK;
    writer->callback = callback;
    writer->callback_data = user_data;
    writer->first_element = true;
    writer->pretty_print = true;
}

void uft_json_set_pretty(uft_json_writer_t *writer, bool pretty) {
    if (writer) {
        writer->pretty_print = pretty;
    }
}

int uft_json_write_raw(uft_json_writer_t *writer, const char *str) {
    return json_write(writer, str, strlen(str));
}

int uft_json_object_start(uft_json_writer_t *writer) {
    json_write(writer, "{", 1);
    json_newline(writer);
    writer->indent_level++;
    writer->first_element = true;
    return 0;
}

int uft_json_object_end(uft_json_writer_t *writer) {
    writer->indent_level--;
    json_newline(writer);
    json_indent(writer);
    json_write(writer, "}", 1);
    writer->first_element = false;
    return 0;
}

int uft_json_array_start(uft_json_writer_t *writer) {
    json_write(writer, "[", 1);
    json_newline(writer);
    writer->indent_level++;
    writer->first_element = true;
    return 0;
}

int uft_json_array_end(uft_json_writer_t *writer) {
    writer->indent_level--;
    json_newline(writer);
    json_indent(writer);
    json_write(writer, "]", 1);
    writer->first_element = false;
    return 0;
}

int uft_json_key(uft_json_writer_t *writer, const char *key) {
    json_separator(writer);
    json_indent(writer);
    json_escape_string(writer, key);
    json_write(writer, ": ", writer->pretty_print ? 2 : 1);
    writer->first_element = true;  /* Next value doesn't need comma */
    return 0;
}

int uft_json_string(uft_json_writer_t *writer, const char *value) {
    return json_escape_string(writer, value);
}

int uft_json_int(uft_json_writer_t *writer, int64_t value) {
    return json_printf(writer, "%lld", (long long)value);
}

int uft_json_uint(uft_json_writer_t *writer, uint64_t value) {
    return json_printf(writer, "%llu", (unsigned long long)value);
}

int uft_json_double(uft_json_writer_t *writer, double value, int precision) {
    return json_printf(writer, "%.*f", precision, value);
}

int uft_json_bool(uft_json_writer_t *writer, bool value) {
    return json_write(writer, value ? "true" : "false", value ? 4 : 5);
}

int uft_json_null(uft_json_writer_t *writer) {
    return json_write(writer, "null", 4);
}

int uft_json_kv_string(uft_json_writer_t *writer, const char *key, const char *value) {
    uft_json_key(writer, key);
    return uft_json_string(writer, value);
}

int uft_json_kv_int(uft_json_writer_t *writer, const char *key, int64_t value) {
    uft_json_key(writer, key);
    return uft_json_int(writer, value);
}

int uft_json_kv_uint(uft_json_writer_t *writer, const char *key, uint64_t value) {
    uft_json_key(writer, key);
    return uft_json_uint(writer, value);
}

int uft_json_kv_double(uft_json_writer_t *writer, const char *key, double value, int precision) {
    uft_json_key(writer, key);
    return uft_json_double(writer, value, precision);
}

int uft_json_kv_bool(uft_json_writer_t *writer, const char *key, bool value) {
    uft_json_key(writer, key);
    return uft_json_bool(writer, value);
}

size_t uft_json_bytes_written(const uft_json_writer_t *writer) {
    return writer ? writer->buffer_pos : 0;
}

bool uft_json_has_error(const uft_json_writer_t *writer) {
    return writer && writer->error != 0;
}

/* ============================================================================
 * Diagnostic Export Functions
 * ============================================================================ */

int uft_json_export_track_diag(
    uft_json_writer_t *writer,
    const uft_track_diag_t *diag
) {
    if (!writer || !diag) return -1;
    
    uft_json_object_start(writer);
    uft_json_kv_uint(writer, "track", diag->track);
    uft_json_kv_uint(writer, "head", diag->head);
    uft_json_kv_uint(writer, "data_bits", diag->data_bits);
    uft_json_kv_uint(writer, "flux_transitions", diag->flux_transitions);
    uft_json_kv_double(writer, "rpm", diag->rpm, 2);
    uft_json_kv_double(writer, "bitrate", diag->bitrate, 0);
    uft_json_kv_uint(writer, "sectors_found", diag->sectors_found);
    uft_json_kv_uint(writer, "sectors_good", diag->sectors_good);
    uft_json_kv_uint(writer, "sectors_bad", diag->sectors_bad);
    uft_json_kv_uint(writer, "quality", diag->quality);
    uft_json_kv_bool(writer, "has_weak_bits", diag->has_weak_bits);
    uft_json_kv_uint(writer, "weak_bit_count", diag->weak_bit_count);
    uft_json_kv_string(writer, "encoding", diag->encoding);
    uft_json_kv_string(writer, "protection", diag->protection);
    uft_json_object_end(writer);
    
    return 0;
}

int uft_json_export_sector_diag(
    uft_json_writer_t *writer,
    const uft_sector_diag_t *diag
) {
    if (!writer || !diag) return -1;
    
    uft_json_object_start(writer);
    uft_json_kv_uint(writer, "track", diag->track);
    uft_json_kv_uint(writer, "head", diag->head);
    uft_json_kv_uint(writer, "sector", diag->sector);
    uft_json_kv_uint(writer, "size", diag->size);
    uft_json_kv_uint(writer, "status", diag->status);
    uft_json_kv_uint(writer, "confidence", diag->confidence);
    uft_json_kv_bool(writer, "header_ok", diag->header_ok);
    uft_json_kv_bool(writer, "data_ok", diag->data_ok);
    uft_json_kv_double(writer, "timing_deviation", diag->timing_deviation, 3);
    uft_json_object_end(writer);
    
    return 0;
}

int uft_json_export_disk_diag(
    uft_json_writer_t *writer,
    const uft_disk_diag_t *diag
) {
    if (!writer || !diag) return -1;
    
    uft_json_object_start(writer);
    
    /* Basic info */
    uft_json_kv_string(writer, "filename", diag->filename);
    uft_json_kv_string(writer, "format", diag->format);
    uft_json_kv_string(writer, "encoding", diag->encoding);
    uft_json_kv_uint(writer, "file_size", diag->file_size);
    
    /* Geometry */
    uft_json_key(writer, "geometry");
    uft_json_object_start(writer);
    uft_json_kv_uint(writer, "tracks", diag->tracks);
    uft_json_kv_uint(writer, "sides", diag->sides);
    uft_json_kv_uint(writer, "sectors_per_track", diag->sectors_per_track);
    uft_json_kv_uint(writer, "sector_size", diag->sector_size);
    uft_json_kv_uint(writer, "total_sectors", diag->total_sectors);
    uft_json_object_end(writer);
    
    /* Analysis */
    uft_json_key(writer, "analysis");
    uft_json_object_start(writer);
    uft_json_kv_uint(writer, "sectors_good", diag->sectors_good);
    uft_json_kv_uint(writer, "sectors_bad", diag->sectors_bad);
    uft_json_kv_uint(writer, "sectors_missing", diag->sectors_missing);
    uft_json_kv_double(writer, "overall_quality", diag->overall_quality, 1);
    uft_json_object_end(writer);
    
    /* Protection */
    uft_json_key(writer, "protection");
    uft_json_object_start(writer);
    uft_json_kv_bool(writer, "detected", diag->has_protection);
    uft_json_kv_string(writer, "type", diag->protection);
    uft_json_object_end(writer);
    
    /* Checksums */
    uft_json_key(writer, "checksums");
    uft_json_object_start(writer);
    uft_json_key(writer, "crc32");
    json_printf(writer, "\"0x%08X\"", diag->crc32);
    uft_json_kv_string(writer, "md5", diag->md5);
    uft_json_kv_string(writer, "sha1", diag->sha1);
    uft_json_object_end(writer);
    
    /* Track diagnostics */
    if (diag->tracks_diag && diag->track_count > 0) {
        uft_json_key(writer, "tracks");
        uft_json_array_start(writer);
        for (size_t i = 0; i < diag->track_count; i++) {
            if (i > 0) writer->first_element = false;
            json_indent(writer);
            uft_json_export_track_diag(writer, &diag->tracks_diag[i]);
        }
        uft_json_array_end(writer);
    }
    
    /* Sector diagnostics (optional) */
    if (diag->sectors_diag && diag->sector_count > 0) {
        uft_json_key(writer, "sectors");
        uft_json_array_start(writer);
        for (size_t i = 0; i < diag->sector_count; i++) {
            if (i > 0) writer->first_element = false;
            json_indent(writer);
            uft_json_export_sector_diag(writer, &diag->sectors_diag[i]);
        }
        uft_json_array_end(writer);
    }
    
    uft_json_object_end(writer);
    
    return 0;
}

int uft_json_export_disk_to_file(
    const uft_disk_diag_t *diag,
    const char *filename
) {
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    
    uft_json_writer_t writer;
    uft_json_init_file(&writer, f);
    
    int result = uft_json_export_disk_diag(&writer, diag);
    
    fclose(f);
    return result;
}

size_t uft_json_export_disk_to_buffer(
    const uft_disk_diag_t *diag,
    char *buffer,
    size_t size
) {
    uft_json_writer_t writer;
    uft_json_init_buffer(&writer, buffer, size);
    
    uft_json_export_disk_diag(&writer, diag);
    
    return uft_json_bytes_written(&writer);
}

/* ============================================================================
 * Diagnostic Report Creation
 * ============================================================================ */

void uft_disk_diag_init(uft_disk_diag_t *diag) {
    if (diag) {
        memset(diag, 0, sizeof(*diag));
    }
}

void uft_disk_diag_free(uft_disk_diag_t *diag) {
    if (!diag) return;
    
    free(diag->tracks_diag);
    free(diag->sectors_diag);
    
    diag->tracks_diag = NULL;
    diag->sectors_diag = NULL;
    diag->track_count = 0;
    diag->sector_count = 0;
}

int uft_disk_diag_alloc_tracks(uft_disk_diag_t *diag, size_t count) {
    if (!diag) return -1;
    
    free(diag->tracks_diag);
    diag->tracks_diag = (uft_track_diag_t *)calloc(count, sizeof(uft_track_diag_t));
    diag->track_count = diag->tracks_diag ? count : 0;
    
    return diag->tracks_diag ? 0 : -1;
}

int uft_disk_diag_alloc_sectors(uft_disk_diag_t *diag, size_t count) {
    if (!diag) return -1;
    
    free(diag->sectors_diag);
    diag->sectors_diag = (uft_sector_diag_t *)calloc(count, sizeof(uft_sector_diag_t));
    diag->sector_count = diag->sectors_diag ? count : 0;
    
    return diag->sectors_diag ? 0 : -1;
}

void uft_disk_diag_calc_summary(uft_disk_diag_t *diag) {
    if (!diag) return;
    
    diag->sectors_good = 0;
    diag->sectors_bad = 0;
    
    if (diag->tracks_diag) {
        for (size_t i = 0; i < diag->track_count; i++) {
            diag->sectors_good += diag->tracks_diag[i].sectors_good;
            diag->sectors_bad += diag->tracks_diag[i].sectors_bad;
        }
    }
    
    diag->total_sectors = diag->sectors_good + diag->sectors_bad + diag->sectors_missing;
    
    if (diag->total_sectors > 0) {
        diag->overall_quality = 100.0 * diag->sectors_good / diag->total_sectors;
    }
}

/* ============================================================================
 * Quick Export Functions
 * ============================================================================ */

size_t uft_json_error_report(
    char *buffer,
    size_t size,
    int error_code,
    const char *error_msg,
    const char *filename,
    int track,
    int sector
) {
    uft_json_writer_t writer;
    uft_json_init_buffer(&writer, buffer, size);
    
    uft_json_object_start(&writer);
    uft_json_kv_string(&writer, "type", "error");
    uft_json_kv_int(&writer, "code", error_code);
    uft_json_kv_string(&writer, "message", error_msg);
    if (filename) uft_json_kv_string(&writer, "filename", filename);
    if (track >= 0) uft_json_kv_int(&writer, "track", track);
    if (sector >= 0) uft_json_kv_int(&writer, "sector", sector);
    uft_json_object_end(&writer);
    
    return uft_json_bytes_written(&writer);
}

size_t uft_json_progress_report(
    char *buffer,
    size_t size,
    int current,
    int total,
    const char *operation,
    double elapsed_sec
) {
    uft_json_writer_t writer;
    uft_json_init_buffer(&writer, buffer, size);
    
    double percent = (total > 0) ? 100.0 * current / total : 0;
    
    uft_json_object_start(&writer);
    uft_json_kv_string(&writer, "type", "progress");
    uft_json_kv_string(&writer, "operation", operation);
    uft_json_kv_int(&writer, "current", current);
    uft_json_kv_int(&writer, "total", total);
    uft_json_kv_double(&writer, "percent", percent, 1);
    uft_json_kv_double(&writer, "elapsed_sec", elapsed_sec, 2);
    uft_json_object_end(&writer);
    
    return uft_json_bytes_written(&writer);
}

size_t uft_json_completion_report(
    char *buffer,
    size_t size,
    bool success,
    const char *operation,
    uint32_t items_processed,
    uint32_t items_failed,
    double elapsed_sec
) {
    uft_json_writer_t writer;
    uft_json_init_buffer(&writer, buffer, size);
    
    uft_json_object_start(&writer);
    uft_json_kv_string(&writer, "type", "completion");
    uft_json_kv_bool(&writer, "success", success);
    uft_json_kv_string(&writer, "operation", operation);
    uft_json_kv_uint(&writer, "items_processed", items_processed);
    uft_json_kv_uint(&writer, "items_failed", items_failed);
    uft_json_kv_double(&writer, "elapsed_sec", elapsed_sec, 2);
    uft_json_object_end(&writer);
    
    return uft_json_bytes_written(&writer);
}
