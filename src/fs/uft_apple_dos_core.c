/**
 * @file uft_apple_dos_core.c
 * @brief Apple II DOS/ProDOS Core Implementation
 * @version 3.7.0
 * 
 * Lifecycle, detection, sector/block access for Apple II filesystems.
 */

#include "uft/fs/uft_apple_dos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Sector Interleave Tables
 *===========================================================================*/

/** DOS 3.3 sector interleave (physical -> logical) */
static const uint8_t DOS_INTERLEAVE[16] = {
    0x0, 0x7, 0xE, 0x6, 0xD, 0x5, 0xC, 0x4,
    0xB, 0x3, 0xA, 0x2, 0x9, 0x1, 0x8, 0xF
};

/** ProDOS sector interleave (physical -> logical) */
static const uint8_t PRODOS_INTERLEAVE[16] = {
    0x0, 0x8, 0x1, 0x9, 0x2, 0xA, 0x3, 0xB,
    0x4, 0xC, 0x5, 0xD, 0x6, 0xE, 0x7, 0xF
};

/** Identity mapping (no interleave) */
static const uint8_t NO_INTERLEAVE[16] = {
    0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
    0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
};

/*===========================================================================
 * Error Messages
 *===========================================================================*/

static const char *error_messages[] = {
    "Success",
    "Invalid argument",
    "Out of memory",
    "I/O error",
    "File not found",
    "File exists",
    "Disk full",
    "Read-only",
    "Bad chain",
    "Bad file type"
};

