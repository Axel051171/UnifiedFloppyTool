/**
 * @file uft_atari_dos_core.c
 * @brief Atari DOS 2.x/MyDOS filesystem core implementation
 * 
 * Context management, detection, sector I/O, VTOC operations
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/fs/uft_atari_dos.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Internal Context Structure
 *===========================================================================*/

struct uft_atari_ctx {
    /* Image data */
    uint8_t *data;                  /**< Disk image data */
    size_t   data_size;             /**< Total data size */
    bool     owns_data;             /**< True if we allocated data */
    bool     modified;              /**< True if modified */
    
    /* ATR handling */
    bool     is_atr;                /**< True if ATR format */
    size_t   data_offset;           /**< Offset past ATR header */
    uft_atari_atr_header_t atr_header; /**< ATR header if present */
    
    /* Disk geometry */
    uft_atari_geometry_t geometry;  /**< Disk geometry */
    uft_atari_dos_type_t dos_type;  /**< DOS variant */
    
    /* Cached VTOC */
    uint8_t  vtoc_cache[256];       /**< Cached VTOC sector */
    bool     vtoc_loaded;           /**< VTOC is cached */
    bool     vtoc_dirty;            /**< VTOC needs write */
    uint16_t vtoc2_sector;          /**< MyDOS VTOC2 location */
    
    /* State */
    bool     open;                  /**< Context is open */
};

/*===========================================================================
 * Geometry Presets
 *===========================================================================*/

static const uft_atari_geometry_t g_geometry_presets[] = {
    /* SD: 40 tracks, 18 sectors, 128 bytes = 90KB (720 sectors) */
    {40, 1, 18, 128, 720,  92160,  360, 361, 8, UFT_ATARI_DENSITY_SD},
    /* ED: 40 tracks, 26 sectors, 128 bytes = 130KB (1040 sectors) */
    {40, 1, 26, 128, 1040, 133120, 360, 361, 8, UFT_ATARI_DENSITY_ED},
    /* DD: 40 tracks, 18 sectors, 256 bytes = 180KB (720 sectors) */
    {40, 1, 18, 256, 720,  184320, 360, 361, 8, UFT_ATARI_DENSITY_DD},
    /* QD: 80 tracks, 18 sectors, 256 bytes = 360KB (1440 sectors) */
    {80, 1, 18, 256, 1440, 368640, 360, 361, 8, UFT_ATARI_DENSITY_QD},
    /* HD: 80 tracks, 36 sectors, 256 bytes = 720KB (2880 sectors) - MyDOS */
    {80, 1, 36, 256, 2880, 737280, 360, 361, 8, UFT_ATARI_DENSITY_HD}
};

/*===========================================================================
 * Name Tables
 *===========================================================================*/

static const char *g_dos_names[] = {
    "Unknown",
    "Atari DOS 1.0",
    "Atari DOS 2.0S",
    "Atari DOS 2.0D",
    "Atari DOS 2.5",
    "MyDOS 4.5",
    "SpartaDOS",
    "DOS XE"
};

static const char *g_density_names[] = {
    "Single Density (SD)",
    "Enhanced Density (ED)",
    "Double Density (DD)",
    "Quad Density (QD)",
    "High Density (HD)"
};

static const char *g_error_strings[] = {
    [UFT_ATARI_OK]           = "Success",
    [UFT_ATARI_ERR_PARAM]    = "Invalid parameter",
    [UFT_ATARI_ERR_MEMORY]   = "Out of memory",
    [UFT_ATARI_ERR_FORMAT]   = "Invalid format",
    [UFT_ATARI_ERR_NOT_ATR]  = "Not an ATR file",
    [UFT_ATARI_ERR_READ]     = "Read error",
    [UFT_ATARI_ERR_WRITE]    = "Write error",
    [UFT_ATARI_ERR_SECTOR]   = "Sector out of range",
    [UFT_ATARI_ERR_VTOC]     = "VTOC error",
    [UFT_ATARI_ERR_NOTFOUND] = "File not found",
    [UFT_ATARI_ERR_EXISTS]   = "File already exists",
    [UFT_ATARI_ERR_FULL]     = "Disk full",
    [UFT_ATARI_ERR_DIRFULL]  = "Directory full",
    [UFT_ATARI_ERR_LOCKED]   = "File is locked",
    [UFT_ATARI_ERR_CORRUPT]  = "Data corrupted",
    [UFT_ATARI_ERR_CHAIN]    = "Bad sector chain",
    [UFT_ATARI_ERR_NOT_OPEN] = "Not open",
    [UFT_ATARI_ERR_READONLY] = "Read only"
};

