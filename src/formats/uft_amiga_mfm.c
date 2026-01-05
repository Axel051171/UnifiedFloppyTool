/**
 * @file uft_amiga_mfm.c
 * @brief Amiga MFM Track Handler Implementation
 * 
 * EXT3-007: Amiga disk format and MFM handling
 * 
 * Features:
 * - Amiga MFM encoding/decoding
 * - ADF format support (DD/HD)
 * - Track checksums
 * - Sector extraction
 * - Copy protection detection
 */

#include "uft/formats/uft_amiga_mfm.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define AMIGA_SYNC          0x4489
#define AMIGA_DD_SECTORS    11
#define AMIGA_HD_SECTORS    22
#define AMIGA_SECTOR_SIZE   512
#define AMIGA_DD_TRACKS     80
#define AMIGA_HD_TRACKS     80

/* ADF file sizes */
#define ADF_DD_SIZE         (AMIGA_DD_TRACKS * 2 * AMIGA_DD_SECTORS * AMIGA_SECTOR_SIZE)  /* 901120 */
#define ADF_HD_SIZE         (AMIGA_HD_TRACKS * 2 * AMIGA_HD_SECTORS * AMIGA_SECTOR_SIZE)  /* 1802240 */

/*===========================================================================
 * MFM Encoding/Decoding
 *===========================================================================*/

/* Amiga MFM odd/even encoding */
static uint32_t mfm_decode_long(uint32_t odd, uint32_t even)
{
    return ((odd & 0x55555555) << 1) | (even & 0x55555555);
}

static void mfm_encode_long(uint32_t value, uint32_t *odd, uint32_t *even)
{
    *odd = (value >> 1) & 0x55555555;
    *even = value & 0x55555555;
    
    /* Add clock bits */
    uint32_t clock_odd = ~(*odd | (*odd >> 1) | (*odd << 1));
    uint32_t clock_even = ~(*even | (*even >> 1) | (*even << 1));
    
    *odd |= (clock_odd & 0xAAAAAAAA);
    *even |= (clock_even & 0xAAAAAAAA);
}

/* Decode MFM data block */
int uft_amiga_mfm_decode(const uint8_t *mfm, size_t mfm_size,
                         uint8_t *data, size_t *data_size)
{
    if (!mfm || !data || !data_size) return -1;
    
    /* MFM is 2:1 ratio */
    size_t out_size = mfm_size / 2;
    if (*data_size < out_size) {
        *data_size = out_size;
        return -1;
    }
    
    /* Decode odd/even pairs */
    for (size_t i = 0; i < out_size / 4; i++) {
        uint32_t odd = (mfm[i * 4] << 24) | (mfm[i * 4 + 1] << 16) |
                       (mfm[i * 4 + 2] << 8) | mfm[i * 4 + 3];
        uint32_t even = (mfm[out_size + i * 4] << 24) | 
                        (mfm[out_size + i * 4 + 1] << 16) |
                        (mfm[out_size + i * 4 + 2] << 8) | 
                        mfm[out_size + i * 4 + 3];
        
        uint32_t decoded = mfm_decode_long(odd, even);
        
        data[i * 4] = (decoded >> 24) & 0xFF;
        data[i * 4 + 1] = (decoded >> 16) & 0xFF;
        data[i * 4 + 2] = (decoded >> 8) & 0xFF;
        data[i * 4 + 3] = decoded & 0xFF;
    }
    
    *data_size = out_size;
    return 0;
}

int uft_amiga_mfm_encode(const uint8_t *data, size_t data_size,
                         uint8_t *mfm, size_t *mfm_size)
{
    if (!data || !mfm || !mfm_size) return -1;
    
    size_t needed = data_size * 2;
    if (*mfm_size < needed) {
        *mfm_size = needed;
        return -1;
    }
    
    /* Encode to odd/even format */
    for (size_t i = 0; i < data_size / 4; i++) {
        uint32_t value = (data[i * 4] << 24) | (data[i * 4 + 1] << 16) |
                         (data[i * 4 + 2] << 8) | data[i * 4 + 3];
        
        uint32_t odd, even;
        mfm_encode_long(value, &odd, &even);
        
        /* Odd bits first half */
        mfm[i * 4] = (odd >> 24) & 0xFF;
        mfm[i * 4 + 1] = (odd >> 16) & 0xFF;
        mfm[i * 4 + 2] = (odd >> 8) & 0xFF;
        mfm[i * 4 + 3] = odd & 0xFF;
        
        /* Even bits second half */
        mfm[data_size + i * 4] = (even >> 24) & 0xFF;
        mfm[data_size + i * 4 + 1] = (even >> 16) & 0xFF;
        mfm[data_size + i * 4 + 2] = (even >> 8) & 0xFF;
        mfm[data_size + i * 4 + 3] = even & 0xFF;
    }
    
    *mfm_size = needed;
    return 0;
}

