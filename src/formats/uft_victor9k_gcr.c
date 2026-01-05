/**
 * @file uft_victor9k_gcr.c
 * @brief Victor 9000 GCR Format Implementation
 * 
 * EXT3-001: Variable-density GCR support for Victor 9000/Sirius 1
 * 
 * The Victor 9000 used a unique variable-density GCR format with:
 * - 8 speed zones (different RPM per zone)
 * - 12-19 sectors per track depending on zone
 * - 512 bytes per sector
 * - GCR 5:4 encoding
 */

#include "uft/formats/uft_victor9k_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Zone Configuration
 *===========================================================================*/

/* Victor 9000 zone definitions */
static const struct {
    uint8_t start_track;
    uint8_t end_track;
    uint8_t sectors_per_track;
    uint32_t data_rate;         /* bits per second */
    uint16_t rpm;               /* approximate RPM */
} VICTOR_ZONES[] = {
    {  0,   3, 19, 394000, 252 },   /* Zone 0: Tracks 0-3 */
    {  4,  15, 18, 375000, 267 },   /* Zone 1: Tracks 4-15 */
    { 16,  26, 17, 356000, 283 },   /* Zone 2: Tracks 16-26 */
    { 27,  37, 16, 338000, 300 },   /* Zone 3: Tracks 27-37 */
    { 38,  47, 15, 319000, 320 },   /* Zone 4: Tracks 38-47 */
    { 48,  59, 14, 300000, 340 },   /* Zone 5: Tracks 48-59 */
    { 60,  70, 13, 281000, 362 },   /* Zone 6: Tracks 60-70 */
    { 71,  79, 12, 262000, 387 },   /* Zone 7: Tracks 71-79 */
};

#define NUM_ZONES 8

/* GCR encoding table (5 bits -> 4 bits) */
static const uint8_t GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07: invalid */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF,  /* 18-1F */
};

static const uint8_t GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* Sync pattern */
static const uint8_t VICTOR_SYNC[] = { 0xFF, 0xFF, 0xFF, 0x08 };

/*===========================================================================
 * Zone Helpers
 *===========================================================================*/

int uft_victor9k_get_zone(uint8_t track)
{
    for (int z = 0; z < NUM_ZONES; z++) {
        if (track >= VICTOR_ZONES[z].start_track && 
            track <= VICTOR_ZONES[z].end_track) {
            return z;
        }
    }
    return -1;
}

int uft_victor9k_sectors_per_track(uint8_t track)
{
    int zone = uft_victor9k_get_zone(track);
    if (zone < 0) return -1;
    return VICTOR_ZONES[zone].sectors_per_track;
}

uint32_t uft_victor9k_data_rate(uint8_t track)
{
    int zone = uft_victor9k_get_zone(track);
    if (zone < 0) return 0;
    return VICTOR_ZONES[zone].data_rate;
}

/*===========================================================================
 * LBA Conversion
 *===========================================================================*/

int uft_victor9k_lba_to_chs(uint32_t lba, uint8_t *track, uint8_t *sector, uint8_t *side)
{
    if (!track || !sector || !side) return -1;
    
    /* Calculate total sectors before each track */
    uint32_t sector_count = 0;
    
    for (int t = 0; t < 80; t++) {
        int spt = uft_victor9k_sectors_per_track(t);
        if (spt < 0) return -1;
        
        /* Check both sides */
        for (int s = 0; s < 2; s++) {
            if (lba < sector_count + spt) {
                *track = t;
                *side = s;
                *sector = lba - sector_count;
                return 0;
            }
            sector_count += spt;
        }
    }
    
    return -1;  /* LBA out of range */
}

uint32_t uft_victor9k_chs_to_lba(uint8_t track, uint8_t sector, uint8_t side)
{
    uint32_t lba = 0;
    
    for (int t = 0; t < track; t++) {
        int spt = uft_victor9k_sectors_per_track(t);
        if (spt < 0) return 0;
        lba += spt * 2;  /* Both sides */
    }
    
    int spt = uft_victor9k_sectors_per_track(track);
    if (spt < 0) return 0;
    
    if (side > 0) {
        lba += spt;
    }
    lba += sector;
    
    return lba;
}

