/**
 * @file uft_axdf.c
 * @brief AXDF Container Format Implementation
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#include "uft/core/uft_error_compat.h"
#include "uft/formats/uft_axdf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*===========================================================================
 * Internal Structures
 *===========================================================================*/

#define MAX_TRACKS 168
#define MAX_REPAIRS 1000

struct axdf_context_s {
    axdf_header_t header;
    axdf_options_t options;
    
    /* Track data */
    axdf_track_entry_t tracks[MAX_TRACKS];
    uint8_t *track_data[MAX_TRACKS];
    size_t track_sizes[MAX_TRACKS];
    
    /* Repair log */
    axdf_repair_entry_t repairs[MAX_REPAIRS];
    size_t repair_count;
    
    /* Error tracking */
    char last_error[256];
};

/*===========================================================================
 * CRC32 (for checksums)
 *===========================================================================*/

static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void) {
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

static uint32_t calc_crc32(const uint8_t *data, size_t len) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

axdf_context_t* axdf_create(void) {
    axdf_context_t *ctx = calloc(1, sizeof(axdf_context_t));
    if (!ctx) return NULL;
    
    /* Initialize header */
    memcpy(ctx->header.magic, AXDF_MAGIC, 4);
    ctx->header.version_major = AXDF_VERSION_MAJOR;
    ctx->header.version_minor = AXDF_VERSION_MINOR;
    ctx->header.header_size = AXDF_HEADER_SIZE;
    ctx->header.num_tracks = 80;
    ctx->header.num_sides = 2;
    ctx->header.sectors_per_track = 11;
    ctx->header.sector_size = 512;
    
    /* Default options */
    ctx->options.include_mfm = true;
    ctx->options.enable_repair = true;
    ctx->options.max_repair_bits = 2;
    ctx->options.max_revolutions = 5;
    
    return ctx;
}

void axdf_destroy(axdf_context_t *ctx) {
    if (!ctx) return;
    
    for (int i = 0; i < MAX_TRACKS; i++) {
        free(ctx->track_data[i]);
    }
    
    free(ctx);
}

int axdf_set_options(axdf_context_t *ctx, const axdf_options_t *options) {
    if (!ctx || !options) return -1;
    ctx->options = *options;
    return 0;
}

/*===========================================================================
 * Import Functions
 *===========================================================================*/

int axdf_import_adf(axdf_context_t *ctx, const char *adf_path) {
    if (!ctx || !adf_path) return -1;
    
    FILE *f = fopen(adf_path, "rb");
    if (!f) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Cannot open ADF file: %s", adf_path);
        return -1;
    }
    
    /* Get file size */
    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    /* Determine disk type */
    if (file_size == 901120) {
        ctx->header.disk_type = AXDF_DISK_DD_OFS;
        ctx->header.num_tracks = 80;
        ctx->header.sectors_per_track = 11;
    } else if (file_size == 1802240) {
        ctx->header.disk_type = AXDF_DISK_HD_OFS;
        ctx->header.num_tracks = 80;
        ctx->header.sectors_per_track = 22;
    } else {
        fclose(f);
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Unknown ADF size: %ld bytes", file_size);
        return -1;
    }
    
    /* Read track by track */
    int track_idx = 0;
    size_t sector_size = ctx->header.sector_size;
    size_t sectors_per_track = ctx->header.sectors_per_track;
    size_t track_size = sector_size * sectors_per_track;
    
    for (int track = 0; track < ctx->header.num_tracks; track++) {
        for (int side = 0; side < ctx->header.num_sides; side++) {
            ctx->track_data[track_idx] = malloc(track_size);
            if (!ctx->track_data[track_idx]) {
                fclose(f);
                return -1;
            }
            
            if (fread(ctx->track_data[track_idx], 1, track_size, f) != track_size) {
                fclose(f);
                snprintf(ctx->last_error, sizeof(ctx->last_error),
                         "Read error at track %d side %d", track, side);
                return -1;
            }
            
            /* Fill track entry */
            ctx->tracks[track_idx].track = (uint8_t)track;
            ctx->tracks[track_idx].side = (uint8_t)side;
            ctx->tracks[track_idx].flags = AXDF_TRK_HAS_SECTORS;
            ctx->tracks[track_idx].data_size = (uint32_t)track_size;
            ctx->tracks[track_idx].sector_count = (uint16_t)sectors_per_track;
            ctx->tracks[track_idx].checksum = calc_crc32(ctx->track_data[track_idx], track_size);
            ctx->track_sizes[track_idx] = track_size;
            
            track_idx++;
        }
    }
    
    fclose(f);
    
    /* Copy source info */
    strncpy(ctx->header.source_device, "ADF Import", sizeof(ctx->header.source_device) - 1);
    
    time_t now = time(NULL);
    struct tm *tm = gmtime(&now);
    strftime(ctx->header.source_date, sizeof(ctx->header.source_date),
             "%Y-%m-%dT%H:%M:%SZ", tm);
    
    return 0;
}