/*===========================================================================
 * Checksum
 *===========================================================================*/

uint32_t uft_amiga_checksum(const uint8_t *data, size_t size)
{
    uint32_t checksum = 0;
    
    for (size_t i = 0; i < size; i += 4) {
        uint32_t value = (data[i] << 24) | (data[i + 1] << 16) |
                         (data[i + 2] << 8) | data[i + 3];
        checksum ^= value;
    }
    
    return checksum & 0x55555555;  /* Only odd bits count */
}

/*===========================================================================
 * ADF Operations
 *===========================================================================*/

int uft_amiga_adf_open(uft_amiga_ctx_t *ctx, const uint8_t *data, size_t size)
{
    if (!ctx || !data) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;
    
    /* Detect format by size */
    if (size == ADF_DD_SIZE) {
        ctx->format = UFT_AMIGA_ADF_DD;
        ctx->tracks = AMIGA_DD_TRACKS;
        ctx->sectors_per_track = AMIGA_DD_SECTORS;
    } else if (size == ADF_HD_SIZE) {
        ctx->format = UFT_AMIGA_ADF_HD;
        ctx->tracks = AMIGA_HD_TRACKS;
        ctx->sectors_per_track = AMIGA_HD_SECTORS;
    } else {
        return -1;
    }
    
    ctx->sides = 2;
    ctx->sector_size = AMIGA_SECTOR_SIZE;
    ctx->total_sectors = ctx->tracks * ctx->sides * ctx->sectors_per_track;
    ctx->is_valid = true;
    
    return 0;
}

void uft_amiga_adf_close(uft_amiga_ctx_t *ctx)
{
    if (ctx) {
        memset(ctx, 0, sizeof(*ctx));
    }
}

int uft_amiga_adf_read_sector(const uft_amiga_ctx_t *ctx,
                              uint8_t track, uint8_t side, uint8_t sector,
                              uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size || !ctx->is_valid) return -1;
    if (track >= ctx->tracks || side >= ctx->sides) return -1;
    if (sector >= ctx->sectors_per_track) return -1;
    
    size_t offset = ((track * ctx->sides + side) * ctx->sectors_per_track + sector) 
                    * ctx->sector_size;
    
    if (offset + ctx->sector_size > ctx->size) return -1;
    
    if (*size < ctx->sector_size) {
        *size = ctx->sector_size;
        return -1;
    }
    
    memcpy(buffer, ctx->data + offset, ctx->sector_size);
    *size = ctx->sector_size;
    
    return 0;
}

int uft_amiga_adf_read_track(const uft_amiga_ctx_t *ctx,
                             uint8_t track, uint8_t side,
                             uint8_t *buffer, size_t *size)
{
    if (!ctx || !buffer || !size || !ctx->is_valid) return -1;
    if (track >= ctx->tracks || side >= ctx->sides) return -1;
    
    size_t track_size = ctx->sectors_per_track * ctx->sector_size;
    
    if (*size < track_size) {
        *size = track_size;
        return -1;
    }
    
    size_t offset = (track * ctx->sides + side) * track_size;
    
    if (offset + track_size > ctx->size) return -1;
    
    memcpy(buffer, ctx->data + offset, track_size);
    *size = track_size;
    
    return 0;
}

/*===========================================================================
 * Bootblock Analysis
 *===========================================================================*/

