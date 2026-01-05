/**
 * @file uft_fdc_gaps.c
 * @brief FDC Gap Tables Implementation
 * 
 * EXT-006: FDC gap calculations and format detection
 */

#include "uft/formats/uft_fdc_gaps.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Format Lookup
 *===========================================================================*/

const uft_fdc_format_t *uft_fdc_get_format(const char *name)
{
    if (!name) return NULL;
    
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        if (strstr(UFT_FDC_FORMATS[i]->name, name) != NULL) {
            return UFT_FDC_FORMATS[i];
        }
    }
    
    return NULL;
}

const uft_fdc_format_t *uft_fdc_detect_format(uint8_t tracks, uint8_t sides,
                                              uint8_t sectors, uint16_t sector_size)
{
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *fmt = UFT_FDC_FORMATS[i];
        
        if (fmt->sectors == sectors &&
            fmt->sector_size == sector_size &&
            fmt->sides == sides) {
            /* Track count can vary, prefer exact match */
            if (fmt->tracks == tracks) {
                return fmt;
            }
        }
    }
    
    /* Second pass: relaxed matching */
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *fmt = UFT_FDC_FORMATS[i];
        
        if (fmt->sectors == sectors && fmt->sector_size == sector_size) {
            return fmt;
        }
    }
    
    return NULL;
}

/*===========================================================================
 * Track Layout Calculation
 *===========================================================================*/

int uft_fdc_calc_track_layout(const uft_fdc_format_t *fmt,
                              uint32_t *sector_offsets, int max_sectors)
{
    if (!fmt || !sector_offsets) return -1;
    
    uint32_t pos = 0;
    int sector_count = 0;
    
    /* Post-index gap (GAP4a) */
    pos += fmt->gaps.gap4a;
    
    /* Index Address Mark (IAM) for MFM */
    if (fmt->mfm) {
        pos += 12 + 4;  /* 12x 0x00 + 3x 0xC2 + FC */
    } else {
        pos += 6 + 1;   /* FM: 6x 0x00 + FC */
    }
    
    /* GAP1 */
    pos += fmt->gaps.gap1;
    
    for (int s = 0; s < fmt->sectors && s < max_sectors; s++) {
        /* Record sector start position */
        sector_offsets[sector_count++] = pos;
        
        /* ID Address Mark */
        if (fmt->mfm) {
            pos += 12 + 4;  /* 12x 0x00 + 3x 0xA1 + FE */
        } else {
            pos += 6 + 1;   /* FM */
        }
        
        /* ID field: C H R N + CRC */
        pos += 4 + 2;
        
        /* GAP2 */
        pos += fmt->gaps.gap2;
        
        /* Data Address Mark */
        if (fmt->mfm) {
            pos += 12 + 4;  /* 12x 0x00 + 3x 0xA1 + FB */
        } else {
            pos += 6 + 1;
        }
        
        /* Data field + CRC */
        pos += fmt->sector_size + 2;
        
        /* GAP3 */
        pos += fmt->gaps.gap3_rw;
    }
    
    return sector_count;
}

/*===========================================================================
 * Gap Calculation
 *===========================================================================*/

uint8_t uft_fdc_calc_gap3(uint32_t track_capacity, uint8_t sectors,
                          uint16_t sector_size, bool mfm)
{
    if (sectors == 0) return 0;
    
    /* Calculate fixed overhead per sector */
    uint32_t overhead_per_sector;
    
    if (mfm) {
        /* MFM overhead: sync + AM + ID + CRC + GAP2 + sync + DAM */
        overhead_per_sector = 12 + 4 + 4 + 2 + 22 + 12 + 4;  /* 60 bytes */
    } else {
        /* FM overhead */
        overhead_per_sector = 6 + 1 + 4 + 2 + 11 + 6 + 1;    /* 31 bytes */
    }
    
    /* Track header overhead */
    uint32_t track_overhead = mfm ? (80 + 12 + 4 + 50) : (40 + 6 + 1 + 26);
    
    /* Calculate available space for gaps */
    uint32_t data_space = (uint32_t)sectors * (sector_size + overhead_per_sector + 2);
    uint32_t gap_space = track_capacity - track_overhead - data_space;
    
    /* Divide among sectors */
    uint32_t gap3 = gap_space / sectors;
    
    /* Clamp to valid range */
    if (gap3 < 10) gap3 = 10;
    if (gap3 > 255) gap3 = 255;
    
    return (uint8_t)gap3;
}

/*===========================================================================
 * Size Code Conversion
 *===========================================================================*/

/* Size code table: 2^(N+7) */
static const uint16_t SIZE_TABLE[] = {
    128, 256, 512, 1024, 2048, 4096, 8192, 16384
};

uint8_t uft_fdc_size_code(uint16_t sector_size)
{
    for (int i = 0; i < 8; i++) {
        if (SIZE_TABLE[i] == sector_size) {
            return (uint8_t)i;
        }
    }
    return 2;  /* Default: 512 bytes */
}

uint16_t uft_fdc_sector_size(uint8_t size_code)
{
    if (size_code > 7) size_code = 7;
    return SIZE_TABLE[size_code];
}

/*===========================================================================
 * Format Listing
 *===========================================================================*/

void uft_fdc_list_formats(void)
{
    printf("Supported FDC Formats:\n");
    printf("%-25s %3s %3s %3s %5s %6s\n", 
           "Name", "Trk", "Sid", "Sec", "Size", "Rate");
    printf("%-25s %3s %3s %3s %5s %6s\n",
           "-------------------------", "---", "---", "---", "-----", "------");
    
    for (int i = 0; UFT_FDC_FORMATS[i]; i++) {
        const uft_fdc_format_t *fmt = UFT_FDC_FORMATS[i];
        
        const char *rate_str;
        switch (fmt->data_rate) {
            case UFT_FDC_RATE_500K: rate_str = "500K"; break;
            case UFT_FDC_RATE_300K: rate_str = "300K"; break;
            case UFT_FDC_RATE_250K: rate_str = "250K"; break;
            case UFT_FDC_RATE_1M:   rate_str = "1M"; break;
            default: rate_str = "???"; break;
        }
        
        printf("%-25s %3d %3d %3d %5d %6s\n",
               fmt->name, fmt->tracks, fmt->sides, fmt->sectors,
               fmt->sector_size, rate_str);
    }
}
