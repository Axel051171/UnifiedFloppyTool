/**
 * @file uft_xdf.c
 * @brief IBM XDF (eXtended Density Format) Implementation
 * 
 * Full XDF support with XCopy integration.
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_xdf.h"
#include <string.h>
#include <stdio.h>

/* Forward declaration of profile struct */
struct uft_platform_profile;

/* ============================================================================
 * XDF Sector Layouts
 * ============================================================================ */

/*
 * XDF Track 0 Layout (Boot Track)
 * 4 sectors: 8KB + 2KB + 1KB + 512B = 11,776 bytes
 */
static const uft_xdf_sector_t XDF_TRACK0_SECTORS[] = {
    { .cylinder = 0, .head = 0, .record = 129, .size_n = XDF_SIZE_8192, .size_bytes = 8192 },
    { .cylinder = 0, .head = 0, .record = 130, .size_n = XDF_SIZE_2048, .size_bytes = 2048 },
    { .cylinder = 0, .head = 0, .record = 131, .size_n = XDF_SIZE_1024, .size_bytes = 1024 },
    { .cylinder = 0, .head = 0, .record = 132, .size_n = XDF_SIZE_512,  .size_bytes = 512  },
};

/*
 * XDF Standard Track Layout (Tracks 1-79)
 * 5 sectors: 8KB + 8KB + 2KB + 1KB + 512B = 19,968 bytes
 */
static const uft_xdf_sector_t XDF_STANDARD_SECTORS[] = {
    { .cylinder = 0, .head = 0, .record = 129, .size_n = XDF_SIZE_8192, .size_bytes = 8192 },
    { .cylinder = 0, .head = 0, .record = 130, .size_n = XDF_SIZE_8192, .size_bytes = 8192 },
    { .cylinder = 0, .head = 0, .record = 131, .size_n = XDF_SIZE_2048, .size_bytes = 2048 },
    { .cylinder = 0, .head = 0, .record = 132, .size_n = XDF_SIZE_1024, .size_bytes = 1024 },
    { .cylinder = 0, .head = 0, .record = 133, .size_n = XDF_SIZE_512,  .size_bytes = 512  },
};

/* Known XDF image sizes */
static const size_t XDF_SIZES[] = {
    1915904,    /* Standard XDF */
    1884160,    /* XDF variant (OS/2 Warp) */
    1900544,    /* XDF variant 2 */
    1802240,    /* XXDF (2m.exe variant) */
    1884160,    /* fdformat XDF */
    0           /* Terminator */
};

/* ============================================================================
 * Core API Implementation
 * ============================================================================ */

int uft_xdf_get_track_layout(int track, int head, uft_xdf_track_layout_t *layout)
{
    if (!layout || track < 0 || track >= XDF_TRACKS || head < 0 || head > 1) {
        return -1;
    }
    
    memset(layout, 0, sizeof(*layout));
    
    if (track == 0) {
        layout->sector_count = XDF_TRACK0_SECTORS;
        layout->total_data = 11776;
        
        for (int i = 0; i < 4; i++) {
            layout->sectors[i] = XDF_TRACK0_SECTORS[i];
            layout->sectors[i].cylinder = (uint8_t)track;
            layout->sectors[i].head = (uint8_t)head;
        }
    } else {
        layout->sector_count = 5;
        layout->total_data = 19968;
        
        for (int i = 0; i < 5; i++) {
            layout->sectors[i] = XDF_STANDARD_SECTORS[i];
            layout->sectors[i].cylinder = (uint8_t)track;
            layout->sectors[i].head = (uint8_t)head;
        }
    }
    
    /* Estimate raw MFM track length */
    /* Data + headers + gaps + sync patterns */
    layout->raw_track_len = layout->total_data * 2 + layout->sector_count * 100;
    
    return 0;
}

/* Note: uft_xdf_sectors_for_track is implemented in 
 * src/analysis/profiles/uft_profile_xdf.c */

int uft_xdf_sector_size(int track, int sector_index)
{
    if (track < 0 || track >= XDF_TRACKS) {
        return 0;
    }
    
    if (track == 0) {
        if (sector_index < 0 || sector_index >= 4) return 0;
        return XDF_TRACK0_SECTORS[sector_index].size_bytes;
    } else {
        if (sector_index < 0 || sector_index >= 5) return 0;
        return XDF_STANDARD_SECTORS[sector_index].size_bytes;
    }
}

size_t uft_xdf_disk_size(void)
{
    return XDF_DISK_SIZE;
}

