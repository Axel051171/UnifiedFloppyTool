/**
 * @file libflux_json_diagnostics.c
 * @brief JSON Diagnostic Export Implementation
 * 
 * CLEAN-ROOM IMPLEMENTATION
 * 
 * Copyright (C) 2025 UFT Project Contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "libflux_json_diagnostics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * API IMPLEMENTATION
 * ============================================================================ */

void libflux_diag_init(libflux_disk_diag_t *diag) {
    if (diag) {
        memset(diag, 0, sizeof(*diag));
    }
}

void libflux_diag_free(libflux_disk_diag_t *diag) {
    if (!diag) return;
    
    free(diag->track_diags);
    free(diag->sector_diags);
    
    diag->track_diags = NULL;
    diag->sector_diags = NULL;
    diag->track_count = 0;
    diag->sector_count = 0;
}

int libflux_diag_alloc_tracks(libflux_disk_diag_t *diag, int count) {
    if (!diag || count <= 0) return -1;
    
    free(diag->track_diags);
    diag->track_diags = (libflux_track_diag_t *)calloc(count, sizeof(libflux_track_diag_t));
    diag->track_count = diag->track_diags ? count : 0;
    
    return diag->track_diags ? 0 : -1;
}

int libflux_diag_alloc_sectors(libflux_disk_diag_t *diag, int count) {
    if (!diag || count <= 0) return -1;
    
    free(diag->sector_diags);
    diag->sector_diags = (libflux_sector_diag_t *)calloc(count, sizeof(libflux_sector_diag_t));
    diag->sector_count = diag->sector_diags ? count : 0;
    
    return diag->sector_diags ? 0 : -1;
}

int libflux_export_track_json(const libflux_track_diag_t *track, FILE *f) {
    if (!track || !f) return -1;
    
    fprintf(f, "    {\n");
    fprintf(f, "      \"track\": %d,\n", track->track);
    fprintf(f, "      \"head\": %d,\n", track->head);
    fprintf(f, "      \"bitrate\": %d,\n", track->bitrate);
    fprintf(f, "      \"encoding\": %d,\n", track->encoding);
    fprintf(f, "      \"sectors_found\": %d,\n", track->sectors_found);
    fprintf(f, "      \"sectors_ok\": %d,\n", track->sectors_ok);
    fprintf(f, "      \"sectors_bad\": %d,\n", track->sectors_bad);
    fprintf(f, "      \"rpm\": %.2f,\n", track->rpm);
    fprintf(f, "      \"quality\": %d\n", track->quality);
    fprintf(f, "    }");
    
    return 0;
}

int libflux_export_sector_json(const libflux_sector_diag_t *sector, FILE *f) {
    if (!sector || !f) return -1;
    
    fprintf(f, "    {\n");
    fprintf(f, "      \"track\": %d,\n", sector->track);
    fprintf(f, "      \"head\": %d,\n", sector->head);
    fprintf(f, "      \"sector\": %d,\n", sector->sector);
    fprintf(f, "      \"size\": %d,\n", sector->size);
    fprintf(f, "      \"header_ok\": %s,\n", sector->header_ok ? "true" : "false");
    fprintf(f, "      \"data_ok\": %s,\n", sector->data_ok ? "true" : "false");
    fprintf(f, "      \"confidence\": %d\n", sector->confidence);
    fprintf(f, "    }");
    
    return 0;
}

