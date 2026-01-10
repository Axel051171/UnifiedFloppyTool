/**
 * @file uft_apple_core.c
 * @brief Apple DOS 3.3 / ProDOS Core Implementation
 * @version 3.7.0
 * 
 * Lifecycle, detection, sector/block access, interleave handling.
 */

#include "uft/fs/uft_apple_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Sector Interleave Tables
 *===========================================================================*/

/** DOS 3.3 logical to physical sector mapping */
static const uint8_t DOS33_INTERLEAVE[16] = {
    0x00, 0x0D, 0x0B, 0x09, 0x07, 0x05, 0x03, 0x01,
    0x0E, 0x0C, 0x0A, 0x08, 0x06, 0x04, 0x02, 0x0F
};

/** ProDOS logical to physical sector mapping */
static const uint8_t PRODOS_INTERLEAVE[16] = {
    0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E,
    0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F
};

/** Physical sector order (no interleave) */
static const uint8_t PHYSICAL_ORDER[16] = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};

/** DOS 3.2 (13-sector) interleave */
static const uint8_t DOS32_INTERLEAVE[13] = {
    0x00, 0x0A, 0x07, 0x04, 0x01, 0x0B, 0x08,
    0x05, 0x02, 0x0C, 0x09, 0x06, 0x03
};

/*===========================================================================
 * Error Messages
 *===========================================================================*/

static const char *ERROR_MESSAGES[] = {
    "Success",
    "Invalid parameter",
    "Out of memory",
    "I/O error",
    "File not found",
    "File already exists",
    "Disk full",
    "Write protected",
    "Bad chain",
    "Invalid file type"
};