int axdf_import_flux(axdf_context_t *ctx, const char *flux_path) {
    if (!ctx || !flux_path) return -1;
    
    /* Placeholder - would integrate with SCP/KryoFlux loaders */
    snprintf(ctx->last_error, sizeof(ctx->last_error),
             "Flux import not yet implemented");
    return -1;
}

/*===========================================================================
 * Export Functions
 *===========================================================================*/

int axdf_export(axdf_context_t *ctx, const char *axdf_path) {
    if (!ctx || !axdf_path) return -1;
    
    FILE *f = fopen(axdf_path, "wb");
    if (!f) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Cannot create AXDF file: %s", axdf_path);
        return -1;
    }
    
    /* Calculate offsets */
    int total_tracks = ctx->header.num_tracks * ctx->header.num_sides;
    size_t track_table_size = total_tracks * sizeof(axdf_track_entry_t);
    
    ctx->header.track_table_offset = AXDF_HEADER_SIZE;
    ctx->header.track_table_size = (uint32_t)track_table_size;
    
    /* Align data offset */
    size_t data_offset = AXDF_HEADER_SIZE + track_table_size;
    data_offset = (data_offset + AXDF_ALIGNMENT - 1) & ~(AXDF_ALIGNMENT - 1);
    ctx->header.data_offset = (uint32_t)data_offset;
    
    /* Calculate total data size */
    size_t data_size = 0;
    for (int i = 0; i < total_tracks; i++) {
        data_size += ctx->track_sizes[i];
    }
    ctx->header.data_size = (uint32_t)data_size;
    ctx->header.file_size = (uint32_t)(data_offset + data_size);
    
    /* Write header */
    fwrite(&ctx->header, 1, AXDF_HEADER_SIZE, f);
    
    /* Write track table */
    uint32_t current_offset = 0;
    for (int i = 0; i < total_tracks; i++) {
        ctx->tracks[i].data_offset = current_offset;
        current_offset += (uint32_t)ctx->track_sizes[i];
    }
    fwrite(ctx->tracks, sizeof(axdf_track_entry_t), total_tracks, f);
    
    /* Pad to alignment */
    size_t padding = data_offset - ftell(f);
    if (padding > 0) {
        uint8_t *pad = calloc(1, padding);
        fwrite(pad, 1, padding, f);
        free(pad);
    }
    
    /* Write track data */
    for (int i = 0; i < total_tracks; i++) {
        if (ctx->track_data[i] && ctx->track_sizes[i] > 0) {
            fwrite(ctx->track_data[i], 1, ctx->track_sizes[i], f);
        }
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

int axdf_export_adf(axdf_context_t *ctx, const char *adf_path) {
    if (!ctx || !adf_path) return -1;
    
    FILE *f = fopen(adf_path, "wb");
    if (!f) {
        snprintf(ctx->last_error, sizeof(ctx->last_error),
                 "Cannot create ADF file: %s", adf_path);
        return -1;
    }
    
    /* Write all track data sequentially */
    int total_tracks = ctx->header.num_tracks * ctx->header.num_sides;
    for (int i = 0; i < total_tracks; i++) {
        if (ctx->track_data[i] && ctx->track_sizes[i] > 0) {
            fwrite(ctx->track_data[i], 1, ctx->track_sizes[i], f);
        }
    }
    if (ferror(f)) {
        fclose(f);
        return UFT_ERR_IO;
    }
    
        
    fclose(f);
return 0;
}

/*===========================================================================
 * Validation
 *===========================================================================*/

int axdf_validate(axdf_context_t *ctx) {
    if (!ctx) return -1;
    
    int errors = 0;
    int total_tracks = ctx->header.num_tracks * ctx->header.num_sides;
    
    for (int i = 0; i < total_tracks; i++) {
        if (!ctx->track_data[i]) continue;
        
        uint32_t computed = calc_crc32(ctx->track_data[i], ctx->track_sizes[i]);
        if (computed != ctx->tracks[i].checksum) {
            ctx->tracks[i].flags |= AXDF_TRK_HAS_ERRORS;
            ctx->tracks[i].error_count++;
            errors++;
            
            if (ctx->options.on_sector_error) {
                ctx->options.on_sector_error(ctx->tracks[i].track, 
                                             ctx->tracks[i].side,
                                             -1, AXDF_SEC_CRC_ERROR,
                                             ctx->options.user_data);
            }
        }
    }
    
    return errors;
}

/*===========================================================================
 * Repair Functions
 *===========================================================================*/

int axdf_repair_crc_1bit(uint8_t *sector_data, size_t size,
                          uint32_t original_crc, uint32_t *new_crc) {
    if (!sector_data || size == 0) return -1;
    
    /* Try flipping each bit */
    for (size_t byte = 0; byte < size; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            /* Flip bit */
            sector_data[byte] ^= (1 << bit);
            
            /* Check CRC */
            uint32_t crc = calc_crc32(sector_data, size);
            if (crc == original_crc || (new_crc && *new_crc == 0)) {
                /* Found it! */
                if (new_crc) *new_crc = crc;
                return 0;
            }
            
            /* Flip back */
            sector_data[byte] ^= (1 << bit);
        }
    }
    
    return -1;  /* No single-bit fix found */
}