const char *uft_apple_strerror(int error) {
    if (error >= 0) return error_messages[0];
    int idx = -error;
    if (idx < (int)(sizeof(error_messages) / sizeof(error_messages[0]))) {
        return error_messages[idx];
    }
    return "Unknown error";
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_apple_ctx_t *uft_apple_create(void) {
    uft_apple_ctx_t *ctx = calloc(1, sizeof(uft_apple_ctx_t));
    if (!ctx) return NULL;
    
    ctx->sector_map = NO_INTERLEAVE;
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
    ctx->is_modified = false;
}

/*===========================================================================
 * Detection
 *===========================================================================*/

/**
 * @brief Check if VTOC is valid for DOS 3.3
 */
static bool is_valid_vtoc(const uft_dos33_vtoc_t *vtoc) {
    /* Catalog track should be 17 */
    if (vtoc->catalog_track != UFT_DOS33_CATALOG_TRACK) return false;
    
    /* Catalog sector should be 1-15 */
    if (vtoc->catalog_sector == 0 || vtoc->catalog_sector > 15) return false;
    
    /* DOS version should be 1-3 */
    if (vtoc->dos_version == 0 || vtoc->dos_version > 4) return false;
    
    /* Volume number should be 1-254 */
    if (vtoc->volume_number == 0 || vtoc->volume_number == 255) return false;
    
    /* Tracks per disk should be reasonable */
    if (vtoc->tracks_per_disk < 17 || vtoc->tracks_per_disk > 50) return false;
    
    /* Sectors per track */
    if (vtoc->sectors_per_track != 13 && vtoc->sectors_per_track != 16) return false;
    
    /* Bytes per sector */
    if (vtoc->bytes_per_sector != 256) return false;
    
    return true;
}

/**
 * @brief Check for ProDOS volume directory header
 */
static bool is_valid_prodos(const uint8_t *data, size_t size) {
    if (size < 1024) return false;  /* Need at least 2 blocks */
    
    /* Key block at block 2 (offset 0x400) */
    const uint8_t *key = data + 0x400;
    
    /* Previous block pointer should be 0 */
    if (key[0] != 0 || key[1] != 0) return false;
    
    /* Next block pointer should be non-zero for root */
    uint16_t next = key[2] | (key[3] << 8);
    if (next == 0 || next > 0x600) return false;  /* Reasonable limit */
    
    /* Storage type should be 0xE or 0xF for volume header */
    uint8_t storage = (key[4] >> 4) & 0x0F;
    if (storage != 0x0E && storage != 0x0F) return false;
    
    /* Name length should be 1-15 */
    uint8_t name_len = key[4] & 0x0F;
    if (name_len == 0 || name_len > 15) return false;
    
    /* Check volume name contains valid characters */
    for (int i = 0; i < name_len; i++) {
        char c = key[5 + i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.')) {
            return false;
        }
    }
    
    /* Entry length should be 0x27 (39) */
    if (key[0x23] != 0x27) return false;
    
    /* Entries per block should be 0x0D (13) */
    if (key[0x24] != 0x0D) return false;
    
    return true;
}

/**
 * @brief Try to detect Apple Pascal filesystem
 */
static bool is_valid_pascal(const uint8_t *data, size_t size) {
    if (size < 1024) return false;
    
    /* Block 2 contains directory */
    const uint8_t *dir = data + 0x400;
    
    /* First entry has start block 0, length = total blocks */
    uint16_t first_block = dir[0] | (dir[1] << 8);
    if (first_block != 0) return false;
    
    /* Check for valid volume name length (1-7) */
    uint8_t name_len = dir[6];
    if (name_len == 0 || name_len > 7) return false;
    
    return true;
}

int uft_apple_detect(const uint8_t *data, size_t size, uft_apple_detect_t *result) {
    if (!data || !result) return UFT_APPLE_ERR_INVALID;
    
    memset(result, 0, sizeof(uft_apple_detect_t));
    
    /* Check valid image sizes */
    if (size == 116480) {
        /* 13-sector disk (DOS 3.2) */
        result->tracks = 35;
        result->sectors = 13;
    } else if (size == 143360) {
        /* 16-sector disk */
        result->tracks = 35;
        result->sectors = 16;
    } else if (size == 163840) {
        /* 40-track disk */
        result->tracks = 40;
        result->sectors = 16;
    } else if (size == 204800) {
        /* 50-track disk (some clones) */
        result->tracks = 50;
        result->sectors = 16;
    } else {
        result->fs_type = UFT_APPLE_FS_UNKNOWN;
        result->confidence = 0;
        return 0;
    }
    
    /* Try ProDOS detection first (more specific signature) */
    if (result->sectors == 16 && is_valid_prodos(data, size)) {
        result->fs_type = UFT_APPLE_FS_PRODOS;
        result->order = UFT_APPLE_ORDER_PRODOS;
        result->confidence = 95;
        
        /* Extract volume name */
        const uint8_t *key = data + 0x400;
        uint8_t name_len = key[4] & 0x0F;
        memcpy(result->volume_name, &key[5], name_len);
        result->volume_name[name_len] = '\0';
        
        return 0;
    }
    
    /* Try DOS 3.3 VTOC */
    size_t vtoc_offset = (UFT_DOS33_VTOC_TRACK * result->sectors + UFT_DOS33_VTOC_SECTOR) 
                         * UFT_APPLE_SECTOR_SIZE;
    
    if (vtoc_offset + sizeof(uft_dos33_vtoc_t) <= size) {
        const uft_dos33_vtoc_t *vtoc = (const uft_dos33_vtoc_t *)(data + vtoc_offset);
        
        if (is_valid_vtoc(vtoc)) {
            if (result->sectors == 13) {
                result->fs_type = UFT_APPLE_FS_DOS32;
            } else {
                result->fs_type = UFT_APPLE_FS_DOS33;
            }
            result->order = UFT_APPLE_ORDER_DOS;
            result->confidence = 90;
            result->volume_number = vtoc->volume_number;
            snprintf(result->volume_name, sizeof(result->volume_name), 
                     "DISK VOLUME %03d", vtoc->volume_number);
            return 0;
        }
    }
    
    /* Try ProDOS order for DOS 3.3 */
    /* Swap sector interleave and try again */
    /* This is more complex - would need to reshuffle sectors */
    
    /* Try Apple Pascal */
    if (is_valid_pascal(data, size)) {
        result->fs_type = UFT_APPLE_FS_PASCAL;
        result->order = UFT_APPLE_ORDER_PRODOS;
        result->confidence = 75;
        
        /* Extract volume name */
        const uint8_t *dir = data + 0x400;
        uint8_t name_len = dir[6];
        memcpy(result->volume_name, &dir[7], name_len);
        result->volume_name[name_len] = '\0';
        
        return 0;
    }
    
    /* Unknown filesystem */
    result->fs_type = UFT_APPLE_FS_UNKNOWN;
    result->confidence = 0;
    
    return 0;
}

/*===========================================================================
 * Open/Save
 *===========================================================================*/

int uft_apple_open(uft_apple_ctx_t *ctx, const uint8_t *data, size_t size, bool copy) {
    if (!ctx || !data || size == 0) return UFT_APPLE_ERR_INVALID;
    
    /* Detect filesystem */
    uft_apple_detect_t detect;
    int ret = uft_apple_detect(data, size, &detect);
    if (ret < 0) return ret;
    
    if (detect.fs_type == UFT_APPLE_FS_UNKNOWN) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    /* Close any existing image */
    uft_apple_close(ctx);
    
    /* Store data */
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
    ctx->sectors_per_track = detect.sectors;
    
    /* Select interleave table */
    switch (detect.order) {
    case UFT_APPLE_ORDER_DOS:
        ctx->sector_map = DOS_INTERLEAVE;
        break;
    case UFT_APPLE_ORDER_PRODOS:
        ctx->sector_map = PRODOS_INTERLEAVE;
        break;
    default:
        ctx->sector_map = NO_INTERLEAVE;
        break;
    }
    
    /* Load filesystem-specific structures */
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        /* Load VTOC */
        size_t vtoc_offset = (UFT_DOS33_VTOC_TRACK * ctx->sectors_per_track + 
                              UFT_DOS33_VTOC_SECTOR) * UFT_APPLE_SECTOR_SIZE;
        memcpy(&ctx->vtoc, ctx->data + vtoc_offset, sizeof(ctx->vtoc));
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        /* Extract ProDOS volume info */
        const uint8_t *key = ctx->data + 0x400;
        uint8_t name_len = key[4] & 0x0F;
        memcpy(ctx->volume_name, &key[5], name_len);
        ctx->volume_name[name_len] = '\0';
        
        /* Total blocks */
        ctx->total_blocks = key[0x29] | (key[0x2A] << 8);
        
        /* Bitmap block */
        ctx->bitmap_block = key[0x27] | (key[0x28] << 8);
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
    
    /* Read file */
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
    
    /* Open with ownership */
    int ret = uft_apple_open(ctx, data, size, false);
    if (ret < 0) {
        free(data);
        return ret;
    }
    
    ctx->owns_data = true;
    return 0;
}

int uft_apple_save(uft_apple_ctx_t *ctx, const char *filename) {
    if (!ctx || !filename || !ctx->data) return UFT_APPLE_ERR_INVALID;
    
    /* Write back VTOC if DOS 3.3 */
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        size_t vtoc_offset = (UFT_DOS33_VTOC_TRACK * ctx->sectors_per_track + 
                              UFT_DOS33_VTOC_SECTOR) * UFT_APPLE_SECTOR_SIZE;
        memcpy(ctx->data + vtoc_offset, &ctx->vtoc, sizeof(ctx->vtoc));
    }
    
    FILE *fp = fopen(filename, "wb");
    if (!fp) return UFT_APPLE_ERR_IO;
    
    size_t written = fwrite(ctx->data, 1, ctx->size, fp);
    fclose(fp);
    
    if (written != ctx->size) return UFT_APPLE_ERR_IO;
    
    ctx->is_modified = false;
    return 0;
}

