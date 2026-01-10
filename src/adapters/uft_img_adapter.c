/**
 * @file uft_img_adapter.c
 * @brief IMG/IMA Format Adapter Implementation
 * @version 1.0.0
 * 
 * PC floppy disk images - FAT12 filesystem.
 * Supports: 360K, 720K, 1.2M, 1.44M, 2.88M
 */

#include "uft/adapters/uft_img_adapter.h"
#include "uft/xdf/uft_xdf_adapter.h"
#include "uft/core/uft_score.h"
#include "uft/core/uft_error_codes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMG Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define IMG_SECTOR_SIZE         512

/* Standard PC floppy sizes */
#define IMG_SIZE_160K           163840      /* 5.25" SS/DD 40T 8S */
#define IMG_SIZE_180K           184320      /* 5.25" SS/DD 40T 9S */
#define IMG_SIZE_320K           327680      /* 5.25" DS/DD 40T 8S */
#define IMG_SIZE_360K           368640      /* 5.25" DS/DD 40T 9S */
#define IMG_SIZE_720K           737280      /* 3.5" DS/DD 80T 9S */
#define IMG_SIZE_1200K          1228800     /* 5.25" DS/HD 80T 15S */
#define IMG_SIZE_1440K          1474560     /* 3.5" DS/HD 80T 18S */
#define IMG_SIZE_2880K          2949120     /* 3.5" DS/ED 80T 36S */

/* Boot sector offsets */
#define BPB_BYTES_PER_SECTOR    11
#define BPB_SECTORS_PER_CLUSTER 13
#define BPB_RESERVED_SECTORS    14
#define BPB_NUM_FATS            16
#define BPB_ROOT_ENTRIES        17
#define BPB_TOTAL_SECTORS_16    19
#define BPB_MEDIA_DESCRIPTOR    21
#define BPB_SECTORS_PER_FAT     22
#define BPB_SECTORS_PER_TRACK   24
#define BPB_NUM_HEADS           26
#define BPB_TOTAL_SECTORS_32    32

/* Media descriptors */
#define MEDIA_160K              0xFE
#define MEDIA_180K              0xFC
#define MEDIA_320K              0xFF
#define MEDIA_360K              0xFD
#define MEDIA_720K              0xF9
#define MEDIA_1200K             0xF9
#define MEDIA_1440K             0xF0
#define MEDIA_2880K             0xF0