int axdf_repair_crc_2bit(uint8_t *sector_data, size_t size,
                          uint32_t original_crc, uint32_t *new_crc) {
    if (!sector_data || size == 0) return -1;
    
    /* Try flipping each pair of bits */
    /* Warning: O(n²) - can be slow for large sectors */
    size_t total_bits = size * 8;
    
    for (size_t bit1 = 0; bit1 < total_bits; bit1++) {
        /* Flip first bit */
        sector_data[bit1 / 8] ^= (1 << (bit1 % 8));
        
        for (size_t bit2 = bit1 + 1; bit2 < total_bits; bit2++) {
            /* Flip second bit */
            sector_data[bit2 / 8] ^= (1 << (bit2 % 8));
            
            /* Check CRC */
            uint32_t crc = calc_crc32(sector_data, size);
            if (crc == original_crc) {
                if (new_crc) *new_crc = crc;
                return 0;
            }
            
            /* Flip back second bit */
            sector_data[bit2 / 8] ^= (1 << (bit2 % 8));
        }
        
        /* Flip back first bit */
        sector_data[bit1 / 8] ^= (1 << (bit1 % 8));
    }
    
    return -1;  /* No two-bit fix found */
}

int axdf_repair_multi_rev(const uint8_t **revolutions, int count,
                           const float *confidence, size_t size,
                           uint8_t *output, float *output_confidence) {
    if (!revolutions || count < 2 || !output) return -1;
    
    for (size_t byte = 0; byte < size; byte++) {
        for (int bit = 0; bit < 8; bit++) {
            /* Vote on each bit */
            int votes_for_1 = 0;
            float total_confidence = 0.0f;
            
            for (int rev = 0; rev < count; rev++) {
                int bit_val = (revolutions[rev][byte] >> bit) & 1;
                float conf = confidence ? confidence[rev * size + byte] : 1.0f;
                
                if (bit_val) {
                    votes_for_1 += (int)(conf * 100);
                }
                total_confidence += conf;
            }
            
            /* Set bit based on vote */
            if (votes_for_1 * 2 > count * 100) {
                output[byte] |= (1 << bit);
            } else {
                output[byte] &= ~(1 << bit);
            }
            
            if (output_confidence) {
                output_confidence[byte] = total_confidence / count;
            }
        }
    }
    
    return 0;
}