/*===========================================================================
 * Sector/Block Access
 *===========================================================================*/

/**
 * @brief Calculate sector offset with interleave
 */
static size_t get_sector_offset(const uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector) {
    /* Apply interleave */
    uint8_t physical = ctx->sector_map[sector & 0x0F];
    
    return ((size_t)track * ctx->sectors_per_track + physical) * UFT_APPLE_SECTOR_SIZE;
}

int uft_apple_read_sector(const uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector,
                          uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    if (track >= ctx->tracks || sector >= ctx->sectors_per_track) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    size_t offset = get_sector_offset(ctx, track, sector);
    if (offset + UFT_APPLE_SECTOR_SIZE > ctx->size) {
        return UFT_APPLE_ERR_IO;
    }
    
    memcpy(buffer, ctx->data + offset, UFT_APPLE_SECTOR_SIZE);
    return 0;
}

int uft_apple_write_sector(uft_apple_ctx_t *ctx, uint8_t track, uint8_t sector,
                           const uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    if (track >= ctx->tracks || sector >= ctx->sectors_per_track) {
        return UFT_APPLE_ERR_INVALID;
    }
    
    size_t offset = get_sector_offset(ctx, track, sector);
    if (offset + UFT_APPLE_SECTOR_SIZE > ctx->size) {
        return UFT_APPLE_ERR_IO;
    }
    
    memcpy(ctx->data + offset, buffer, UFT_APPLE_SECTOR_SIZE);
    ctx->is_modified = true;
    return 0;
}

/**
 * @brief Convert ProDOS block number to track/sector
 */
static void block_to_ts(const uft_apple_ctx_t *ctx, uint16_t block, 
                        uint8_t *track, uint8_t *sector) {
    /* Each block = 2 sectors */
    uint16_t logical_sector = block * 2;
    *track = logical_sector / ctx->sectors_per_track;
    *sector = logical_sector % ctx->sectors_per_track;
}