/*===========================================================================
 * Context Lifecycle
 *===========================================================================*/

uft_atari_ctx_t *uft_atari_create(void)
{
    uft_atari_ctx_t *ctx = calloc(1, sizeof(uft_atari_ctx_t));
    return ctx;
}

void uft_atari_destroy(uft_atari_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    free(ctx);
}

/*===========================================================================
 * ATR Header Support
 *===========================================================================*/

bool uft_atari_is_atr(const uint8_t *data, size_t size)
{
    if (!data || size < 16) return false;
    
    uint16_t magic = data[0] | ((uint16_t)data[1] << 8);
    return (magic == UFT_ATARI_ATR_MAGIC);
}

uft_atari_error_t uft_atari_parse_atr(const uint8_t *data,
                                      size_t size,
                                      uft_atari_atr_header_t *header,
                                      size_t *data_offset)
{
    if (!data || !header || !data_offset || size < 16) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    uint16_t magic = data[0] | ((uint16_t)data[1] << 8);
    if (magic != UFT_ATARI_ATR_MAGIC) {
        return UFT_ATARI_ERR_FORMAT;
    }
    
    header->magic = magic;
    header->paragraphs = data[2] | ((uint16_t)data[3] << 8);
    header->sector_size = data[4] | ((uint16_t)data[5] << 8);
    header->paragraphs_hi = data[6];
    header->crc = data[7] | ((uint32_t)data[8] << 8) |
                  ((uint32_t)data[9] << 16) | ((uint32_t)data[10] << 24);
    header->reserved = data[11] | ((uint32_t)data[12] << 8) |
                       ((uint32_t)data[13] << 16) | ((uint32_t)data[14] << 24);
    header->flags = data[15];
    
    *data_offset = 16;
    return UFT_ATARI_OK;
}

uft_atari_error_t uft_atari_make_atr_header(uft_atari_density_t density,
                                            uft_atari_atr_header_t *header)
{
    if (!header || density >= UFT_ATARI_DENSITY_COUNT) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    memset(header, 0, sizeof(*header));
    header->magic = UFT_ATARI_ATR_MAGIC;
    
    const uft_atari_geometry_t *geom = &g_geometry_presets[density];
    
    /* Calculate paragraphs (16-byte units) */
    /* Note: First 3 sectors use 128 bytes even in DD */
    size_t image_size;
    if (geom->sector_size == 256 && geom->total_sectors > 3) {
        /* 3 boot sectors at 128 bytes + rest at 256 */
        image_size = 3 * 128 + (geom->total_sectors - 3) * 256;
    } else {
        image_size = geom->total_sectors * geom->sector_size;
    }
    
    uint32_t paragraphs = (uint32_t)(image_size / 16);
    header->paragraphs = paragraphs & 0xFFFF;
    header->paragraphs_hi = (paragraphs >> 16) & 0xFF;
    header->sector_size = geom->sector_size;
    
    return UFT_ATARI_OK;
}

/*===========================================================================
 * Geometry Detection
 *===========================================================================*/

/**
 * @brief Detect geometry from image size
 */
