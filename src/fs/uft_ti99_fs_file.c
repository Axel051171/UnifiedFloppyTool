/**
 * @file uft_ti99_fs_file.c
 * @brief TI-99/4A Filesystem - File Operations
 * 
 * Implements: inject, delete, rename, protect, validate, rebuild_bitmap
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/fs/uft_ti99_fs.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/*===========================================================================
 * Internal Structure Access (must match uft_ti99_fs.c)
 *===========================================================================*/

struct uft_ti99_ctx {
    uint8_t            *data;
    size_t              data_size;
    bool                owns_data;
    bool                modified;
    uft_ti99_format_t   format;
    uft_ti99_geometry_t geometry;
    uft_ti99_vib_t      vib;
    bool                vib_loaded;
    bool                vib_dirty;
    bool                open;
};

/*===========================================================================
 * Helper Functions
 *===========================================================================*/

/**
 * @brief Read big-endian 16-bit value
 */
static inline uint16_t read_be16(const uint8_t *p)
{
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/**
 * @brief Write big-endian 16-bit value
 */
static inline void write_be16(uint8_t *p, uint16_t val)
{
    p[0] = (uint8_t)(val >> 8);
    p[1] = (uint8_t)(val & 0xFF);
}

/**
 * @brief Parse data chain entry
 */
static void parse_chain_entry(const uint8_t *chain, 
                               uint16_t *start_sector,
                               uint16_t *num_sectors)
{
    uint16_t start = ((uint16_t)chain[0] << 4) | ((chain[1] >> 4) & 0x0F);
    *start_sector = start;
    *num_sectors = (uint16_t)(chain[2] + 1);
}

/**
 * @brief Write data chain entry
 */
static void write_chain_entry(uint8_t *chain,
                               uint16_t start_sector,
                               uint16_t num_sectors)
{
    /* Encode: byte0 = start[11:4], byte1[7:4] = start[3:0], byte2 = num-1 */
    chain[0] = (uint8_t)(start_sector >> 4);
    chain[1] = (uint8_t)((start_sector & 0x0F) << 4);
    chain[2] = (uint8_t)(num_sectors > 0 ? num_sectors - 1 : 0);
}

/**
 * @brief Find free slot in FDIR
 * @return Slot index (0-255) or -1 if full
 */
static int find_free_fdir_slot(uft_ti99_ctx_t *ctx)
{
    uint8_t sector[UFT_TI99_SECTOR_SIZE];
    
    /* Check FDIR sectors 1 and 2 */
    for (int fdir_sec = 0; fdir_sec < 2; fdir_sec++) {
        if (uft_ti99_read_sector(ctx, 1 + fdir_sec, sector) != UFT_TI99_OK) {
            continue;
        }
        
        /* 128 entries per sector (2 bytes each) */
        for (int i = 0; i < 128; i++) {
            uint16_t fdr_ptr = read_be16(sector + i * 2);
            if (fdr_ptr == 0) {
                return fdir_sec * 128 + i;
            }
        }
    }
    
    return -1;  /* Directory full */
}

/**
 * @brief Set FDIR entry
 */
static uft_ti99_error_t set_fdir_entry(uft_ti99_ctx_t *ctx,
                                        int slot,
                                        uint16_t fdr_sector)
{
    int fdir_sec = slot / 128;
    int index = slot % 128;
    
    uint8_t sector[UFT_TI99_SECTOR_SIZE];
    uft_ti99_error_t err = uft_ti99_read_sector(ctx, 1 + fdir_sec, sector);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    write_be16(sector + index * 2, fdr_sector);
    
    return uft_ti99_write_sector(ctx, 1 + fdir_sec, sector);
}

/**
 * @brief Get FDIR entry for a file
 * @return FDIR slot index or -1 if not found
 */
static int get_fdir_slot_for_file(uft_ti99_ctx_t *ctx, uint16_t fdr_sector)
{
    uint8_t sector[UFT_TI99_SECTOR_SIZE];
    
    for (int fdir_sec = 0; fdir_sec < 2; fdir_sec++) {
        if (uft_ti99_read_sector(ctx, 1 + fdir_sec, sector) != UFT_TI99_OK) {
            continue;
        }
        
        for (int i = 0; i < 128; i++) {
            uint16_t ptr = read_be16(sector + i * 2);
            if (ptr == fdr_sector) {
                return fdir_sec * 128 + i;
            }
        }
    }
    
    return -1;
}

/**
 * @brief Free all sectors in a file's data chain
 */
static uft_ti99_error_t free_file_chain(uft_ti99_ctx_t *ctx,
                                         const uft_ti99_fdr_t *fdr)
{
    const uint8_t *chain = fdr->data_chain;
    
    for (int i = 0; i < UFT_TI99_MAX_CHAIN_ENTRIES; i++) {
        const uint8_t *entry = chain + (i * 3);
        
        if (entry[0] == 0 && entry[1] == 0 && entry[2] == 0) {
            break;
        }
        
        uint16_t start_sector, num_sectors;
        parse_chain_entry(entry, &start_sector, &num_sectors);
        
        if (start_sector == 0) {
            break;
        }
        
        /* Free all sectors in this run */
        for (uint16_t s = 0; s < num_sectors; s++) {
            uft_ti99_free_sector(ctx, start_sector + s);
        }
    }
    
    return UFT_TI99_OK;
}

/**
 * @brief Allocate contiguous sectors if possible
 * @return Start sector or 0 on failure
 */
static uint16_t allocate_contiguous(uft_ti99_ctx_t *ctx, uint16_t count)
{
    if (count == 0) return 0;
    
    uint16_t total = ctx->geometry.total_sectors;
    
    /* Start search after system sectors */
    for (uint16_t start = 3; start + count <= total; start++) {
        bool all_free = true;
        
        for (uint16_t i = 0; i < count && all_free; i++) {
            if (!uft_ti99_is_sector_free(ctx, start + i)) {
                all_free = false;
                start = start + i;  /* Skip ahead */
            }
        }
        
        if (all_free) {
            /* Allocate all */
            for (uint16_t i = 0; i < count; i++) {
                if (uft_ti99_allocate_sector(ctx, start + i) != UFT_TI99_OK) {
                    /* Rollback */
                    for (uint16_t j = 0; j < i; j++) {
                        uft_ti99_free_sector(ctx, start + j);
                    }
                    return 0;
                }
            }
            return start;
        }
    }
    
    return 0;
}

/**
 * @brief Build data chain for file sectors
 */
static int build_data_chain(uint8_t *chain_buf,
                             const uint16_t *sectors,
                             size_t sector_count)
{
    if (sector_count == 0) {
        memset(chain_buf, 0, UFT_TI99_MAX_CHAIN_ENTRIES * 3);
        return 0;
    }
    
    int chain_idx = 0;
    size_t i = 0;
    
    while (i < sector_count && chain_idx < UFT_TI99_MAX_CHAIN_ENTRIES) {
        uint16_t start = sectors[i];
        uint16_t count = 1;
        
        /* Count contiguous sectors */
        while (i + count < sector_count && 
               sectors[i + count] == start + count &&
               count < 256) {
            count++;
        }
        
        write_chain_entry(chain_buf + chain_idx * 3, start, count);
        chain_idx++;
        i += count;
    }
    
    /* Zero remaining entries */
    for (int j = chain_idx; j < UFT_TI99_MAX_CHAIN_ENTRIES; j++) {
        memset(chain_buf + j * 3, 0, 3);
    }
    
    return chain_idx;
}

/*===========================================================================
 * File Injection
 *===========================================================================*/

uft_ti99_error_t uft_ti99_inject_file(uft_ti99_ctx_t *ctx,
                                       const char *filename,
                                       const uint8_t *data,
                                       size_t size,
                                       uft_ti99_file_type_t type,
                                       uint8_t record_length)
{
    if (!ctx || !ctx->open || !filename) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Validate filename */
    if (!uft_ti99_valid_filename(filename)) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Check if file exists */
    uft_ti99_entry_t existing;
    if (uft_ti99_find_file(ctx, filename, &existing) == UFT_TI99_OK) {
        return UFT_TI99_ERR_EXISTS;
    }
    
    /* Find free FDIR slot */
    int fdir_slot = find_free_fdir_slot(ctx);
    if (fdir_slot < 0) {
        return UFT_TI99_ERR_DIRFULL;
    }
    
    /* Calculate sectors needed */
    uint16_t sectors_needed = (size + UFT_TI99_SECTOR_SIZE - 1) / UFT_TI99_SECTOR_SIZE;
    if (sectors_needed == 0 && size > 0) {
        sectors_needed = 1;
    }
    
    /* Need +1 for FDR */
    uint16_t free_count = uft_ti99_free_sectors(ctx);
    if (free_count < sectors_needed + 1) {
        return UFT_TI99_ERR_FULL;
    }
    
    /* Allocate FDR sector */
    uint16_t fdr_sector = uft_ti99_find_free_sector(ctx);
    if (fdr_sector == 0) {
        return UFT_TI99_ERR_FULL;
    }
    
    uft_ti99_error_t err = uft_ti99_allocate_sector(ctx, fdr_sector);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    /* Allocate data sectors */
    uint16_t *data_sectors = NULL;
    if (sectors_needed > 0) {
        data_sectors = calloc(sectors_needed, sizeof(uint16_t));
        if (!data_sectors) {
            uft_ti99_free_sector(ctx, fdr_sector);
            return UFT_TI99_ERR_MEMORY;
        }
        
        /* Try contiguous allocation first */
        uint16_t start = allocate_contiguous(ctx, sectors_needed);
        if (start != 0) {
            for (uint16_t i = 0; i < sectors_needed; i++) {
                data_sectors[i] = start + i;
            }
        } else {
            /* Fragment allocation */
            for (uint16_t i = 0; i < sectors_needed; i++) {
                uint16_t sec = uft_ti99_find_free_sector(ctx);
                if (sec == 0) {
                    /* Rollback */
                    for (uint16_t j = 0; j < i; j++) {
                        uft_ti99_free_sector(ctx, data_sectors[j]);
                    }
                    uft_ti99_free_sector(ctx, fdr_sector);
                    free(data_sectors);
                    return UFT_TI99_ERR_FULL;
                }
                data_sectors[i] = sec;
                uft_ti99_allocate_sector(ctx, sec);
            }
        }
    }
    
    /* Build FDR */
    uint8_t fdr_buf[UFT_TI99_SECTOR_SIZE];
    memset(fdr_buf, 0, UFT_TI99_SECTOR_SIZE);
    uft_ti99_fdr_t *fdr = (uft_ti99_fdr_t *)fdr_buf;
    
    /* Filename (space-padded) */
    char padded_name[11];
    uft_ti99_parse_filename(filename, padded_name);
    memcpy(fdr->filename, padded_name, 10);
    
    /* Status flags */
    uint8_t status = 0x00;
    switch (type) {
        case UFT_TI99_TYPE_DIS_FIX:
            status = 0x00;
            break;
        case UFT_TI99_TYPE_DIS_VAR:
            status = UFT_TI99_FLAG_VARIABLE;
            break;
        case UFT_TI99_TYPE_INT_FIX:
            status = UFT_TI99_FLAG_INTERNAL;
            break;
        case UFT_TI99_TYPE_INT_VAR:
            status = UFT_TI99_FLAG_INTERNAL | UFT_TI99_FLAG_VARIABLE;
            break;
        case UFT_TI99_TYPE_PROGRAM:
            status = UFT_TI99_FLAG_PROGRAM;
            break;
    }
    fdr->status_flags = status;
    
    /* Record length */
    fdr->records_per_sector = (type == UFT_TI99_TYPE_PROGRAM) ? 0 : 
                               (record_length > 0 ? 256 / record_length : 1);
    
    /* Total sectors and records */
    write_be16((uint8_t *)&fdr->total_sectors, sectors_needed);
    fdr->eof_offset = (size > 0) ? (size % UFT_TI99_SECTOR_SIZE) : 0;
    if (fdr->eof_offset == 0 && size > 0) {
        fdr->eof_offset = 0;  /* Full last sector */
    }
    fdr->record_length = record_length;
    
    /* Build data chain */
    if (sectors_needed > 0 && data_sectors) {
        build_data_chain(fdr->data_chain, data_sectors, sectors_needed);
    }
    
    /* Write FDR */
    err = uft_ti99_write_sector(ctx, fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        /* Rollback */
        if (data_sectors) {
            for (uint16_t i = 0; i < sectors_needed; i++) {
                uft_ti99_free_sector(ctx, data_sectors[i]);
            }
            free(data_sectors);
        }
        uft_ti99_free_sector(ctx, fdr_sector);
        return err;
    }
    
    /* Write data sectors */
    if (data && size > 0 && data_sectors) {
        size_t offset = 0;
        for (uint16_t i = 0; i < sectors_needed; i++) {
            uint8_t sector_buf[UFT_TI99_SECTOR_SIZE];
            memset(sector_buf, 0, UFT_TI99_SECTOR_SIZE);
            
            size_t to_copy = size - offset;
            if (to_copy > UFT_TI99_SECTOR_SIZE) {
                to_copy = UFT_TI99_SECTOR_SIZE;
            }
            memcpy(sector_buf, data + offset, to_copy);
            
            err = uft_ti99_write_sector(ctx, data_sectors[i], sector_buf);
            if (err != UFT_TI99_OK) {
                /* Note: partial write - cleanup is complex, just mark error */
                free(data_sectors);
                return err;
            }
            offset += to_copy;
        }
    }
    
    if (data_sectors) {
        free(data_sectors);
    }
    
    /* Update FDIR */
    err = set_fdir_entry(ctx, fdir_slot, fdr_sector);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    ctx->modified = true;
    return UFT_TI99_OK;
}

uft_ti99_error_t uft_ti99_inject_from_file(uft_ti99_ctx_t *ctx,
                                            const char *host_path,
                                            const char *ti_name,
                                            uft_ti99_file_type_t type,
                                            uint8_t record_length)
{
    if (!ctx || !host_path) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Read host file */
    FILE *f = fopen(host_path, "rb");
    if (!f) {
        return UFT_TI99_ERR_READ;
    }
    
    if (fseek(f, 0, SEEK_END) != 0) { /* seek error */ }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { /* seek error */ }
    if (file_size < 0 || file_size > 0x100000) {  /* 1MB max */
        fclose(f);
        return UFT_TI99_ERR_PARAM;
    }
    
    uint8_t *buffer = NULL;
    if (file_size > 0) {
        buffer = malloc((size_t)file_size);
        if (!buffer) {
            fclose(f);
            return UFT_TI99_ERR_MEMORY;
        }
        
        if (fread(buffer, 1, (size_t)file_size, f) != (size_t)file_size) {
            free(buffer);
            fclose(f);
            return UFT_TI99_ERR_READ;
        }
    }
    fclose(f);
    
    /* Derive filename if not provided */
    char derived_name[11];
    if (!ti_name || ti_name[0] == '\0') {
        /* Extract from path */
        const char *slash = strrchr(host_path, '/');
        const char *bslash = strrchr(host_path, '\\');
        const char *base = host_path;
        if (slash && slash > base) base = slash + 1;
        if (bslash && bslash > base) base = bslash + 1;
        
        /* Copy up to dot or end */
        int j = 0;
        for (int i = 0; base[i] && base[i] != '.' && j < 10; i++) {
            char c = (char)toupper((unsigned char)base[i]);
            if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                c == '_' || c == '-' || c == '.') {
                derived_name[j++] = c;
            }
        }
        derived_name[j] = '\0';
        ti_name = derived_name;
    }
    
    uft_ti99_error_t err = uft_ti99_inject_file(ctx, ti_name, buffer,
                                                  (size_t)file_size, type,
                                                  record_length);
    
    if (buffer) {
        free(buffer);
    }
    
    return err;
}