int uft_apple_read_block(const uft_apple_ctx_t *ctx, uint16_t block, uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    uint8_t track, sector;
    block_to_ts(ctx, block, &track, &sector);
    
    /* Read first sector */
    int ret = uft_apple_read_sector(ctx, track, sector, buffer);
    if (ret < 0) return ret;
    
    /* Read second sector */
    if (sector + 1 < ctx->sectors_per_track) {
        ret = uft_apple_read_sector(ctx, track, sector + 1, buffer + UFT_APPLE_SECTOR_SIZE);
    } else {
        ret = uft_apple_read_sector(ctx, track + 1, 0, buffer + UFT_APPLE_SECTOR_SIZE);
    }
    
    return ret;
}

int uft_apple_write_block(uft_apple_ctx_t *ctx, uint16_t block, const uint8_t *buffer) {
    if (!ctx || !ctx->data || !buffer) return UFT_APPLE_ERR_INVALID;
    
    uint8_t track, sector;
    block_to_ts(ctx, block, &track, &sector);
    
    /* Write first sector */
    int ret = uft_apple_write_sector(ctx, track, sector, buffer);
    if (ret < 0) return ret;
    
    /* Write second sector */
    if (sector + 1 < ctx->sectors_per_track) {
        ret = uft_apple_write_sector(ctx, track, sector + 1, buffer + UFT_APPLE_SECTOR_SIZE);
    } else {
        ret = uft_apple_write_sector(ctx, track + 1, 0, buffer + UFT_APPLE_SECTOR_SIZE);
    }
    
    return ret;
}

/*===========================================================================
 * Volume Info
 *===========================================================================*/

int uft_apple_get_volume_name(const uft_apple_ctx_t *ctx, char *name, size_t size) {
    if (!ctx || !name || size == 0) return UFT_APPLE_ERR_INVALID;
    
    if (ctx->fs_type == UFT_APPLE_FS_DOS33 || ctx->fs_type == UFT_APPLE_FS_DOS32) {
        snprintf(name, size, "DISK VOLUME %03d", ctx->vtoc.volume_number);
    } else if (ctx->fs_type == UFT_APPLE_FS_PRODOS) {
        snprintf(name, size, "/%s", ctx->volume_name);
    } else {
        strncpy(name, "UNKNOWN", size);
        name[size - 1] = '\0';
    }
    
    return 0;
}

/*===========================================================================
 * Type Conversion Utilities
 *===========================================================================*/

char uft_dos33_type_char(uint8_t type) {
    type &= 0x7F;  /* Remove lock bit */
    
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
} prodos_type_names[] = {
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
    for (int i = 0; prodos_type_names[i].name; i++) {
        if (prodos_type_names[i].type == type) {
            return prodos_type_names[i].name;
        }
    }
    return "???";
}

/*===========================================================================
 * Time Conversion
 *===========================================================================*/

time_t uft_prodos_to_unix_time(uft_prodos_datetime_t dt) {
    if (dt.date == 0 && dt.time == 0) return 0;
    
    struct tm tm = {0};
    
    /* Date: YYYYYYYMMMMDDDDD */
    int year = (dt.date >> 9) & 0x7F;
    int month = (dt.date >> 5) & 0x0F;
    int day = dt.date & 0x1F;
    
    /* Time: 000HHHHH00MMMMMM */
    int hour = (dt.time >> 8) & 0x1F;
    int minute = dt.time & 0x3F;
    
    /* ProDOS year is relative to 1900, but wraps at 100 */
    tm.tm_year = year;  /* Already relative to 1900 */
    if (tm.tm_year < 40) tm.tm_year += 100;  /* Y2K adjustment */
    
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = minute;
    tm.tm_sec = 0;
    
    return mktime(&tm);
}

uft_prodos_datetime_t uft_prodos_from_unix_time(time_t t) {
    uft_prodos_datetime_t dt = {0, 0};
    
    if (t == 0) return dt;
    
    struct tm *tm = localtime(&t);
    if (!tm) return dt;
    
    /* Year (0-127, relative to 1900) */
    int year = tm->tm_year;
    if (year >= 100) year -= 100;  /* Wrap for Y2K */
    
    dt.date = ((year & 0x7F) << 9) |
              (((tm->tm_mon + 1) & 0x0F) << 5) |
              (tm->tm_mday & 0x1F);
    
    dt.time = ((tm->tm_hour & 0x1F) << 8) |
              (tm->tm_min & 0x3F);
    
    return dt;
}
