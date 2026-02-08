/**
 * @file uft_ti99_fs.c
 * @brief TI-99/4A Disk Manager Filesystem Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/fs/uft_ti99_fs.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*===========================================================================
 * Internal Context Structure
 *===========================================================================*/

struct uft_ti99_ctx {
    /* Image data */
    uint8_t            *data;
    size_t              data_size;
    bool                owns_data;
    bool                modified;
    
    /* Detected parameters */
    uft_ti99_format_t   format;
    uft_ti99_geometry_t geometry;
    
    /* VIB cache */
    uft_ti99_vib_t      vib;
    bool                vib_loaded;
    bool                vib_dirty;
    
    /* State */
    bool                open;
};

/*===========================================================================
 * Geometry Presets
 *===========================================================================*/

static const uft_ti99_geometry_t g_geometries[] = {
    [UFT_TI99_FORMAT_UNKNOWN] = {0},
    [UFT_TI99_FORMAT_SSSD] = {
        .tracks = 40, .sides = 1, .sectors_per_track = 9,
        .total_sectors = 360, .total_bytes = 92160,
        .density = 1, .format = UFT_TI99_FORMAT_SSSD
    },
    [UFT_TI99_FORMAT_SSDD] = {
        .tracks = 40, .sides = 1, .sectors_per_track = 18,
        .total_sectors = 720, .total_bytes = 184320,
        .density = 2, .format = UFT_TI99_FORMAT_SSDD
    },
    [UFT_TI99_FORMAT_DSDD] = {
        .tracks = 40, .sides = 2, .sectors_per_track = 18,
        .total_sectors = 1440, .total_bytes = 368640,
        .density = 2, .format = UFT_TI99_FORMAT_DSDD
    },
    [UFT_TI99_FORMAT_DSQD] = {
        .tracks = 80, .sides = 2, .sectors_per_track = 18,
        .total_sectors = 2880, .total_bytes = 737280,
        .density = 2, .format = UFT_TI99_FORMAT_DSQD
    },
    [UFT_TI99_FORMAT_DSHD] = {
        .tracks = 80, .sides = 2, .sectors_per_track = 36,
        .total_sectors = 5760, .total_bytes = 1474560,
        .density = 3, .format = UFT_TI99_FORMAT_DSHD
    }
};

/*===========================================================================
 * Name Tables
 *===========================================================================*/

static const char *g_format_names[] = {
    [UFT_TI99_FORMAT_UNKNOWN] = "Unknown",
    [UFT_TI99_FORMAT_SSSD]    = "SSSD (90KB)",
    [UFT_TI99_FORMAT_SSDD]    = "SSDD (180KB)",
    [UFT_TI99_FORMAT_DSDD]    = "DSDD (360KB)",
    [UFT_TI99_FORMAT_DSQD]    = "DSQD (720KB)",
    [UFT_TI99_FORMAT_DSHD]    = "DSHD (1.44MB)"
};

static const char *g_type_names[] = {
    [UFT_TI99_TYPE_DIS_FIX] = "DIS/FIX",
    [UFT_TI99_TYPE_DIS_VAR] = "DIS/VAR",
    [UFT_TI99_TYPE_INT_FIX] = "INT/FIX",
    [UFT_TI99_TYPE_INT_VAR] = "INT/VAR",
    [UFT_TI99_TYPE_PROGRAM] = "PROGRAM"
};

static const char *g_error_strings[] = {
    [UFT_TI99_OK]           = "Success",
    [UFT_TI99_ERR_PARAM]    = "Invalid parameter",
    [UFT_TI99_ERR_MEMORY]   = "Out of memory",
    [UFT_TI99_ERR_FORMAT]   = "Invalid format",
    [UFT_TI99_ERR_READ]     = "Read error",
    [UFT_TI99_ERR_WRITE]    = "Write error",
    [UFT_TI99_ERR_SECTOR]   = "Sector out of range",
    [UFT_TI99_ERR_VIB]      = "VIB error",
    [UFT_TI99_ERR_NOTFOUND] = "File not found",
    [UFT_TI99_ERR_EXISTS]   = "File already exists",
    [UFT_TI99_ERR_FULL]     = "Disk full",
    [UFT_TI99_ERR_DIRFULL]  = "Directory full",
    [UFT_TI99_ERR_PROTECTED]= "Protected",
    [UFT_TI99_ERR_CORRUPT]  = "Data corrupted",
    [UFT_TI99_ERR_CHAIN]    = "Bad data chain",
    [UFT_TI99_ERR_NOT_OPEN] = "Not open"
};

/*===========================================================================
 * Big-Endian Helpers
 *===========================================================================*/

static uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

static void write_be16(uint8_t *p, uint16_t val)
{
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val & 0xFF);
}

/*===========================================================================
 * Context Lifecycle
 *===========================================================================*/

