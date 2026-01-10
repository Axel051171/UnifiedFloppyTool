/**
 * @file uft_trd_adapter.c
 * @brief TRD Format Adapter Implementation
 * @version 1.0.0
 * 
 * ZX Spectrum TR-DOS disk images.
 * Supports: 80T DS (640K), 40T DS (320K), 80T SS (320K), 40T SS (160K)
 */

#include "uft/adapters/uft_trd_adapter.h"
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/core/uft_score.h"
#include "uft/core/uft_error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRD Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define TRD_SECTOR_SIZE         256
#define TRD_SECTORS_PER_TRACK   16

/* Standard TRD sizes */
#define TRD_SIZE_640K           655360      /* 80T × 2S × 16S × 256B */
#define TRD_SIZE_320K_DS        327680      /* 40T × 2S × 16S × 256B */
#define TRD_SIZE_320K_SS        327680      /* 80T × 1S × 16S × 256B */
#define TRD_SIZE_160K           163840      /* 40T × 1S × 16S × 256B */

/* System sector (track 0, sector 9) offsets */
#define TRD_SYS_SECTOR          8           /* Sector 9 (0-based: 8) */
#define TRD_SYS_OFFSET          (TRD_SYS_SECTOR * TRD_SECTOR_SIZE)

#define TRD_SYS_FIRST_FREE_SEC  0xE1
#define TRD_SYS_FIRST_FREE_TRK  0xE2
#define TRD_SYS_DISK_TYPE       0xE3
#define TRD_SYS_FILE_COUNT      0xE4
#define TRD_SYS_FREE_SECTORS    0xE5        /* 2 bytes LE */
#define TRD_SYS_TRDOS_ID        0xE7        /* Should be 0x10 */
#define TRD_SYS_DISK_LABEL      0xF5        /* 8 bytes */

/* Disk type values */
#define TRD_TYPE_80T_DS         0x16
#define TRD_TYPE_40T_DS         0x17
#define TRD_TYPE_80T_SS         0x18
#define TRD_TYPE_40T_SS         0x19

