/**
 * @file uft_loss_report.c
 * @brief Prinzip 1 Implementierung — `.loss.json` Sidecar-Writer.
 */

#include "uft/core/uft_loss_report.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UFT_LOSS_SCHEMA_VERSION "uft-loss-report-v1"

const char *uft_loss_report_schema_version(void) {
    return UFT_LOSS_SCHEMA_VERSION;
}

const char *uft_loss_category_string(uft_loss_category_t c) {
    switch (c) {
        case UFT_LOSS_WEAK_BITS:        return "WEAK_BITS";
        case UFT_LOSS_FLUX_TIMING:      return "FLUX_TIMING";
        case UFT_LOSS_INDEX_PULSES:     return "INDEX_PULSES";
        case UFT_LOSS_SYNC_PATTERNS:    return "SYNC_PATTERNS";
        case UFT_LOSS_MULTI_REVOLUTION: return "MULTI_REVOLUTION";
        case UFT_LOSS_CUSTOM_METADATA:  return "CUSTOM_METADATA";
        case UFT_LOSS_COPY_PROTECTION:  return "COPY_PROTECTION";
        case UFT_LOSS_LONG_TRACKS:      return "LONG_TRACKS";
        case UFT_LOSS_HALF_TRACKS:      return "HALF_TRACKS";
        case UFT_LOSS_WRITE_SPLICE:     return "WRITE_SPLICE";
        case UFT_LOSS_OTHER:            return "OTHER";
        default:                         return "OTHER";
    }
}

/* Minimal JSON string-escape for ASCII + common control chars.
 * Writes escaped string (without surrounding quotes) to `out`. */
static void write_json_string(FILE *out, const char *s) {
    if (!s) { fputs("null", out); return; }
    fputc('"', out);
    for (const unsigned char *p = (const unsigned char *)s; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
            case '"':  fputs("\\\"", out); break;
            case '\\': fputs("\\\\", out); break;
            case '\b': fputs("\\b", out);  break;
            case '\f': fputs("\\f", out);  break;
            case '\n': fputs("\\n", out);  break;
            case '\r': fputs("\\r", out);  break;
            case '\t': fputs("\\t", out);  break;
            default:
                if (c < 0x20) {
                    fprintf(out, "\\u%04x", c);
                } else {
                    fputc((int)c, out);
                }
        }
    }
    fputc('"', out);
}

uft_error_t uft_loss_report_write_stream(const uft_loss_report_t *report,
                                          void *out_file) {
    if (!report || !out_file) return UFT_ERROR_NULL_POINTER;
    if (report->entry_count && !report->entries) return UFT_ERROR_INVALID_ARG;

    FILE *out = (FILE *)out_file;

    fputs("{\n", out);
    fputs("  \"schema\": ", out);
    write_json_string(out, UFT_LOSS_SCHEMA_VERSION);
    fputs(",\n  \"source\": {\n    \"path\": ", out);
    write_json_string(out, report->source_path);
    fputs(",\n    \"format\": ", out);
    write_json_string(out, report->source_format);
    fputs("\n  },\n  \"target\": {\n    \"path\": ", out);
    write_json_string(out, report->target_path);
    fputs(",\n    \"format\": ", out);
    write_json_string(out, report->target_format);
    fputs("\n  },\n", out);

    fprintf(out, "  \"timestamp_unix\": %llu,\n",
            (unsigned long long)report->timestamp_unix);

    fputs("  \"uft_version\": ", out);
    write_json_string(out, report->uft_version);
    fputs(",\n", out);

    fputs("  \"entries\": [", out);
    for (size_t i = 0; i < report->entry_count; ++i) {
        const uft_loss_entry_t *e = &report->entries[i];
        fputs(i == 0 ? "\n" : ",\n", out);
        fputs("    { \"category\": ", out);
        write_json_string(out, uft_loss_category_string(e->category));
        fprintf(out, ", \"count\": %u", e->count);
        fputs(", \"description\": ", out);
        write_json_string(out, e->description);
        fputs(" }", out);
    }
    if (report->entry_count) fputs("\n  ", out);
    fputs("]\n}\n", out);

    return UFT_OK;
}

uft_error_t uft_loss_report_write(const uft_loss_report_t *report,
                                    const char *sidecar_path) {
    if (!report) return UFT_ERROR_NULL_POINTER;

    char buf[1024];
    const char *path = sidecar_path;
    if (!path) {
        if (!report->target_path) return UFT_ERROR_INVALID_ARG;
        int n = snprintf(buf, sizeof(buf), "%s.loss.json", report->target_path);
        if (n < 0 || (size_t)n >= sizeof(buf)) return UFT_ERROR_BUFFER_TOO_SMALL;
        path = buf;
    }

    FILE *out = fopen(path, "w");
    if (!out) return UFT_ERROR_IO;
    uft_error_t rc = uft_loss_report_write_stream(report, out);
    fclose(out);
    return rc;
}