uft_ti99_ctx_t *uft_ti99_create(void)
{
    return calloc(1, sizeof(uft_ti99_ctx_t));
}

void uft_ti99_destroy(uft_ti99_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    free(ctx);
}

void uft_ti99_close(uft_ti99_ctx_t *ctx)
{
    if (!ctx) return;
    
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    memset(ctx, 0, sizeof(*ctx));
}

uft_ti99_error_t uft_ti99_save(uft_ti99_ctx_t *ctx, const char *path)
{
    if (!ctx || !path || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Flush VIB if dirty */
    if (ctx->vib_dirty) {
        uft_ti99_write_vib(ctx, &ctx->vib);
    }
    
    FILE *f = fopen(path, "wb");
    if (!f) {
        return UFT_TI99_ERR_WRITE;
    }
    
    size_t written = fwrite(ctx->data, 1, ctx->data_size, f);
    fclose(f);
    
    if (written != ctx->data_size) {
        return UFT_TI99_ERR_WRITE;
    }
    
    ctx->modified = false;
    return UFT_TI99_OK;
}

/*===========================================================================
 * Sector I/O
 *===========================================================================*/

uft_ti99_error_t uft_ti99_read_sector(uft_ti99_ctx_t *ctx,
                                       uint16_t sector,
                                       uint8_t *buffer)
{
    if (!ctx || !buffer || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    if (sector >= ctx->geometry.total_sectors) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    size_t offset = (size_t)sector * UFT_TI99_SECTOR_SIZE;
    if (offset + UFT_TI99_SECTOR_SIZE > ctx->data_size) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    memcpy(buffer, ctx->data + offset, UFT_TI99_SECTOR_SIZE);
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_write_sector(uft_ti99_ctx_t *ctx,
                                        uint16_t sector,
                                        const uint8_t *buffer)
{
    if (!ctx || !buffer || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    if (sector >= ctx->geometry.total_sectors) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    size_t offset = (size_t)sector * UFT_TI99_SECTOR_SIZE;
    if (offset + UFT_TI99_SECTOR_SIZE > ctx->data_size) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    memcpy(ctx->data + offset, buffer, UFT_TI99_SECTOR_SIZE);
    ctx->modified = true;
    
    return UFT_TI99_OK;
}

/*===========================================================================
 * VIB Operations
 *===========================================================================*/

uft_ti99_error_t uft_ti99_read_vib(uft_ti99_ctx_t *ctx, uft_ti99_vib_t *vib)
{
    if (!ctx || !vib || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    uint8_t sector[UFT_TI99_SECTOR_SIZE];
    uft_ti99_error_t err = uft_ti99_read_sector(ctx, 0, sector);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    memcpy(vib, sector, sizeof(*vib));
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_write_vib(uft_ti99_ctx_t *ctx, const uft_ti99_vib_t *vib)
{
    if (!ctx || !vib || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    uint8_t sector[UFT_TI99_SECTOR_SIZE];
    memcpy(sector, vib, sizeof(*vib));
    
    uft_ti99_error_t err = uft_ti99_write_sector(ctx, 0, sector);
    if (err == UFT_TI99_OK) {
        ctx->vib_dirty = false;
    }
    
    return err;
}

/*===========================================================================
 * Bitmap Operations
 *===========================================================================*/

bool uft_ti99_is_sector_free(uft_ti99_ctx_t *ctx, uint16_t sector)
{
    if (!ctx || !ctx->vib_loaded) {
        return false;
    }
    
    if (sector >= ctx->geometry.total_sectors) {
        return false;
    }
    
    /* Bitmap: bit set = allocated, bit clear = free */
    /* Byte 0 bit 0 = sector 0, byte 0 bit 7 = sector 7 */
    uint16_t byte_idx = sector / 8;
    uint8_t bit_mask = 1 << (sector % 8);
    
    if (byte_idx >= sizeof(ctx->vib.bitmap)) {
        return false;
    }
    
    return (ctx->vib.bitmap[byte_idx] & bit_mask) == 0;
}

uft_ti99_error_t uft_ti99_allocate_sector(uft_ti99_ctx_t *ctx, uint16_t sector)
{
    if (!ctx || !ctx->vib_loaded) {
        return UFT_TI99_ERR_PARAM;
    }
    
    if (sector >= ctx->geometry.total_sectors) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    uint16_t byte_idx = sector / 8;
    uint8_t bit_mask = 1 << (sector % 8);
    
    if (byte_idx >= sizeof(ctx->vib.bitmap)) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    ctx->vib.bitmap[byte_idx] |= bit_mask;  /* Set bit = allocated */
    ctx->vib_dirty = true;
    
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_free_sector(uft_ti99_ctx_t *ctx, uint16_t sector)
{
    if (!ctx || !ctx->vib_loaded) {
        return UFT_TI99_ERR_PARAM;
    }
    
    if (sector >= ctx->geometry.total_sectors) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    uint16_t byte_idx = sector / 8;
    uint8_t bit_mask = 1 << (sector % 8);
    
    if (byte_idx >= sizeof(ctx->vib.bitmap)) {
        return UFT_TI99_ERR_SECTOR;
    }
    
    ctx->vib.bitmap[byte_idx] &= ~bit_mask;  /* Clear bit = free */
    ctx->vib_dirty = true;
    
    return UFT_TI99_OK;
}

uint16_t uft_ti99_find_free_sector(uft_ti99_ctx_t *ctx)
{
    if (!ctx || !ctx->vib_loaded) {
        return 0;
    }
    
    /* Start after FDIR sectors */
    uint16_t start = UFT_TI99_FDIR_START + UFT_TI99_FDIR_COUNT;
    
    for (uint16_t s = start; s < ctx->geometry.total_sectors; s++) {
        if (uft_ti99_is_sector_free(ctx, s)) {
            return s;
        }
    }
    
    return 0;  /* No free sector */
}

uint16_t uft_ti99_free_sectors(uft_ti99_ctx_t *ctx)
{
    if (!ctx || !ctx->vib_loaded) {
        return 0;
    }
    
    uint16_t count = 0;
    for (uint16_t s = 0; s < ctx->geometry.total_sectors; s++) {
        if (uft_ti99_is_sector_free(ctx, s)) {
            count++;
        }
    }
    
    return count;
}

/*===========================================================================
 * Detection
 *===========================================================================*/

static uft_ti99_format_t detect_format_from_vib(const uft_ti99_vib_t *vib, size_t file_size)
{
    /* Check DSK signature */
    if (memcmp(vib->dsk_id, UFT_TI99_VIB_DSK_ID, 3) != 0) {
        /* Try detecting from size alone */
        if (file_size == UFT_TI99_SIZE_SSSD) return UFT_TI99_FORMAT_SSSD;
        if (file_size == UFT_TI99_SIZE_SSDD) return UFT_TI99_FORMAT_SSDD;
        if (file_size == UFT_TI99_SIZE_DSDD) return UFT_TI99_FORMAT_DSDD;
        if (file_size == UFT_TI99_SIZE_DSQD) return UFT_TI99_FORMAT_DSQD;
        return UFT_TI99_FORMAT_UNKNOWN;
    }
    
    /* Use VIB parameters */
    uint8_t sides = vib->sides ? vib->sides : 1;
    uint8_t tracks = vib->tracks_per_side ? vib->tracks_per_side : 40;
    uint8_t spt = vib->sectors_per_track ? vib->sectors_per_track : 9;
    
    if (sides == 1 && spt <= 9) {
        return UFT_TI99_FORMAT_SSSD;
    } else if (sides == 1 && spt <= 18) {
        return UFT_TI99_FORMAT_SSDD;
    } else if (sides == 2 && tracks <= 40) {
        return UFT_TI99_FORMAT_DSDD;
    } else if (sides == 2 && tracks <= 80 && spt <= 18) {
        return UFT_TI99_FORMAT_DSQD;
    } else if (sides == 2 && spt > 18) {
        return UFT_TI99_FORMAT_DSHD;
    }
    
    return UFT_TI99_FORMAT_DSDD;  /* Default */
}

uft_ti99_error_t uft_ti99_detect(const uint8_t *data,
                                  size_t size,
                                  uft_ti99_detect_result_t *result)
{
    if (!data || !result) {
        return UFT_TI99_ERR_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    
    if (size < UFT_TI99_SIZE_SSSD) {
        return UFT_TI99_ERR_FORMAT;
    }
    
    /* Read VIB */
    const uft_ti99_vib_t *vib = (const uft_ti99_vib_t *)data;
    
    /* Check DSK signature */
    bool has_dsk = (memcmp(vib->dsk_id, UFT_TI99_VIB_DSK_ID, 3) == 0);
    
    result->confidence = 0;
    
    if (has_dsk) {
        result->confidence = 80;
    }
    
    /* Check total sectors matches */
    uint16_t total = read_be16((const uint8_t *)&vib->total_sectors);
    if (total > 0 && total * UFT_TI99_SECTOR_SIZE <= size) {
        result->confidence += 10;
    }
    
    /* Check sectors per track is reasonable */
    if (vib->sectors_per_track >= 9 && vib->sectors_per_track <= 36) {
        result->confidence += 5;
    }
    
    /* Check disk name has valid characters */
    bool valid_name = true;
    for (int i = 0; i < 10; i++) {
        char c = vib->disk_name[i];
        if (c != ' ' && !isprint((unsigned char)c)) {
            valid_name = false;
            break;
        }
    }
    if (valid_name) {
        result->confidence += 5;
    }
    
    /* Detect format */
    result->format = detect_format_from_vib(vib, size);
    
    if (result->format != UFT_TI99_FORMAT_UNKNOWN) {
        result->geometry = g_geometries[result->format];
    }
    
    /* Copy disk name */
    memcpy(result->disk_name, vib->disk_name, 10);
    result->disk_name[10] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 9; i >= 0 && result->disk_name[i] == ' '; i--) {
        result->disk_name[i] = '\0';
    }
    
    /* Cap confidence */
    if (result->confidence > 95) {
        result->confidence = 95;
    }
    
    result->valid = (result->confidence >= 50);
    
    return UFT_TI99_OK;
}

/*===========================================================================
 * Open
 *===========================================================================*/

uft_ti99_error_t uft_ti99_open(uft_ti99_ctx_t *ctx,
                               uint8_t *data,
                               size_t size,
                               bool copy_data)
{
    if (!ctx || !data || size == 0) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Detect format */
    uft_ti99_detect_result_t result;
    uft_ti99_error_t err = uft_ti99_detect(data, size, &result);
    if (err != UFT_TI99_OK || !result.valid) {
        return UFT_TI99_ERR_FORMAT;
    }
    
    /* Store or copy data */
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    
    if (copy_data) {
        ctx->data = malloc(size);
        if (!ctx->data) {
            return UFT_TI99_ERR_MEMORY;
        }
        memcpy(ctx->data, data, size);
        ctx->owns_data = true;
    } else {
        ctx->data = data;
        ctx->owns_data = false;
    }
    
    ctx->data_size = size;
    ctx->format = result.format;
    ctx->geometry = result.geometry;
    ctx->modified = false;
    ctx->open = true;
    
    /* Load VIB */
    err = uft_ti99_read_vib(ctx, &ctx->vib);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    ctx->vib_loaded = true;
    ctx->vib_dirty = false;
    
    return UFT_TI99_OK;
}

/*===========================================================================
 * Directory Operations
 *===========================================================================*/

/**
 * @brief Parse file type from status flags
 */
static uft_ti99_file_type_t parse_file_type(uint8_t status)
{
    if (status & UFT_TI99_FLAG_PROGRAM) {
        return UFT_TI99_TYPE_PROGRAM;
    }
    
    bool internal = (status & UFT_TI99_FLAG_INTERNAL) != 0;
    bool variable = (status & UFT_TI99_FLAG_VARIABLE) != 0;
    
    if (internal) {
        return variable ? UFT_TI99_TYPE_INT_VAR : UFT_TI99_TYPE_INT_FIX;
    } else {
        return variable ? UFT_TI99_TYPE_DIS_VAR : UFT_TI99_TYPE_DIS_FIX;
    }
}

/**
 * @brief Parse FDR into entry
 */
static void parse_fdr(const uft_ti99_fdr_t *fdr, uft_ti99_entry_t *entry,
                      uint16_t fdr_sector, uint8_t fdir_index)
{
    memset(entry, 0, sizeof(*entry));
    
    /* Copy filename */
    memcpy(entry->filename, fdr->filename, 10);
    entry->filename[10] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 9; i >= 0 && entry->filename[i] == ' '; i--) {
        entry->filename[i] = '\0';
    }
    
    entry->status_flags = fdr->status_flags;
    entry->type = parse_file_type(fdr->status_flags);
    entry->record_length = fdr->record_length;
    entry->total_sectors = read_be16((const uint8_t *)&fdr->total_sectors);
    entry->total_records = read_be16((const uint8_t *)&fdr->level3_records);
    entry->fdr_sector = fdr_sector;
    entry->fdir_index = fdir_index;
    
    entry->protected = (fdr->status_flags & UFT_TI99_FLAG_PROTECTED) != 0;
    entry->variable_length = (fdr->status_flags & UFT_TI99_FLAG_VARIABLE) != 0;
    entry->internal_format = (fdr->status_flags & UFT_TI99_FLAG_INTERNAL) != 0;
    entry->is_program = (fdr->status_flags & UFT_TI99_FLAG_PROGRAM) != 0;
    
    /* Calculate approximate file size */
    if (entry->is_program) {
        entry->file_size = (uint32_t)entry->total_sectors * UFT_TI99_SECTOR_SIZE;
    } else if (entry->record_length > 0) {
        if (entry->variable_length) {
            /* Variable: estimate based on sectors */
            entry->file_size = (uint32_t)entry->total_sectors * UFT_TI99_SECTOR_SIZE;
        } else {
            /* Fixed: records * length */
            entry->file_size = (uint32_t)entry->total_records * entry->record_length;
        }
    }
}

uft_ti99_error_t uft_ti99_read_directory(uft_ti99_ctx_t *ctx,
                                          uft_ti99_dir_t *dir)
{
    if (!ctx || !dir || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    memset(dir, 0, sizeof(*dir));
    
    /* Copy disk name */
    memcpy(dir->disk_name, ctx->vib.disk_name, 10);
    dir->disk_name[10] = '\0';
    for (int i = 9; i >= 0 && dir->disk_name[i] == ' '; i--) {
        dir->disk_name[i] = '\0';
    }
    
    dir->format = ctx->format;
    dir->total_sectors = ctx->geometry.total_sectors;
    dir->free_sectors = uft_ti99_free_sectors(ctx);
    dir->free_bytes = (uint32_t)dir->free_sectors * UFT_TI99_SECTOR_SIZE;
    
    /* Read FDIR sectors */
    uint8_t fdir_buf[UFT_TI99_SECTOR_SIZE];
    uint8_t fdr_buf[UFT_TI99_SECTOR_SIZE];
    uint8_t file_idx = 0;
    
    for (uint8_t fdir_sec = 0; fdir_sec < UFT_TI99_FDIR_COUNT; fdir_sec++) {
        uft_ti99_error_t err = uft_ti99_read_sector(ctx, 
                                                     UFT_TI99_FDIR_START + fdir_sec,
                                                     fdir_buf);
        if (err != UFT_TI99_OK) {
            continue;
        }
        
        /* Each FDIR entry is 2 bytes */
        for (int i = 0; i < UFT_TI99_FDIR_ENTRIES_PER_SECTOR && 
                        file_idx < UFT_TI99_MAX_FILES; i++) {
            const uft_ti99_fdir_entry_t *fdir_entry = 
                (const uft_ti99_fdir_entry_t *)(fdir_buf + i * 2);
            
            uint16_t fdr_sector = UFT_TI99_FDIR_SECTOR(fdir_entry);
            
            if (fdr_sector == 0) {
                continue;  /* Empty slot */
            }
            
            /* Read FDR */
            err = uft_ti99_read_sector(ctx, fdr_sector, fdr_buf);
            if (err != UFT_TI99_OK) {
                continue;
            }
            
            const uft_ti99_fdr_t *fdr = (const uft_ti99_fdr_t *)fdr_buf;
            
            /* Check if valid entry (first char not space or null) */
            if (fdr->filename[0] == ' ' || fdr->filename[0] == '\0') {
                continue;
            }
            
            parse_fdr(fdr, &dir->files[file_idx], fdr_sector, 
                      (uint8_t)(fdir_sec * UFT_TI99_FDIR_ENTRIES_PER_SECTOR + i));
            
            file_idx++;
            dir->file_count++;
        }
    }
    
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_find_file(uft_ti99_ctx_t *ctx,
                                     const char *filename,
                                     uft_ti99_entry_t *entry)
{
    if (!ctx || !filename || !entry || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Parse search filename */
    char search_name[11];
    uft_ti99_error_t err = uft_ti99_parse_filename(filename, search_name);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    /* Read directory */
    uft_ti99_dir_t dir;
    err = uft_ti99_read_directory(ctx, &dir);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    /* Search */
    for (size_t i = 0; i < dir.file_count; i++) {
        /* Compare (case-insensitive) */
        bool match = true;
        for (int j = 0; j < 10; j++) {
            char c1 = (char)toupper((unsigned char)search_name[j]);
            char c2 = (char)toupper((unsigned char)dir.files[i].filename[j]);
            if (c1 == '\0') c1 = ' ';
            if (c2 == '\0') c2 = ' ';
            if (c1 != c2) {
                match = false;
                break;
            }
        }
        
        if (match) {
            *entry = dir.files[i];
            return UFT_TI99_OK;
        }
    }
    
    return UFT_TI99_ERR_NOTFOUND;
}

uft_ti99_error_t uft_ti99_foreach_file(uft_ti99_ctx_t *ctx,
                                        uft_ti99_file_cb callback,
                                        void *user_data)
{
    if (!ctx || !callback) {
        return UFT_TI99_ERR_PARAM;
    }
    
    uft_ti99_dir_t dir;
    uft_ti99_error_t err = uft_ti99_read_directory(ctx, &dir);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    for (size_t i = 0; i < dir.file_count; i++) {
        if (!callback(&dir.files[i], user_data)) {
            break;
        }
    }
    
    return UFT_TI99_OK;
}

/*===========================================================================
 * Data Chain Parsing
 *===========================================================================*/

/**
 * @brief Parse data chain entry
 * 
 * Each entry is 3 bytes:
 * - Bits 0-11 of bytes 0-1: Start sector (big-endian with special encoding)
 * - Byte 2: Number of sectors - 1
 */
static void parse_chain_entry(const uint8_t *chain, 
                               uint16_t *start_sector,
                               uint16_t *num_sectors)
{
    /* Chain format: AUxxxx (A=start hi 4 bits, U=offset, xxxx=start lo 8 bits) */
    /* Actually: byte0=start[11:4], byte1[7:4]=start[3:0], byte1[3:0]=offset high, 
       byte2=sectors-1 */
    
    /* Simplified parsing - may vary by disk format */
    uint16_t start = ((uint16_t)chain[0] << 4) | ((chain[1] >> 4) & 0x0F);
    *start_sector = start;
    *num_sectors = (uint16_t)(chain[2] + 1);
}

/**
 * @brief Follow data chain and read file data
 */
static uft_ti99_error_t read_file_chain(uft_ti99_ctx_t *ctx,
                                         const uft_ti99_fdr_t *fdr,
                                         uint8_t *buffer,
                                         size_t buffer_size,
                                         size_t *bytes_read)
{
    size_t offset = 0;
    const uint8_t *chain = fdr->data_chain;
    
    for (int i = 0; i < UFT_TI99_MAX_CHAIN_ENTRIES; i++) {
        const uint8_t *entry = chain + (i * 3);
        
        /* Check for end of chain */
        if (entry[0] == 0 && entry[1] == 0 && entry[2] == 0) {
            break;
        }
        
        uint16_t start_sector, num_sectors;
        parse_chain_entry(entry, &start_sector, &num_sectors);
        
        if (start_sector == 0) {
            break;
        }
        
        /* Read sectors */
        for (uint16_t s = 0; s < num_sectors; s++) {
            if (offset + UFT_TI99_SECTOR_SIZE > buffer_size) {
                break;
            }
            
            uft_ti99_error_t err = uft_ti99_read_sector(ctx, 
                                                         start_sector + s,
                                                         buffer + offset);
            if (err != UFT_TI99_OK) {
                return err;
            }
            
            offset += UFT_TI99_SECTOR_SIZE;
        }
    }
    
    *bytes_read = offset;
    return UFT_TI99_OK;
}

/*===========================================================================
 * File Extraction
 *===========================================================================*/

uft_ti99_error_t uft_ti99_extract_file(uft_ti99_ctx_t *ctx,
                                        const char *filename,
                                        uint8_t **buffer,
                                        size_t *size)
{
    if (!ctx || !filename || !buffer || !size) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Find file */
    uft_ti99_entry_t entry;
    uft_ti99_error_t err = uft_ti99_find_file(ctx, filename, &entry);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    if (entry.total_sectors == 0) {
        *buffer = NULL;
        *size = 0;
        return UFT_TI99_OK;
    }
    
    /* Read FDR */
    uint8_t fdr_buf[UFT_TI99_SECTOR_SIZE];
    err = uft_ti99_read_sector(ctx, entry.fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    const uft_ti99_fdr_t *fdr = (const uft_ti99_fdr_t *)fdr_buf;
    
    /* Allocate buffer */
    size_t alloc_size = (size_t)entry.total_sectors * UFT_TI99_SECTOR_SIZE;
    uint8_t *data = malloc(alloc_size);
    if (!data) {
        return UFT_TI99_ERR_MEMORY;
    }
    
    /* Read data chain */
    size_t bytes_read;
    err = read_file_chain(ctx, fdr, data, alloc_size, &bytes_read);
    if (err != UFT_TI99_OK) {
        free(data);
        return err;
    }
    
    /* Adjust size based on EOF offset for last sector */
    size_t actual_size = bytes_read;
    if (fdr->eof_offset > 0 && bytes_read >= UFT_TI99_SECTOR_SIZE) {
        actual_size = bytes_read - UFT_TI99_SECTOR_SIZE + fdr->eof_offset;
    }
    
    *buffer = data;
    *size = actual_size;
    
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_extract_to_file(uft_ti99_ctx_t *ctx,
                                           const char *ti_name,
                                           const char *host_path)
{
    if (!ctx || !ti_name || !host_path) {
        return UFT_TI99_ERR_PARAM;
    }
    
    uint8_t *buffer;
    size_t size;
    
    uft_ti99_error_t err = uft_ti99_extract_file(ctx, ti_name, &buffer, &size);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    FILE *f = fopen(host_path, "wb");
    if (!f) {
        free(buffer);
        return UFT_TI99_ERR_WRITE;
    }
    
    if (size > 0 && buffer) {
        if (fwrite(buffer, 1, size, f) != size) {
            fclose(f);
            free(buffer);
            return UFT_TI99_ERR_WRITE;
        }
    }
    
    fclose(f);
    free(buffer);
    
    return UFT_TI99_OK;
}

/*===========================================================================
 * Image Creation
 *===========================================================================*/

uft_ti99_error_t uft_ti99_create_image(uft_ti99_ctx_t *ctx,
                                        uft_ti99_format_t format,
                                        const char *disk_name)
{
    if (!ctx || format >= UFT_TI99_FORMAT_COUNT || format == UFT_TI99_FORMAT_UNKNOWN) {
        return UFT_TI99_ERR_PARAM;
    }
    
    const uft_ti99_geometry_t *geom = &g_geometries[format];
    
    /* Allocate image */
    if (ctx->owns_data && ctx->data) {
        free(ctx->data);
    }
    
    ctx->data = calloc(1, geom->total_bytes);
    if (!ctx->data) {
        return UFT_TI99_ERR_MEMORY;
    }
    
    ctx->data_size = geom->total_bytes;
    ctx->owns_data = true;
    ctx->format = format;
    ctx->geometry = *geom;
    ctx->open = true;
    
    /* Initialize VIB */
    memset(&ctx->vib, 0, sizeof(ctx->vib));
    
    /* Set disk name */
    memset(ctx->vib.disk_name, ' ', 10);
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > 10) len = 10;
        for (size_t i = 0; i < len; i++) {
            ctx->vib.disk_name[i] = (char)toupper((unsigned char)disk_name[i]);
        }
    } else {
        memcpy(ctx->vib.disk_name, "BLANK     ", 10);
    }
    
    /* Set VIB parameters */
    write_be16((uint8_t *)&ctx->vib.total_sectors, geom->total_sectors);
    ctx->vib.sectors_per_track = geom->sectors_per_track;
    memcpy(ctx->vib.dsk_id, UFT_TI99_VIB_DSK_ID, 3);
    ctx->vib.tracks_per_side = geom->tracks;
    ctx->vib.sides = geom->sides;
    ctx->vib.density = geom->density;
    
    /* Initialize bitmap - all free */
    memset(ctx->vib.bitmap, 0, sizeof(ctx->vib.bitmap));
    
    /* Mark system sectors as used */
    /* Sector 0 = VIB */
    uft_ti99_allocate_sector(ctx, 0);
    
    /* Sectors 1-2 = FDIR */
    for (uint16_t s = UFT_TI99_FDIR_START; s < UFT_TI99_FDIR_START + UFT_TI99_FDIR_COUNT; s++) {
        uft_ti99_allocate_sector(ctx, s);
    }
    
    ctx->vib_loaded = true;
    ctx->vib_dirty = true;
    
    /* Write VIB */
    uft_ti99_write_vib(ctx, &ctx->vib);
    
    /* Initialize FDIR sectors to zero (already done by calloc) */
    
    ctx->modified = true;
    
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_format(uft_ti99_ctx_t *ctx, const char *disk_name)
{
    if (!ctx || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Clear image */
    memset(ctx->data, 0, ctx->data_size);
    
    /* Reinitialize */
    return uft_ti99_create_image(ctx, ctx->format, 
                                  disk_name ? disk_name : (const char *)ctx->vib.disk_name);
}

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

uft_ti99_error_t uft_ti99_parse_filename(const char *input, char *filename)
{
    if (!input || !filename) {
        return UFT_TI99_ERR_PARAM;
    }
    
    memset(filename, ' ', 10);
    filename[10] = '\0';
    
    size_t len = strlen(input);
    if (len > 10) len = 10;
    
    for (size_t i = 0; i < len; i++) {
        char c = (char)toupper((unsigned char)input[i]);
        /* TI-99 allows: A-Z, 0-9, _, -, . */
        if (!isalnum((unsigned char)c) && c != '_' && c != '-' && c != '.') {
            return UFT_TI99_ERR_PARAM;
        }
        filename[i] = c;
    }
    
    return UFT_TI99_OK;
}

void uft_ti99_format_filename(const char *filename, char *buffer)
{
    if (!filename || !buffer) return;
    
    int i;
    for (i = 0; i < 10 && filename[i] != ' ' && filename[i] != '\0'; i++) {
        buffer[i] = filename[i];
    }
    buffer[i] = '\0';
}

bool uft_ti99_valid_filename(const char *filename)
{
    if (!filename || !*filename) return false;
    
    char parsed[11];
    return (uft_ti99_parse_filename(filename, parsed) == UFT_TI99_OK);
}

const char *uft_ti99_format_name(uft_ti99_format_t format)
{
    if (format >= UFT_TI99_FORMAT_COUNT) return "Unknown";
    return g_format_names[format];
}

const char *uft_ti99_file_type_name(uft_ti99_file_type_t type)
{
    if (type > UFT_TI99_TYPE_PROGRAM) return "Unknown";
    return g_type_names[type];
}

const char *uft_ti99_error_string(uft_ti99_error_t error)
{
    if (error >= UFT_TI99_ERR_COUNT) return "Unknown error";
    return g_error_strings[error];
}

/*===========================================================================
 * Display Functions
 *===========================================================================*/

void uft_ti99_print_directory(uft_ti99_ctx_t *ctx, FILE *output)
{
    if (!ctx || !ctx->open) return;
    if (!output) output = stdout;
    
    uft_ti99_dir_t dir;
    if (uft_ti99_read_directory(ctx, &dir) != UFT_TI99_OK) {
        fprintf(output, "Error reading directory\n");
        return;
    }
    
    fprintf(output, "\nDisk: %s\n", dir.disk_name);
    fprintf(output, "Format: %s\n\n", uft_ti99_format_name(dir.format));
    fprintf(output, "  Filename     Type      Size  Reclen  Prot\n");
    fprintf(output, "  ----------   -------  -----  ------  ----\n");
    
    for (size_t i = 0; i < dir.file_count; i++) {
        const uft_ti99_entry_t *f = &dir.files[i];
        fprintf(output, "  %-10s   %-7s  %5u  %6u   %c\n",
                f->filename,
                uft_ti99_file_type_name(f->type),
                f->total_sectors,
                f->record_length,
                f->protected ? 'P' : ' ');
    }
    
    fprintf(output, "\n  %zu file(s), %u sectors free (%u bytes)\n",
            dir.file_count, dir.free_sectors, dir.free_bytes);
}

void uft_ti99_print_info(uft_ti99_ctx_t *ctx, FILE *output)
{
    if (!ctx || !ctx->open) return;
    if (!output) output = stdout;
    
    fprintf(output, "\nTI-99/4A Disk Information:\n");
    fprintf(output, "  Disk Name:   %.10s\n", ctx->vib.disk_name);
    fprintf(output, "  Format:      %s\n", uft_ti99_format_name(ctx->format));
    fprintf(output, "  Tracks:      %u\n", ctx->geometry.tracks);
    fprintf(output, "  Sides:       %u\n", ctx->geometry.sides);
    fprintf(output, "  Sectors/Trk: %u\n", ctx->geometry.sectors_per_track);
    fprintf(output, "  Total:       %u sectors (%u bytes)\n",
            ctx->geometry.total_sectors, ctx->geometry.total_bytes);
    fprintf(output, "  Free:        %u sectors (%u bytes)\n",
            uft_ti99_free_sectors(ctx), 
            (uint32_t)uft_ti99_free_sectors(ctx) * UFT_TI99_SECTOR_SIZE);
}

int uft_ti99_directory_to_json(uft_ti99_ctx_t *ctx,
                               char *buffer,
                               size_t buffer_size)
{
    if (!ctx || !buffer || buffer_size < 64) return -1;
    
    uft_ti99_dir_t dir;
    if (uft_ti99_read_directory(ctx, &dir) != UFT_TI99_OK) {
        return -1;
    }
    
    int pos = 0;
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "{\n  \"disk_name\": \"%s\",\n", dir.disk_name);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"format\": \"%s\",\n", uft_ti99_format_name(dir.format));
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"total_sectors\": %u,\n", dir.total_sectors);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"free_sectors\": %u,\n", dir.free_sectors);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"file_count\": %zu,\n", dir.file_count);
    pos += snprintf(buffer + pos, buffer_size - pos,
                    "  \"files\": [\n");
    
    for (size_t i = 0; i < dir.file_count; i++) {
        const uft_ti99_entry_t *f = &dir.files[i];
        if (i > 0) {
            pos += snprintf(buffer + pos, buffer_size - pos, ",\n");
        }
        pos += snprintf(buffer + pos, buffer_size - pos,
                        "    {\"name\": \"%s\", \"type\": \"%s\", "
                        "\"sectors\": %u, \"reclen\": %u, \"protected\": %s}",
                        f->filename,
                        uft_ti99_file_type_name(f->type),
                        f->total_sectors,
                        f->record_length,
                        f->protected ? "true" : "false");
        
        if (pos >= (int)buffer_size - 64) break;
    }
    
    pos += snprintf(buffer + pos, buffer_size - pos, "\n  ]\n}\n");
    
    return pos;
}

/*===========================================================================
 * Accessors
 *===========================================================================*/

uft_ti99_format_t uft_ti99_get_format(const uft_ti99_ctx_t *ctx)
{
    return ctx ? ctx->format : UFT_TI99_FORMAT_UNKNOWN;
}

const uft_ti99_geometry_t *uft_ti99_get_geometry(const uft_ti99_ctx_t *ctx)
{
    return ctx ? &ctx->geometry : NULL;
}

bool uft_ti99_is_modified(const uft_ti99_ctx_t *ctx)
{
    return ctx ? ctx->modified : false;
}

uint8_t *uft_ti99_get_data(uft_ti99_ctx_t *ctx, size_t *size)
{
    if (!ctx || !size) return NULL;
    *size = ctx->data_size;
    return ctx->data;
}
