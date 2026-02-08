/**
 * @file uft_trsdos_core.c
 * @brief TRSDOS/LDOS/NewDOS Filesystem - Core Implementation
 * 
 * Core operations: detection, initialization, sector I/O, GAT management
 */

#include "uft/fs/uft_trsdos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*===========================================================================
 * Geometry Definitions
 *===========================================================================*/

static const uft_trsdos_geometry_t s_geometries[] = {
    [UFT_TRSDOS_GEOM_UNKNOWN] = {
        .tracks = 0, .sides = 0, .sectors_per_track = 0, .sector_size = 0,
        .dir_track = 0, .granule_sectors = 0, .total_granules = 0, .total_bytes = 0,
        .density = UFT_TRSDOS_DENSITY_SD, .name = "Unknown"
    },
    [UFT_TRSDOS_GEOM_M1_SSSD] = {
        .tracks = 35, .sides = 1, .sectors_per_track = 10, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 5, .total_granules = 68, .total_bytes = 89600,
        .density = UFT_TRSDOS_DENSITY_SD, .name = "Model I SSSD (35×10×256)"
    },
    [UFT_TRSDOS_GEOM_M1_SSDD] = {
        .tracks = 35, .sides = 1, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 6, .total_granules = 105, .total_bytes = 161280,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "Model I SSDD (35×18×256)"
    },
    [UFT_TRSDOS_GEOM_M1_DSSD] = {
        .tracks = 35, .sides = 2, .sectors_per_track = 10, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 5, .total_granules = 136, .total_bytes = 179200,
        .density = UFT_TRSDOS_DENSITY_SD, .name = "Model I DSSD (35×2×10×256)"
    },
    [UFT_TRSDOS_GEOM_M1_DSDD] = {
        .tracks = 35, .sides = 2, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 6, .total_granules = 210, .total_bytes = 322560,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "Model I DSDD (35×2×18×256)"
    },
    [UFT_TRSDOS_GEOM_M3_SSDD] = {
        .tracks = 40, .sides = 1, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 6, .total_granules = 120, .total_bytes = 184320,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "Model III SSDD (40×18×256)"
    },
    [UFT_TRSDOS_GEOM_M3_DSDD] = {
        .tracks = 40, .sides = 2, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 6, .total_granules = 240, .total_bytes = 368640,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "Model III DSDD (40×2×18×256)"
    },
    [UFT_TRSDOS_GEOM_M4_DSDD] = {
        .tracks = 40, .sides = 2, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 6, .total_granules = 240, .total_bytes = 368640,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "Model 4 DSDD (40×2×18×256)"
    },
    [UFT_TRSDOS_GEOM_M4_80T] = {
        .tracks = 80, .sides = 2, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 6, .total_granules = 480, .total_bytes = 737280,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "Model 4 80T (80×2×18×256)"
    },
    [UFT_TRSDOS_GEOM_COCO_SSSD] = {
        .tracks = 35, .sides = 1, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 9, .total_granules = 68, .total_bytes = 161280,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "CoCo SSSD (35×18×256)"
    },
    [UFT_TRSDOS_GEOM_COCO_DSDD] = {
        .tracks = 40, .sides = 2, .sectors_per_track = 18, .sector_size = 256,
        .dir_track = 17, .granule_sectors = 9, .total_granules = 156, .total_bytes = 368640,
        .density = UFT_TRSDOS_DENSITY_DD, .name = "CoCo DSDD (40×2×18×256)"
    },
};

static const char *s_version_names[] = {
    [UFT_TRSDOS_VERSION_UNKNOWN] = "Unknown",
    [UFT_TRSDOS_VERSION_23] = "TRSDOS 2.3",
    [UFT_TRSDOS_VERSION_13] = "TRSDOS 1.3",
    [UFT_TRSDOS_VERSION_6] = "TRSDOS 6.x / LS-DOS",
    [UFT_TRSDOS_VERSION_LDOS5] = "LDOS 5.x",
    [UFT_TRSDOS_VERSION_NEWDOS80] = "NewDOS/80",
    [UFT_TRSDOS_VERSION_DOSPLUS] = "DOS+",
    [UFT_TRSDOS_VERSION_MULTIDOS] = "MultiDOS",
    [UFT_TRSDOS_VERSION_DOUBLEDOS] = "DoubleDOS",
    [UFT_TRSDOS_VERSION_RSDOS] = "RS-DOS",
};