uint32_t uft_victor9k_total_sectors(void)
{
    uint32_t total = 0;
    for (int t = 0; t < 80; t++) {
        int spt = uft_victor9k_sectors_per_track(t);
        if (spt > 0) {
            total += spt * 2;  /* Both sides */
        }
    }
    return total;
}

/*===========================================================================
 * GCR Encoding/Decoding
 *===========================================================================*/

int uft_victor9k_gcr_decode(const uint8_t *gcr, size_t gcr_size,
                            uint8_t *data, size_t *data_size)
{
    if (!gcr || !data || !data_size) return -1;
    
    /* GCR: 5 bytes -> 4 bytes */
    size_t out_size = (gcr_size * 4) / 5;
    if (*data_size < out_size) {
        *data_size = out_size;
        return -1;
    }
    
    size_t out_idx = 0;
    
    for (size_t i = 0; i + 5 <= gcr_size; i += 5) {
        /* Combine 5 GCR bytes into 40-bit value */
        uint64_t gcr_val = 0;
        for (int j = 0; j < 5; j++) {
            gcr_val = (gcr_val << 8) | gcr[i + j];
        }
        
        /* Extract 8 nybbles (4 bytes) */
        for (int n = 7; n >= 0; n--) {
            uint8_t nybble = (gcr_val >> (n * 5)) & 0x1F;
            uint8_t decoded = GCR_DECODE[nybble];
            
            if (decoded == 0xFF) {
                /* GCR decode error */
                return -1;
            }
            
            if (n % 2 == 1) {
                data[out_idx] = decoded << 4;
            } else {
                data[out_idx++] |= decoded;
            }
        }
    }
    
    *data_size = out_idx;
    return 0;
}

int uft_victor9k_gcr_encode(const uint8_t *data, size_t data_size,
                            uint8_t *gcr, size_t *gcr_size)
{
    if (!data || !gcr || !gcr_size) return -1;
    
    /* GCR: 4 bytes -> 5 bytes */
    size_t out_size = (data_size * 5 + 3) / 4;
    if (*gcr_size < out_size) {
        *gcr_size = out_size;
        return -1;
    }
    
    size_t out_idx = 0;
    
    for (size_t i = 0; i + 4 <= data_size; i += 4) {
        /* Convert 4 data bytes to 8 nybbles, then GCR encode */
        uint64_t gcr_val = 0;
        
        for (int j = 0; j < 4; j++) {
            uint8_t hi = (data[i + j] >> 4) & 0x0F;
            uint8_t lo = data[i + j] & 0x0F;
            gcr_val = (gcr_val << 5) | GCR_ENCODE[hi];
            gcr_val = (gcr_val << 5) | GCR_ENCODE[lo];
        }
        
        /* Extract 5 GCR bytes */
        for (int j = 4; j >= 0; j--) {
            gcr[out_idx + j] = gcr_val & 0xFF;
            gcr_val >>= 8;
        }
        out_idx += 5;
    }
    
    *gcr_size = out_idx;
    return 0;
}

/*===========================================================================
 * Sector Operations
 *===========================================================================*/

int uft_victor9k_read_sector(const uint8_t *track_data, size_t track_size,
                             uint8_t track, uint8_t sector,
                             uint8_t *data, size_t *data_size)
{
    if (!track_data || !data || !data_size) return -1;
    
    int spt = uft_victor9k_sectors_per_track(track);
    if (spt < 0 || sector >= spt) return -1;
    
    /* Search for sector header */
    for (size_t pos = 0; pos + 10 < track_size; pos++) {
        /* Look for sync pattern */
        if (memcmp(track_data + pos, VICTOR_SYNC, 4) != 0) continue;
        
        /* Check sector header */
        uint8_t hdr_track = track_data[pos + 4];
        uint8_t hdr_sector = track_data[pos + 5];
        
        if (hdr_track == track && hdr_sector == sector) {
            /* Found sector - skip header and read data */
            size_t data_start = pos + 10;  /* After header */
            
            if (data_start + 640 > track_size) {  /* 512 * 5/4 = 640 GCR bytes */
                return -1;
            }
            
            /* Decode GCR data */
            return uft_victor9k_gcr_decode(track_data + data_start, 640,
                                           data, data_size);
        }
    }
    
    return -1;  /* Sector not found */
}