int axdf_repair_interpolate(uint8_t *data, size_t size,
                             const uint8_t *weak_mask) {
    if (!data || size == 0) return -1;
    
    /* For each weak byte, try to interpolate from neighbors */
    for (size_t i = 0; i < size; i++) {
        if (weak_mask && !(weak_mask[i])) continue;  /* Not weak */
        
        /* Simple interpolation: average of neighbors */
        uint8_t prev = (i > 0) ? data[i - 1] : 0;
        uint8_t next = (i < size - 1) ? data[i + 1] : 0;
        
        /* For each bit, use neighbor majority */
        uint8_t result = 0;
        for (int bit = 0; bit < 8; bit++) {
            int prev_bit = (prev >> bit) & 1;
            int next_bit = (next >> bit) & 1;
            
            if (prev_bit && next_bit) {
                result |= (1 << bit);
            }
        }
        
        data[i] = result;
    }
    
    return 0;
}

int axdf_repair(axdf_context_t *ctx) {
    if (!ctx || !ctx->options.enable_repair) return -1;
    
    int total_repairs = 0;
    int total_tracks = ctx->header.num_tracks * ctx->header.num_sides;
    
    for (int i = 0; i < total_tracks; i++) {
        if (!(ctx->tracks[i].flags & AXDF_TRK_HAS_ERRORS)) continue;
        if (!ctx->track_data[i]) continue;
        
        /* Try 1-bit repair */
        uint32_t new_crc;
        if (ctx->options.max_repair_bits >= 1) {
            if (axdf_repair_crc_1bit(ctx->track_data[i], ctx->track_sizes[i],
                                      ctx->tracks[i].checksum, &new_crc) == 0) {
                /* Record repair */
                if (ctx->repair_count < MAX_REPAIRS) {
                    axdf_repair_entry_t *entry = &ctx->repairs[ctx->repair_count++];
                    entry->timestamp = (uint32_t)time(NULL);
                    entry->track = ctx->tracks[i].track;
                    entry->side = ctx->tracks[i].side;
                    entry->repair_type = AXDF_REPAIR_CRC_1BIT;
                    entry->bits_corrected = 1;
                    entry->original_crc = ctx->tracks[i].checksum;
                    entry->repaired_crc = new_crc;
                    strncpy(entry->method, "1-bit CRC correction", 31);
                    
                    if (ctx->options.on_repair) {
                        ctx->options.on_repair(entry, ctx->options.user_data);
                    }
                }
                
                ctx->tracks[i].flags |= AXDF_TRK_REPAIRED;
                ctx->tracks[i].flags &= ~AXDF_TRK_HAS_ERRORS;
                ctx->tracks[i].checksum = new_crc;
                total_repairs++;
                continue;
            }
        }
        
        /* Try 2-bit repair */
        if (ctx->options.max_repair_bits >= 2) {
            if (axdf_repair_crc_2bit(ctx->track_data[i], ctx->track_sizes[i],
                                      ctx->tracks[i].checksum, &new_crc) == 0) {
                if (ctx->repair_count < MAX_REPAIRS) {
                    axdf_repair_entry_t *entry = &ctx->repairs[ctx->repair_count++];
                    entry->timestamp = (uint32_t)time(NULL);
                    entry->track = ctx->tracks[i].track;
                    entry->side = ctx->tracks[i].side;
                    entry->repair_type = AXDF_REPAIR_CRC_2BIT;
                    entry->bits_corrected = 2;
                    strncpy(entry->method, "2-bit CRC correction", 31);
                }
                
                ctx->tracks[i].flags |= AXDF_TRK_REPAIRED;
                ctx->tracks[i].flags &= ~AXDF_TRK_HAS_ERRORS;
                ctx->tracks[i].checksum = new_crc;
                total_repairs++;
            }
        }
    }
    
    ctx->header.sectors_repaired = (uint16_t)total_repairs;
    ctx->header.repair_flags |= (1 << 0);  /* Repair attempted */
    ctx->header.repair_date = (uint32_t)time(NULL);
    
    return total_repairs;
}

