/**
 * @file uft_atx_writer.c
 * @brief ATX Format Write Support implementation
 * 
 * P2-005: Complete ATX write implementation
 */

#include "uft/formats/uft_atx_writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* Atari disk constants */
#define ATX_ANGULAR_UNITS       26042   /* Angular units per rotation */
#define ATX_SD_SECTORS          18      /* Single density sectors */
#define ATX_ED_SECTORS          26      /* Enhanced density sectors */
#define ATX_DD_SECTORS          18      /* Double density sectors */
#define ATX_SD_SECTOR_SIZE      128
#define ATX_DD_SECTOR_SIZE      256

/* ============================================================================
 * Initialization
 * ============================================================================ */

void atx_write_options_init(atx_write_options_t *opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->density = 0;  /* SD */
    opts->preserve_timing = true;
    opts->preserve_weak_bits = true;
    opts->preserve_errors = true;
    opts->default_sector_time_us = 1040;  /* ~1ms for SD */
    opts->rpm = 288;
}

void atx_writer_init(atx_writer_ctx_t *ctx) {
    if (!ctx) return;
    
    memset(ctx, 0, sizeof(*ctx));
    atx_write_options_init(&ctx->opts);
}

void atx_writer_free(atx_writer_ctx_t *ctx) {
    if (!ctx) return;
    
    for (int t = 0; t < ctx->track_count; t++) {
        free(ctx->tracks[t].sectors);
        ctx->tracks[t].sectors = NULL;
    }
    
    ctx->track_count = 0;
}

/* ============================================================================
 * Track/Sector Building
 * ============================================================================ */

int atx_writer_add_track(atx_writer_ctx_t *ctx, uint8_t track_num) {
    if (!ctx || track_num >= UFT_ATX_MAX_TRACKS) {
        return -1;
    }
    
    /* Check if track already exists */
    for (int t = 0; t < ctx->track_count; t++) {
        if (ctx->tracks[t].track_number == track_num) {
            return t;  /* Already exists */
        }
    }
    
    if (ctx->track_count >= UFT_ATX_MAX_TRACKS) {
        return -1;
    }
    
    int idx = ctx->track_count++;
    atx_write_track_t *track = &ctx->tracks[idx];
    
    track->track_number = track_num;
    track->side = 0;
    track->flags = 0;
    track->sector_count = 0;
    track->sector_capacity = UFT_ATX_MAX_SECTORS;
    track->sectors = calloc(track->sector_capacity, sizeof(atx_write_sector_t));
    
    if (!track->sectors) {
        ctx->track_count--;
        return -1;
    }
    
    return idx;
}

static atx_write_track_t* find_track(atx_writer_ctx_t *ctx, uint8_t track_num) {
    for (int t = 0; t < ctx->track_count; t++) {
        if (ctx->tracks[t].track_number == track_num) {
            return &ctx->tracks[t];
        }
    }
    return NULL;
}

int atx_writer_add_sector(atx_writer_ctx_t *ctx, uint8_t track_num,
                          const atx_write_sector_t *sector) {
    if (!ctx || !sector) return -1;
    
    atx_write_track_t *track = find_track(ctx, track_num);
    if (!track) {
        int idx = atx_writer_add_track(ctx, track_num);
        if (idx < 0) return -1;
        track = &ctx->tracks[idx];
    }
    
    if (track->sector_count >= track->sector_capacity) {
        return -1;
    }
    
    /* Copy sector */
    atx_write_sector_t *dest = &track->sectors[track->sector_count++];
    memcpy(dest, sector, sizeof(atx_write_sector_t));
    
    /* Update statistics */
    ctx->total_sectors++;
    if (sector->status & (UFT_ATX_FDC_CRC_ERROR | UFT_ATX_FDC_RNF)) {
        ctx->error_sectors++;
    }
    if (sector->weak_mask) {
        ctx->weak_sectors++;
        track->flags |= ATX_TRACK_HAS_WEAK;
    }
    if (sector->extended_data) {
        track->flags |= ATX_TRACK_HAS_LONG;
    }
    
    return 0;
}