static const char *s_error_messages[] = {
    [UFT_TRSDOS_OK] = "Success",
    [UFT_TRSDOS_ERR_NULL] = "Null pointer",
    [UFT_TRSDOS_ERR_NOMEM] = "Out of memory",
    [UFT_TRSDOS_ERR_IO] = "I/O error",
    [UFT_TRSDOS_ERR_NOT_TRSDOS] = "Not a TRSDOS disk",
    [UFT_TRSDOS_ERR_CORRUPT] = "Corrupt filesystem",
    [UFT_TRSDOS_ERR_NOT_FOUND] = "File not found",
    [UFT_TRSDOS_ERR_EXISTS] = "File already exists",
    [UFT_TRSDOS_ERR_FULL] = "Disk full",
    [UFT_TRSDOS_ERR_PROTECTED] = "Write protected",
    [UFT_TRSDOS_ERR_INVALID] = "Invalid parameter",
    [UFT_TRSDOS_ERR_READONLY] = "Read-only mode",
    [UFT_TRSDOS_ERR_PASSWORD] = "Password required",
    [UFT_TRSDOS_ERR_LOCKED] = "File locked",
    [UFT_TRSDOS_ERR_RANGE] = "Out of range",
};

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_trsdos_ctx_t *uft_trsdos_create(void) {
    uft_trsdos_ctx_t *ctx = calloc(1, sizeof(uft_trsdos_ctx_t));
    if (!ctx) return NULL;
    
    ctx->version = UFT_TRSDOS_VERSION_UNKNOWN;
    return ctx;
}

void uft_trsdos_destroy(uft_trsdos_ctx_t *ctx) {
    if (!ctx) return;
    
    uft_trsdos_close(ctx);
    free(ctx);
}

void uft_trsdos_close(uft_trsdos_ctx_t *ctx) {
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    ctx->data = NULL;
    ctx->size = 0;
    ctx->owns_data = false;
    
    uft_trsdos_free_dir(&ctx->dir_cache);
    ctx->dir_cache_valid = false;
}

/*===========================================================================
 * Geometry API
 *===========================================================================*/

const uft_trsdos_geometry_t *uft_trsdos_get_geometry(uft_trsdos_geom_type_t type) {
    if (type >= UFT_TRSDOS_GEOM_COUNT) {
        return &s_geometries[UFT_TRSDOS_GEOM_UNKNOWN];
    }
    return &s_geometries[type];
}

uft_trsdos_geom_type_t uft_trsdos_detect_geometry(size_t size, uint8_t *confidence) {
    uint8_t conf = 0;
    uft_trsdos_geom_type_t result = UFT_TRSDOS_GEOM_UNKNOWN;
    
    /* Check against known sizes */
    struct {
        size_t size;
        uft_trsdos_geom_type_t geom;
        uint8_t conf;
    } sizes[] = {
        { 89600,  UFT_TRSDOS_GEOM_M1_SSSD, 90 },   /* 35×10×256 */
        { 161280, UFT_TRSDOS_GEOM_M1_SSDD, 70 },   /* 35×18×256 or CoCo */
        { 179200, UFT_TRSDOS_GEOM_M1_DSSD, 85 },   /* 35×2×10×256 */
        { 184320, UFT_TRSDOS_GEOM_M3_SSDD, 75 },   /* 40×18×256 */
        { 322560, UFT_TRSDOS_GEOM_M1_DSDD, 70 },   /* 35×2×18×256 */
        { 368640, UFT_TRSDOS_GEOM_M3_DSDD, 80 },   /* 40×2×18×256 */
        { 737280, UFT_TRSDOS_GEOM_M4_80T,  85 },   /* 80×2×18×256 */
    };
    
    for (size_t i = 0; i < sizeof(sizes)/sizeof(sizes[0]); i++) {
        if (size == sizes[i].size) {
            result = sizes[i].geom;
            conf = sizes[i].conf;
            break;
        }
    }
    
    if (confidence) *confidence = conf;
    return result;
}

/*===========================================================================
 * Sector I/O
 *===========================================================================*/

/**
 * @brief Calculate sector offset in image
 */