/*===========================================================================
 * File Deletion
 *===========================================================================*/

uft_ti99_error_t uft_ti99_delete_file(uft_ti99_ctx_t *ctx,
                                       const char *filename)
{
    if (!ctx || !ctx->open || !filename) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Find file */
    uft_ti99_entry_t entry;
    uft_ti99_error_t err = uft_ti99_find_file(ctx, filename, &entry);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    /* Check protection */
    if (entry.protected) {
        return UFT_TI99_ERR_PROTECTED;
    }
    
    /* Read FDR */
    uint8_t fdr_buf[UFT_TI99_SECTOR_SIZE];
    err = uft_ti99_read_sector(ctx, entry.fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    const uft_ti99_fdr_t *fdr = (const uft_ti99_fdr_t *)fdr_buf;
    
    /* Free data chain sectors */
    free_file_chain(ctx, fdr);
    
    /* Free FDR sector */
    uft_ti99_free_sector(ctx, entry.fdr_sector);
    
    /* Clear FDIR entry */
    int slot = get_fdir_slot_for_file(ctx, entry.fdr_sector);
    if (slot >= 0) {
        set_fdir_entry(ctx, slot, 0);
    }
    
    ctx->modified = true;
    return UFT_TI99_OK;
}

/*===========================================================================
 * File Renaming
 *===========================================================================*/

uft_ti99_error_t uft_ti99_rename_file(uft_ti99_ctx_t *ctx,
                                       const char *old_name,
                                       const char *new_name)
{
    if (!ctx || !ctx->open || !old_name || !new_name) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Validate new name */
    if (!uft_ti99_valid_filename(new_name)) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Find old file */
    uft_ti99_entry_t entry;
    uft_ti99_error_t err = uft_ti99_find_file(ctx, old_name, &entry);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    /* Check if new name already exists (case-insensitive) */
    uft_ti99_entry_t existing;
    if (uft_ti99_find_file(ctx, new_name, &existing) == UFT_TI99_OK) {
        /* Check if it's the same file */
        if (existing.fdr_sector != entry.fdr_sector) {
            return UFT_TI99_ERR_EXISTS;
        }
    }
    
    /* Read FDR */
    uint8_t fdr_buf[UFT_TI99_SECTOR_SIZE];
    err = uft_ti99_read_sector(ctx, entry.fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    uft_ti99_fdr_t *fdr = (uft_ti99_fdr_t *)fdr_buf;
    
    /* Update filename */
    char padded_name[11];
    uft_ti99_parse_filename(new_name, padded_name);
    memcpy(fdr->filename, padded_name, 10);
    
    /* Write FDR */
    err = uft_ti99_write_sector(ctx, entry.fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    ctx->modified = true;
    return UFT_TI99_OK;
}

/*===========================================================================
 * File Protection
 *===========================================================================*/

uft_ti99_error_t uft_ti99_set_protected(uft_ti99_ctx_t *ctx,
                                         const char *filename,
                                         bool is_protected)
{
    if (!ctx || !ctx->open || !filename) {
        return UFT_TI99_ERR_PARAM;
    }
    
    /* Find file */
    uft_ti99_entry_t entry;
    uft_ti99_error_t err = uft_ti99_find_file(ctx, filename, &entry);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    /* Read FDR */
    uint8_t fdr_buf[UFT_TI99_SECTOR_SIZE];
    err = uft_ti99_read_sector(ctx, entry.fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    uft_ti99_fdr_t *fdr = (uft_ti99_fdr_t *)fdr_buf;
    
    /* Update protection flag */
    if (is_protected) {
        fdr->status_flags |= UFT_TI99_FLAG_PROTECTED;
    } else {
        fdr->status_flags &= ~UFT_TI99_FLAG_PROTECTED;
    }
    
    /* Write FDR */
    err = uft_ti99_write_sector(ctx, entry.fdr_sector, fdr_buf);
    if (err != UFT_TI99_OK) {
        return err;
    }
    
    ctx->modified = true;
    return UFT_TI99_OK;
}

/*===========================================================================
 * Validation
 *===========================================================================*/

uft_ti99_error_t uft_ti99_validate(uft_ti99_ctx_t *ctx,
                                    uft_ti99_val_result_t *result)
{
    if (!ctx || !result) {
        return UFT_TI99_ERR_PARAM;
    }
    
    memset(result, 0, sizeof(*result));
    result->valid = true;
    result->vib_ok = true;
    result->fdir_ok = true;
    result->chains_ok = true;
    
    int report_pos = 0;
    
    if (!ctx->open) {
        result->valid = false;
        report_pos += snprintf(result->report + report_pos,
                                sizeof(result->report) - report_pos,
                                "ERROR: Image not open\n");
        return UFT_TI99_ERR_NOT_OPEN;
    }
    
    /* Build sector usage map */
    uint16_t total_sectors = ctx->geometry.total_sectors;
    uint8_t *usage_map = calloc((total_sectors + 7) / 8, 1);
    if (!usage_map) {
        return UFT_TI99_ERR_MEMORY;
    }
    
    /* Mark system sectors */
    for (int i = 0; i < 3; i++) {
        usage_map[i / 8] |= (1 << (i % 8));
    }
    
    /* Validate VIB */
    if (ctx->vib.dsk_id[0] != 'D' || ctx->vib.dsk_id[1] != 'S' || 
        ctx->vib.dsk_id[2] != 'K') {
        result->vib_ok = false;
        result->valid = false;
        result->errors++;
        report_pos += snprintf(result->report + report_pos,
                                sizeof(result->report) - report_pos,
                                "ERROR: Invalid DSK signature in VIB\n");
    }
    
    /* Validate FDIR and file chains */
    uint8_t fdir_buf[UFT_TI99_SECTOR_SIZE];
    
    for (int fdir_sec = 0; fdir_sec < 2; fdir_sec++) {
        if (uft_ti99_read_sector(ctx, 1 + fdir_sec, fdir_buf) != UFT_TI99_OK) {
            result->fdir_ok = false;
            result->errors++;
            report_pos += snprintf(result->report + report_pos,
                                    sizeof(result->report) - report_pos,
                                    "ERROR: Cannot read FDIR sector %d\n", 
                                    1 + fdir_sec);
            continue;
        }
        
        for (int i = 0; i < 128; i++) {
            uint16_t fdr_ptr = read_be16(fdir_buf + i * 2);
            if (fdr_ptr == 0) continue;
            
            /* Validate FDR sector range */
            if (fdr_ptr >= total_sectors) {
                result->fdir_ok = false;
                result->errors++;
                report_pos += snprintf(result->report + report_pos,
                                        sizeof(result->report) - report_pos,
                                        "ERROR: FDIR[%d] points to invalid sector %u\n",
                                        fdir_sec * 128 + i, fdr_ptr);
                continue;
            }
            
            /* Check for duplicate FDR references */
            if (usage_map[fdr_ptr / 8] & (1 << (fdr_ptr % 8))) {
                result->cross_linked++;
                result->errors++;
                report_pos += snprintf(result->report + report_pos,
                                        sizeof(result->report) - report_pos,
                                        "ERROR: FDR sector %u referenced multiple times\n",
                                        fdr_ptr);
            }
            usage_map[fdr_ptr / 8] |= (1 << (fdr_ptr % 8));
            
            /* Read and validate FDR */
            uint8_t fdr_data[UFT_TI99_SECTOR_SIZE];
            if (uft_ti99_read_sector(ctx, fdr_ptr, fdr_data) != UFT_TI99_OK) {
                result->errors++;
                continue;
            }
            
            const uft_ti99_fdr_t *fdr = (const uft_ti99_fdr_t *)fdr_data;
            
            /* Validate data chain */
            const uint8_t *chain = fdr->data_chain;
            for (int c = 0; c < UFT_TI99_MAX_CHAIN_ENTRIES; c++) {
                const uint8_t *entry = chain + c * 3;
                
                if (entry[0] == 0 && entry[1] == 0 && entry[2] == 0) {
                    break;
                }
                
                uint16_t start, count;
                parse_chain_entry(entry, &start, &count);
                
                if (start == 0) break;
                
                for (uint16_t s = 0; s < count; s++) {
                    uint16_t sec = start + s;
                    
                    if (sec >= total_sectors) {
                        result->chains_ok = false;
                        result->errors++;
                        report_pos += snprintf(result->report + report_pos,
                                                sizeof(result->report) - report_pos,
                                                "ERROR: File %.10s has invalid sector %u\n",
                                                fdr->filename, sec);
                        continue;
                    }
                    
                    if (usage_map[sec / 8] & (1 << (sec % 8))) {
                        result->cross_linked++;
                        result->errors++;
                        report_pos += snprintf(result->report + report_pos,
                                                sizeof(result->report) - report_pos,
                                                "ERROR: Sector %u cross-linked (file %.10s)\n",
                                                sec, fdr->filename);
                    }
                    usage_map[sec / 8] |= (1 << (sec % 8));
                }
            }
        }
    }
    
    /* Check for orphan sectors (allocated in bitmap but not in any file) */
    for (uint16_t sec = 3; sec < total_sectors; sec++) {
        bool in_bitmap = !uft_ti99_is_sector_free(ctx, sec);
        bool in_usage = (usage_map[sec / 8] & (1 << (sec % 8))) != 0;
        
        if (in_bitmap && !in_usage) {
            result->orphan_sectors++;
            result->warnings++;
        }
    }
    
    if (result->orphan_sectors > 0) {
        report_pos += snprintf(result->report + report_pos,
                                sizeof(result->report) - report_pos,
                                "WARNING: %u orphan sectors found\n",
                                result->orphan_sectors);
    }
    
    /* Overall validity */
    if (!result->vib_ok || !result->fdir_ok || !result->chains_ok ||
        result->cross_linked > 0) {
        result->valid = false;
    }
    
    free(usage_map);
    
    report_pos += snprintf(result->report + report_pos,
                            sizeof(result->report) - report_pos,
                            "\nValidation: %s (%u errors, %u warnings)\n",
                            result->valid ? "PASS" : "FAIL",
                            result->errors, result->warnings);
    
    return UFT_TI99_OK;
}

/*===========================================================================
 * Bitmap Rebuild
 *===========================================================================*/

uft_ti99_error_t uft_ti99_rebuild_bitmap(uft_ti99_ctx_t *ctx)
{
    if (!ctx || !ctx->open) {
        return UFT_TI99_ERR_PARAM;
    }
    
    uint16_t total_sectors = ctx->geometry.total_sectors;
    
    /* Build usage map from file chains */
    uint8_t *usage_map = calloc((total_sectors + 7) / 8, 1);
    if (!usage_map) {
        return UFT_TI99_ERR_MEMORY;
    }
    
    /* Mark system sectors (VIB + 2 FDIR) */
    for (int i = 0; i < 3; i++) {
        usage_map[i / 8] |= (1 << (i % 8));
    }
    
    /* Scan FDIR and trace all file chains */
    uint8_t fdir_buf[UFT_TI99_SECTOR_SIZE];
    
    for (int fdir_sec = 0; fdir_sec < 2; fdir_sec++) {
        if (uft_ti99_read_sector(ctx, 1 + fdir_sec, fdir_buf) != UFT_TI99_OK) {
            continue;
        }
        
        for (int i = 0; i < 128; i++) {
            uint16_t fdr_ptr = read_be16(fdir_buf + i * 2);
            if (fdr_ptr == 0 || fdr_ptr >= total_sectors) continue;
            
            /* Mark FDR sector */
            usage_map[fdr_ptr / 8] |= (1 << (fdr_ptr % 8));
            
            /* Read FDR and trace chain */
            uint8_t fdr_data[UFT_TI99_SECTOR_SIZE];
            if (uft_ti99_read_sector(ctx, fdr_ptr, fdr_data) != UFT_TI99_OK) {
                continue;
            }
            
            const uft_ti99_fdr_t *fdr = (const uft_ti99_fdr_t *)fdr_data;
            const uint8_t *chain = fdr->data_chain;
            
            for (int c = 0; c < UFT_TI99_MAX_CHAIN_ENTRIES; c++) {
                const uint8_t *entry = chain + c * 3;
                
                if (entry[0] == 0 && entry[1] == 0 && entry[2] == 0) {
                    break;
                }
                
                uint16_t start, count;
                parse_chain_entry(entry, &start, &count);
                
                if (start == 0) break;
                
                for (uint16_t s = 0; s < count; s++) {
                    uint16_t sec = start + s;
                    if (sec < total_sectors) {
                        usage_map[sec / 8] |= (1 << (sec % 8));
                    }
                }
            }
        }
    }
    
    /* Rebuild VIB bitmap from usage map */
    /* VIB bitmap: bit set = allocated, bit clear = free */
    memset(ctx->vib.bitmap, 0, sizeof(ctx->vib.bitmap));
    
    for (uint16_t sec = 0; sec < total_sectors && sec < 1600; sec++) {
        if (usage_map[sec / 8] & (1 << (sec % 8))) {
            /* Sector is used - mark as allocated in VIB */
            ctx->vib.bitmap[sec / 8] |= (1 << (sec % 8));
        }
    }
    
    ctx->vib_dirty = true;
    
    /* Write VIB back */
    uint8_t vib_buf[UFT_TI99_SECTOR_SIZE];
    memset(vib_buf, 0, UFT_TI99_SECTOR_SIZE);
    memcpy(vib_buf, &ctx->vib, sizeof(uft_ti99_vib_t));
    
    uft_ti99_error_t err = uft_ti99_write_sector(ctx, 0, vib_buf);
    
    free(usage_map);
    
    if (err == UFT_TI99_OK) {
        ctx->vib_dirty = false;
        ctx->modified = true;
    }
    
    return err;
}