int uft_amiga_read_bootblock(const uft_amiga_ctx_t *ctx, uft_amiga_bootblock_t *boot)
{
    if (!ctx || !boot || !ctx->is_valid) return -1;
    
    memset(boot, 0, sizeof(*boot));
    
    /* Bootblock is first 1024 bytes (2 sectors) */
    if (ctx->size < 1024) return -1;
    
    const uint8_t *bb = ctx->data;
    
    /* Disk type */
    memcpy(boot->disk_type, bb, 4);
    boot->disk_type[4] = '\0';
    
    /* Check for DOS disk */
    boot->is_dos = (memcmp(boot->disk_type, "DOS", 3) == 0);
    
    if (boot->is_dos) {
        boot->fs_type = boot->disk_type[3];  /* 0=OFS, 1=FFS, etc. */
    }
    
    /* Checksum (at offset 4) */
    boot->checksum = (bb[4] << 24) | (bb[5] << 16) | (bb[6] << 8) | bb[7];
    
    /* Root block pointer (at offset 8) */
    boot->rootblock = (bb[8] << 24) | (bb[9] << 16) | (bb[10] << 8) | bb[11];
    
    /* Verify checksum */
    uint32_t calc_sum = 0;
    for (int i = 0; i < 256; i++) {
        uint32_t val = (bb[i * 4] << 24) | (bb[i * 4 + 1] << 16) |
                       (bb[i * 4 + 2] << 8) | bb[i * 4 + 3];
        
        uint32_t prev_sum = calc_sum;
        calc_sum += val;
        if (calc_sum < prev_sum) calc_sum++;  /* Carry */
    }
    
    boot->checksum_valid = (calc_sum == 0);
    
    /* Check for bootable */
    boot->is_bootable = (bb[12] != 0);  /* Has boot code */
    
    return 0;
}

/*===========================================================================
 * Copy Protection Detection
 *===========================================================================*/

int uft_amiga_detect_protection(const uft_amiga_ctx_t *ctx, uint32_t *protection_flags)
{
    if (!ctx || !protection_flags || !ctx->is_valid) return -1;
    
    *protection_flags = 0;
    
    /* Read bootblock */
    uft_amiga_bootblock_t boot;
    if (uft_amiga_read_bootblock(ctx, &boot) != 0) {
        return -1;
    }
    
    /* Check bootblock for protection signatures */
    const uint8_t *bb = ctx->data;
    
    /* CopyLock: Specific patterns in bootblock */
    /* Rob Northen Computing signature */
    for (int i = 0; i < 512 - 8; i++) {
        if (memcmp(bb + i, "Rob Nort", 8) == 0 ||
            memcmp(bb + i, "RNC ", 4) == 0) {
            *protection_flags |= UFT_AMIGA_PROT_COPYLOCK;
            break;
        }
    }
    
    /* Speedlock: Check for timing loops */
    /* Look for CIA timer access patterns */
    for (int i = 0; i < 1020; i++) {
        if (bb[i] == 0xBF && bb[i + 1] == 0xE0 && bb[i + 2] == 0x01) {
            *protection_flags |= UFT_AMIGA_PROT_SPEEDLOCK;
            break;
        }
    }
    
    /* Long tracks: Would need raw track data */
    /* For ADF, we can't detect this directly */
    
    /* Check for non-standard sector counts */
    /* This would require raw flux data */
    
    return 0;
}

/*===========================================================================
 * Track Generation
 *===========================================================================*/