/* ============================================================================
 * Detection
 * ============================================================================ */

bool uft_xdf_detect_by_size(size_t size)
{
    for (int i = 0; XDF_SIZES[i] != 0; i++) {
        if (size == XDF_SIZES[i]) {
            return true;
        }
    }
    return false;
}

int uft_xdf_detect(const uint8_t *data, size_t size)
{
    if (!data || size < 512) {
        return 0;
    }
    
    int confidence = 0;
    
    /* Check file size */
    if (uft_xdf_detect_by_size(size)) {
        confidence += 40;
    }
    
    /* Check boot sector signature */
    /* XDF boot sectors have specific patterns */
    if (size >= 512) {
        /* Check for OS/2 boot signature or XDF marker */
        if (data[0] == 0xEB || data[0] == 0xE9) {
            confidence += 10;  /* Jump instruction */
        }
        
        /* Check OEM name for OS/2 or XDF indicators */
        if (size >= 11) {
            if (memcmp(&data[3], "IBM", 3) == 0 ||
                memcmp(&data[3], "OS2", 3) == 0 ||
                memcmp(&data[3], "MSDOS", 5) == 0) {
                confidence += 15;
            }
        }
        
        /* Check BPB for XDF-specific values */
        if (size >= 25) {
            uint16_t bytes_per_sector = data[11] | (data[12] << 8);
            uint16_t sectors_per_track = data[24];
            
            /* XDF has unusual sectors per track */
            if (sectors_per_track == 0 || sectors_per_track > 36) {
                confidence += 20;  /* Non-standard = likely XDF */
            }
            
            /* XDF often has 512 or 2048 byte logical sectors */
            if (bytes_per_sector == 512 || bytes_per_sector == 2048) {
                confidence += 10;
            }
        }
    }
    
    /* Cap at 100 */
    return (confidence > 100) ? 100 : confidence;
}

/* ============================================================================
 * XCopy Integration
 * ============================================================================ */

/* Note: uft_xdf_recommended_copy_mode and uft_format_requires_track_copy
 * are implemented in src/analysis/profiles/uft_profile_xdf.c */

int uft_xdf_analyze_for_copy(const uint8_t *track_data, size_t track_len,
                             int *out_mode, const char **out_reason)
{
    if (!track_data || !out_mode || !out_reason) {
        return -1;
    }
    
    /* Default: Track Copy */
    *out_mode = 2;
    *out_reason = "XDF has variable sector sizes (512B-8KB)";
    
    /* Check for suspicious patterns that might indicate protection */
    int unusual_bytes = 0;
    for (size_t i = 0; i < track_len && i < 1000; i++) {
        /* Count non-standard gap/sync bytes */
        if (track_data[i] != 0x4E && track_data[i] != 0x00 &&
            track_data[i] != 0xA1 && track_data[i] != 0xFE &&
            track_data[i] != 0xFB && track_data[i] != 0xF8) {
            unusual_bytes++;
        }
    }
    
    /* If >20% unusual, might be protected */
    if (track_len > 0 && unusual_bytes * 5 > (int)track_len) {
        *out_mode = 3;  /* Flux */
        *out_reason = "XDF with possible protection - use Flux Copy";
    }
    
    return 0;
}

int uft_xdf_validate_track(const uint8_t *track_data, size_t track_len,
                           int track, int head)
{
    if (!track_data || track < 0 || track >= 80 || head < 0 || head > 1) {
        return -1;
    }
    
    uft_xdf_track_layout_t layout;
    if (uft_xdf_get_track_layout(track, head, &layout) != 0) {
        return -1;
    }
    
    /* Check if track length is reasonable for XDF */
    size_t min_len = layout.total_data;  /* Absolute minimum */
    size_t max_len = layout.total_data * 3;  /* With MFM overhead */
    
    if (track_len < min_len) {
        return -2;  /* Track too short */
    }
    if (track_len > max_len) {
        return -3;  /* Track too long (might be different format) */
    }
    
    /* Look for MFM sync patterns */
    int sync_count = 0;
    for (size_t i = 0; i + 1 < track_len; i++) {
        if (track_data[i] == 0xA1 && track_data[i+1] == 0xA1) {
            sync_count++;
            i++;  /* Skip next byte */
        }
    }
    
    /* Should have at least one sync per sector */
    if (sync_count < layout.sector_count) {
        return -4;  /* Too few sync patterns */
    }
    
    return 0;  /* Valid */
}