int uft_victor9k_write_sector(uint8_t *track_data, size_t track_size,
                              uint8_t track, uint8_t sector,
                              const uint8_t *data, size_t data_size)
{
    if (!track_data || !data || data_size != 512) return -1;
    
    int spt = uft_victor9k_sectors_per_track(track);
    if (spt < 0 || sector >= spt) return -1;
    
    /* Find sector location */
    for (size_t pos = 0; pos + 10 < track_size; pos++) {
        if (memcmp(track_data + pos, VICTOR_SYNC, 4) != 0) continue;
        
        uint8_t hdr_track = track_data[pos + 4];
        uint8_t hdr_sector = track_data[pos + 5];
        
        if (hdr_track == track && hdr_sector == sector) {
            size_t data_start = pos + 10;
            
            if (data_start + 640 > track_size) {
                return -1;
            }
            
            /* Encode and write GCR data */
            size_t gcr_size = 640;
            return uft_victor9k_gcr_encode(data, 512,
                                           track_data + data_start, &gcr_size);
        }
    }
    
    return -1;
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_victor9k_analyze_track(const uint8_t *track_data, size_t track_size,
                               uint8_t track, uft_victor9k_track_info_t *info)
{
    if (!track_data || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    info->track = track;
    info->zone = uft_victor9k_get_zone(track);
    info->expected_sectors = uft_victor9k_sectors_per_track(track);
    info->data_rate = uft_victor9k_data_rate(track);
    
    /* Scan for sectors */
    for (size_t pos = 0; pos + 10 < track_size; pos++) {
        if (memcmp(track_data + pos, VICTOR_SYNC, 4) != 0) continue;
        
        uint8_t hdr_track = track_data[pos + 4];
        uint8_t hdr_sector = track_data[pos + 5];
        
        if (hdr_track == track && hdr_sector < 19) {
            info->sector_found[hdr_sector] = true;
            info->found_sectors++;
            
            /* Try to decode sector data */
            uint8_t sector_data[512];
            size_t data_size = 512;
            size_t data_start = pos + 10;
            
            if (data_start + 640 <= track_size) {
                if (uft_victor9k_gcr_decode(track_data + data_start, 640,
                                            sector_data, &data_size) == 0) {
                    info->sector_valid[hdr_sector] = true;
                    info->valid_sectors++;
                }
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Disk Context
 *===========================================================================*/

int uft_victor9k_open(uft_victor9k_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Calculate total capacity */
    ctx->total_sectors = uft_victor9k_total_sectors();
    ctx->total_size = ctx->total_sectors * 512;
    
    /* Validate - try to read track 0 */
    /* For raw images, assume correct size */
    if (size == ctx->total_size) {
        ctx->is_valid = true;
    }
    
    return 0;
}

void uft_victor9k_close(uft_victor9k_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Report Generation
 *===========================================================================*/

int uft_victor9k_report_json(const uft_victor9k_ctx_t *ctx,
                             char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"Victor 9000 GCR\",\n"
        "  \"valid\": %s,\n"
        "  \"total_sectors\": %u,\n"
        "  \"total_size\": %u,\n"
        "  \"tracks\": 80,\n"
        "  \"sides\": 2,\n"
        "  \"zones\": 8,\n"
        "  \"sector_size\": 512,\n"
        "  \"encoding\": \"GCR 5:4\"\n"
        "}",
        ctx->is_valid ? "true" : "false",
        ctx->total_sectors,
        ctx->total_size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