static uft_atari_error_t detect_geometry_from_size(size_t size, 
                                                    uft_atari_geometry_t *geom)
{
    /* Check standard sizes */
    for (int i = 0; i < UFT_ATARI_DENSITY_COUNT; i++) {
        if (size == g_geometry_presets[i].total_bytes) {
            *geom = g_geometry_presets[i];
            return UFT_ATARI_OK;
        }
    }
    
    /* Check DD with 128-byte boot sectors */
    /* 3×128 + (720-3)×256 = 384 + 183,552 = 183,936 */
    if (size == 183936) {
        *geom = g_geometry_presets[UFT_ATARI_DENSITY_DD];
        return UFT_ATARI_OK;
    }
    
    /* 3×128 + (1440-3)×256 = 384 + 367,872 = 368,256 */
    if (size == 368256) {
        *geom = g_geometry_presets[UFT_ATARI_DENSITY_QD];
        return UFT_ATARI_OK;
    }
    
    /* Try to infer from size */
    if (size >= 92160 && size < 133120) {
        *geom = g_geometry_presets[UFT_ATARI_DENSITY_SD];
        return UFT_ATARI_OK;
    }
    if (size >= 133120 && size < 184320) {
        *geom = g_geometry_presets[UFT_ATARI_DENSITY_ED];
        return UFT_ATARI_OK;
    }
    if (size >= 184320 && size < 368640) {
        *geom = g_geometry_presets[UFT_ATARI_DENSITY_DD];
        return UFT_ATARI_OK;
    }
    if (size >= 368640) {
        *geom = g_geometry_presets[UFT_ATARI_DENSITY_QD];
        return UFT_ATARI_OK;
    }
    
    return UFT_ATARI_ERR_FORMAT;
}

/*===========================================================================
 * Sector I/O
 *===========================================================================*/

/**
 * @brief Calculate byte offset for sector (1-based Atari convention)
 */
static size_t sector_offset(uft_atari_ctx_t *ctx, uint16_t sector)
{
    if (sector == 0 || sector > ctx->geometry.total_sectors) {
        return SIZE_MAX; /* Invalid */
    }
    
    size_t offset = ctx->data_offset;
    uint16_t sec_size = ctx->geometry.sector_size;
    
    /* Handle DD boot sectors at 128 bytes */
    if (sec_size == 256 && sector <= 3) {
        /* Boot sectors 1-3 are 128 bytes */
        offset += (sector - 1) * 128;
    } else if (sec_size == 256 && sector > 3) {
        /* Boot sectors + remaining at 256 */
        offset += 3 * 128 + (sector - 4) * 256;
    } else {
        /* SD/ED: all sectors same size */
        offset += (sector - 1) * sec_size;
    }
    
    return offset;
}

/**
 * @brief Get actual sector size (boot sectors may differ)
 */
static uint16_t get_sector_size(uft_atari_ctx_t *ctx, uint16_t sector)
{
    if (ctx->geometry.sector_size == 256 && sector <= 3) {
        return 128;
    }
    return ctx->geometry.sector_size;
}

/**
 * @brief Read sector data
 */
static uft_atari_error_t read_sector(uft_atari_ctx_t *ctx, 
                                      uint16_t sector,
                                      uint8_t *buffer)
{
    if (!ctx || !ctx->open || !buffer) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    size_t offset = sector_offset(ctx, sector);
    if (offset == SIZE_MAX) {
        return UFT_ATARI_ERR_READ;
    }
    
    uint16_t size = get_sector_size(ctx, sector);
    if (offset + size > ctx->data_size) {
        return UFT_ATARI_ERR_READ;
    }
    
    memcpy(buffer, ctx->data + offset, size);
    return UFT_ATARI_OK;
}

/**
 * @brief Write sector data
 */
static uft_atari_error_t write_sector(uft_atari_ctx_t *ctx,
                                       uint16_t sector,
                                       const uint8_t *buffer)
{
    if (!ctx || !ctx->open || !buffer) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    size_t offset = sector_offset(ctx, sector);
    if (offset == SIZE_MAX) {
        return UFT_ATARI_ERR_WRITE;
    }
    
    uint16_t size = get_sector_size(ctx, sector);
    if (offset + size > ctx->data_size) {
        return UFT_ATARI_ERR_WRITE;
    }
    
    memcpy(ctx->data + offset, buffer, size);
    ctx->modified = true;
    return UFT_ATARI_OK;
}

/*===========================================================================
 * VTOC Operations
 *===========================================================================*/