const char *uft_apple_strerror(int error) {
    if (error >= 0) return "Success";
    int idx = -error;
    if (idx < (int)(sizeof(ERROR_MESSAGES) / sizeof(ERROR_MESSAGES[0]))) {
        return ERROR_MESSAGES[idx];
    }
    return "Unknown error";
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_apple_ctx_t *uft_apple_create(void) {
    uft_apple_ctx_t *ctx = calloc(1, sizeof(uft_apple_ctx_t));
    if (!ctx) return NULL;
    
    ctx->fs_type = UFT_APPLE_FS_UNKNOWN;
    ctx->order = UFT_APPLE_ORDER_DOS;
    ctx->sector_map = DOS33_INTERLEAVE;
    
    return ctx;
}

void uft_apple_destroy(uft_apple_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    free(ctx);
}

void uft_apple_close(uft_apple_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    ctx->data = NULL;
    ctx->size = 0;
    ctx->owns_data = false;
    ctx->fs_type = UFT_APPLE_FS_UNKNOWN;
}

/*===========================================================================
 * Detection Helpers
 *===========================================================================*/

/**
 * @brief Check if data looks like DOS 3.3 VTOC
 */
static bool is_valid_vtoc(const uint8_t *vtoc) {
    /* Track 17 Sector 0 should be VTOC */
    const uft_dos33_vtoc_t *v = (const uft_dos33_vtoc_t *)vtoc;
    
    /* Check catalog pointer is valid */
    if (v->catalog_track == 0 || v->catalog_track > 40) return false;
    if (v->catalog_sector > 15) return false;
    
    /* Check DOS version (usually 3) */
    if (v->dos_version > 5) return false;
    
    /* Check sectors per track (13 or 16) */
    if (v->sectors_per_track != 13 && v->sectors_per_track != 16) return false;
    
    /* Check tracks (35 or 40) */
    if (v->tracks_per_disk != 35 && v->tracks_per_disk != 40) return false;
    
    /* Check bytes per sector */
    uint16_t bps = v->bytes_per_sector;
    if (bps != 256 && bps != 0) return false;
    
    return true;
}

/**
 * @brief Check if data looks like ProDOS volume directory
 */
static bool is_valid_prodos_volume(const uint8_t *block) {
    /* Block 2 should be volume directory key block */
    if (block[0] != 0 || block[1] != 0) return false;  /* prev pointer = 0 */
    
    /* Storage type should be 0xF (volume directory key) */
    uint8_t storage_type = (block[4] >> 4) & 0x0F;
    if (storage_type != 0x0F) return false;
    
    /* Name length should be 1-15 */
    uint8_t name_len = block[4] & 0x0F;
    if (name_len == 0 || name_len > 15) return false;
    
    /* Check name characters are valid */
    for (int i = 0; i < name_len; i++) {
        uint8_t c = block[5 + i];
        if (!isalnum(c) && c != '.') return false;
    }
    
    /* Entry length should be 0x27 (39) */
    if (block[0x23] != 0x27) return false;
    
    /* Entries per block should be 0x0D (13) */
    if (block[0x24] != 0x0D) return false;
    
    return true;
}

/**
 * @brief Detect sector order by analyzing data
 */
static uft_apple_order_t detect_sector_order(const uint8_t *data, size_t size) {
    if (size < 143360) return UFT_APPLE_ORDER_DOS;
    
    /* Try DOS order first - check for valid VTOC at T17S0 */
    size_t dos_vtoc_offset = (17 * 16 + 0) * 256;  /* Logical T17S0 */
    if (is_valid_vtoc(data + dos_vtoc_offset)) {
        return UFT_APPLE_ORDER_DOS;
    }
    
    /* Try ProDOS order - check for valid volume at block 2 */
    size_t prodos_block2_offset = 2 * 512;
    if (prodos_block2_offset + 512 <= size) {
        if (is_valid_prodos_volume(data + prodos_block2_offset)) {
            return UFT_APPLE_ORDER_PRODOS;
        }
    }
    
    /* Default to DOS order */
    return UFT_APPLE_ORDER_DOS;
}

/*===========================================================================
 * Detection
 *===========================================================================*/

int uft_apple_detect(const uint8_t *data, size_t size, uft_apple_detect_t *result) {
    if (!data || !result) return UFT_APPLE_ERR_INVALID;
    
    memset(result, 0, sizeof(*result));
    result->fs_type = UFT_APPLE_FS_UNKNOWN;
    result->confidence = 0;
    
    /* Check size - standard Apple II disks */
    if (size == 143360) {
        /* 35 tracks × 16 sectors × 256 bytes = 140KB (standard) */
        result->tracks = 35;
        result->sectors_per_track = 16;
    } else if (size == 116480) {
        /* 35 tracks × 13 sectors × 256 bytes = 113.75KB (DOS 3.2) */
        result->tracks = 35;
        result->sectors_per_track = 13;
    } else if (size == 163840) {
        /* 40 tracks × 16 sectors × 256 bytes = 160KB */
        result->tracks = 40;
        result->sectors_per_track = 16;
    } else if (size == 819200) {
        /* 3.5" 800KB (ProDOS) */
        result->tracks = 80;
        result->sectors_per_track = 32;  /* Actually block-addressed */
    } else {
        /* Unknown size - try to detect anyway */
        result->tracks = 35;
        result->sectors_per_track = 16;
        result->confidence = 20;
    }
    
    /* Detect sector order */
    result->order = detect_sector_order(data, size);
    
    /* Try DOS 3.3 detection */
    size_t vtoc_offset;
    if (result->order == UFT_APPLE_ORDER_DOS) {
        vtoc_offset = (17 * 16 + 0) * 256;
    } else {
        /* ProDOS order: T17S0 maps to different physical location */
        vtoc_offset = (17 * 16) * 256;
    }
    
    if (vtoc_offset + 256 <= size) {
        const uft_dos33_vtoc_t *vtoc = (const uft_dos33_vtoc_t *)(data + vtoc_offset);
        
        if (is_valid_vtoc(data + vtoc_offset)) {
            if (vtoc->sectors_per_track == 13) {
                result->fs_type = UFT_APPLE_FS_DOS32;
                result->sectors_per_track = 13;
            } else {
                result->fs_type = UFT_APPLE_FS_DOS33;
            }
            result->volume_number = vtoc->volume_number;
            result->confidence = 90;
            return 0;
        }
    }
    
    /* Try ProDOS detection */
    if (size >= 143360) {
        size_t block2_offset = 2 * 512;
        if (is_valid_prodos_volume(data + block2_offset)) {
            result->fs_type = UFT_APPLE_FS_PRODOS;
            result->order = UFT_APPLE_ORDER_PRODOS;
            result->confidence = 95;
            
            /* Extract volume name */
            uint8_t name_len = data[block2_offset + 4] & 0x0F;
            if (name_len <= 15) {
                memcpy(result->volume_name, data + block2_offset + 5, name_len);
                result->volume_name[name_len] = '\0';
            }
            return 0;
        }
    }
    
    /* Could not detect filesystem type */
    result->fs_type = UFT_APPLE_FS_UNKNOWN;
    result->confidence = 10;
    return UFT_APPLE_ERR_INVALID;
}

/*===========================================================================
 * Open
 *===========================================================================*/

int uft_apple_open(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size, bool copy) {
    if (!ctx || !data || size == 0) return UFT_APPLE_ERR_INVALID;
    
    /* Close any existing image */
    uft_apple_close(ctx);
    
    /* Detect filesystem type */
    uft_apple_detect_t detect;
    int ret = uft_apple_detect(data, size, &detect);
    if (ret < 0 && detect.confidence < 20) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Copy or reference data */
    if (copy) {
        ctx->data = malloc(size);
        if (!ctx->data) return UFT_APPLE_ERR_NOMEM;
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = (uint8_t *)data;
        ctx->owns_data = false;
    }
    ctx->size = size;
    
    /* Set filesystem parameters */
    ctx->fs_type = detect.fs_type;
    ctx->order = detect.order;
    ctx->tracks = detect.tracks;
    ctx->sectors_per_track = detect.sectors_per_track;
    ctx->modified = false;
    
    /* Set sector interleave map */
    switch (ctx->order) {
        case UFT_APPLE_ORDER_PRODOS:
            ctx->sector_map = PRODOS_INTERLEAVE;
            break;
        case UFT_APPLE_ORDER_PHYSICAL:
            ctx->sector_map = PHYSICAL_ORDER;
            break;
        case UFT_APPLE_ORDER_DOS:
        default:
            if (ctx->sectors_per_track == 13) {
                ctx->sector_map = DOS32_INTERLEAVE;
            } else {
                ctx->sector_map = DOS33_INTERLEAVE;
            }
            break;
    }
    
    /* Parse filesystem-specific structures */
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* Read VTOC */
        uint8_t vtoc_buf[256];
        if (uft_apple_read_sector(ctx, UFT_DOS33_VTOC_TRACK, UFT_DOS33_VTOC_SECTOR, 
                                   vtoc_buf) == 0) {
            const uft_dos33_vtoc_t *vtoc = (const uft_dos33_vtoc_t *)vtoc_buf;
            ctx->volume_number = vtoc->volume_number;
            ctx->catalog_track = vtoc->catalog_track;
            ctx->catalog_sector = vtoc->catalog_sector;
        }
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* Read volume directory header */
        uint8_t block[512];
        if (uft_apple_read_block(ctx, UFT_PRODOS_KEY_BLOCK, block) == 0) {
            uint8_t name_len = block[4] & 0x0F;
            if (name_len <= 15) {
                memcpy(ctx->volume_name, block + 5, name_len);
                ctx->volume_name[name_len] = '\0';
            }
            ctx->total_blocks = block[0x29] | (block[0x2A] << 8);
            ctx->bitmap_block = block[0x27] | (block[0x28] << 8);
        }
    }
    
    return 0;
}

