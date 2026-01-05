/**
 * @file uft_jv3.c
 * @brief JV3 Container Format Implementation
 * 
 * EXT3-019: JV3 disk image format for TRS-80
 */

#include "uft/formats/uft_jv3.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Size Code Conversion
 *===========================================================================*/

uint16_t uft_jv3_size_from_code(uint8_t code)
{
    switch (code & 0x03) {
        case UFT_JV3_SIZE_128:  return 128;
        case UFT_JV3_SIZE_256:  return 256;
        case UFT_JV3_SIZE_512:  return 512;
        case UFT_JV3_SIZE_1024: return 1024;
        default: return 256;
    }
}

uint8_t uft_jv3_code_from_size(uint16_t size)
{
    switch (size) {
        case 128:  return UFT_JV3_SIZE_128;
        case 256:  return UFT_JV3_SIZE_256;
        case 512:  return UFT_JV3_SIZE_512;
        case 1024: return UFT_JV3_SIZE_1024;
        default:   return UFT_JV3_SIZE_256;
    }
}

/*===========================================================================
 * Detection
 *===========================================================================*/

bool uft_jv3_detect(const uint8_t *data, size_t size)
{
    if (!data || size < UFT_JV3_HEADER_SIZE) {
        return false;
    }
    
    /* JV3 has no magic number - detect by analyzing header structure */
    /* Header is 2901 bytes: 967 sector entries of 3 bytes each */
    
    int valid_entries = 0;
    int free_entries = 0;
    
    for (int i = 0; i < 967; i++) {
        uint8_t track = data[i * 3];
        uint8_t sector = data[i * 3 + 1];
        uint8_t flags = data[i * 3 + 2];
        
        if (track == UFT_JV3_FREE_ENTRY) {
            free_entries++;
            continue;
        }
        
        /* Sanity checks */
        if (track > 80) continue;  /* Reasonable track limit */
        if (sector > 30) continue; /* Reasonable sector limit */
        
        valid_entries++;
    }
    
    /* Need reasonable number of valid entries */
    return (valid_entries >= 5 && valid_entries + free_entries >= 100);
}

/*===========================================================================
 * Opening/Closing
 *===========================================================================*/

int uft_jv3_open(uft_jv3_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data || size < UFT_JV3_HEADER_SIZE) {
        return -1;
    }
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Parse header */
    size_t data_offset = UFT_JV3_HEADER_SIZE;
    
    for (int i = 0; i < 967 && ctx->entry_count < UFT_JV3_MAX_SECTORS; i++) {
        uint8_t track = data[i * 3];
        uint8_t sector = data[i * 3 + 1];
        uint8_t flags = data[i * 3 + 2];
        
        if (track == UFT_JV3_FREE_ENTRY) {
            continue;
        }
        
        ctx->entries[ctx->entry_count].track = track;
        ctx->entries[ctx->entry_count].sector = sector;
        ctx->entries[ctx->entry_count].flags = flags;
        ctx->entry_count++;
        
        /* Update statistics */
        if (track > ctx->max_track) ctx->max_track = track;
        if (sector > ctx->max_sector) ctx->max_sector = sector;
        
        if (flags & UFT_JV3_SIDE_MASK) {
            ctx->sides = 2;
        }
        
        if (flags & UFT_JV3_DENSITY_MASK) {
            ctx->has_mfm = true;
            ctx->mfm_sectors++;
        } else {
            ctx->has_fm = true;
            ctx->fm_sectors++;
        }
        
        if ((flags & UFT_JV3_DAM_MASK) >= UFT_JV3_DAM_DELETED_F8) {
            ctx->deleted_sectors++;
        }
        
        if (flags & UFT_JV3_CRC_MASK) {
            ctx->crc_errors++;
        }
        
        ctx->total_sectors++;
    }
    
    if (ctx->sides == 0) ctx->sides = 1;
    
    return 0;
}

void uft_jv3_close(uft_jv3_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Sector Access
 *===========================================================================*/

int uft_jv3_get_sector_info(const uft_jv3_ctx_t *ctx, int index,
                            uft_jv3_sector_info_t *info)
{
    if (!ctx || !info || index < 0 || index >= ctx->entry_count) {
        return -1;
    }
    
    const uft_jv3_sector_entry_t *entry = &ctx->entries[index];
    
    info->track = entry->track;
    info->sector = entry->sector;
    info->side = (entry->flags & UFT_JV3_SIDE_MASK) ? 1 : 0;
    info->size = uft_jv3_size_from_code(entry->flags & UFT_JV3_SIZE_MASK);
    info->is_mfm = (entry->flags & UFT_JV3_DENSITY_MASK) != 0;
    info->is_deleted = (entry->flags & UFT_JV3_DAM_MASK) >= UFT_JV3_DAM_DELETED_F8;
    info->has_crc_error = (entry->flags & UFT_JV3_CRC_MASK) != 0;
    
    /* Calculate data offset */
    size_t offset = UFT_JV3_HEADER_SIZE;
    for (int i = 0; i < index; i++) {
        uint16_t sec_size = uft_jv3_size_from_code(ctx->entries[i].flags & UFT_JV3_SIZE_MASK);
        offset += sec_size;
    }
    info->data_offset = offset;
    
    return 0;
}

int uft_jv3_find_sector(const uft_jv3_ctx_t *ctx, 
                        uint8_t track, uint8_t sector, uint8_t side,
                        uft_jv3_sector_info_t *info)
{
    if (!ctx) return -1;
    
    for (int i = 0; i < ctx->entry_count; i++) {
        const uft_jv3_sector_entry_t *entry = &ctx->entries[i];
        uint8_t entry_side = (entry->flags & UFT_JV3_SIDE_MASK) ? 1 : 0;
        
        if (entry->track == track && entry->sector == sector && entry_side == side) {
            return uft_jv3_get_sector_info(ctx, i, info);
        }
    }
    
    return -1;  /* Not found */
}

int uft_jv3_read_sector(const uft_jv3_ctx_t *ctx,
                        uint8_t track, uint8_t sector, uint8_t side,
                        uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size) return -1;
    
    uft_jv3_sector_info_t info;
    if (uft_jv3_find_sector(ctx, track, sector, side, &info) != 0) {
        return -1;
    }
    
    if (info.data_offset + info.size > ctx->size) {
        return -1;
    }
    
    if (*size < info.size) {
        *size = info.size;
        return -1;
    }
    
    memcpy(buffer, ctx->data + info.data_offset, info.size);
    *size = info.size;
    
    return info.has_crc_error ? 1 : 0;
}