/**
 * @brief Load VTOC into cache
 */
static uft_atari_error_t load_vtoc(uft_atari_ctx_t *ctx)
{
    if (ctx->vtoc_loaded) {
        return UFT_ATARI_OK;
    }
    
    uft_atari_error_t err = read_sector(ctx, ctx->geometry.vtoc_sector,
                                         ctx->vtoc_cache);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    ctx->vtoc_loaded = true;
    ctx->vtoc_dirty = false;
    
    /* Check for MyDOS VTOC2 */
    if (ctx->dos_type == UFT_ATARI_MYDOS) {
        uft_atari_mydos_vtoc_t *vtoc = (uft_atari_mydos_vtoc_t *)ctx->vtoc_cache;
        ctx->vtoc2_sector = vtoc->vtoc2_sector;
    }
    
    return UFT_ATARI_OK;
}

/**
 * @brief Flush VTOC cache to disk
 */
static uft_atari_error_t flush_vtoc(uft_atari_ctx_t *ctx)
{
    if (!ctx->vtoc_dirty) {
        return UFT_ATARI_OK;
    }
    
    uft_atari_error_t err = write_sector(ctx, ctx->geometry.vtoc_sector,
                                          ctx->vtoc_cache);
    if (err != UFT_ATARI_OK) {
        return err;
    }
    
    ctx->vtoc_dirty = false;
    return UFT_ATARI_OK;
}

/**
 * @brief Check if sector is allocated in VTOC bitmap
 */
static bool is_sector_allocated(uft_atari_ctx_t *ctx, uint16_t sector)
{
    if (!ctx->vtoc_loaded) {
        if (load_vtoc(ctx) != UFT_ATARI_OK) {
            return true; /* Assume allocated on error */
        }
    }
    
    /* Bitmap starts at offset 10 in VTOC */
    /* Each bit represents one sector, LSB first */
    /* 1 = free, 0 = allocated (Atari convention) */
    uint16_t byte_idx = sector / 8;
    uint8_t bit_idx = sector % 8;
    
    if (byte_idx >= 90) {
        return true; /* Out of range */
    }
    
    uint8_t bitmap_byte = ctx->vtoc_cache[10 + byte_idx];
    return !(bitmap_byte & (1 << bit_idx)); /* 0 = allocated */
}

/**
 * @brief Allocate sector in VTOC
 */
static uft_atari_error_t allocate_sector(uft_atari_ctx_t *ctx, uint16_t sector)
{
    if (!ctx->vtoc_loaded) {
        uft_atari_error_t err = load_vtoc(ctx);
        if (err != UFT_ATARI_OK) return err;
    }
    
    uint16_t byte_idx = sector / 8;
    uint8_t bit_idx = sector % 8;
    
    if (byte_idx >= 90) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Clear bit to mark as allocated */
    ctx->vtoc_cache[10 + byte_idx] &= ~(1 << bit_idx);
    
    /* Update free sector count */
    uint16_t free_count = ctx->vtoc_cache[3] | ((uint16_t)ctx->vtoc_cache[4] << 8);
    if (free_count > 0) {
        free_count--;
        ctx->vtoc_cache[3] = free_count & 0xFF;
        ctx->vtoc_cache[4] = (free_count >> 8) & 0xFF;
    }
    
    ctx->vtoc_dirty = true;
    return UFT_ATARI_OK;
}

/**
 * @brief Free sector in VTOC
 */
static uft_atari_error_t free_sector(uft_atari_ctx_t *ctx, uint16_t sector)
{
    if (!ctx->vtoc_loaded) {
        uft_atari_error_t err = load_vtoc(ctx);
        if (err != UFT_ATARI_OK) return err;
    }
    
    uint16_t byte_idx = sector / 8;
    uint8_t bit_idx = sector % 8;
    
    if (byte_idx >= 90) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Set bit to mark as free */
    ctx->vtoc_cache[10 + byte_idx] |= (1 << bit_idx);
    
    /* Update free sector count */
    uint16_t free_count = ctx->vtoc_cache[3] | ((uint16_t)ctx->vtoc_cache[4] << 8);
    free_count++;
    ctx->vtoc_cache[3] = free_count & 0xFF;
    ctx->vtoc_cache[4] = (free_count >> 8) & 0xFF;
    
    ctx->vtoc_dirty = true;
    return UFT_ATARI_OK;
}