int uft_amiga_generate_track(const uint8_t *sectors, int sector_count,
                             uint8_t track, uint8_t side,
                             uint8_t *mfm_track, size_t *mfm_size)
{
    if (!sectors || !mfm_track || !mfm_size) return -1;
    
    size_t pos = 0;
    
    /* Gap before first sector */
    for (int i = 0; i < 64 && pos < *mfm_size; i++) {
        mfm_track[pos++] = 0xAA;
    }
    
    for (int s = 0; s < sector_count; s++) {
        /* Sync words */
        if (pos + 4 <= *mfm_size) {
            mfm_track[pos++] = 0x44;
            mfm_track[pos++] = 0x89;
            mfm_track[pos++] = 0x44;
            mfm_track[pos++] = 0x89;
        }
        
        /* Sector header info */
        uint32_t info = 0xFF000000 | (track << 16) | (side << 8) | s;
        info |= ((sector_count - s) << 24);  /* Sectors to gap */
        
        uint32_t odd, even;
        mfm_encode_long(info, &odd, &even);
        
        /* Write header (odd then even) */
        if (pos + 8 <= *mfm_size) {
            mfm_track[pos++] = (odd >> 24) & 0xFF;
            mfm_track[pos++] = (odd >> 16) & 0xFF;
            mfm_track[pos++] = (odd >> 8) & 0xFF;
            mfm_track[pos++] = odd & 0xFF;
            mfm_track[pos++] = (even >> 24) & 0xFF;
            mfm_track[pos++] = (even >> 16) & 0xFF;
            mfm_track[pos++] = (even >> 8) & 0xFF;
            mfm_track[pos++] = even & 0xFF;
        }
        
        /* Sector label (16 bytes, usually zeros) */
        for (int i = 0; i < 32 && pos < *mfm_size; i++) {
            mfm_track[pos++] = 0xAA;
        }
        
        /* Header checksum */
        uint32_t hdr_cksum = info;
        mfm_encode_long(hdr_cksum, &odd, &even);
        if (pos + 8 <= *mfm_size) {
            mfm_track[pos++] = (odd >> 24) & 0xFF;
            mfm_track[pos++] = (odd >> 16) & 0xFF;
            mfm_track[pos++] = (odd >> 8) & 0xFF;
            mfm_track[pos++] = odd & 0xFF;
            mfm_track[pos++] = (even >> 24) & 0xFF;
            mfm_track[pos++] = (even >> 16) & 0xFF;
            mfm_track[pos++] = (even >> 8) & 0xFF;
            mfm_track[pos++] = even & 0xFF;
        }
        
        /* Data checksum */
        uint32_t data_cksum = uft_amiga_checksum(sectors + s * 512, 512);
        mfm_encode_long(data_cksum, &odd, &even);
        if (pos + 8 <= *mfm_size) {
            mfm_track[pos++] = (odd >> 24) & 0xFF;
            mfm_track[pos++] = (odd >> 16) & 0xFF;
            mfm_track[pos++] = (odd >> 8) & 0xFF;
            mfm_track[pos++] = odd & 0xFF;
            mfm_track[pos++] = (even >> 24) & 0xFF;
            mfm_track[pos++] = (even >> 16) & 0xFF;
            mfm_track[pos++] = (even >> 8) & 0xFF;
            mfm_track[pos++] = even & 0xFF;
        }
        
        /* Sector data (MFM encoded) */
        size_t mfm_data_size = 1024;
        if (pos + mfm_data_size <= *mfm_size) {
            uft_amiga_mfm_encode(sectors + s * 512, 512, 
                                mfm_track + pos, &mfm_data_size);
            pos += mfm_data_size;
        }
        
        /* Inter-sector gap */
        for (int i = 0; i < 16 && pos < *mfm_size; i++) {
            mfm_track[pos++] = 0xAA;
        }
    }
    
    /* Fill rest with gap */
    while (pos < *mfm_size) {
        mfm_track[pos++] = 0xAA;
    }
    
    return 0;
}

/*===========================================================================
 * Report
 *===========================================================================*/

const char *uft_amiga_format_name(uft_amiga_format_t format)
{
    switch (format) {
        case UFT_AMIGA_ADF_DD: return "ADF (DD)";
        case UFT_AMIGA_ADF_HD: return "ADF (HD)";
        case UFT_AMIGA_ADZ:    return "ADZ (Gzipped ADF)";
        case UFT_AMIGA_DMS:    return "DMS (Disk Masher)";
        case UFT_AMIGA_IPF:    return "IPF (SPS/CAPS)";
        default:              return "Unknown";
    }
}

int uft_amiga_report_json(const uft_amiga_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    uft_amiga_bootblock_t boot;
    const char *disk_type = "Unknown";
    int is_bootable = 0;
    
    if (uft_amiga_read_bootblock(ctx, &boot) == 0) {
        disk_type = boot.disk_type;
        is_bootable = boot.is_bootable;
    }
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"format\": \"%s\",\n"
        "  \"valid\": %s,\n"
        "  \"tracks\": %u,\n"
        "  \"sides\": %u,\n"
        "  \"sectors_per_track\": %u,\n"
        "  \"sector_size\": %u,\n"
        "  \"total_sectors\": %u,\n"
        "  \"disk_type\": \"%s\",\n"
        "  \"bootable\": %s,\n"
        "  \"file_size\": %zu\n"
        "}",
        uft_amiga_format_name(ctx->format),
        ctx->is_valid ? "true" : "false",
        ctx->tracks,
        ctx->sides,
        ctx->sectors_per_track,
        ctx->sector_size,
        ctx->total_sectors,
        disk_type,
        is_bootable ? "true" : "false",
        ctx->size
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