int uft_apple_open_file(uft_apple_ctx_t *ctx, const char *filename) {
    if (!ctx || !filename) return UFT_APPLE_ERR_INVALID;
    
    FILE *fp = fopen(filename, "rb");
    if (!fp) return UFT_APPLE_ERR_IO;
    
    /* Get file size */
    if (fseek(fp, 0, SEEK_END) != 0) { /* seek error */ }
    long size = ftell(fp);
    if (fseek(fp, 0, SEEK_SET) != 0) { /* seek error */ }
    if (size <= 0) {
        fclose(fp);
        return UFT_APPLE_ERR_IO;
    }
    
    /* Allocate and read data */
    uint8_t *data = malloc(size);
    if (!data) {
        fclose(fp);
        return UFT_APPLE_ERR_NOMEM;
    }
    
    if (fread(data, 1, size, fp) != (size_t)size) {
        free(data);
        fclose(fp);
        return UFT_APPLE_ERR_IO;
    }
    fclose(fp);
    
    /* Open with copied data */
    int ret = uft_apple_open(ctx, data, size, false);
    if (ret < 0) {
        free(data);
        return ret;
    }
    
    /* Mark that we own the data */
    ctx->owns_data = true;
    
    return 0;
}

int uft_apple_save(uft_apple_ctx_t *ctx, const char *filename) {
    if (!ctx || !ctx->data || !filename) return UFT_APPLE_ERR_INVALID;
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return UFT_APPLE_ERR_IO;
    
    if (fwrite(ctx->data, 1, ctx->size, fp) != ctx->size) {
        fclose(fp);
        return UFT_APPLE_ERR_IO;
    }
    
    fclose(fp);
    ctx->modified = false;
    
    return 0;
}

