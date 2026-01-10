/**
 * @file uft_d64_adapter.c
 * @brief D64 Format Adapter Implementation
 * @version 1.0.0
 * 
 * Bridges the D64 (C64) parser with the XDF Adapter system.
 */

#include "uft/adapters/uft_d64_adapter.h"
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/core/uft_score.h"
#include "uft/core/uft_error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * D64 Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define D64_SECTOR_SIZE         256
#define D64_TRACKS_35           35
#define D64_TRACKS_40           40

/* Standard D64 sizes */
#define D64_SIZE_35             174848      /* 35 tracks, no errors */
#define D64_SIZE_35_ERR         175531      /* 35 tracks + error info */
#define D64_SIZE_40             196608      /* 40 tracks, no errors */
#define D64_SIZE_40_ERR         197376      /* 40 tracks + error info */

/* Sectors per track (varies by zone) */
static const uint8_t d64_sectors_per_track[] = {
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,                                          /* 18-24 */
    18, 18, 18, 18, 18, 18,                                              /* 25-30 */
    17, 17, 17, 17, 17,                                                  /* 31-35 */
    17, 17, 17, 17, 17                                                   /* 36-40 (extended) */
};

/* Track offsets (precalculated) */
static const uint32_t d64_track_offsets[] = {
    0,      /* Track 1 */
    0x1500, 0x2A00, 0x3F00, 0x5400, 0x6900, 0x7E00, 0x9300, 0xA800,
    0xBD00, 0xD200, 0xE700, 0xFC00, 0x11100, 0x12600, 0x13B00, 0x15000,
    0x16500,  /* Track 18 (directory) */
    0x17800, 0x18B00, 0x19E00, 0x1B100, 0x1C400, 0x1D700,
    0x1EA00, 0x1FC00, 0x20E00, 0x22000, 0x23200, 0x24400,
    0x25600, 0x26700, 0x27800, 0x28900, 0x29A00,
    0x2AB00, 0x2BC00, 0x2CD00, 0x2DE00, 0x2EF00
};

#define D64_DIR_TRACK           18
#define D64_DIR_SECTOR          1
#define D64_BAM_TRACK           18
#define D64_BAM_SECTOR          0

/* ═══════════════════════════════════════════════════════════════════════════════
 * D64 Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t *data;
    size_t size;
    
    /* Format info */
    uint8_t tracks;
    bool has_error_bytes;
    uint16_t total_sectors;
    
    /* BAM info */
    char disk_name[17];
    char disk_id[3];
    uint8_t dos_type;
    uint16_t free_sectors;
    
    /* Error bytes (if present) */
    const uint8_t *error_bytes;
    
} d64_context_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint8_t d64_get_sectors_for_track(uint8_t track) {
    if (track < 1 || track > 40) return 0;
    return d64_sectors_per_track[track - 1];
}

static uint32_t d64_get_track_offset(uint8_t track) {
    if (track < 1 || track > 40) return 0;
    return d64_track_offsets[track - 1];
}