/**
 * @brief Find first free sector
 */
static uint16_t find_free_sector(uft_atari_ctx_t *ctx)
{
    if (!ctx->vtoc_loaded) {
        if (load_vtoc(ctx) != UFT_ATARI_OK) {
            return 0;
        }
    }
    
    /* Skip system sectors (1-3 boot, 360 VTOC, 361-368 directory) */
    for (uint16_t sector = 4; sector <= ctx->geometry.total_sectors; sector++) {
        /* Skip VTOC and directory sectors */
        if (sector >= 360 && sector <= 368) continue;
        
        uint16_t byte_idx = sector / 8;
        uint8_t bit_idx = sector % 8;
        
        if (byte_idx >= 90) continue;
        
        uint8_t bitmap_byte = ctx->vtoc_cache[10 + byte_idx];
        if (bitmap_byte & (1 << bit_idx)) {
            /* Bit is set = sector is free */
            return sector;
        }
    }
    
    return 0; /* No free sector */
}

/**
 * @brief Count free sectors
 */
static uint16_t count_free_sectors(uft_atari_ctx_t *ctx)
{
    if (!ctx->vtoc_loaded) {
        if (load_vtoc(ctx) != UFT_ATARI_OK) {
            return 0;
        }
    }
    
    return ctx->vtoc_cache[3] | ((uint16_t)ctx->vtoc_cache[4] << 8);
}

/*===========================================================================
 * DOS Type Detection
 *===========================================================================*/

static uft_atari_dos_type_t detect_dos_type(uft_atari_ctx_t *ctx)
{
    uft_atari_error_t err = load_vtoc(ctx);
    if (err != UFT_ATARI_OK) {
        return UFT_ATARI_DOS_UNKNOWN;
    }
    
    uint8_t dos_code = ctx->vtoc_cache[0];
    
    switch (dos_code) {
        case 0:
            /* DOS 1.0 uses code 0 */
            return UFT_ATARI_DOS_1;
        case 1:
            /* Could be DOS 2.0 */
            if (ctx->geometry.sector_size == 128) {
                return UFT_ATARI_DOS_2S;
            }
            return UFT_ATARI_DOS_2D;
        case 2:
            /* DOS 2.0S/2.5 or MyDOS */
            /* Check for MyDOS signature in extended VTOC area */
            if (ctx->vtoc_cache[100] != 0 || ctx->vtoc_cache[101] != 0) {
                return UFT_ATARI_MYDOS;
            }
            if (ctx->geometry.density == UFT_ATARI_DENSITY_ED) {
                return UFT_ATARI_DOS_25;
            }
            if (ctx->geometry.sector_size == 128) {
                return UFT_ATARI_DOS_2S;
            }
            return UFT_ATARI_DOS_2D;
        default:
            /* Check for SpartaDOS signature in boot sector */
            {
                uint8_t boot[128];
                if (read_sector(ctx, 1, boot) == UFT_ATARI_OK) {
                    /* SpartaDOS has "SD" at offset 0 */
                    if (boot[0] == 'S' && boot[1] == 'D') {
                        return UFT_ATARI_SPARTADOS;
                    }
                }
            }
            return UFT_ATARI_DOS_UNKNOWN;
    }
}

/*===========================================================================
 * Context Open/Close
 *===========================================================================*/