/* ═══════════════════════════════════════════════════════════════════════════════
 * TRD Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t *data;
    size_t size;
    
    /* Geometry */
    uint8_t tracks;
    uint8_t sides;
    uint16_t total_sectors;
    
    /* System info */
    uint8_t disk_type;
    uint8_t file_count;
    uint16_t free_sectors;
    uint8_t first_free_track;
    uint8_t first_free_sector;
    char disk_label[9];
    
    /* Validation */
    bool sys_sector_valid;
    
} trd_context_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void trd_get_geometry_from_type(uint8_t type, uint8_t *tracks, uint8_t *sides) {
    switch (type) {
        case TRD_TYPE_80T_DS: *tracks = 80; *sides = 2; break;
        case TRD_TYPE_40T_DS: *tracks = 40; *sides = 2; break;
        case TRD_TYPE_80T_SS: *tracks = 80; *sides = 1; break;
        case TRD_TYPE_40T_SS: *tracks = 40; *sides = 1; break;
        default:             *tracks = 80; *sides = 2; break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_format_score_t trd_probe(
    const uint8_t *data,
    size_t size,
    const char *filename
) {
    uft_format_score_t score = uft_score_init();
    
    /* Size check */
    bool valid_size = false;
    const char *size_desc = NULL;
    
    if (size == TRD_SIZE_640K) {
        valid_size = true;
        size_desc = "640K (80T DS)";
        score.detail.spectrum.tracks = 80;
        score.detail.spectrum.is_double = true;
    } else if (size == TRD_SIZE_320K_DS) {
        valid_size = true;
        size_desc = "320K";
        score.detail.spectrum.tracks = 40;
    } else if (size == TRD_SIZE_160K) {
        valid_size = true;
        size_desc = "160K (40T SS)";
        score.detail.spectrum.tracks = 40;
        score.detail.spectrum.is_double = false;
    }
    
    if (valid_size) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, size_desc);
    } else if (size % (TRD_SECTOR_SIZE * TRD_SECTORS_PER_TRACK) == 0) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_LOW, true, "Non-standard TRD size");
    } else {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, false, "Invalid size");
        uft_score_finalize(&score);
        return score;
    }
    
    /* Check system sector */
    if (size >= TRD_SYS_OFFSET + TRD_SECTOR_SIZE) {
        const uint8_t *sys = data + TRD_SYS_OFFSET;
        
        /* TR-DOS ID at 0xE7 should be 0x10 */
        if (sys[TRD_SYS_TRDOS_ID - TRD_SYS_OFFSET + TRD_SYS_OFFSET - TRD_SYS_OFFSET] == 0x10) {
            /* Recalculate offset properly */
            if (sys[0xE7 - 0xE1 + 0xE1 - TRD_SYS_OFFSET + TRD_SYS_OFFSET] == 0x10 ||
                data[TRD_SYS_OFFSET + 0xE7 - (TRD_SYS_SECTOR * TRD_SECTOR_SIZE)] == 0x10) {
                /* Check at absolute offset */
            }
        }
        
        /* Check TR-DOS ID byte at offset 0x8E7 (sector 9, offset 0xE7) */
        size_t trdos_id_offset = TRD_SYS_OFFSET + (0xE7 - 0xE1);
        if (trdos_id_offset < size && data[TRD_SYS_OFFSET + 6] == 0x10) {
            uft_score_add_match(&score, "trdos_id", UFT_SCORE_WEIGHT_MAGIC, true, "TR-DOS signature");
        }
        
        /* Check disk type */
        uint8_t disk_type = data[TRD_SYS_OFFSET + 2];  /* 0xE3 offset in sector */
        if (disk_type >= TRD_TYPE_80T_DS && disk_type <= TRD_TYPE_40T_SS) {
            uft_score_add_match(&score, "disk_type", UFT_SCORE_WEIGHT_MEDIUM, true, "Valid disk type");
            score.detail.spectrum.type = disk_type;
        }
        
        /* Check free sectors makes sense */
        uint16_t free_secs = read_le16(data + TRD_SYS_OFFSET + 4);
        uint16_t max_secs = (size / TRD_SECTOR_SIZE);
        if (free_secs <= max_secs) {
            uft_score_add_match(&score, "free_secs", UFT_SCORE_WEIGHT_LOW, true, "Valid free sectors");
        }
    }
    
    /* Extension check */
    if (filename) {
        size_t len = strlen(filename);
        if (len > 4) {
            const char *ext = filename + len - 4;
            if (strcasecmp(ext, ".trd") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".trd");
            }
        }
    }
    
    uft_score_finalize(&score);
    return score;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t trd_open(
    struct uft_xdf_context *ctx,
    const uint8_t *data,
    size_t size
) {
    if (!ctx || !data) {
        return UFT_E_INVALID_ARG;
    }
    
    trd_context_t *trd = calloc(1, sizeof(trd_context_t));
    if (!trd) {
        return UFT_E_NOMEM;
    }
    
    trd->data = data;
    trd->size = size;
    
    /* Determine geometry from size */
    if (size == TRD_SIZE_640K) {
        trd->tracks = 80;
        trd->sides = 2;
    } else if (size == TRD_SIZE_320K_DS) {
        trd->tracks = 40;
        trd->sides = 2;
    } else if (size == TRD_SIZE_160K) {
        trd->tracks = 40;
        trd->sides = 1;
    } else {
        /* Guess: assume 16 sectors per track, 256 bytes per sector */
        uint32_t total_tracks = size / (TRD_SECTORS_PER_TRACK * TRD_SECTOR_SIZE);
        if (total_tracks >= 160) {
            trd->tracks = 80;
            trd->sides = 2;
        } else if (total_tracks >= 80) {
            trd->tracks = 80;
            trd->sides = 1;
        } else {
            trd->tracks = 40;
            trd->sides = (total_tracks >= 80) ? 2 : 1;
        }
    }
    
    trd->total_sectors = trd->tracks * trd->sides * TRD_SECTORS_PER_TRACK;
    
    /* Parse system sector */
    if (size >= TRD_SYS_OFFSET + TRD_SECTOR_SIZE) {
        const uint8_t *sys = data + TRD_SYS_OFFSET;
        
        trd->first_free_sector = sys[0];        /* 0xE1 */
        trd->first_free_track = sys[1];         /* 0xE2 */
        trd->disk_type = sys[2];                /* 0xE3 */
        trd->file_count = sys[3];               /* 0xE4 */
        trd->free_sectors = read_le16(sys + 4); /* 0xE5-E6 */
        
        /* Override geometry from disk type if valid */
        if (trd->disk_type >= TRD_TYPE_80T_DS && trd->disk_type <= TRD_TYPE_40T_SS) {
            trd_get_geometry_from_type(trd->disk_type, &trd->tracks, &trd->sides);
            trd->sys_sector_valid = true;
        }
        
        /* Disk label at 0xF5 */
        memcpy(trd->disk_label, sys + 20, 8);  /* 0xF5 - 0xE1 = 20 */
        trd->disk_label[8] = '\0';
    }
    
    ctx->format_data = trd;
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Track Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t trd_read_track(
    struct uft_xdf_context *ctx,
    uint16_t track,
    uint8_t side,
    uft_track_data_t *out
) {
    if (!ctx || !ctx->format_data || !out) {
        return UFT_E_INVALID_ARG;
    }
    
    trd_context_t *trd = (trd_context_t *)ctx->format_data;
    
    if (track >= trd->tracks || side >= trd->sides) {
        return UFT_E_RANGE;
    }
    
    uft_track_data_init(out);
    
    out->track_num = track;
    out->side = side;
    out->encoding = 1;  /* MFM */
    
    /* TRD interleaves sides: T0S0, T0S1, T1S0, T1S1, ... */
    uint32_t track_index = track * trd->sides + side;
    size_t track_size = TRD_SECTORS_PER_TRACK * TRD_SECTOR_SIZE;
    size_t track_offset = track_index * track_size;
    
    if (track_offset + track_size > trd->size) {
        return UFT_E_RANGE;
    }
    
    /* Copy raw track data */
    out->raw_size = track_size;
    out->raw_data = malloc(track_size);
    if (!out->raw_data) {
        return UFT_E_NOMEM;
    }
    memcpy(out->raw_data, trd->data + track_offset, track_size);
    
    /* Allocate sectors */
    uft_error_t err = uft_track_alloc_sectors(out, TRD_SECTORS_PER_TRACK);
    if (err != UFT_SUCCESS) {
        free(out->raw_data);
        out->raw_data = NULL;
        return err;
    }
    
    /* Fill sector data (TR-DOS uses 1-based sector IDs) */
    for (uint8_t s = 0; s < TRD_SECTORS_PER_TRACK; s++) {
        uft_sector_data_t *sector = &out->sectors[s];
        
        sector->logical_track = track;
        sector->head = side;
        sector->sector_id = s + 1;
        sector->size_code = 1;  /* 256 bytes */
        
        size_t sector_offset = s * TRD_SECTOR_SIZE;
        sector->data_size = TRD_SECTOR_SIZE;
        sector->data = malloc(TRD_SECTOR_SIZE);
        if (sector->data) {
            memcpy(sector->data, out->raw_data + sector_offset, TRD_SECTOR_SIZE);
        }
        
        sector->confidence = 10000;
        sector->crc_ok = true;
        sector->deleted = false;
    }
    
    out->confidence = 10000;
    snprintf(out->diag_message, sizeof(out->diag_message),
             "Track %u.%u: %u sectors, %zu bytes",
             track, side, TRD_SECTORS_PER_TRACK, track_size);
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Get Geometry Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void trd_get_geometry(
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
    
    trd_context_t *trd = (trd_context_t *)ctx->format_data;
    
    if (tracks) *tracks = trd->tracks;
    if (sides) *sides = trd->sides;
    if (sectors) *sectors = TRD_SECTORS_PER_TRACK;
    if (sector_size) *sector_size = TRD_SECTOR_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Close Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void trd_close(struct uft_xdf_context *ctx) {
    if (!ctx || !ctx->format_data) return;
    
    trd_context_t *trd = (trd_context_t *)ctx->format_data;
    free(trd);
    ctx->format_data = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_format_adapter_t uft_trd_adapter = {
    .name = "TRD",
    .description = "ZX Spectrum TR-DOS Disk Image",
    .extensions = "trd",
    .format_id = UFT_FORMAT_ID_TRD,
    
    .can_read = true,
    .can_write = false,
    .can_create = false,
    .supports_errors = false,
    .supports_timing = false,
    
    .probe = trd_probe,
    .open = trd_open,
    .read_track = trd_read_track,
    .get_geometry = trd_get_geometry,
    .write_track = NULL,
    .export_native = NULL,
    .close = trd_close,
    
    .private_data = NULL
};

void uft_trd_adapter_init(void) {
    uft_adapter_register(&uft_trd_adapter);
}