static uint16_t d64_count_sectors(uint8_t tracks) {
    uint16_t count = 0;
    for (uint8_t t = 1; t <= tracks; t++) {
        count += d64_get_sectors_for_track(t);
    }
    return count;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_format_score_t d64_probe(
    const uint8_t *data,
    size_t size,
    const char *filename
) {
    uft_format_score_t score = uft_score_init();
    
    /* Size check */
    bool valid_size = false;
    if (size == D64_SIZE_35) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "35 tracks");
        score.detail.c64.tracks = 35;
        valid_size = true;
    } else if (size == D64_SIZE_35_ERR) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "35 tracks + errors");
        score.detail.c64.tracks = 35;
        score.detail.c64.has_errors = true;
        valid_size = true;
    } else if (size == D64_SIZE_40) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "40 tracks");
        score.detail.c64.tracks = 40;
        valid_size = true;
    } else if (size == D64_SIZE_40_ERR) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "40 tracks + errors");
        score.detail.c64.tracks = 40;
        score.detail.c64.has_errors = true;
        valid_size = true;
    }
    
    if (!valid_size) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, false, "Invalid size");
        uft_score_finalize(&score);
        return score;
    }
    
    /* Check BAM (track 18, sector 0) */
    if (size >= d64_track_offsets[17] + D64_SECTOR_SIZE) {
        const uint8_t *bam = data + d64_track_offsets[17];
        
        /* First directory track/sector pointer */
        if (bam[0] == D64_DIR_TRACK && bam[1] == D64_DIR_SECTOR) {
            uft_score_add_match(&score, "bam_ptr", UFT_SCORE_WEIGHT_MEDIUM, true, "Valid BAM pointer");
        }
        
        /* DOS type at offset 2 */
        uint8_t dos = bam[2];
        if (dos == 0x41 || dos == 0x00) {  /* 'A' or null */
            uft_score_add_match(&score, "dos_type", UFT_SCORE_WEIGHT_LOW, true, "Valid DOS type");
            score.detail.c64.dos_type = dos;
        }
        
        /* Disk name at offset 0x90 (PETSCII, padded with 0xA0) */
        bool valid_name = true;
        for (int i = 0; i < 16; i++) {
            uint8_t c = bam[0x90 + i];
            if (c != 0xA0 && (c < 0x20 || c > 0x7F)) {
                if (c < 0x41 || c > 0x5A) {  /* Not uppercase PETSCII */
                    valid_name = false;
                    break;
                }
            }
        }
        if (valid_name) {
            uft_score_add_match(&score, "disk_name", UFT_SCORE_WEIGHT_LOW, true, "Valid disk name");
        }
    }
    
    /* Extension check */
    if (filename) {
        size_t len = strlen(filename);
        if (len > 4) {
            const char *ext = filename + len - 4;
            if (strcasecmp(ext, ".d64") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".d64");
            }
        }
    }
    
    uft_score_finalize(&score);
    return score;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t d64_open(
    struct uft_xdf_context *ctx,
    const uint8_t *data,
    size_t size
) {
    if (!ctx || !data) {
        return UFT_E_INVALID_ARG;
    }
    
    d64_context_t *d64 = calloc(1, sizeof(d64_context_t));
    if (!d64) {
        return UFT_E_NOMEM;
    }
    
    d64->data = data;
    d64->size = size;
    
    /* Determine format */
    if (size == D64_SIZE_35 || size == D64_SIZE_35_ERR) {
        d64->tracks = 35;
        d64->has_error_bytes = (size == D64_SIZE_35_ERR);
    } else if (size == D64_SIZE_40 || size == D64_SIZE_40_ERR) {
        d64->tracks = 40;
        d64->has_error_bytes = (size == D64_SIZE_40_ERR);
    } else {
        free(d64);
        return UFT_E_FORMAT;
    }
    
    d64->total_sectors = d64_count_sectors(d64->tracks);
    
    /* Set up error bytes pointer */
    if (d64->has_error_bytes) {
        d64->error_bytes = data + (d64->total_sectors * D64_SECTOR_SIZE);
    }
    
    /* Parse BAM */
    if (size >= d64_track_offsets[17] + D64_SECTOR_SIZE) {
        const uint8_t *bam = data + d64_track_offsets[17];
        
        d64->dos_type = bam[2];
        
        /* Disk name (convert from PETSCII) */
        for (int i = 0; i < 16; i++) {
            uint8_t c = bam[0x90 + i];
            if (c == 0xA0) break;
            d64->disk_name[i] = (c >= 0x41 && c <= 0x5A) ? c : (c & 0x7F);
        }
        
        /* Disk ID */
        d64->disk_id[0] = bam[0xA2];
        d64->disk_id[1] = bam[0xA3];
        d64->disk_id[2] = '\0';
        
        /* Count free sectors */
        d64->free_sectors = 0;
        for (int t = 1; t <= 35; t++) {
            if (t != 18) {  /* Skip directory track */
                d64->free_sectors += bam[4 * t];
            }
        }
    }
    
    ctx->format_data = d64;
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Track Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t d64_read_track(
    struct uft_xdf_context *ctx,
    uint16_t track,
    uint8_t side,
    uft_track_data_t *out
) {
    if (!ctx || !ctx->format_data || !out) {
        return UFT_E_INVALID_ARG;
    }
    
    d64_context_t *d64 = (d64_context_t *)ctx->format_data;
    
    /* D64 is single-sided, track numbers are 1-based */
    if (track == 0) track = 1;  /* Convert 0-based to 1-based */
    if (track > d64->tracks || side > 0) {
        return UFT_E_RANGE;
    }
    
    uft_track_data_init(out);
    
    out->track_num = track;
    out->side = 0;
    out->encoding = 2;  /* GCR */
    
    uint8_t sectors = d64_get_sectors_for_track(track);
    uint32_t track_offset = d64_get_track_offset(track);
    size_t track_size = sectors * D64_SECTOR_SIZE;
    
    if (track_offset + track_size > d64->size) {
        return UFT_E_RANGE;
    }
    
    /* Copy raw track data */
    out->raw_size = track_size;
    out->raw_data = malloc(track_size);
    if (!out->raw_data) {
        return UFT_E_NOMEM;
    }
    memcpy(out->raw_data, d64->data + track_offset, track_size);
    
    /* Allocate sectors */
    uft_error_t err = uft_track_alloc_sectors(out, sectors);
    if (err != UFT_SUCCESS) {
        free(out->raw_data);
        out->raw_data = NULL;
        return err;
    }
    
    /* Calculate sector index for error bytes */
    uint16_t sector_index = 0;
    for (uint8_t t = 1; t < track; t++) {
        sector_index += d64_get_sectors_for_track(t);
    }
    
    /* Fill sector data */
    for (uint8_t s = 0; s < sectors; s++) {
        uft_sector_data_t *sector = &out->sectors[s];
        
        sector->logical_track = track;
        sector->head = 0;
        sector->sector_id = s;
        sector->size_code = 1;  /* 256 bytes */
        
        size_t sector_offset = s * D64_SECTOR_SIZE;
        sector->data_size = D64_SECTOR_SIZE;
        sector->data = malloc(D64_SECTOR_SIZE);
        if (sector->data) {
            memcpy(sector->data, out->raw_data + sector_offset, D64_SECTOR_SIZE);
        }
        
        /* Check error byte if present */
        if (d64->has_error_bytes && d64->error_bytes) {
            uint8_t err_byte = d64->error_bytes[sector_index + s];
            sector->crc_ok = (err_byte == 0x01);  /* 0x01 = OK */
            sector->confidence = sector->crc_ok ? 10000 : 5000;
            sector->st1 = err_byte;
        } else {
            sector->crc_ok = true;
            sector->confidence = 10000;
        }
        
        sector->deleted = false;
    }
    
    out->confidence = 10000;
    snprintf(out->diag_message, sizeof(out->diag_message),
             "Track %u: %u sectors, %zu bytes",
             track, sectors, track_size);
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Get Geometry Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void d64_get_geometry(
    struct uft_xdf_context *ctx,
    uint16_t *tracks,
    uint8_t *sides,
    uint8_t *sectors,
    uint16_t *sector_size
) {
    if (!ctx || !ctx->format_data) {
        if (tracks) *tracks = 0;
        if (sides) *sides = 0;
        if (sectors) *sectors = 0;
        if (sector_size) *sector_size = 0;
        return;
    }
    
    d64_context_t *d64 = (d64_context_t *)ctx->format_data;
    
    if (tracks) *tracks = d64->tracks;
    if (sides) *sides = 1;  /* Single-sided */
    if (sectors) *sectors = 21;  /* Max sectors (track 1-17) */
    if (sector_size) *sector_size = D64_SECTOR_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Close Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void d64_close(struct uft_xdf_context *ctx) {
    if (!ctx || !ctx->format_data) return;
    
    d64_context_t *d64 = (d64_context_t *)ctx->format_data;
    free(d64);
    ctx->format_data = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_format_adapter_t uft_d64_adapter = {
    .name = "D64",
    .description = "Commodore 64 Disk Image",
    .extensions = "d64",
    .format_id = UFT_FORMAT_ID_D64,
    
    .can_read = true,
    .can_write = false,
    .can_create = false,
    .supports_errors = true,
    .supports_timing = false,
    
    .probe = d64_probe,
    .open = d64_open,
    .read_track = d64_read_track,
    .get_geometry = d64_get_geometry,
    .write_track = NULL,
    .export_native = NULL,
    .close = d64_close,
    
    .private_data = NULL
};

void uft_d64_adapter_init(void) {
    uft_adapter_register(&uft_d64_adapter);
}