/*===========================================================================
 * Getters
 *===========================================================================*/

int axdf_get_track(axdf_context_t *ctx, int track, int side,
                   axdf_track_entry_t *info, uint8_t **data, size_t *size) {
    if (!ctx) return -1;
    
    int idx = track * ctx->header.num_sides + side;
    if (idx < 0 || idx >= MAX_TRACKS) return -1;
    
    if (info) *info = ctx->tracks[idx];
    if (data) *data = ctx->track_data[idx];
    if (size) *size = ctx->track_sizes[idx];
    
    return 0;
}

int axdf_get_sector(axdf_context_t *ctx, int track, int side, int sector,
                    axdf_sector_header_t *info, uint8_t **data, size_t *size) {
    if (!ctx) return -1;
    
    int idx = track * ctx->header.num_sides + side;
    if (idx < 0 || idx >= MAX_TRACKS) return -1;
    if (sector < 0 || sector >= ctx->header.sectors_per_track) return -1;
    
    size_t sector_size = ctx->header.sector_size;
    size_t offset = sector * sector_size;
    
    if (info) {
        info->sector = (uint8_t)sector;
        info->status = AXDF_SEC_OK;
        info->size = (uint16_t)sector_size;
    }
    
    if (data && ctx->track_data[idx]) {
        *data = ctx->track_data[idx] + offset;
    }
    
    if (size) *size = sector_size;
    
    return 0;
}

int axdf_get_repairs(axdf_context_t *ctx, axdf_repair_entry_t **entries,
                     size_t *count) {
    if (!ctx || !entries || !count) return -1;
    
    *entries = ctx->repairs;
    *count = ctx->repair_count;
    return 0;
}

int axdf_get_error_summary(axdf_context_t *ctx, int *total_errors,
                           int *repaired, int *unreadable) {
    if (!ctx) return -1;
    
    int errors = 0, fixed = 0, unread = 0;
    int total_tracks = ctx->header.num_tracks * ctx->header.num_sides;
    
    for (int i = 0; i < total_tracks; i++) {
        if (ctx->tracks[i].flags & AXDF_TRK_HAS_ERRORS) {
            errors += ctx->tracks[i].error_count;
            unread += ctx->tracks[i].error_count;
        }
        if (ctx->tracks[i].flags & AXDF_TRK_REPAIRED) {
            fixed++;
        }
    }
    
    if (total_errors) *total_errors = errors;
    if (repaired) *repaired = fixed;
    if (unreadable) *unreadable = unread - fixed;
    
    return 0;
}

const axdf_header_t* axdf_get_header(const axdf_context_t *ctx) {
    return ctx ? &ctx->header : NULL;
}

const char* axdf_get_error(const axdf_context_t *ctx) {
    return ctx ? ctx->last_error : "NULL context";
}