/*===========================================================================
 * Sector/Block Access
 *===========================================================================*/

/**
 * @brief Calculate physical offset for track/sector (DOS order)
 */
static size_t calc_sector_offset_dos(uint8_t track, uint8_t sector,
                                     uint8_t sectors_per_track,
                                     const uint8_t *sector_map) {
    /* Apply interleave mapping */
    uint8_t phys_sector = sector;
    if (sector_map && sector < 16) {
        phys_sector = sector_map[sector];
    }
    
    return (size_t)track * sectors_per_track * 256 + (size_t)phys_sector * 256;
}

/**
 * @brief Calculate physical offset for ProDOS block
 * 
 * ProDOS blocks are 512 bytes (2 sectors).
 * Block N = sectors at track (N/8), sectors (N%8)*2 and (N%8)*2+1
 */
static size_t calc_block_offset(uint16_t block) {
    return (size_t)block * 512;
}

int uft_apple_read_sector(const uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector,
                          uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    if (track >= ctx->tracks || sector >= ctx->sectors_per_track) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    size_t offset = calc_sector_offset_dos(track, sector, ctx->sectors_per_track,
                                           ctx->sector_map);
    
    if (offset + 256 > ctx->size) {
        return UFT_APPLE_ERR_IO;
    }
    
    memcpy(buffer, ctx->data + offset, 256);
    return 0;
}

int uft_apple_write_sector(uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector,
                           const uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    if (track >= ctx->tracks || sector >= ctx->sectors_per_track) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    size_t offset = calc_sector_offset_dos(track, sector, ctx->sectors_per_track,
                                           ctx->sector_map);
    
    if (offset + 256 > ctx->size) {
        return UFT_APPLE_ERR_IO;
    }
    
    memcpy(ctx->data + offset, buffer, 256);
    ctx->modified = true;
    
    return 0;
}

int uft_apple_read_block(const uft_apple_ctx_t *ctx, uint16_t block, uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    size_t offset = calc_block_offset(block);
    
    if (offset + 512 > ctx->size) {
        return UFT_APPLE_ERR_IO;
    }
    
    memcpy(buffer, ctx->data + offset, 512);
    return 0;
}

int uft_apple_write_block(uft_apple_ctx_t *ctx, uint16_t block, const uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    size_t offset = calc_block_offset(block);
    
    if (offset + 512 > ctx->size) {
        return UFT_APPLE_ERR_IO;
    }
    
    memcpy(ctx->data + offset, buffer, 512);
    ctx->modified = true;
    
    return 0;
}

/*===========================================================================
 * Volume Name
 *===========================================================================*/

