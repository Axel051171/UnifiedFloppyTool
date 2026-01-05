/**
 * @file uft_c64_gcr.c
 * @brief Commodore 64 GCR Track Implementation
 * 
 * EXT3-005: C64 GCR encoding/decoding and track handling
 * 
 * Features:
 * - GCR 5:4 encoding (Commodore variant)
 * - Zone-based timing (4 zones, 17-21 sectors)
 * - D64/G64 format support
 * - Copy protection detection
 */

#include "uft/formats/uft_c64_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * GCR Tables
 *===========================================================================*/

/* GCR encode: 4 bits -> 5 bits */
static const uint8_t GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* GCR decode: 5 bits -> 4 bits (0xFF = invalid) */
static const uint8_t GCR_DECODE[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/* Zone definitions: tracks, sectors per track, bit rate */
static const struct {
    uint8_t start_track;
    uint8_t end_track;
    uint8_t sectors;
    uint32_t bit_rate;      /* bits per second */
    uint16_t track_size;    /* raw bytes per track */
} C64_ZONES[] = {
    {  1, 17, 21, 307692, 7692 },   /* Zone 0 */
    { 18, 24, 19, 285714, 7142 },   /* Zone 1 */
    { 25, 30, 18, 266667, 6666 },   /* Zone 2 */
    { 31, 35, 17, 250000, 6250 },   /* Zone 3 */
};

#define NUM_ZONES 4
#define C64_TRACKS 35
#define C64_SECTOR_SIZE 256
#define C64_HEADER_SIZE 8
#define C64_DATA_SIZE 325  /* 256 data + overhead in GCR */

/*===========================================================================
 * Zone Helpers
 *===========================================================================*/

int uft_c64_get_zone(uint8_t track)
{
    for (int z = 0; z < NUM_ZONES; z++) {
        if (track >= C64_ZONES[z].start_track && 
            track <= C64_ZONES[z].end_track) {
            return z;
        }
    }
    return -1;
}

int uft_c64_sectors_per_track(uint8_t track)
{
    int zone = uft_c64_get_zone(track);
    if (zone < 0) return -1;
    return C64_ZONES[zone].sectors;
}

uint32_t uft_c64_bit_rate(uint8_t track)
{
    int zone = uft_c64_get_zone(track);
    if (zone < 0) return 0;
    return C64_ZONES[zone].bit_rate;
}

uint16_t uft_c64_track_size(uint8_t track)
{
    int zone = uft_c64_get_zone(track);
    if (zone < 0) return 0;
    return C64_ZONES[zone].track_size;
}

/*===========================================================================
 * GCR Encoding/Decoding
 *===========================================================================*/

int uft_c64_gcr_encode(const uint8_t *data, size_t data_size,
                       uint8_t *gcr, size_t *gcr_size)
{
    if (!data || !gcr || !gcr_size) return -1;
    
    /* 4 bytes -> 5 GCR bytes */
    size_t needed = (data_size * 5 + 3) / 4;
    if (*gcr_size < needed) {
        *gcr_size = needed;
        return -1;
    }
    
    size_t out_idx = 0;
    
    for (size_t i = 0; i + 4 <= data_size; i += 4) {
        /* Get 8 nybbles */
        uint8_t n[8];
        for (int j = 0; j < 4; j++) {
            n[j * 2] = (data[i + j] >> 4) & 0x0F;
            n[j * 2 + 1] = data[i + j] & 0x0F;
        }
        
        /* Encode to 8 x 5-bit values = 40 bits = 5 bytes */
        uint64_t bits = 0;
        for (int j = 0; j < 8; j++) {
            bits = (bits << 5) | GCR_ENCODE[n[j]];
        }
        
        /* Extract 5 bytes */
        for (int j = 4; j >= 0; j--) {
            gcr[out_idx + j] = bits & 0xFF;
            bits >>= 8;
        }
        out_idx += 5;
    }
    
    /* Handle remaining bytes */
    size_t remaining = data_size % 4;
    if (remaining > 0) {
        uint8_t temp[4] = {0};
        memcpy(temp, data + data_size - remaining, remaining);
        
        uint64_t bits = 0;
        for (int j = 0; j < 4; j++) {
            bits = (bits << 5) | GCR_ENCODE[(temp[j] >> 4) & 0x0F];
            bits = (bits << 5) | GCR_ENCODE[temp[j] & 0x0F];
        }
        
        size_t extra_bytes = (remaining * 5 + 3) / 4;
        for (int j = extra_bytes - 1; j >= 0; j--) {
            gcr[out_idx + j] = bits & 0xFF;
            bits >>= 8;
        }
        out_idx += extra_bytes;
    }
    
    *gcr_size = out_idx;
    return 0;
}

int uft_c64_gcr_decode(const uint8_t *gcr, size_t gcr_size,
                       uint8_t *data, size_t *data_size)
{
    if (!gcr || !data || !data_size) return -1;
    
    /* 5 GCR bytes -> 4 data bytes */
    size_t out_size = (gcr_size * 4) / 5;
    if (*data_size < out_size) {
        *data_size = out_size;
        return -1;
    }
    
    size_t out_idx = 0;
    int errors = 0;
    
    for (size_t i = 0; i + 5 <= gcr_size; i += 5) {
        /* Combine 5 bytes into 40 bits */
        uint64_t bits = 0;
        for (int j = 0; j < 5; j++) {
            bits = (bits << 8) | gcr[i + j];
        }
        
        /* Extract 8 nybbles (4 bytes) */
        for (int j = 3; j >= 0; j--) {
            uint8_t hi = GCR_DECODE[(bits >> (j * 10 + 5)) & 0x1F];
            uint8_t lo = GCR_DECODE[(bits >> (j * 10)) & 0x1F];
            
            if (hi == 0xFF || lo == 0xFF) {
                errors++;
                data[out_idx++] = 0x00;
            } else {
                data[out_idx++] = (hi << 4) | lo;
            }
        }
    }
    
    *data_size = out_idx;
    return errors;
}

/*===========================================================================
 * Checksum
 *===========================================================================*/

uint8_t uft_c64_checksum(const uint8_t *data, size_t size)
{
    uint8_t checksum = 0;
    for (size_t i = 0; i < size; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/*===========================================================================
 * Sector Header
 *===========================================================================*/

int uft_c64_parse_header(const uint8_t *gcr_header, uft_c64_sector_header_t *header)
{
    if (!gcr_header || !header) return -1;
    
    /* GCR header is 10 bytes: sync + 8 data + gap
     * Decoded: type, checksum, sector, track, id2, id1, 0x0F, 0x0F */
    
    uint8_t decoded[8];
    size_t dec_size = 8;
    
    if (uft_c64_gcr_decode(gcr_header, 10, decoded, &dec_size) < 0) {
        return -1;
    }
    
    header->block_type = decoded[0];
    header->header_checksum = decoded[1];
    header->sector = decoded[2];
    header->track = decoded[3];
    header->disk_id[0] = decoded[4];
    header->disk_id[1] = decoded[5];
    
    /* Verify checksum */
    uint8_t calc_checksum = decoded[2] ^ decoded[3] ^ decoded[4] ^ decoded[5];
    header->valid = (header->block_type == 0x08 && 
                     header->header_checksum == calc_checksum);
    
    return header->valid ? 0 : -1;
}

/*===========================================================================
 * D64 Operations
 *===========================================================================*/

int uft_c64_d64_open(uft_c64_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Detect D64 variant by size */
    switch (size) {
        case 174848:  /* 35 tracks, no errors */
            ctx->tracks = 35;
            ctx->has_errors = false;
            break;
        case 175531:  /* 35 tracks + error info */
            ctx->tracks = 35;
            ctx->has_errors = true;
            break;
        case 196608:  /* 40 tracks, no errors */
            ctx->tracks = 40;
            ctx->has_errors = false;
            break;
        case 197376:  /* 40 tracks + error info */
            ctx->tracks = 40;
            ctx->has_errors = true;
            break;
        default:
            return -1;
    }
    
    /* Calculate total sectors */
    ctx->total_sectors = 0;
    for (int t = 1; t <= ctx->tracks; t++) {
        int spt = uft_c64_sectors_per_track(t);
        if (spt > 0) ctx->total_sectors += spt;
    }
    
    ctx->is_valid = true;
    return 0;
}

void uft_c64_d64_close(uft_c64_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

static size_t d64_sector_offset(uint8_t track, uint8_t sector)
{
    if (track < 1 || track > 40) return 0;
    
    size_t offset = 0;
    for (int t = 1; t < track; t++) {
        int spt = uft_c64_sectors_per_track(t);
        if (spt > 0) offset += spt * C64_SECTOR_SIZE;
    }
    offset += sector * C64_SECTOR_SIZE;
    
    return offset;
}

int uft_c64_d64_read_sector(const uft_c64_ctx_t *ctx, uint8_t track, uint8_t sector,
                            uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size) return -1;
    if (!ctx->is_valid) return -1;
    
    int spt = uft_c64_sectors_per_track(track);
    if (spt < 0 || sector >= spt) return -1;
    
    size_t offset = d64_sector_offset(track, sector);
    if (offset + C64_SECTOR_SIZE > ctx->size) return -1;
    
    if (*size < C64_SECTOR_SIZE) {
        *size = C64_SECTOR_SIZE;
        return -1;
    }
    
    memcpy(buffer, ctx->data + offset, C64_SECTOR_SIZE);
    *size = C64_SECTOR_SIZE;
    
    /* Check error byte if available */
    if (ctx->has_errors) {
        size_t err_offset = ctx->total_sectors * C64_SECTOR_SIZE;
        size_t sec_num = 0;
        for (int t = 1; t < track; t++) {
            sec_num += uft_c64_sectors_per_track(t);
        }
        sec_num += sector;
        
        if (err_offset + sec_num < ctx->size) {
            uint8_t err = ctx->data[err_offset + sec_num];
            if (err != 0x01) return err;  /* Non-OK error code */
        }
    }
    
    return 0;
}

int uft_c64_d64_write_sector(uft_c64_ctx_t *ctx, uint8_t track, uint8_t sector,
                             const uint8_t *buffer, size_t size)
{
    if (!ctx || !buffer || size != C64_SECTOR_SIZE) return -1;
    if (!ctx->is_valid) return -1;
    
    int spt = uft_c64_sectors_per_track(track);
    if (spt < 0 || sector >= spt) return -1;
    
    size_t offset = d64_sector_offset(track, sector);
    if (offset + C64_SECTOR_SIZE > ctx->size) return -1;
    
    /* Note: ctx->data is const, so this would need a non-const version */
    /* For now, return error - write needs mutable buffer */
    return -1;
}

/*===========================================================================
 * BAM (Block Availability Map)
 *===========================================================================*/

int uft_c64_read_bam(const uft_c64_ctx_t *ctx, uft_c64_bam_t *bam)
{
    if (!ctx || !bam || !ctx->is_valid) return -1;
    
    /* BAM is at track 18, sector 0 */
    uint8_t bam_sector[256];
    size_t size = 256;
    
    if (uft_c64_d64_read_sector(ctx, 18, 0, bam_sector, &size) != 0) {
        return -1;
    }
    
    memset(bam, 0, sizeof(*bam));
    
    /* Parse BAM */
    bam->dir_track = bam_sector[0];
    bam->dir_sector = bam_sector[1];
    bam->dos_version = bam_sector[2];
    
    /* Disk name (offset 0x90, 16 bytes, padded with 0xA0) */
    for (int i = 0; i < 16; i++) {
        uint8_t c = bam_sector[0x90 + i];
        bam->disk_name[i] = (c == 0xA0) ? ' ' : c;
    }
    bam->disk_name[16] = '\0';
    
    /* Disk ID (offset 0xA2, 2 bytes) */
    bam->disk_id[0] = bam_sector[0xA2];
    bam->disk_id[1] = bam_sector[0xA3];
    
    /* DOS type (offset 0xA5, 2 bytes) */
    bam->dos_type[0] = bam_sector[0xA5];
    bam->dos_type[1] = bam_sector[0xA6];
    
    /* Count free blocks */
    bam->free_blocks = 0;
    for (int t = 1; t <= 35; t++) {
        if (t == 18) continue;  /* Skip directory track */
        int idx = 4 * t;
        bam->free_blocks += bam_sector[idx];
    }
    
    return 0;
}

/*===========================================================================
 * Directory
 *===========================================================================*/

int uft_c64_read_directory(const uft_c64_ctx_t *ctx, 
                           uft_c64_dirent_t *entries, size_t max_entries,
                           size_t *count)
{
    if (!ctx || !entries || !count || !ctx->is_valid) return -1;
    
    *count = 0;
    
    /* Directory starts at track 18, sector 1 */
    uint8_t track = 18;
    uint8_t sector = 1;
    
    while (track != 0 && *count < max_entries) {
        uint8_t dir_sector[256];
        size_t size = 256;
        
        if (uft_c64_d64_read_sector(ctx, track, sector, dir_sector, &size) != 0) {
            break;
        }
        
        /* 8 entries per sector, 32 bytes each */
        for (int e = 0; e < 8 && *count < max_entries; e++) {
            const uint8_t *entry = dir_sector + e * 32;
            
            uint8_t file_type = entry[2];
            if (file_type == 0) continue;  /* Empty entry */
            
            uft_c64_dirent_t *de = &entries[*count];
            memset(de, 0, sizeof(*de));
            
            de->file_type = file_type & 0x07;
            de->locked = (file_type & 0x40) != 0;
            de->closed = (file_type & 0x80) != 0;
            
            de->start_track = entry[3];
            de->start_sector = entry[4];
            
            /* Filename (16 bytes, padded with 0xA0) */
            for (int i = 0; i < 16; i++) {
                uint8_t c = entry[5 + i];
                de->filename[i] = (c == 0xA0) ? '\0' : c;
            }
            de->filename[16] = '\0';
            
            /* File size in blocks */
            de->blocks = entry[30] | (entry[31] << 8);
            
            (*count)++;
        }
        
        /* Next directory sector */
        track = dir_sector[0];
        sector = dir_sector[1];
    }
    
    return 0;
}

/*===========================================================================
 * Copy Protection Detection
 *===========================================================================*/

int uft_c64_detect_protection(const uft_c64_ctx_t *ctx, uint32_t *protection_flags)
{
    if (!ctx || !protection_flags || !ctx->is_valid) return -1;
    
    *protection_flags = 0;
    
    /* Read track 18 sector 0 (BAM) */
    uint8_t bam[256];
    size_t size = 256;
    
    if (uft_c64_d64_read_sector(ctx, 18, 0, bam, &size) != 0) {
        return -1;
    }
    
    /* Check for various protection signatures */
    
    /* V-MAX!: Check for specific patterns in BAM */
    if (bam[0] == 0x12 && bam[1] == 0x01) {
        /* Check track 36+ usage (extended tracks) */
        if (ctx->tracks > 35) {
            *protection_flags |= UFT_C64_PROT_VMAX;
        }
    }
    
    /* Check boot sector (track 1, sector 0) for loaders */
    uint8_t boot[256];
    if (uft_c64_d64_read_sector(ctx, 1, 0, boot, &size) == 0) {
        /* RapidLok signature */
        if (boot[0] == 0x4C && memcmp(boot + 3, "RAPIDLOK", 8) == 0) {
            *protection_flags |= UFT_C64_PROT_RAPIDLOK;
        }
        
        /* Vorpal signature */
        if (boot[0] == 0x00 && boot[1] == 0x00 && boot[2] == 0x09) {
            *protection_flags |= UFT_C64_PROT_VORPAL;
        }
    }
    
    /* Check for half-tracks (G64 indicator) */
    /* This would require G64 format */
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

int uft_c64_report_json(const uft_c64_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_c64_bam_t bam;
    const char *disk_name = "Unknown";
    int free_blocks = 0;
    
    if (uft_c64_read_bam(ctx, &bam) == 0) {
        disk_name = bam.disk_name;
        free_blocks = bam.free_blocks;
    }
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"D64\",\n"
        "  \"valid\": %s,\n"
        "  \"tracks\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"has_errors\": %s,\n"
        "  \"disk_name\": \"%s\",\n"
        "  \"free_blocks\": %d,\n"
        "  \"file_size\": %zu\n"
        "}",
        ctx->is_valid ? "true" : "false",
        ctx->tracks,
        ctx->total_sectors,
        ctx->has_errors ? "true" : "false",
        disk_name,
        free_blocks,
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

/*===========================================================================
 * File Type Names
 *===========================================================================*/

const char *uft_c64_file_type_name(uint8_t type)
{
    switch (type & 0x07) {
        case 0: return "DEL";
        case 1: return "SEQ";
        case 2: return "PRG";
        case 3: return "USR";
        case 4: return "REL";
        default: return "???";
    }
}