int atx_writer_add_sector_simple(atx_writer_ctx_t *ctx, uint8_t track_num,
                                 uint8_t sector_num, const uint8_t *data,
                                 uint16_t data_size, uint8_t status) {
    atx_write_sector_t sector = {0};
    sector.number = sector_num;
    sector.status = status;
    sector.data = data;
    sector.data_size = data_size;
    sector.position = atx_sector_position(sector_num, 
        ctx->opts.density == 1 ? ATX_ED_SECTORS : ATX_SD_SECTORS);
    sector.timing_us = atx_default_timing(ctx->opts.density);
    
    return atx_writer_add_sector(ctx, track_num, &sector);
}

int atx_writer_add_sector_timed(atx_writer_ctx_t *ctx, uint8_t track_num,
                                uint8_t sector_num, uint16_t position,
                                const uint8_t *data, uint16_t data_size,
                                uint32_t timing_us, uint8_t status) {
    atx_write_sector_t sector = {0};
    sector.number = sector_num;
    sector.status = status;
    sector.position = position;
    sector.data = data;
    sector.data_size = data_size;
    sector.timing_us = timing_us;
    
    return atx_writer_add_sector(ctx, track_num, &sector);
}

int atx_writer_add_sector_weak(atx_writer_ctx_t *ctx, uint8_t track_num,
                               uint8_t sector_num, const uint8_t *data,
                               uint16_t data_size, const uint8_t *weak_mask,
                               uint16_t weak_offset, uint16_t weak_length) {
    atx_write_sector_t sector = {0};
    sector.number = sector_num;
    sector.status = UFT_ATX_STATUS_WEAK;
    sector.data = data;
    sector.data_size = data_size;
    sector.position = atx_sector_position(sector_num,
        ctx->opts.density == 1 ? ATX_ED_SECTORS : ATX_SD_SECTORS);
    sector.timing_us = atx_default_timing(ctx->opts.density);
    sector.weak_mask = weak_mask;
    sector.weak_offset = weak_offset;
    sector.weak_length = weak_length;
    
    return atx_writer_add_sector(ctx, track_num, &sector);
}

int atx_writer_add_phantom(atx_writer_ctx_t *ctx, uint8_t track_num,
                           uint8_t sector_num, uint16_t position) {
    atx_write_sector_t sector = {0};
    sector.number = sector_num;
    sector.status = UFT_ATX_FDC_RNF;  /* Record not found */
    sector.position = position;
    sector.data = NULL;
    sector.data_size = 0;
    sector.extended_type = UFT_ATX_EXT_PHANTOM;
    
    atx_write_track_t *track = find_track(ctx, track_num);
    if (track) {
        track->flags |= ATX_TRACK_HAS_PHANTOM;
    }
    
    return atx_writer_add_sector(ctx, track_num, &sector);
}

int atx_writer_auto_positions(atx_writer_ctx_t *ctx, uint8_t track_num) {
    atx_write_track_t *track = find_track(ctx, track_num);
    if (!track || track->sector_count == 0) return -1;
    
    uint16_t spacing = ATX_ANGULAR_UNITS / track->sector_count;
    
    for (int s = 0; s < track->sector_count; s++) {
        /* Use sector number for interleave */
        int pos_idx = (track->sectors[s].number - 1) % track->sector_count;
        track->sectors[s].position = pos_idx * spacing;
    }
    
    return 0;
}

/* ============================================================================
 * Writing
 * ============================================================================ */

size_t atx_writer_calculate_size(const atx_writer_ctx_t *ctx) {
    if (!ctx) return 0;
    
    size_t size = sizeof(uft_atx_header_t);
    
    for (int t = 0; t < ctx->track_count; t++) {
        const atx_write_track_t *track = &ctx->tracks[t];
        
        /* Track header */
        size += sizeof(uft_atx_track_header_t);
        
        /* Sector headers */
        size += track->sector_count * sizeof(uft_atx_sector_header_t);
        
        /* Sector data */
        for (int s = 0; s < track->sector_count; s++) {
            const atx_write_sector_t *sect = &track->sectors[s];
            size += sect->data_size;
            
            /* Extended data */
            if (sect->weak_mask) {
                size += sizeof(uft_atx_extended_header_t) + sect->weak_length;
            }
            if (sect->extended_data) {
                size += sizeof(uft_atx_extended_header_t) + sect->extended_size;
            }
        }
        
        /* Alignment */
        size = (size + 3) & ~3;
    }
    
    return size;
}

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