int uft_apple_get_volume_name(const uft_apple_ctx_t *ctx, char *name, size_t size) {
    if (!ctx || !name || size == 0) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* DOS 3.3 uses volume number */
        snprintf(name, size, "DISK VOLUME %03u", ctx->volume_number);
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* ProDOS has volume name */
        if (ctx->volume_name[0]) {
            snprintf(name, size, "/%s", ctx->volume_name);
        } else {
            snprintf(name, size, "/UNTITLED");
        }
    } else {
        snprintf(name, size, "UNKNOWN");
    }
    
    return 0;
}

/*===========================================================================
 * Type Conversion Utilities
 *===========================================================================*/

char uft_dos33_type_char(uint8_t type) {
    /* Strip lock flag (bit 7) */
    type &= 0x7F;
    
    switch (type) {
        case UFT_DOS33_TYPE_TEXT:      return 'T';
        case UFT_DOS33_TYPE_INTEGER:   return 'I';
        case UFT_DOS33_TYPE_APPLESOFT: return 'A';
        case UFT_DOS33_TYPE_BINARY:    return 'B';
        case UFT_DOS33_TYPE_S:         return 'S';
        case UFT_DOS33_TYPE_REL:       return 'R';
        case UFT_DOS33_TYPE_AA:        return 'a';
        case UFT_DOS33_TYPE_BB:        return 'b';
        default:                       return '?';
    }
}

static const struct {
    uint8_t type;
    const char *name;
} PRODOS_TYPES[] = {
    { 0x00, "UNK" },
    { 0x01, "BAD" },
    { 0x04, "TXT" },
    { 0x06, "BIN" },
    { 0x0F, "DIR" },
    { 0x19, "ADB" },
    { 0x1A, "AWP" },
    { 0x1B, "ASP" },
    { 0xB3, "S16" },
    { 0xEF, "PAS" },
    { 0xF0, "CMD" },
    { 0xFC, "BAS" },
    { 0xFD, "VAR" },
    { 0xFE, "REL" },
    { 0xFF, "SYS" },
    { 0, NULL }
};

const char *uft_prodos_type_string(uint8_t type) {
    for (int i = 0; PRODOS_TYPES[i].name; i++) {
        if (PRODOS_TYPES[i].type == type) {
            return PRODOS_TYPES[i].name;
        }
    }
    return "???";
}

/*===========================================================================
 * Time Conversion
 *===========================================================================*/

time_t uft_prodos_to_unix_time(uft_prodos_datetime_t dt) {
    struct tm tm = {0};
    
    /* Date: YYYYYYYMMMMDDDDD */
    /* Year is 7 bits, 0-99 maps to 1900-1999 or 2000-2099 */
    int year = (dt.date >> 9) & 0x7F;
    int month = (dt.date >> 5) & 0x0F;
    int day = dt.date & 0x1F;
    
    /* Time: 000HHHHH00MMMMMM */
    int hour = (dt.time >> 8) & 0x1F;
    int minute = dt.time & 0x3F;
    
    /* Adjust year */
    if (year < 40) {
        tm.tm_year = 100 + year;  /* 2000-2039 */
    } else {
        tm.tm_year = year;        /* 1940-1999 */
    }
    
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    
    return mktime(&tm);
}

uft_prodos_datetime_t uft_prodos_from_unix_time(time_t t) {
    uft_prodos_datetime_t dt = {0, 0};
    struct tm *tm = localtime(&t);
    if (!tm) return dt;
    
    int year = tm->tm_year;
    if (year >= 100) year -= 100;  /* Map 2000+ to 0-39 */
    
    dt.date = ((year & 0x7F) << 9) |
              (((tm->tm_mon + 1) & 0x0F) << 5) |
              (tm->tm_mday & 0x1F);
    
    dt.time = ((tm->tm_hour & 0x1F) << 8) |
              (tm->tm_min & 0x3F);
    
    return dt;
}

/*===========================================================================
 * Free Space
 *===========================================================================*/