uft_atari_error_t uft_atari_open(uft_atari_ctx_t *ctx,
                                  uint8_t *data,
                                  size_t size,
                                  bool copy)
{
    if (!ctx || !data || size < UFT_ATARI_SIZE_SD) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Close any existing context */
    uft_atari_close(ctx);
    
    /* Check for ATR format */
    size_t data_offset = 0;
    bool is_atr = uft_atari_is_atr(data, size);
    
    if (is_atr) {
        uft_atari_error_t err = uft_atari_parse_atr(data, size,
                                                     &ctx->atr_header,
                                                     &data_offset);
        if (err != UFT_ATARI_OK) {
            return err;
        }
        ctx->is_atr = true;
        ctx->data_offset = data_offset;
    }
    
    /* Copy or reference data */
    if (copy) {
        ctx->data = malloc(size);
        if (!ctx->data) {
            return UFT_ATARI_ERR_MEMORY;
        }
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = data;
        ctx->owns_data = false;
    }
    ctx->data_size = size;
    
    /* Detect geometry */
    size_t disk_size = size - data_offset;
    uft_atari_error_t err = detect_geometry_from_size(disk_size, &ctx->geometry);
    if (err != UFT_ATARI_OK) {
        if (ctx->owns_data) {
            free(ctx->data);
            ctx->data = NULL;
        }
        return err;
    }
    
    /* Update geometry from ATR if present */
    if (is_atr) {
        ctx->geometry.sector_size = ctx->atr_header.sector_size;
        /* Recalculate total sectors based on ATR paragraphs */
        uint32_t paragraphs = ctx->atr_header.paragraphs |
                              ((uint32_t)ctx->atr_header.paragraphs_hi << 16);
        uint32_t bytes = paragraphs * 16;
        /* Approximate total sectors */
        if (ctx->geometry.sector_size == 256) {
            /* Account for 128-byte boot sectors */
            if (bytes > 384) {
                ctx->geometry.total_sectors = 3 + (uint16_t)((bytes - 384) / 256);
            }
        } else {
            ctx->geometry.total_sectors = (uint16_t)(bytes / ctx->geometry.sector_size);
        }
    }
    
    ctx->open = true;
    
    /* Detect DOS type */
    ctx->dos_type = detect_dos_type(ctx);
    
    return UFT_ATARI_OK;
}

uft_atari_error_t uft_atari_close(uft_atari_ctx_t *ctx)
{
    if (!ctx) return UFT_ATARI_ERR_PARAM;
    
    /* Flush pending writes */
    if (ctx->vtoc_dirty) {
        flush_vtoc(ctx);
    }
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    
    ctx->data = NULL;
    ctx->data_size = 0;
    ctx->owns_data = false;
    ctx->modified = false;
    ctx->is_atr = false;
    ctx->data_offset = 0;
    ctx->vtoc_loaded = false;
    ctx->vtoc_dirty = false;
    ctx->open = false;
    
    return UFT_ATARI_OK;
}

uft_atari_error_t uft_atari_save(uft_atari_ctx_t *ctx, 
                                  uint8_t **out_data,
                                  size_t *out_size)
{
    if (!ctx || !ctx->open || !out_data || !out_size) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Flush VTOC if dirty */
    if (ctx->vtoc_dirty) {
        uft_atari_error_t err = flush_vtoc(ctx);
        if (err != UFT_ATARI_OK) return err;
    }
    
    *out_data = ctx->data;
    *out_size = ctx->data_size;
    
    return UFT_ATARI_OK;
}

/*===========================================================================
 * Detection API
 *===========================================================================*/

