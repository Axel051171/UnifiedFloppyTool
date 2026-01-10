/**
 * @file uft_adf_adapter.c
 * @brief ADF Format Adapter Implementation
 * @version 1.0.0
 * 
 * Bridges the ADF parser with the XDF Adapter system.
 * All ADF operations go through this adapter to access the XDF-API.
 */

#include "uft/adapters/uft_adf_adapter.h"
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/core/uft_score.h"
#include "uft/core/uft_error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define ADF_SECTOR_SIZE         512
#define ADF_TRACKS              80
#define ADF_SIDES               2
#define ADF_SECTORS_DD          11
#define ADF_SECTORS_HD          22

#define ADF_SIZE_DD             (ADF_TRACKS * ADF_SIDES * ADF_SECTORS_DD * ADF_SECTOR_SIZE)
#define ADF_SIZE_HD             (ADF_TRACKS * ADF_SIDES * ADF_SECTORS_HD * ADF_SECTOR_SIZE)

#define ADF_DOS_OFS             0x444F5300  /* "DOS\0" */
#define ADF_DOS_FFS             0x444F5301  /* "DOS\1" */

/* ═══════════════════════════════════════════════════════════════════════════════
 * ADF Context (private)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t *data;
    size_t size;
    
    /* Format info */
    bool is_hd;
    uint8_t sectors_per_track;
    uint32_t total_blocks;
    
    /* Boot block */
    uint32_t dos_type;
    bool bootblock_valid;
    bool has_bootcode;
    char disk_name[32];
    
    /* Filesystem */
    uint8_t fs_type;  /* 0=OFS, 1=FFS */
    uint32_t free_blocks;
    
} adf_context_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint32_t read_be32(const uint8_t *data) {
    return ((uint32_t)data[0] << 24) | 
           ((uint32_t)data[1] << 16) | 
           ((uint32_t)data[2] << 8) | 
           (uint32_t)data[3];
}