int uft_apple_get_free(const uft_apple_ctx_t *ctx, uint16_t *free_count) {
    if (!ctx || !ctx->data || !free_count) return UFT_APPLE_ERR_INVALID;
    
    *free_count = 0;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* Read VTOC and count free sectors in bitmap */
        uint8_t vtoc[256];
        if (uft_apple_read_sector(ctx, UFT_DOS33_VTOC_TRACK, 
                                   UFT_DOS33_VTOC_SECTOR, vtoc) < 0) {
            return UFT_APPLE_ERR_IO;
        }
        
        /* Bitmap starts at offset 0x38, 4 bytes per track */
        uint16_t count = 0;
        for (int track = 0; track < ctx->tracks; track++) {
            uint32_t track_bits = 0;
            for (int i = 0; i < 4; i++) {
                track_bits |= (uint32_t)vtoc[0x38 + track * 4 + i] << (i * 8);
            }
            /* Count set bits (1 = free) */
            for (int s = 0; s < ctx->sectors_per_track; s++) {
                if (track_bits & (1u << s)) count++;
            }
        }
        *free_count = count;
        
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* Read volume bitmap */
        uint8_t block[512];
        uint16_t bitmap_block = ctx->bitmap_block;
        uint16_t total = ctx->total_blocks;
        uint16_t count = 0;
        uint16_t blocks_checked = 0;
        
        while (bitmap_block && blocks_checked < total) {
            if (uft_apple_read_block(ctx, bitmap_block, block) < 0) {
                return UFT_APPLE_ERR_IO;
            }
            
            for (int i = 0; i < 512 && blocks_checked < total; i++) {
                uint8_t byte = block[i];
                for (int b = 7; b >= 0 && blocks_checked < total; b--) {
                    if (byte & (1 << b)) count++;
                    blocks_checked++;
                }
            }
            
            /* Next bitmap block */
            bitmap_block++;
            if (bitmap_block * 512 > ctx->size) break;
        }
        *free_count = count;
    }
    
    return 0;
}

/*===========================================================================
 * Bitmap Operations
 *===========================================================================*/

int uft_apple_alloc_sector(uft_apple_ctx_t *ctx, uint8_t *track_out, uint8_t *sector_out) {
    if (!ctx || !ctx->data || !track_out || !sector_out) return UFT_APPLE_ERR_INVALID;
    if (ctx->fs_type != UFT_APPLE_FS_DOS33 && ctx->fs_type != UFT_APPLE_FS_DOS32) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    /* Read VTOC */
    uint8_t vtoc[256];
    if (uft_apple_read_sector(ctx, UFT_DOS33_VTOC_TRACK, 
                               UFT_DOS33_VTOC_SECTOR, vtoc) < 0) {
        return UFT_APPLE_ERR_IO;
    }
    
    const uft_dos33_vtoc_t *v = (const uft_dos33_vtoc_t *)vtoc;
    int8_t direction = v->alloc_direction;
    uint8_t start_track = v->last_track_alloc;
    
    if (direction == 0) direction = 1;
    
    /* Search for free sector */
    for (int pass = 0; pass < ctx->tracks; pass++) {
        int track = (int)start_track + (direction * pass);
        
        /* Wrap around */
        if (track >= ctx->tracks) track = 0;
        if (track < 0) track = ctx->tracks - 1;
        
        /* Skip VTOC track */
        if (track == UFT_DOS33_VTOC_TRACK) continue;
        
        /* Get track bitmap (4 bytes) */
        uint32_t track_bits = 0;
        for (int i = 0; i < 4; i++) {
            track_bits |= (uint32_t)vtoc[0x38 + track * 4 + i] << (i * 8);
        }
        
        /* Find free sector */
        for (int sector = 0; sector < ctx->sectors_per_track; sector++) {
            if (track_bits & (1u << sector)) {
                /* Mark as allocated */
                track_bits &= ~(1u << sector);
                for (int i = 0; i < 4; i++) {
                    vtoc[0x38 + track * 4 + i] = (track_bits >> (i * 8)) & 0xFF;
                }
                
                /* Update last allocated track */
                vtoc[0x30] = (uint8_t)track;
                
                /* Write VTOC */
                if (uft_apple_write_sector(ctx, UFT_DOS33_VTOC_TRACK,
                                           UFT_DOS33_VTOC_SECTOR, vtoc) < 0) {
                    return UFT_APPLE_ERR_IO;
                }
                
                *track_out = (uint8_t)track;
                *sector_out = (uint8_t)sector;
                return 0;
            }
        }
    }
    
    return UFT_APPLE_ERR_DISKFULL;
}