/* ═══════════════════════════════════════════════════════════════════════════════
 * IMG Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    const uint8_t *data;
    size_t size;
    
    /* Geometry */
    uint16_t bytes_per_sector;
    uint8_t sectors_per_track;
    uint8_t heads;
    uint16_t tracks;
    uint32_t total_sectors;
    
    /* BPB info */
    uint8_t media_descriptor;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t num_fats;
    uint16_t root_entries;
    uint16_t sectors_per_fat;
    
    /* Volume info */
    char oem_name[9];
    char volume_label[12];
    char fs_type[9];
    
} img_context_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uint16_t read_le16(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint32_t read_le32(const uint8_t *data) {
    return (uint32_t)data[0] | 
           ((uint32_t)data[1] << 8) | 
           ((uint32_t)data[2] << 16) | 
           ((uint32_t)data[3] << 24);
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_format_score_t img_probe(
    const uint8_t *data,
    size_t size,
    const char *filename
) {
    uft_format_score_t score = uft_score_init();
    
    /* Size check */
    bool valid_size = false;
    const char *size_desc = NULL;
    
    switch (size) {
        case IMG_SIZE_160K:  valid_size = true; size_desc = "160K"; break;
        case IMG_SIZE_180K:  valid_size = true; size_desc = "180K"; break;
        case IMG_SIZE_320K:  valid_size = true; size_desc = "320K"; break;
        case IMG_SIZE_360K:  valid_size = true; size_desc = "360K"; break;
        case IMG_SIZE_720K:  valid_size = true; size_desc = "720K"; break;
        case IMG_SIZE_1200K: valid_size = true; size_desc = "1.2M"; break;
        case IMG_SIZE_1440K: valid_size = true; size_desc = "1.44M"; break;
        case IMG_SIZE_2880K: valid_size = true; size_desc = "2.88M"; break;
    }
    
    if (valid_size) {
        uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, true, size_desc);
    } else {
        /* Check if it's a multiple of 512 */
        if (size % IMG_SECTOR_SIZE == 0 && size >= IMG_SIZE_160K && size <= IMG_SIZE_2880K * 2) {
            uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_LOW, true, "Non-standard size");
        } else {
            uft_score_add_match(&score, "size", UFT_SCORE_WEIGHT_HIGH, false, "Invalid size");
            uft_score_finalize(&score);
            return score;
        }
    }
    
    /* Boot sector check */
    if (size >= 512) {
        /* Check for x86 jump instruction */
        if ((data[0] == 0xEB && data[2] == 0x90) ||  /* JMP short + NOP */
            (data[0] == 0xE9)) {                      /* JMP near */
            uft_score_add_match(&score, "jump", UFT_SCORE_WEIGHT_MEDIUM, true, "x86 jump instruction");
        }
        
        /* Check media descriptor */
        uint8_t media = data[BPB_MEDIA_DESCRIPTOR];
        if (media >= 0xF0) {
            uft_score_add_match(&score, "media", UFT_SCORE_WEIGHT_MEDIUM, true, "Valid media descriptor");
            score.detail.pc.media_type = media;
        }
        
        /* Check bytes per sector */
        uint16_t bps = read_le16(data + BPB_BYTES_PER_SECTOR);
        if (bps == 512 || bps == 1024 || bps == 2048 || bps == 4096) {
            uft_score_add_match(&score, "bps", UFT_SCORE_WEIGHT_LOW, true, "Valid sector size");
        }
        
        /* Check sectors per track */
        uint16_t spt = read_le16(data + BPB_SECTORS_PER_TRACK);
        if (spt >= 8 && spt <= 36) {
            score.detail.pc.sectors = spt;
        }
        
        /* Check 0x55AA signature */
        if (data[510] == 0x55 && data[511] == 0xAA) {
            uft_score_add_match(&score, "signature", UFT_SCORE_WEIGHT_MEDIUM, true, "Boot signature");
        }
    }
    
    /* Extension check */
    if (filename) {
        size_t len = strlen(filename);
        if (len > 4) {
            const char *ext = filename + len - 4;
            if (strcasecmp(ext, ".img") == 0 || strcasecmp(ext, ".ima") == 0) {
                uft_score_add_match(&score, "extension", UFT_SCORE_WEIGHT_LOW, true, ".img/.ima");
            }
        }
    }
    
    uft_score_finalize(&score);
    return score;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Open Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t img_open(
    struct uft_xdf_context *ctx,
    const uint8_t *data,
    size_t size
) {
    if (!ctx || !data || size < 512) {
        return UFT_E_INVALID_ARG;
    }
    
    img_context_t *img = calloc(1, sizeof(img_context_t));
    if (!img) {
        return UFT_E_NOMEM;
    }
    
    img->data = data;
    img->size = size;
    
    /* Parse BPB */
    img->bytes_per_sector = read_le16(data + BPB_BYTES_PER_SECTOR);
    if (img->bytes_per_sector == 0) img->bytes_per_sector = IMG_SECTOR_SIZE;
    
    img->sectors_per_cluster = data[BPB_SECTORS_PER_CLUSTER];
    img->reserved_sectors = read_le16(data + BPB_RESERVED_SECTORS);
    img->num_fats = data[BPB_NUM_FATS];
    img->root_entries = read_le16(data + BPB_ROOT_ENTRIES);
    img->media_descriptor = data[BPB_MEDIA_DESCRIPTOR];
    img->sectors_per_fat = read_le16(data + BPB_SECTORS_PER_FAT);
    img->sectors_per_track = read_le16(data + BPB_SECTORS_PER_TRACK);
    img->heads = read_le16(data + BPB_NUM_HEADS);
    
    /* Total sectors */
    uint16_t total16 = read_le16(data + BPB_TOTAL_SECTORS_16);
    if (total16 != 0) {
        img->total_sectors = total16;
    } else {
        img->total_sectors = read_le32(data + BPB_TOTAL_SECTORS_32);
    }
    
    /* Calculate tracks */
    if (img->sectors_per_track > 0 && img->heads > 0) {
        img->tracks = (uint16_t)(img->total_sectors / (img->sectors_per_track * img->heads));
    } else {
        /* Guess from size */
        switch (size) {
            case IMG_SIZE_360K:  img->tracks = 40; img->heads = 2; img->sectors_per_track = 9; break;
            case IMG_SIZE_720K:  img->tracks = 80; img->heads = 2; img->sectors_per_track = 9; break;
            case IMG_SIZE_1200K: img->tracks = 80; img->heads = 2; img->sectors_per_track = 15; break;
            case IMG_SIZE_1440K: img->tracks = 80; img->heads = 2; img->sectors_per_track = 18; break;
            case IMG_SIZE_2880K: img->tracks = 80; img->heads = 2; img->sectors_per_track = 36; break;
            default:
                img->tracks = 80;
                img->heads = 2;
                img->sectors_per_track = 18;
        }
    }
    
    /* OEM name */
    memcpy(img->oem_name, data + 3, 8);
    img->oem_name[8] = '\0';
    
    /* Volume label (at offset 0x2B for FAT12/16) */
    if (size >= 0x36) {
        memcpy(img->volume_label, data + 0x2B, 11);
        img->volume_label[11] = '\0';
    }
    
    ctx->format_data = img;
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Read Track Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static uft_error_t img_read_track(
    struct uft_xdf_context *ctx,
    uint16_t track,
    uint8_t side,
    uft_track_data_t *out
) {
    if (!ctx || !ctx->format_data || !out) {
        return UFT_E_INVALID_ARG;
    }
    
    img_context_t *img = (img_context_t *)ctx->format_data;
    
    if (track >= img->tracks || side >= img->heads) {
        return UFT_E_RANGE;
    }
    
    uft_track_data_init(out);
    
    out->track_num = track;
    out->side = side;
    out->encoding = 1;  /* MFM */
    
    /* Calculate track offset */
    uint32_t track_index = track * img->heads + side;
    size_t track_size = img->sectors_per_track * img->bytes_per_sector;
    size_t track_offset = track_index * track_size;
    
    if (track_offset + track_size > img->size) {
        return UFT_E_RANGE;
    }
    
    /* Copy raw track data */
    out->raw_size = track_size;
    out->raw_data = malloc(track_size);
    if (!out->raw_data) {
        return UFT_E_NOMEM;
    }
    memcpy(out->raw_data, img->data + track_offset, track_size);
    
    /* Allocate sectors */
    uft_error_t err = uft_track_alloc_sectors(out, img->sectors_per_track);
    if (err != UFT_SUCCESS) {
        free(out->raw_data);
        out->raw_data = NULL;
        return err;
    }
    
    /* Fill sector data (PC uses 1-based sector IDs) */
    for (uint8_t s = 0; s < img->sectors_per_track; s++) {
        uft_sector_data_t *sector = &out->sectors[s];
        
        sector->logical_track = track;
        sector->head = side;
        sector->sector_id = s + 1;  /* 1-based */
        sector->size_code = 2;  /* 512 bytes */
        
        size_t sector_offset = s * img->bytes_per_sector;
        sector->data_size = img->bytes_per_sector;
        sector->data = malloc(img->bytes_per_sector);
        if (sector->data) {
            memcpy(sector->data, out->raw_data + sector_offset, img->bytes_per_sector);
        }
        
        sector->confidence = 10000;
        sector->crc_ok = true;
        sector->deleted = false;
    }
    
    out->confidence = 10000;
    snprintf(out->diag_message, sizeof(out->diag_message),
             "Track %u.%u: %u sectors, %zu bytes",
             track, side, img->sectors_per_track, track_size);
    
    return UFT_SUCCESS;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Get Geometry Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void img_get_geometry(
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
    
    img_context_t *img = (img_context_t *)ctx->format_data;
    
    if (tracks) *tracks = img->tracks;
    if (sides) *sides = img->heads;
    if (sectors) *sectors = img->sectors_per_track;
    if (sector_size) *sector_size = img->bytes_per_sector;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Close Function
 * ═══════════════════════════════════════════════════════════════════════════════ */

static void img_close(struct uft_xdf_context *ctx) {
    if (!ctx || !ctx->format_data) return;
    
    img_context_t *img = (img_context_t *)ctx->format_data;
    free(img);
    ctx->format_data = NULL;
}

/* ═══════════════════════════════════════════════════════════════════════════════
 * Adapter Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

const uft_format_adapter_t uft_img_adapter = {
    .name = "IMG",
    .description = "PC Floppy Disk Image (FAT12)",
    .extensions = "img,ima,dsk,bin",
    .format_id = UFT_FORMAT_ID_IMG,
    
    .can_read = true,
    .can_write = false,
    .can_create = false,
    .supports_errors = false,
    .supports_timing = false,
    
    .probe = img_probe,
    .open = img_open,
    .read_track = img_read_track,
    .get_geometry = img_get_geometry,
    .write_track = NULL,
    .export_native = NULL,
    .close = img_close,
    
    .private_data = NULL
};

void uft_img_adapter_init(void) {
    uft_adapter_register(&uft_img_adapter);
}