int libflux_export_json(const libflux_disk_diag_t *diag, const char *filename) {
    if (!diag || !filename) return -1;
    
    FILE *f = fopen(filename, "w");
    if (!f) return -1;
    
    fprintf(f, "{\n");
    fprintf(f, "  \"disk_diagnostics\": {\n");
    fprintf(f, "    \"filename\": \"%s\",\n", diag->filename);
    fprintf(f, "    \"format\": \"%s\",\n", diag->format);
    fprintf(f, "    \"geometry\": {\n");
    fprintf(f, "      \"tracks\": %d,\n", diag->tracks);
    fprintf(f, "      \"sides\": %d,\n", diag->sides);
    fprintf(f, "      \"sectors_per_track\": %d,\n", diag->sectors_per_track);
    fprintf(f, "      \"sector_size\": %d\n", diag->sector_size);
    fprintf(f, "    },\n");
    fprintf(f, "    \"analysis\": {\n");
    fprintf(f, "      \"sectors_ok\": %d,\n", diag->total_sectors_ok);
    fprintf(f, "      \"sectors_bad\": %d,\n", diag->total_sectors_bad);
    fprintf(f, "      \"overall_quality\": %.1f\n", diag->overall_quality);
    fprintf(f, "    },\n");
    fprintf(f, "    \"checksums\": {\n");
    fprintf(f, "      \"crc32\": \"0x%08X\",\n", diag->crc32);
    fprintf(f, "      \"md5\": \"%s\"\n", diag->md5);
    fprintf(f, "    }");
    
    if (diag->track_diags && diag->track_count > 0) {
        fprintf(f, ",\n    \"tracks\": [\n");
        for (int i = 0; i < diag->track_count; i++) {
            libflux_export_track_json(&diag->track_diags[i], f);
            fprintf(f, "%s\n", (i < diag->track_count - 1) ? "," : "");
        }
        fprintf(f, "    ]");
    }
    
    if (diag->sector_diags && diag->sector_count > 0) {
        fprintf(f, ",\n    \"sectors\": [\n");
        for (int i = 0; i < diag->sector_count; i++) {
            libflux_export_sector_json(&diag->sector_diags[i], f);
            fprintf(f, "%s\n", (i < diag->sector_count - 1) ? "," : "");
        }
        fprintf(f, "    ]");
    }
    
    fprintf(f, "\n  }\n");
    fprintf(f, "}\n");
    
    fclose(f);
    return 0;
}

int libflux_export_json_string(const libflux_disk_diag_t *diag, char *buffer, int size) {
    if (!diag || !buffer || size <= 0) return -1;
    
    int pos = 0;
    
    #define APPEND(...) do { \
        int n = snprintf(buffer + pos, size - pos, __VA_ARGS__); \
        if (n > 0) pos += n; \
    } while(0)
    
    APPEND("{\"disk_diagnostics\":{");
    APPEND("\"filename\":\"%s\",", diag->filename);
    APPEND("\"format\":\"%s\",", diag->format);
    APPEND("\"tracks\":%d,", diag->tracks);
    APPEND("\"sides\":%d,", diag->sides);
    APPEND("\"sectors_ok\":%d,", diag->total_sectors_ok);
    APPEND("\"sectors_bad\":%d,", diag->total_sectors_bad);
    APPEND("\"quality\":%.1f", diag->overall_quality);
    APPEND("}}");
    
    #undef APPEND
    
    return pos;
}

int libflux_json_error(char *buffer, int size, int code, const char *message) {
    if (!buffer || size <= 0) return -1;
    
    return snprintf(buffer, size,
        "{\"error\":{\"code\":%d,\"message\":\"%s\"}}",
        code, message ? message : "Unknown error");
}

int libflux_json_progress(char *buffer, int size, int current, int total, const char *op) {
    if (!buffer || size <= 0) return -1;
    
    double percent = (total > 0) ? (100.0 * current / total) : 0;
    
    return snprintf(buffer, size,
        "{\"progress\":{\"operation\":\"%s\",\"current\":%d,\"total\":%d,\"percent\":%.1f}}",
        op ? op : "processing", current, total, percent);
}

int libflux_json_complete(char *buffer, int size, bool success, int processed, int failed) {
    if (!buffer || size <= 0) return -1;
    
    return snprintf(buffer, size,
        "{\"complete\":{\"success\":%s,\"processed\":%d,\"failed\":%d}}",
        success ? "true" : "false", processed, failed);
}