int uft_apple_free_sector(uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector) {
    if (!ctx || !ctx->data) return UFT_APPLE_ERR_INVALID;
    if (ctx->fs_type != UFT_APPLE_FS_DOS33 && ctx->fs_type != UFT_APPLE_FS_DOS32) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    if (track >= ctx->tracks || sector >= ctx->sectors_per_track) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Read VTOC */
    uint8_t vtoc[256];
    if (uft_apple_read_sector(ctx, UFT_DOS33_VTOC_TRACK,
                               UFT_DOS33_VTOC_SECTOR, vtoc) < 0) {
        return UFT_APPLE_ERR_IO;
    }
    
    /* Mark sector as free */
    uint32_t track_bits = 0;
    for (int i = 0; i < 4; i++) {
        track_bits |= (uint32_t)vtoc[0x38 + track * 4 + i] << (i * 8);
    }
    track_bits |= (1u << sector);
    for (int i = 0; i < 4; i++) {
        vtoc[0x38 + track * 4 + i] = (track_bits >> (i * 8)) & 0xFF;
    }
    
    /* Write VTOC */
    return uft_apple_write_sector(ctx, UFT_DOS33_VTOC_TRACK,
                                   UFT_DOS33_VTOC_SECTOR, vtoc);
}

int uft_apple_alloc_block(uft_apple_ctx_t *ctx, uint16_t *block_out) {
    if (!ctx || !ctx->data || !block_out) return UFT_APPLE_ERR_INVALID;
    if (ctx->fs_type != UFT_APPLE_FS_PRODOS) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    
    /* Read volume bitmap */
    uint8_t block[512];
    uint16_t bitmap_block = ctx->bitmap_block;
    uint16_t total = ctx->total_blocks;
    uint16_t blocks_checked = 0;
    
    while (bitmap_block && blocks_checked < total) {
        if (uft_apple_read_block(ctx, bitmap_block, block) < 0) {
            return UFT_APPLE_ERR_IO;
        }
        
        for (int i = 0; i < 512 && blocks_checked < total; i++) {
            uint8_t byte = block[i];
            for (int b = 7; b >= 0 && blocks_checked < total; b--) {
                if (byte & (1 << b)) {
                    /* Found free block - mark as allocated */
                    block[i] &= ~(1 << b);
                    if (uft_apple_write_block(ctx, bitmap_block, block) < 0) {
                        return UFT_APPLE_ERR_IO;
                    }
                    *block_out = blocks_checked;
                    return 0;
                }
                blocks_checked++;
            }
        }
        
        bitmap_block++;
        if (bitmap_block * 512 > ctx->size) break;
    }
    
    return UFT_APPLE_ERR_DISKFULL;
}

int uft_apple_free_block(uft_apple_ctx_t *ctx, uint16_t block_num) {
    if (!ctx || !ctx->data) return UFT_APPLE_ERR_INVALID;
    if (ctx->fs_type != UFT_APPLE_FS_PRODOS) {
        return UFT_APPLE_ERR_BADTYPE;
    }
    if (block_num >= ctx->total_blocks) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Calculate which bitmap block and bit */
    uint16_t bitmap_block = ctx->bitmap_block + (block_num / 4096);
    int byte_offset = (block_num % 4096) / 8;
    int bit_offset = 7 - (block_num % 8);
    
    /* Read, modify, write */
    uint8_t block[512];
    if (uft_apple_read_block(ctx, bitmap_block, block) < 0) {
        return UFT_APPLE_ERR_IO;
    }
    
    block[byte_offset] |= (1 << bit_offset);
    
    return uft_apple_write_block(ctx, bitmap_block, block);
}