static size_t calc_sector_offset(const uft_trsdos_ctx_t *ctx,
                                  uint8_t track, uint8_t side, uint8_t sector) {
    const uft_trsdos_geometry_t *g = &ctx->geometry;
    
    if (track >= g->tracks || side >= g->sides || sector >= g->sectors_per_track) {
        return SIZE_MAX;
    }
    
    size_t offset;
    
    if (g->sides == 1) {
        /* Single-sided */
        offset = (size_t)track * g->sectors_per_track * g->sector_size +
                 (size_t)sector * g->sector_size;
    } else {
        /* Double-sided: alternating sides (track 0 side 0, track 0 side 1, ...) */
        offset = (size_t)track * g->sides * g->sectors_per_track * g->sector_size +
                 (size_t)side * g->sectors_per_track * g->sector_size +
                 (size_t)sector * g->sector_size;
    }
    
    return offset;
}

uft_trsdos_err_t uft_trsdos_read_sector(const uft_trsdos_ctx_t *ctx,
                                         uint8_t track, uint8_t side,
                                         uint8_t sector,
                                         uint8_t *buffer, size_t size) {
    if (!ctx || !ctx->data || !buffer) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    size_t offset = calc_sector_offset(ctx, track, side, sector);
    if (offset == SIZE_MAX) {
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    size_t read_size = (size < ctx->geometry.sector_size) ? size : ctx->geometry.sector_size;
    
    if (offset + read_size > ctx->size) {
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    memcpy(buffer, ctx->data + offset, read_size);
    return UFT_TRSDOS_OK;
}

uft_trsdos_err_t uft_trsdos_write_sector(uft_trsdos_ctx_t *ctx,
                                          uint8_t track, uint8_t side,
                                          uint8_t sector,
                                          const uint8_t *data, size_t size) {
    if (!ctx || !ctx->data || !data) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    size_t offset = calc_sector_offset(ctx, track, side, sector);
    if (offset == SIZE_MAX) {
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    size_t write_size = (size < ctx->geometry.sector_size) ? size : ctx->geometry.sector_size;
    
    if (offset + write_size > ctx->size) {
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    memcpy(ctx->data + offset, data, write_size);
    ctx->modified = true;
    return UFT_TRSDOS_OK;
}

/*===========================================================================
 * GAT Operations
 *===========================================================================*/

/**
 * @brief Read GAT sector for TRSDOS 2.3
 */
static uft_trsdos_err_t read_gat_trsdos23(uft_trsdos_ctx_t *ctx) {
    uint8_t sector[256];
    
    /* GAT is at track 17, sector 0 */
    uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, 0, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) {
        return err;
    }
    
    /* GAT format: first byte is allocation code, rest is bitmap */
    /* Bytes 0-47: GAT entries (one per track except dir track) */
    /* Each byte: bits indicate free granules on track */
    
    memset(&ctx->gat, 0, sizeof(ctx->gat));
    
    uint16_t free_count = 0;
    uint8_t granule_num = 0;
    
    for (int track = 0; track < (int)ctx->geometry.tracks; track++) {
        if (track == (int)ctx->dir_track) continue;
        
        uint8_t gat_byte = sector[track];
        
        /* For TRSDOS 2.3, each track has 2 granules (5 sectors each) */
        /* Bit set = granule in use */
        for (int g = 0; g < 2 && granule_num < UFT_TRSDOS_MAX_GRANULES; g++) {
            bool in_use = (gat_byte & (1 << g)) != 0;
            ctx->gat.raw[granule_num] = in_use ? 0xFF : 0x00;
            if (!in_use) free_count++;
            granule_num++;
        }
    }
    
    ctx->gat.total_granules = granule_num;
    ctx->gat.free_granules = free_count;
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Read GAT sector for TRSDOS 6/LDOS
 */
static uft_trsdos_err_t read_gat_trsdos6(uft_trsdos_ctx_t *ctx) {
    uint8_t sector[256];
    
    /* GAT is at track 17, sector 1 */
    uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, 1, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) {
        return err;
    }
    
    memset(&ctx->gat, 0, sizeof(ctx->gat));
    
    /* TRSDOS 6 GAT format:
     * Bytes 0x00-0x5F: Allocation bitmap
     * Each byte covers 8 granules, bit=1 means in use
     */
    
    uint16_t free_count = 0;
    uint16_t total = ctx->geometry.total_granules;
    
    for (uint16_t i = 0; i < total && i < UFT_TRSDOS_MAX_GRANULES; i++) {
        uint8_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        
        bool in_use = (sector[byte_idx] & (1 << bit_idx)) != 0;
        ctx->gat.raw[i] = in_use ? 0xFF : 0x00;
        if (!in_use) free_count++;
    }
    
    ctx->gat.total_granules = total;
    ctx->gat.free_granules = free_count;
    
    /* Read lockout table if present */
    memcpy(ctx->gat.lockout_table, &sector[0xCE], 16);
    
    return UFT_TRSDOS_OK;
}

/**
 * @brief Read FAT for RS-DOS (CoCo)
 */
static uft_trsdos_err_t read_gat_rsdos(uft_trsdos_ctx_t *ctx) {
    uint8_t sector[256];
    
    /* FAT is at track 17, sector 2 (sector numbering starts at 1, so index 1) */
    uft_trsdos_err_t err = uft_trsdos_read_sector(ctx, 17, 0, 1, sector, sizeof(sector));
    if (err != UFT_TRSDOS_OK) {
        return err;
    }
    
    memset(&ctx->gat, 0, sizeof(ctx->gat));
    
    /* RS-DOS FAT format:
     * 68 bytes for 68 granules
     * Each byte: next granule in chain (0xFF = last, 0x00 = free)
     * High nibble of last granule = sectors used in last granule
     */
    
    uint16_t free_count = 0;
    uint16_t total = 68; /* Standard RS-DOS has 68 granules */
    
    for (uint16_t i = 0; i < total; i++) {
        uint8_t val = sector[i];
        
        if (val == 0x00) {
            ctx->gat.raw[i] = 0x00; /* Free */
            free_count++;
        } else {
            ctx->gat.raw[i] = val; /* Store chain pointer */
        }
    }
    
    ctx->gat.total_granules = total;
    ctx->gat.free_granules = free_count;
    
    return UFT_TRSDOS_OK;
}

uft_trsdos_err_t uft_trsdos_read_gat(uft_trsdos_ctx_t *ctx) {
    if (!ctx || !ctx->data) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    switch (ctx->version) {
        case UFT_TRSDOS_VERSION_23:
            return read_gat_trsdos23(ctx);
            
        case UFT_TRSDOS_VERSION_6:
        case UFT_TRSDOS_VERSION_LDOS5:
        case UFT_TRSDOS_VERSION_NEWDOS80:
            return read_gat_trsdos6(ctx);
            
        case UFT_TRSDOS_VERSION_RSDOS:
            return read_gat_rsdos(ctx);
            
        default:
            /* Try TRSDOS 6 format as default */
            return read_gat_trsdos6(ctx);
    }
}

uft_trsdos_err_t uft_trsdos_write_gat(uft_trsdos_ctx_t *ctx) {
    if (!ctx || !ctx->data) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!ctx->writable) {
        return UFT_TRSDOS_ERR_READONLY;
    }
    
    uint8_t sector[256];
    memset(sector, 0, sizeof(sector));
    
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        /* RS-DOS FAT */
        memcpy(sector, ctx->gat.raw, 68);
        return uft_trsdos_write_sector(ctx, 17, 0, 1, sector, sizeof(sector));
    }
    
    /* TRSDOS 6/LDOS format */
    uint16_t total = ctx->gat.total_granules;
    
    for (uint16_t i = 0; i < total && i < UFT_TRSDOS_MAX_GRANULES; i++) {
        uint8_t byte_idx = i / 8;
        uint8_t bit_idx = i % 8;
        
        if (ctx->gat.raw[i] != 0x00) {
            sector[byte_idx] |= (1 << bit_idx);
        }
    }
    
    /* Copy lockout table */
    memcpy(&sector[0xCE], ctx->gat.lockout_table, 16);
    
    return uft_trsdos_write_sector(ctx, 17, 0, 1, sector, sizeof(sector));
}

bool uft_trsdos_granule_allocated(const uft_trsdos_ctx_t *ctx, uint8_t granule) {
    if (!ctx || granule >= UFT_TRSDOS_MAX_GRANULES) {
        return true; /* Assume allocated if invalid */
    }
    
    return ctx->gat.raw[granule] != 0x00;
}

uint8_t uft_trsdos_alloc_granule(uft_trsdos_ctx_t *ctx) {
    if (!ctx) return 0xFF;
    
    for (uint16_t i = 0; i < ctx->gat.total_granules; i++) {
        if (ctx->gat.raw[i] == 0x00) {
            ctx->gat.raw[i] = 0xFE; /* Mark as last in chain */
            ctx->gat.free_granules--;
            return (uint8_t)i;
        }
    }
    
    return 0xFF; /* Disk full */
}

void uft_trsdos_free_granule(uft_trsdos_ctx_t *ctx, uint8_t granule) {
    if (!ctx || granule >= UFT_TRSDOS_MAX_GRANULES) return;
    
    if (ctx->gat.raw[granule] != 0x00) {
        ctx->gat.raw[granule] = 0x00;
        ctx->gat.free_granules++;
    }
}

uint16_t uft_trsdos_free_granules(const uft_trsdos_ctx_t *ctx) {
    if (!ctx) return 0;
    return ctx->gat.free_granules;
}

uint32_t uft_trsdos_free_space(const uft_trsdos_ctx_t *ctx) {
    if (!ctx) return 0;
    
    return (uint32_t)ctx->gat.free_granules * 
           ctx->geometry.granule_sectors * 
           ctx->geometry.sector_size;
}

uft_trsdos_err_t uft_trsdos_granule_to_ts(const uft_trsdos_ctx_t *ctx,
                                           uint8_t granule,
                                           uint8_t *track, uint8_t *first_sector) {
    if (!ctx || !track || !first_sector) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (granule >= ctx->gat.total_granules) {
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    /* Calculate track and sector from granule number */
    uint8_t granules_per_track;
    
    if (ctx->version == UFT_TRSDOS_VERSION_RSDOS) {
        /* RS-DOS: 2 granules per track (9 sectors each) */
        granules_per_track = 2;
        *track = granule / granules_per_track;
        
        /* Skip directory track */
        if (*track >= ctx->dir_track) {
            (*track)++;
        }
        
        uint8_t gran_in_track = granule % granules_per_track;
        *first_sector = gran_in_track * ctx->geometry.granule_sectors;
    } else {
        /* TRSDOS: 2 granules per track (5 sectors each) */
        granules_per_track = 2;
        *track = granule / granules_per_track;
        
        /* Skip directory track */
        if (*track >= ctx->dir_track) {
            (*track)++;
        }
        
        uint8_t gran_in_track = granule % granules_per_track;
        *first_sector = gran_in_track * ctx->geometry.granule_sectors;
    }
    
    return UFT_TRSDOS_OK;
}

/*===========================================================================
 * Detection
 *===========================================================================*/

/**
 * @brief Detect TRSDOS 2.3 format
 */
static bool detect_trsdos23(const uint8_t *data, size_t size, uint8_t *confidence) {
    if (size < 89600) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    /* TRSDOS 2.3: Check GAT at track 17, sector 0 */
    size_t gat_offset = 17 * 10 * 256; /* Track 17, sector 0 */
    
    if (gat_offset + 256 > size) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    const uint8_t *gat = data + gat_offset;
    
    /* First byte should be password or 0x00 */
    /* Check for reasonable GAT values */
    uint8_t conf = 50;
    
    /* Check directory at track 17, sector 1-9 */
    size_t dir_offset = gat_offset + 256;
    const uint8_t *dir = data + dir_offset;
    
    /* First directory entry should not be 0xE5 (CP/M deleted marker) */
    if (dir[0] == 0xE5) {
        conf -= 20;
    }
    
    /* Look for valid TRSDOS filenames */
    int valid_entries = 0;
    for (int i = 0; i < 9 * 5; i++) { /* 9 sectors, ~5 entries per sector */
        const uint8_t *entry = dir + i * 48;
        
        /* Check if valid entry (attribute byte not 0x00 or 0xFF) */
        if (entry[0] != 0x00 && entry[0] != 0xFF) {
            /* Check filename (bytes 8-15) */
            bool valid_name = true;
            for (int j = 8; j < 16; j++) {
                uint8_t c = entry[j];
                if (c != 0x20 && (c < 0x21 || c > 0x7E)) {
                    valid_name = false;
                    break;
                }
            }
            if (valid_name) valid_entries++;
        }
    }
    
    if (valid_entries > 0) conf += 20;
    if (valid_entries > 3) conf += 10;
    
    if (confidence) *confidence = (conf > 100) ? 100 : conf;
    return conf >= 50;
}

/**
 * @brief Detect TRSDOS 6/LDOS format
 */
static bool detect_trsdos6(const uint8_t *data, size_t size, uint8_t *confidence) {
    if (size < 184320) { /* Minimum for 40-track DD */
        if (confidence) *confidence = 0;
        return false;
    }
    
    /* TRSDOS 6: Check boot sector signature */
    /* Track 0, Sector 0 should have boot code */
    
    uint8_t conf = 40;
    
    /* Check for JMP instruction at start (common boot pattern) */
    if (data[0] == 0xC3 || data[0] == 0x18) {
        conf += 20;
    }
    
    /* Check GAT at track 17, sector 1 */
    size_t spt = 18; /* Assume DD */
    size_t gat_offset = 17 * spt * 256 + 256;
    
    if (gat_offset + 256 <= size) {
        const uint8_t *gat = data + gat_offset;
        
        /* GAT should have some bits set */
        int set_bits = 0;
        for (int i = 0; i < 96; i++) {
            for (int b = 0; b < 8; b++) {
                if (gat[i] & (1 << b)) set_bits++;
            }
        }
        
        if (set_bits > 10 && set_bits < 700) conf += 20;
    }
    
    /* Check HIT at track 17, sector 2 */
    size_t hit_offset = gat_offset + 256;
    if (hit_offset + 256 <= size) {
        const uint8_t *hit = data + hit_offset;
        
        /* HIT should have hash values */
        int nonzero = 0;
        for (int i = 0; i < 256; i++) {
            if (hit[i] != 0x00 && hit[i] != 0xFF) nonzero++;
        }
        
        if (nonzero > 5) conf += 15;
    }
    
    if (confidence) *confidence = (conf > 100) ? 100 : conf;
    return conf >= 50;
}

/**
 * @brief Detect RS-DOS format (CoCo)
 */
static bool detect_rsdos(const uint8_t *data, size_t size, uint8_t *confidence) {
    if (size < 161280) { /* 35×18×256 */
        if (confidence) *confidence = 0;
        return false;
    }
    
    uint8_t conf = 30;
    
    /* RS-DOS: Directory at track 17, sector 3-11 (sectors numbered 1-18) */
    /* FAT at track 17, sector 2 */
    size_t spt = 18;
    size_t dir_offset = 17 * spt * 256 + 2 * 256; /* Track 17, sector 3 */
    
    if (dir_offset + 256 > size) {
        if (confidence) *confidence = 0;
        return false;
    }
    
    const uint8_t *dir = data + dir_offset;
    
    /* Check for valid RS-DOS directory entries */
    int valid_entries = 0;
    
    for (int i = 0; i < 8; i++) { /* 8 entries per sector */
        const uft_rsdos_dir_entry_t *entry = (const uft_rsdos_dir_entry_t *)(dir + i * 32);
        
        /* Check filename - should be printable or spaces */
        bool valid_name = true;
        bool has_chars = false;
        
        for (int j = 0; j < 8; j++) {
            uint8_t c = entry->name[j];
            if (c == 0x00 || c == 0xFF) {
                /* Empty or deleted entry - skip rest of check */
                valid_name = false;
                break;
            }
            if (c != 0x20 && (c < 0x21 || c > 0x7A)) {
                valid_name = false;
                break;
            }
            if (c != 0x20) has_chars = true;
        }
        
        if (valid_name && has_chars) {
            /* Check file type (should be 0-3) */
            if (entry->file_type <= 3) {
                valid_entries++;
            }
        }
    }
    
    if (valid_entries > 0) conf += 30;
    if (valid_entries > 2) conf += 20;
    
    /* Check FAT */
    size_t fat_offset = 17 * spt * 256 + 1 * 256;
    if (fat_offset + 68 <= size) {
        const uint8_t *fat = data + fat_offset;
        
        /* FAT should have valid granule pointers */
        int valid_fat = 0;
        for (int i = 0; i < 68; i++) {
            uint8_t val = fat[i];
            /* Valid: 0x00 (free), 0xC0-0xC9 (last), or granule number < 68 */
            if (val == 0x00 || (val >= 0xC0 && val <= 0xC9) || val < 68) {
                valid_fat++;
            }
        }
        
        if (valid_fat > 60) conf += 15;
    }
    
    if (confidence) *confidence = (conf > 100) ? 100 : conf;
    return conf >= 50;
}

uft_trsdos_err_t uft_trsdos_detect(const uint8_t *data, size_t size,
                                    uft_trsdos_detect_t *result) {
    if (!data || !result) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    memset(result, 0, sizeof(*result));
    
    /* Try each format */
    uint8_t conf_23 = 0, conf_6 = 0, conf_rsdos = 0;
    
    detect_trsdos23(data, size, &conf_23);
    detect_trsdos6(data, size, &conf_6);
    detect_rsdos(data, size, &conf_rsdos);
    
    /* Pick best match */
    if (conf_rsdos >= conf_23 && conf_rsdos >= conf_6 && conf_rsdos >= 50) {
        result->valid = true;
        result->version = UFT_TRSDOS_VERSION_RSDOS;
        result->confidence = conf_rsdos;
        result->description = "RS-DOS / Disk BASIC";
    } else if (conf_6 >= conf_23 && conf_6 >= 50) {
        result->valid = true;
        result->version = UFT_TRSDOS_VERSION_6;
        result->confidence = conf_6;
        result->description = "TRSDOS 6.x / LDOS compatible";
    } else if (conf_23 >= 50) {
        result->valid = true;
        result->version = UFT_TRSDOS_VERSION_23;
        result->confidence = conf_23;
        result->description = "TRSDOS 2.3 (Model I)";
    } else {
        result->valid = false;
        result->confidence = 0;
        result->description = "Not a recognized TRS-80 DOS format";
        return UFT_TRSDOS_ERR_NOT_TRSDOS;
    }
    
    /* Detect geometry */
    result->geometry = uft_trsdos_detect_geometry(size, NULL);
    
    return UFT_TRSDOS_OK;
}

/*===========================================================================
 * Open/Close
 *===========================================================================*/

uft_trsdos_err_t uft_trsdos_open(uft_trsdos_ctx_t *ctx,
                                  const uint8_t *data, size_t size,
                                  bool copy, bool writable) {
    if (!ctx || !data || size == 0) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Detect filesystem */
    uft_trsdos_detect_t detect;
    uft_trsdos_err_t err = uft_trsdos_detect(data, size, &detect);
    if (err != UFT_TRSDOS_OK) {
        return err;
    }
    
    return uft_trsdos_open_as(ctx, data, size, detect.version, detect.geometry,
                              copy, writable);
}

uft_trsdos_err_t uft_trsdos_open_as(uft_trsdos_ctx_t *ctx,
                                     const uint8_t *data, size_t size,
                                     uft_trsdos_version_t version,
                                     uft_trsdos_geom_type_t geom,
                                     bool copy, bool writable) {
    if (!ctx || !data || size == 0) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    /* Close any existing image */
    uft_trsdos_close(ctx);
    
    /* Copy or reference data */
    if (copy) {
        ctx->data = malloc(size);
        if (!ctx->data) {
            return UFT_TRSDOS_ERR_NOMEM;
        }
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = (uint8_t *)data;
        ctx->owns_data = false;
    }
    
    ctx->size = size;
    ctx->writable = writable && copy;
    ctx->modified = false;
    ctx->version = version;
    
    /* Set geometry */
    const uft_trsdos_geometry_t *g = uft_trsdos_get_geometry(geom);
    memcpy(&ctx->geometry, g, sizeof(ctx->geometry));
    
    ctx->dir_track = g->dir_track;
    
    /* Calculate directory info */
    if (version == UFT_TRSDOS_VERSION_23) {
        ctx->dir_sectors = 9; /* Sectors 1-9 */
        ctx->dir_entries_max = ctx->dir_sectors * 5; /* ~5 entries per sector */
    } else if (version == UFT_TRSDOS_VERSION_RSDOS) {
        ctx->dir_sectors = 9; /* Sectors 3-11 */
        ctx->dir_entries_max = ctx->dir_sectors * 8; /* 8 entries per sector */
    } else {
        ctx->dir_sectors = 8; /* TRSDOS 6: sectors 1-8 */
        ctx->dir_entries_max = ctx->dir_sectors * 8;
    }
    
    /* Read GAT */
    uft_trsdos_err_t err = uft_trsdos_read_gat(ctx);
    if (err != UFT_TRSDOS_OK) {
        uft_trsdos_close(ctx);
        return err;
    }
    
    return UFT_TRSDOS_OK;
}

uft_trsdos_err_t uft_trsdos_save(uft_trsdos_ctx_t *ctx,
                                  uint8_t *data, size_t *size) {
    if (!ctx || !size) {
        return UFT_TRSDOS_ERR_NULL;
    }
    
    if (!data) {
        /* Query size only */
        *size = ctx->size;
        return UFT_TRSDOS_OK;
    }
    
    if (*size < ctx->size) {
        return UFT_TRSDOS_ERR_RANGE;
    }
    
    /* Write GAT if modified */
    if (ctx->modified) {
        uft_trsdos_err_t err = uft_trsdos_write_gat(ctx);
        if (err != UFT_TRSDOS_OK) {
            return err;
        }
    }
    
    memcpy(data, ctx->data, ctx->size);
    *size = ctx->size;
    
    return UFT_TRSDOS_OK;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

bool uft_trsdos_parse_filename(const char *input, char *name, char *ext) {
    if (!input || !name || !ext) return false;
    
    /* Initialize with spaces */
    memset(name, ' ', UFT_TRSDOS_MAX_NAME);
    memset(ext, ' ', UFT_TRSDOS_MAX_EXT);
    name[UFT_TRSDOS_MAX_NAME] = '\0';
    ext[UFT_TRSDOS_MAX_EXT] = '\0';
    
    /* Find separator (. or /) */
    const char *sep = strchr(input, '.');
    if (!sep) sep = strchr(input, '/');
    
    size_t name_len;
    if (sep) {
        name_len = sep - input;
        size_t ext_len = strlen(sep + 1);
        if (ext_len > UFT_TRSDOS_MAX_EXT) ext_len = UFT_TRSDOS_MAX_EXT;
        
        for (size_t i = 0; i < ext_len; i++) {
            ext[i] = toupper((unsigned char)sep[1 + i]);
        }
    } else {
        name_len = strlen(input);
    }
    
    if (name_len > UFT_TRSDOS_MAX_NAME) name_len = UFT_TRSDOS_MAX_NAME;
    
    for (size_t i = 0; i < name_len; i++) {
        name[i] = toupper((unsigned char)input[i]);
    }
    
    return true;
}

void uft_trsdos_format_filename(const char *name, const char *ext, char *buffer) {
    if (!buffer) return;
    
    char n[UFT_TRSDOS_MAX_NAME + 1] = {0};
    char e[UFT_TRSDOS_MAX_EXT + 1] = {0};
    
    /* Trim trailing spaces */
    if (name) {
        strncpy(n, name, UFT_TRSDOS_MAX_NAME);
        for (int i = UFT_TRSDOS_MAX_NAME - 1; i >= 0 && n[i] == ' '; i--) {
            n[i] = '\0';
        }
    }
    
    if (ext) {
        strncpy(e, ext, UFT_TRSDOS_MAX_EXT);
        for (int i = UFT_TRSDOS_MAX_EXT - 1; i >= 0 && e[i] == ' '; i--) {
            e[i] = '\0';
        }
    }
    
    if (e[0]) {
        snprintf(buffer, 16, "%s/%s", n, e);
    } else {
        snprintf(buffer, 16, "%s", n);
    }
}

bool uft_trsdos_valid_filename(const char *name) {
    if (!name || !*name) return false;
    
    size_t len = strlen(name);
    if (len > UFT_TRSDOS_MAX_NAME) return false;
    
    for (size_t i = 0; i < len; i++) {
        unsigned char c = name[i];
        /* Valid: A-Z, 0-9, some punctuation */
        if (!isalnum(c) && c != '_' && c != '-' && c != ' ') {
            return false;
        }
    }
    
    return true;
}

void uft_trsdos_hash_password(const char *password, uint8_t *hash) {
    if (!hash) return;
    
    hash[0] = 0;
    hash[1] = 0;
    
    if (!password || !*password) return;
    
    /* Simple TRSDOS password hash */
    uint16_t h = 0;
    while (*password) {
        h = (h << 1) ^ (uint8_t)toupper(*password);
        password++;
    }
    
    hash[0] = h & 0xFF;
    hash[1] = (h >> 8) & 0xFF;
}

bool uft_trsdos_verify_password(const char *password, const uint8_t *hash) {
    uint8_t calc[2];
    uft_trsdos_hash_password(password, calc);
    return (calc[0] == hash[0] && calc[1] == hash[1]);
}

const char *uft_trsdos_version_name(uft_trsdos_version_t version) {
    if (version < sizeof(s_version_names)/sizeof(s_version_names[0])) {
        return s_version_names[version];
    }
    return "Unknown";
}

const char *uft_trsdos_strerror(uft_trsdos_err_t err) {
    if (err < 0 || (size_t)err >= sizeof(s_error_messages)/sizeof(s_error_messages[0])) {
        return "Unknown error";
    }
    return s_error_messages[err];
}

bool uft_trsdos_is_rsdos(const uft_trsdos_ctx_t *ctx) {
    return ctx && ctx->version == UFT_TRSDOS_VERSION_RSDOS;
}