int uft_jv3_get_track_sectors(const uft_jv3_ctx_t *ctx,
                              uint8_t track, uint8_t side,
                              uft_jv3_sector_info_t *sectors, int max_sectors,
                              int *count)
{
    if (!ctx || !sectors || !count) return -1;
    
    *count = 0;
    
    for (int i = 0; i < ctx->entry_count && *count < max_sectors; i++) {
        const uft_jv3_sector_entry_t *entry = &ctx->entries[i];
        uint8_t entry_side = (entry->flags & UFT_JV3_SIDE_MASK) ? 1 : 0;
        
        if (entry->track == track && entry_side == side) {
            if (uft_jv3_get_sector_info(ctx, i, &sectors[*count]) == 0) {
                (*count)++;
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Writer
 *===========================================================================*/

uft_jv3_writer_t *uft_jv3_writer_create(size_t initial_capacity)
{
    uft_jv3_writer_t *writer = calloc(1, sizeof(uft_jv3_writer_t));
    if (!writer) return NULL;
    
    writer->capacity = initial_capacity > 0 ? initial_capacity : 1024 * 1024;
    writer->buffer = malloc(writer->capacity);
    
    if (!writer->buffer) {
        free(writer);
        return NULL;
    }
    
    /* Initialize header area */
    memset(writer->buffer, UFT_JV3_FREE_ENTRY, UFT_JV3_HEADER_SIZE);
    writer->data_offset = UFT_JV3_HEADER_SIZE;
    writer->used = UFT_JV3_HEADER_SIZE;
    
    return writer;
}

void uft_jv3_writer_destroy(uft_jv3_writer_t *writer)
{
    if (writer) {
        free(writer->buffer);
        free(writer);
    }
}

int uft_jv3_writer_add_sector(uft_jv3_writer_t *writer,
                              uint8_t track, uint8_t sector, uint8_t side,
                              uint16_t size, bool is_mfm, bool is_deleted,
                              const uint8_t *data)
{
    if (!writer || !data) return -1;
    if (writer->entry_count >= 967) return -1;  /* Header full */
    
    /* Ensure capacity */
    size_t needed = writer->used + size;
    if (needed > writer->capacity) {
        size_t new_cap = writer->capacity * 2;
        while (new_cap < needed) new_cap *= 2;
        
        uint8_t *new_buf = realloc(writer->buffer, new_cap);
        if (!new_buf) return -1;
        
        writer->buffer = new_buf;
        writer->capacity = new_cap;
    }
    
    /* Build flags byte */
    uint8_t flags = 0;
    flags |= uft_jv3_code_from_size(size);
    if (side) flags |= UFT_JV3_SIDE_MASK;
    if (is_mfm) flags |= UFT_JV3_DENSITY_MASK;
    if (is_deleted) flags |= UFT_JV3_DAM_DELETED_F8;
    
    /* Add header entry */
    int idx = writer->entry_count;
    writer->buffer[idx * 3] = track;
    writer->buffer[idx * 3 + 1] = sector;
    writer->buffer[idx * 3 + 2] = flags;
    
    writer->entries[idx].track = track;
    writer->entries[idx].sector = sector;
    writer->entries[idx].flags = flags;
    writer->entry_count++;
    
    /* Add sector data */
    memcpy(writer->buffer + writer->data_offset, data, size);
    writer->data_offset += size;
    writer->used = writer->data_offset;
    
    return 0;
}

int uft_jv3_writer_finalize(uft_jv3_writer_t *writer,
                            uint8_t **data, size_t *size)
{
    if (!writer || !data || !size) return -1;
    
    *data = writer->buffer;
    *size = writer->used;
    
    /* Detach buffer from writer */
    writer->buffer = NULL;
    writer->capacity = 0;
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_jv3_report_json(const uft_jv3_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"JV3\",\n"
        "  \"total_sectors\": %d,\n"
        "  \"max_track\": %u,\n"
        "  \"max_sector\": %u,\n"
        "  \"sides\": %u,\n"
        "  \"fm_sectors\": %d,\n"
        "  \"mfm_sectors\": %d,\n"
        "  \"deleted_sectors\": %d,\n"
        "  \"crc_errors\": %d,\n"
        "  \"file_size\": %zu\n"
        "}",
        ctx->total_sectors,
        ctx->max_track,
        ctx->max_sector,
        ctx->sides,
        ctx->fm_sectors,
        ctx->mfm_sectors,
        ctx->deleted_sectors,
        ctx->crc_errors,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