uft_atari_error_t uft_atari_detect(const uint8_t *data,
                                    size_t size,
                                    uft_atari_detect_t *result)
{
    if (!data || !result) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    result->confidence = 0;
    
    /* Check minimum size */
    if (size < UFT_ATARI_SIZE_SD - 16) {
        return UFT_ATARI_OK; /* Not Atari format */
    }
    
    /* Check for ATR header */
    size_t data_offset = 0;
    if (uft_atari_is_atr(data, size)) {
        uft_atari_atr_header_t hdr;
        if (uft_atari_parse_atr(data, size, &hdr, &data_offset) == UFT_ATARI_OK) {
            result->is_atr = true;
            result->confidence = 60; /* ATR header is good indicator */
        }
    }
    
    /* Detect geometry */
    size_t disk_size = size - data_offset;
    uft_atari_geometry_t geom;
    if (detect_geometry_from_size(disk_size, &geom) != UFT_ATARI_OK) {
        return UFT_ATARI_OK; /* Unknown size */
    }
    
    result->geometry = geom;
    result->confidence += 20;
    
    /* Try to validate VTOC */
    size_t vtoc_offset = data_offset;
    if (geom.sector_size == 256 && geom.vtoc_sector > 3) {
        vtoc_offset += 3 * 128 + (geom.vtoc_sector - 4) * 256;
    } else {
        vtoc_offset += (geom.vtoc_sector - 1) * geom.sector_size;
    }
    
    if (vtoc_offset + 128 <= size) {
        const uint8_t *vtoc = data + vtoc_offset;
        
        /* Check VTOC format */
        uint8_t dos_code = vtoc[0];
        uint16_t total = vtoc[1] | ((uint16_t)vtoc[2] << 8);
        uint16_t free_count = vtoc[3] | ((uint16_t)vtoc[4] << 8);
        
        /* DOS code should be 0-2 */
        if (dos_code <= 2) {
            result->confidence += 10;
        }
        
        /* Total sectors should match geometry */
        if (total == geom.total_sectors) {
            result->confidence += 10;
            result->dos_type = (dos_code == 2) ? UFT_ATARI_DOS_2S : 
                               (dos_code == 1) ? UFT_ATARI_DOS_2S : UFT_ATARI_DOS_1;
        }
        
        /* Free count should be reasonable */
        if (free_count <= total) {
            result->confidence += 5;
        }
    }
    
    /* Cap confidence */
    if (result->confidence > 95) {
        result->confidence = 95;
    }
    
    return UFT_ATARI_OK;
}

/*===========================================================================
 * Info Accessors
 *===========================================================================*/

uft_atari_dos_type_t uft_atari_get_dos_type(uft_atari_ctx_t *ctx)
{
    return ctx ? ctx->dos_type : UFT_ATARI_DOS_UNKNOWN;
}

uft_atari_density_t uft_atari_get_density(uft_atari_ctx_t *ctx)
{
    return ctx ? ctx->geometry.density : UFT_ATARI_DENSITY_SD;
}

uft_atari_error_t uft_atari_get_geometry(uft_atari_ctx_t *ctx,
                                          uft_atari_geometry_t *geometry)
{
    if (!ctx || !geometry) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    *geometry = ctx->geometry;
    return UFT_ATARI_OK;
}

uft_atari_error_t uft_atari_get_free_space(uft_atari_ctx_t *ctx,
                                            uint16_t *sectors,
                                            uint32_t *bytes)
{
    if (!ctx || !ctx->open) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    uint16_t free_secs = count_free_sectors(ctx);
    
    if (sectors) *sectors = free_secs;
    
    if (bytes) {
        /* Data bytes per sector (minus link bytes) */
        uint16_t data_per_sector = (ctx->geometry.sector_size == 256) ? 253 : 125;
        *bytes = (uint32_t)free_secs * data_per_sector;
    }
    
    return UFT_ATARI_OK;
}