static uint32_t adf_bootblock_checksum(const uint8_t *data) {
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        if (i != 4) {  /* Skip checksum field */
            uint32_t val = read_be32(data + i);
            uint32_t old = sum;
            sum += val;
            if (sum < old) sum++;  /* Handle overflow */
        }
    }
    return ~sum;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_format_score_t adf_probe(
    const uint8_t *data,
    size_t size,
    const char *filename
) {
    uft_format_score_t score = uft_score_init();
    
    /* Size check */
    if (size == ADF_SIZE_DD) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "DD size (901120)");
        score.detail.amiga.fs_type = 0;
    } else if (size == ADF_SIZE_HD) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, "HD size (1802240)");
        score.detail.amiga.fs_type = 1;
    } else {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, false, "Invalid size");
        uft_score_finalize(&score);
        return score;
    }
    
    /* DOS type check */
    if (size >= 4) {
        uint32_t dos_type = read_be32(data);
        
        if ((dos_type & 0xFFFFFF00) == 0x444F5300) {  /* "DOS" */
            uft_score_add_match(&score, "magic", UFT_SCORE_WEIGHT_MAGIC, true, "DOS signature");
            score.detail.amiga.has_bootblock = true;
            
            uint8_t fs_variant = dos_type & 0xFF;
            if (fs_variant <= 5) {
                score.detail.amiga.fs_type = fs_variant;
            }
        } else if ((dos_type & 0xFFFFFF00) == 0x4B49434B) {  /* "KICK" */
            uft_score_add_match(&score, "magic", UFT_SCORE_WEIGHT_HIGH, true, "Kickstart signature");
            score.detail.amiga.has_bootblock = true;
        } else {
            /* No DOS signature - could still be valid but unformatted/custom */
            uft_score_add_match(&score, "magic", UFT_SCORE_WEIGHT_LOW, false, "No DOS signature");
        }
    }
    
    /* Bootblock checksum */
    if (size >= 1024) {
        uint32_t stored = read_be32(data + 4);
        uint32_t calculated = adf_bootblock_checksum(data);
        
        if (stored == calculated) {
            uft_score_add_match(&score, "checksum", UFT_SCORE_WEIGHT_MEDIUM, true, "Bootblock checksum valid");
        }
    }
    
    /* Extension check */
    if (filename) {
        size_t len = strlen(filename);
        if (len > 4) {
            const char *ext = filename + len - 4;
            if (strcasecmp(ext, ".adf") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".adf");
            } else if (strcasecmp(ext, ".adz") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".adz");
            }
        }
    }
    
    uft_score_finalize(&score);
    return score;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t adf_open(
    struct uft_xdf_context *ctx,
    const uint8_t *data,
    size_t size
) {
    if (!ctx || !data) {
        return UFT_E_INVALID_ARG;
    }
    
    /* Allocate context */
    adf_context_t *adf = calloc(1, sizeof(adf_context_t));
    if (!adf) {
        return UFT_E_NOMEM;
    }
    
    adf->data = data;
    adf->size = size;
    
    /* Determine format */
    if (size == ADF_SIZE_DD) {
        adf->is_hd = false;
        adf->sectors_per_track = ADF_SECTORS_DD;
    } else if (size == ADF_SIZE_HD) {
        adf->is_hd = true;
        adf->sectors_per_track = ADF_SECTORS_HD;
    } else {
        free(adf);
        return UFT_E_FORMAT;
    }
    
    adf->total_blocks = size / ADF_SECTOR_SIZE;
    
    /* Parse bootblock */
    if (size >= 4) {
        adf->dos_type = read_be32(data);
        adf->bootblock_valid = ((adf->dos_type & 0xFFFFFF00) == 0x444F5300);
        
        /* Check for bootcode */
        if (size >= 1024) {
            adf->has_bootcode = false;
            for (size_t i = 12; i < 1024; i++) {
                if (data[i] != 0) {
                    adf->has_bootcode = true;
                    break;
                }
            }
        }
    }
    
    /* Parse root block (block 880 for DD) */
    uint32_t root_block = (adf->total_blocks / 2);  /* Middle of disk */
    size_t root_offset = root_block * ADF_SECTOR_SIZE;
    
    if (root_offset + ADF_SECTOR_SIZE <= size) {
        const uint8_t *root = data + root_offset;
        
        /* Read disk name (offset 0x1B1, BCPL string) */
        uint8_t name_len = root[0x1B0];
        if (name_len > 0 && name_len < 31) {
            memcpy(adf->disk_name, root + 0x1B1, name_len);
            adf->disk_name[name_len] = '\0';
        }
    }
    
    /* Store context */
    ctx->format_data = adf;
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Track Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t adf_read_track(
    struct uft_xdf_context *ctx,
    uint16_t track,
    uint8_t side,
    uft_track_data_t *out
) {
    if (!ctx || !ctx->format_data || !out) {
        return UFT_E_INVALID_ARG;
    }
    
    adf_context_t *adf = (adf_context_t *)ctx->format_data;
    
    if (track >= ADF_TRACKS || side >= ADF_SIDES) {
        return UFT_E_RANGE;
    }
    
    uft_track_data_init(out);
    
    out->track_num = track;
    out->side = side;
    out->encoding = 1;  /* MFM */
    
    /* Calculate track offset */
    uint32_t track_index = track * ADF_SIDES + side;
    size_t track_size = adf->sectors_per_track * ADF_SECTOR_SIZE;
    size_t track_offset = track_index * track_size;
    
    if (track_offset + track_size > adf->size) {
        return UFT_E_RANGE;
    }
    
    /* Copy raw track data */
    out->raw_size = track_size;
    out->raw_data = malloc(track_size);
    if (!out->raw_data) {
        return UFT_E_NOMEM;
    }
    memcpy(out->raw_data, adf->data + track_offset, track_size);
    
    /* Allocate sectors */
    uft_error_t err = uft_track_alloc_sectors(out, adf->sectors_per_track);
    if (err != UFT_SUCCESS) {
        free(out->raw_data);
        out->raw_data = NULL;
        return err;
    }
    
    /* Fill sector data */
    for (uint8_t s = 0; s < adf->sectors_per_track; s++) {
        uft_sector_data_t *sector = &out->sectors[s];
        
        sector->logical_track = track;
        sector->head = side;
        sector->sector_id = s;
        sector->size_code = 2;  /* 512 bytes */
        
        size_t sector_offset = s * ADF_SECTOR_SIZE;
        sector->data_size = ADF_SECTOR_SIZE;
        sector->data = malloc(ADF_SECTOR_SIZE);
        if (sector->data) {
            memcpy(sector->data, out->raw_data + sector_offset, ADF_SECTOR_SIZE);
        }
        
        sector->confidence = 10000;  /* Perfect for ADF (no errors stored) */
        sector->crc_ok = true;
        sector->deleted = false;
    }
    
    out->confidence = 10000;
    snprintf(out->diag_message, sizeof(out->diag_message),
             "Track %u.%u: %u sectors, %zu bytes",
             track, side, adf->sectors_per_track, track_size);
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Get Geometry Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void adf_get_geometry(
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
    
    adf_context_t *adf = (adf_context_t *)ctx->format_data;
    
    if (tracks) *tracks = ADF_TRACKS;
    if (sides) *sides = ADF_SIDES;
    if (sectors) *sectors = adf->sectors_per_track;
    if (sector_size) *sector_size = ADF_SECTOR_SIZE;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Close Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void adf_close(struct uft_xdf_context *ctx) {
    if (!ctx || !ctx->format_data) return;
    
    adf_context_t *adf = (adf_context_t *)ctx->format_data;
    free(adf);
    ctx->format_data = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_format_adapter_t uft_adf_adapter = {
    .name = "ADF",
    .description = "Amiga Disk File (DD/HD)",
    .extensions = "adf,adz",
    .format_id = UFT_FORMAT_ID_ADF,
    
    .can_read = true,
    .can_write = false,  /* TODO: implement write */
    .can_create = false,
    .supports_errors = false,
    .supports_timing = false,
    
    .probe = adf_probe,
    .open = adf_open,
    .read_track = adf_read_track,
    .get_geometry = adf_get_geometry,
    .write_track = NULL,
    .export_native = NULL,
    .close = adf_close,
    
    .private_data = NULL
};

/* ═══════════════════════════════════════════════════════════════════════════════
 * Initialization
 * ═══════════════════════════════════════════════════════════════════════════════ */

void uft_adf_adapter_init(void) {
    uft_adapter_register(&uft_adf_adapter);
}