uft_error_t atx_writer_write(const atx_writer_ctx_t *ctx,
                             uint8_t *buffer, size_t buffer_size,
                             size_t *out_size) {
    if (!ctx || !buffer || !out_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t required = atx_writer_calculate_size(ctx);
    if (buffer_size < required) {
        return UFT_ERR_TRACK_OVERFLOW;
    }
    
    memset(buffer, 0, required);
    size_t pos = 0;
    
    /* Write file header */
    uft_atx_header_t *header = (uft_atx_header_t*)buffer;
    write_le32((uint8_t*)&header->magic, UFT_ATX_MAGIC);
    write_le16((uint8_t*)&header->version, UFT_ATX_VERSION);
    write_le16((uint8_t*)&header->min_version, UFT_ATX_VERSION);
    write_le16((uint8_t*)&header->creator, ATX_CREATOR_UFT);
    write_le16((uint8_t*)&header->creator_version, ATX_CREATOR_VERSION);
    header->density = ctx->opts.density;
    write_le32((uint8_t*)&header->image_id, 
               ctx->opts.image_id ? ctx->opts.image_id : (uint32_t)time(NULL));
    write_le32((uint8_t*)&header->start_offset, sizeof(uft_atx_header_t));
    
    pos = sizeof(uft_atx_header_t);
    
    /* Write tracks */
    for (int t = 0; t < ctx->track_count; t++) {
        const atx_write_track_t *track = &ctx->tracks[t];
        size_t track_start = pos;
        
        /* Track header */
        uft_atx_track_header_t *thdr = (uft_atx_track_header_t*)(buffer + pos);
        thdr->type = 0;  /* Track record */
        thdr->track_number = track->track_number;
        thdr->side = track->side;
        write_le16((uint8_t*)&thdr->sector_count, track->sector_count);
        write_le32((uint8_t*)&thdr->flags, track->flags);
        write_le32((uint8_t*)&thdr->header_size, 
                   track->sector_count * sizeof(uft_atx_sector_header_t));
        
        pos += sizeof(uft_atx_track_header_t);
        
        /* Sector headers */
        size_t sector_headers_pos = pos;
        pos += track->sector_count * sizeof(uft_atx_sector_header_t);
        
        /* Sector data */
        size_t data_start = pos;
        
        for (int s = 0; s < track->sector_count; s++) {
            const atx_write_sector_t *sect = &track->sectors[s];
            
            /* Write sector header */
            uft_atx_sector_header_t *shdr = 
                (uft_atx_sector_header_t*)(buffer + sector_headers_pos + 
                                           s * sizeof(uft_atx_sector_header_t));
            shdr->number = sect->number;
            shdr->status = sect->status;
            write_le16((uint8_t*)&shdr->position, sect->position);
            write_le32((uint8_t*)&shdr->start_data, (uint32_t)(pos - data_start));
            
            /* Write sector data */
            if (sect->data && sect->data_size > 0) {
                memcpy(buffer + pos, sect->data, sect->data_size);
                pos += sect->data_size;
            }
            
            /* Write weak bit extended data */
            if (sect->weak_mask && sect->weak_length > 0) {
                uft_atx_extended_header_t *ext = 
                    (uft_atx_extended_header_t*)(buffer + pos);
                write_le32((uint8_t*)&ext->size, 
                           sizeof(uft_atx_extended_header_t) + sect->weak_length);
                ext->type = UFT_ATX_EXT_WEAK_BITS;
                ext->sector_index = s;
                pos += sizeof(uft_atx_extended_header_t);
                
                memcpy(buffer + pos, sect->weak_mask, sect->weak_length);
                pos += sect->weak_length;
            }
            
            /* Write other extended data */
            if (sect->extended_data && sect->extended_size > 0) {
                uft_atx_extended_header_t *ext = 
                    (uft_atx_extended_header_t*)(buffer + pos);
                write_le32((uint8_t*)&ext->size,
                           sizeof(uft_atx_extended_header_t) + sect->extended_size);
                ext->type = sect->extended_type;
                ext->sector_index = s;
                pos += sizeof(uft_atx_extended_header_t);
                
                memcpy(buffer + pos, sect->extended_data, sect->extended_size);
                pos += sect->extended_size;
            }
        }
        
        /* Align to 4 bytes */
        pos = (pos + 3) & ~3;
        
        /* Update track size */
        write_le32((uint8_t*)&thdr->size, (uint32_t)(pos - track_start));
    }
    
    /* Update end offset */
    write_le32((uint8_t*)&header->end_offset, (uint32_t)pos);
    
    *out_size = pos;
    return UFT_OK;
}

uft_error_t atx_writer_write_file(const atx_writer_ctx_t *ctx,
                                  const char *path) {
    if (!ctx || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t size = atx_writer_calculate_size(ctx);
    uint8_t *buffer = malloc(size);
    if (!buffer) {
        return UFT_ERR_MEMORY;
    }
    
    size_t written;
    uft_error_t err = atx_writer_write(ctx, buffer, size, &written);
    if (err != UFT_OK) {
        free(buffer);
        return err;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(buffer);
        return UFT_ERR_IO;
    }
    
    if (fwrite(buffer, 1, written, fp) != written) {
        fclose(fp);
        free(buffer);
        return UFT_ERR_IO;
    }
    
    fclose(fp);
    free(buffer);
    return UFT_OK;
}

/* ============================================================================
 * Conversion
 * ============================================================================ */

uft_error_t uft_atr_to_atx(const uint8_t *atr_data, size_t atr_size,
                           uint8_t *atx_buffer, size_t buffer_size,
                           size_t *out_size,
                           const atx_write_options_t *opts) {
    if (!atr_data || atr_size < 16 || !atx_buffer || !out_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Parse ATR header */
    uint16_t magic = atr_data[0] | (atr_data[1] << 8);
    if (magic != 0x0296) {
        return UFT_ERR_CORRUPT;
    }
    
    uint16_t paragraphs = atr_data[2] | (atr_data[3] << 8);
    uint16_t sector_size = atr_data[4] | (atr_data[5] << 8);
    uint8_t paragraphs_hi = atr_data[6];
    
    uint32_t image_size = ((uint32_t)paragraphs_hi << 16 | paragraphs) * 16;
    
    /* Determine density */
    uint8_t density = 0;
    uint8_t sectors_per_track = ATX_SD_SECTORS;
    
    if (sector_size == 256) {
        density = 2;  /* DD */
    } else if (sector_size == 128 && image_size > 720 * 18 * 128) {
        density = 1;  /* ED */
        sectors_per_track = ATX_ED_SECTORS;
    }
    
    /* Create writer */
    atx_writer_ctx_t ctx;
    atx_writer_init(&ctx);
    
    if (opts) {
        ctx.opts = *opts;
    }
    ctx.opts.density = density;
    
    /* Add sectors */
    size_t data_pos = 16;  /* ATR header size */
    uint8_t track = 1;
    uint8_t sector = 1;
    
    while (data_pos + sector_size <= atr_size && track <= 40) {
        /* First 3 sectors are always 128 bytes in DD ATR */
        uint16_t actual_size = sector_size;
        if (density == 2 && track == 1 && sector <= 3) {
            actual_size = 128;
        }
        
        atx_writer_add_sector_simple(&ctx, track, sector,
                                     atr_data + data_pos, actual_size, 0);
        
        data_pos += actual_size;
        sector++;
        
        if (sector > sectors_per_track) {
            sector = 1;
            track++;
        }
    }
    
    /* Write ATX */
    uft_error_t err = atx_writer_write(&ctx, atx_buffer, buffer_size, out_size);
    atx_writer_free(&ctx);
    
    return err;
}

uft_error_t uft_disk_to_atx(const uft_disk_image_t *disk,
                            uint8_t *atx_buffer, size_t buffer_size,
                            size_t *out_size,
                            const atx_write_options_t *opts) {
    if (!disk || !atx_buffer || !out_size) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    atx_writer_ctx_t ctx;
    atx_writer_init(&ctx);
    
    if (opts) {
        ctx.opts = *opts;
    }
    
    /* Determine density from sector size */
    if (disk->bytes_per_sector == 256) {
        ctx.opts.density = 2;
    } else if (disk->sectors_per_track > 18) {
        ctx.opts.density = 1;
    }
    
    /* Convert tracks */
    for (size_t t = 0; t < disk->track_count && t < UFT_ATX_MAX_TRACKS; t++) {
        uft_track_t *track = disk->track_data[t];
        if (!track) continue;
        
        uint8_t track_num = track->track_num + 1;  /* ATX uses 1-based */
        
        for (size_t s = 0; s < track->sector_count; s++) {
            uft_sector_t *sect = &track->sectors[s];
            
            uint8_t status = 0;
            if (sect->id.status & UFT_SECTOR_CRC_ERROR) {
                status |= UFT_ATX_FDC_CRC_ERROR;
            }
            if (sect->id.status & UFT_SECTOR_DELETED) {
                status |= UFT_ATX_FDC_DELETED;
            }
            if (sect->id.status & UFT_SECTOR_MISSING) {
                status |= UFT_ATX_FDC_RNF;
            }
            
            atx_writer_add_sector_simple(&ctx, track_num,
                                         sect->id.sector,
                                         sect->data, sect->data_len,
                                         status);
        }
    }
    
    uft_error_t err = atx_writer_write(&ctx, atx_buffer, buffer_size, out_size);
    atx_writer_free(&ctx);
    
    return err;
}

uft_error_t uft_atx_write(const uft_disk_image_t *disk,
                          const char *path,
                          const atx_write_options_t *opts) {
    if (!disk || !path) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    size_t est_size = disk->track_count * 26 * 256 + 4096;
    uint8_t *buffer = malloc(est_size);
    if (!buffer) {
        return UFT_ERR_MEMORY;
    }
    
    size_t written;
    uft_error_t err = uft_disk_to_atx(disk, buffer, est_size, &written, opts);
    if (err != UFT_OK) {
        free(buffer);
        return err;
    }
    
    FILE *fp = fopen(path, "wb");
    if (!fp) {
        free(buffer);
        return UFT_ERR_IO;
    }
    
    if (fwrite(buffer, 1, written, fp) != written) {
        fclose(fp);
        free(buffer);
        return UFT_ERR_IO;
    }
    
    fclose(fp);
    free(buffer);
    return UFT_OK;
}

/* ============================================================================
 * Timing Helpers
 * ============================================================================ */

uint16_t atx_sector_position(uint8_t sector, uint8_t sectors_per_track) {
    if (sectors_per_track == 0) sectors_per_track = 18;
    
    /* Standard interleave for Atari */
    int pos_idx = ((sector - 1) * 7) % sectors_per_track;
    return (pos_idx * ATX_ANGULAR_UNITS) / sectors_per_track;
}

uint32_t atx_default_timing(uint8_t density) {
    switch (density) {
        case 0: return 1040;   /* SD: ~1ms */
        case 1: return 720;    /* ED: ~0.7ms */
        case 2: return 520;    /* DD: ~0.5ms */
        default: return 1040;
    }
}

uint32_t atx_timing_variation(uint32_t base_timing, int variation_percent) {
    int variation = (base_timing * variation_percent) / 100;
    return base_timing + (rand() % (variation * 2 + 1)) - variation;
}

/* ============================================================================
 * Weak Bit Helpers
 * ============================================================================ */

void atx_make_weak_mask_full(uint8_t *mask, uint16_t sector_size) {
    if (!mask) return;
    memset(mask, 0xFF, sector_size);
}

void atx_make_weak_mask_region(uint8_t *mask, uint16_t sector_size,
                               uint16_t start, uint16_t length) {
    if (!mask || start >= sector_size) return;
    
    memset(mask, 0, sector_size);
    
    if (start + length > sector_size) {
        length = sector_size - start;
    }
    
    memset(mask + start, 0xFF, length);
}

void atx_apply_weak_bits(uint8_t *data, const uint8_t *mask, uint16_t size) {
    if (!data || !mask) return;
    
    for (uint16_t i = 0; i < size; i++) {
        if (mask[i]) {
            /* Apply random bits where mask is set */
            data[i] ^= (rand() & mask[i]);
        }
    }
}