bool uft_atari_is_modified(uft_atari_ctx_t *ctx)
{
    return ctx ? ctx->modified : false;
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

const char *uft_atari_dos_name(uft_atari_dos_type_t type)
{
    if (type >= UFT_ATARI_DOS_COUNT) {
        return "Unknown";
    }
    return g_dos_names[type];
}

const char *uft_atari_density_name(uft_atari_density_t density)
{
    if (density >= UFT_ATARI_DENSITY_COUNT) {
        return "Unknown";
    }
    return g_density_names[density];
}

const char *uft_atari_error_string(uft_atari_error_t error)
{
    if (error >= sizeof(g_error_strings) / sizeof(g_error_strings[0])) {
        return "Unknown error";
    }
    return g_error_strings[error];
}

uft_atari_error_t uft_atari_parse_filename(const char *input,
                                           char *filename,
                                           char *extension)
{
    if (!input || !filename || !extension) {
        return UFT_ATARI_ERR_PARAM;
    }
    
    /* Initialize with spaces */
    memset(filename, ' ', 8);
    memset(extension, ' ', 3);
    
    /* Find dot */
    const char *dot = strchr(input, '.');
    
    /* Copy filename part */
    size_t name_len = dot ? (size_t)(dot - input) : strlen(input);
    if (name_len > 8) name_len = 8;
    
    for (size_t i = 0; i < name_len; i++) {
        filename[i] = (char)toupper((unsigned char)input[i]);
    }
    
    /* Copy extension if present */
    if (dot && dot[1]) {
        const char *ext = dot + 1;
        size_t ext_len = strlen(ext);
        if (ext_len > 3) ext_len = 3;
        
        for (size_t i = 0; i < ext_len; i++) {
            extension[i] = (char)toupper((unsigned char)ext[i]);
        }
    }
    
    return UFT_ATARI_OK;
}

void uft_atari_format_filename(const char *filename,
                               const char *extension,
                               char *buffer)
{
    if (!filename || !extension || !buffer) {
        return;
    }
    
    /* Copy filename, trimming trailing spaces */
    int i;
    for (i = 0; i < 8 && filename[i] != ' '; i++) {
        buffer[i] = filename[i];
    }
    
    /* Check if extension exists */
    bool has_ext = false;
    for (int j = 0; j < 3; j++) {
        if (extension[j] != ' ') {
            has_ext = true;
            break;
        }
    }
    
    if (has_ext) {
        buffer[i++] = '.';
        for (int j = 0; j < 3 && extension[j] != ' '; j++) {
            buffer[i++] = extension[j];
        }
    }
    
    buffer[i] = '\0';
}

bool uft_atari_valid_filename(const char *filename)
{
    if (!filename || !*filename) {
        return false;
    }
    
    /* Parse and validate */
    char name[9], ext[4];
    if (uft_atari_parse_filename(filename, name, ext) != UFT_ATARI_OK) {
        return false;
    }
    
    /* Check for valid characters */
    /* Atari DOS allows: A-Z, 0-9, _ */
    for (int i = 0; i < 8 && name[i] != ' '; i++) {
        char c = name[i];
        if (!((c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') ||
              c == '_')) {
            return false;
        }
    }
    
    for (int i = 0; i < 3 && ext[i] != ' '; i++) {
        char c = ext[i];
        if (!((c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') ||
              c == '_')) {
            return false;
        }
    }
    
    /* At least one character in filename */
    if (name[0] == ' ') {
        return false;
    }
    
    return true;
}

/*===========================================================================
 * Internal Helpers (exported for other source files)
 *===========================================================================*/

/* These functions are used by uft_atari_dos_dir.c and uft_atari_dos_file.c */

uft_atari_error_t uft_atari_read_sector(uft_atari_ctx_t *ctx,
                                         uint16_t sector,
                                         uint8_t *buffer)
{
    return read_sector(ctx, sector, buffer);
}

uft_atari_error_t uft_atari_write_sector(uft_atari_ctx_t *ctx,
                                          uint16_t sector,
                                          const uint8_t *buffer)
{
    return write_sector(ctx, sector, buffer);
}

uint16_t uft_atari_get_sector_size(uft_atari_ctx_t *ctx, uint16_t sector)
{
    return get_sector_size(ctx, sector);
}

uft_atari_error_t uft_atari_load_vtoc(uft_atari_ctx_t *ctx)
{
    return load_vtoc(ctx);
}

uft_atari_error_t uft_atari_flush_vtoc(uft_atari_ctx_t *ctx)
{
    return flush_vtoc(ctx);
}

bool uft_atari_is_allocated(uft_atari_ctx_t *ctx, uint16_t sector)
{
    return is_sector_allocated(ctx, sector);
}

uft_atari_error_t uft_atari_alloc_sector(uft_atari_ctx_t *ctx, uint16_t sector)
{
    return allocate_sector(ctx, sector);
}

uft_atari_error_t uft_atari_free_sector_vtoc(uft_atari_ctx_t *ctx, uint16_t sector)
{
    return free_sector(ctx, sector);
}

uint16_t uft_atari_find_free(uft_atari_ctx_t *ctx)
{
    return find_free_sector(ctx);
}
