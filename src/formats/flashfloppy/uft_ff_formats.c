/**
 * @file uft_ff_formats.c
 * @brief FlashFloppy-derived format detection algorithms
 *
 * Original code by Keir Fraser (Public Domain/Unlicense)
 * Adapted for UFT
 *
 * SPDX-License-Identifier: Unlicense
 */

#include "uft/formats/flashfloppy/uft_ff_formats.h"
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

static void init_geometry(uft_ff_geometry_t *geom) {
    memset(geom, 0, sizeof(*geom));
    geom->sector_size = 512;
    geom->rpm = 300;
    geom->gap3 = 84;
    geom->interleave = 1;
}

/* ============================================================================
 * TI-99/4A Format Detection
 * ============================================================================ */

int uft_ff_detect_ti99(const uint8_t *data, size_t file_size,
                       uft_ff_detect_result_t *result)
{
    if (!data || !result || file_size == 0) {
        return UFT_FF_ERR_INVALID;
    }

    /* Must be a multiple of 256-byte sectors */
    if ((file_size % 256) != 0) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    size_t num_sectors = file_size / 256;

    /* Check for 3-sector footer (bad sector map) - ignore it */
    if ((num_sectors % 10) == 3) {
        num_sectors -= 3;
    }

    if (num_sectors == 0) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    /* Check for Volume Information Block (VIB) in sector 0 */
    const uft_ti99_vib_t *vib = (const uft_ti99_vib_t *)data;
    bool have_vib = (memcmp(vib->id, "DSK", 3) == 0);

    memset(result, 0, sizeof(*result));
    init_geometry(&result->geometry);
    
    result->geometry.sector_size = 256;
    result->geometry.interleave = 4;
    result->geometry.skew = 3;
    result->flags = UFT_FF_FLAG_SEQUENTIAL | UFT_FF_FLAG_REVERSE_SIDE1;

    /* Detect format based on sector count */
    if ((num_sectors % (40 * 9)) == 0) {
        switch (num_sectors / (40 * 9)) {
        case 1: /* SSSD 90K */
            result->geometry.cylinders = 40;
            result->geometry.heads = 1;
            result->geometry.sectors_per_track = 9;
            result->geometry.gap3 = 44;
            result->geometry.is_fm = true;
            result->format_name = "TI99-SSSD";
            result->format_desc = "TI-99/4A Single-Sided Single-Density";
            result->confidence = 80;
            break;

        case 2: /* DSSD 180K or SSDD 180K */
            if (have_vib && (vib->sides == 1)) {
                /* SSDD */
                result->geometry.cylinders = 40;
                result->geometry.heads = 1;
                result->geometry.sectors_per_track = 18;
                result->geometry.gap3 = 24;
                result->geometry.interleave = 5;
                result->geometry.is_fm = false;
                result->format_name = "TI99-SSDD";
                result->format_desc = "TI-99/4A Single-Sided Double-Density";
                result->confidence = 90;
            } else {
                /* DSSD */
                result->geometry.cylinders = 40;
                result->geometry.heads = 2;
                result->geometry.sectors_per_track = 9;
                result->geometry.gap3 = 44;
                result->geometry.is_fm = true;
                result->format_name = "TI99-DSSD";
                result->format_desc = "TI-99/4A Double-Sided Single-Density";
                result->confidence = have_vib ? 90 : 70;
            }
            break;

        case 4: /* DSDD 360K or DSSD80 360K */
            if (have_vib && (vib->tracks_per_side == 80)) {
                /* DSSD80 */
                result->geometry.cylinders = 80;
                result->geometry.heads = 2;
                result->geometry.sectors_per_track = 9;
                result->geometry.gap3 = 44;
                result->geometry.is_fm = true;
                result->format_name = "TI99-DSSD80";
                result->format_desc = "TI-99/4A DS/SD 80-track";
                result->confidence = 90;
            } else {
                /* DSDD */
                result->geometry.cylinders = 40;
                result->geometry.heads = 2;
                result->geometry.sectors_per_track = 18;
                result->geometry.gap3 = 24;
                result->geometry.interleave = 5;
                result->geometry.is_fm = false;
                result->format_name = "TI99-DSDD";
                result->format_desc = "TI-99/4A Double-Sided Double-Density";
                result->confidence = have_vib ? 90 : 70;
            }
            break;

        case 8: /* DSDD80 720K */
            result->geometry.cylinders = 80;
            result->geometry.heads = 2;
            result->geometry.sectors_per_track = 18;
            result->geometry.gap3 = 24;
            result->geometry.interleave = 5;
            result->geometry.is_fm = false;
            result->format_name = "TI99-DSDD80";
            result->format_desc = "TI-99/4A DS/DD 80-track";
            result->confidence = 85;
            break;

        case 16: /* DSHD80 1.44M */
            result->geometry.cylinders = 80;
            result->geometry.heads = 2;
            result->geometry.sectors_per_track = 36;
            result->geometry.gap3 = 24;
            result->geometry.interleave = 5;
            result->geometry.is_fm = false;
            result->format_name = "TI99-DSHD";
            result->format_desc = "TI-99/4A DS/HD 80-track";
            result->confidence = 80;
            break;

        default:
            return UFT_FF_ERR_NOT_DETECTED;
        }
    } else if ((num_sectors % (40 * 16)) == 0) {
        /* SSDD/DSDD, 16 sectors */
        size_t sides = num_sectors / (40 * 16);
        if (sides <= 2) {
            result->geometry.cylinders = 40;
            result->geometry.heads = (uint8_t)sides;
            result->geometry.sectors_per_track = 16;
            result->geometry.gap3 = 44;
            result->geometry.interleave = 5;
            result->geometry.is_fm = false;
            result->format_name = sides == 1 ? "TI99-SSDD16" : "TI99-DSDD16";
            result->format_desc = "TI-99/4A 16-sector format";
            result->confidence = 75;
        } else {
            return UFT_FF_ERR_NOT_DETECTED;
        }
    } else {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    /* Boost confidence if VIB present */
    if (have_vib && result->confidence < 95) {
        result->confidence += 5;
    }

    return UFT_FF_OK;
}

/* ============================================================================
 * PC-98 FDI Format Detection
 * ============================================================================ */

int uft_ff_detect_pc98_fdi(const uint8_t *header, size_t file_size,
                           uft_ff_detect_result_t *result)
{
    if (!header || !result) {
        return UFT_FF_ERR_INVALID;
    }

    const uft_pc98_fdi_header_t *fdi = (const uft_pc98_fdi_header_t *)header;

    /* First 4 bytes must be zero */
    if (read_le32((const uint8_t *)&fdi->zero) != 0) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    uint32_t density = read_le32((const uint8_t *)&fdi->density);
    uint32_t header_size = read_le32((const uint8_t *)&fdi->header_size);
    uint32_t sector_size = read_le32((const uint8_t *)&fdi->sector_size);
    uint32_t spt = read_le32((const uint8_t *)&fdi->sectors_per_track);
    uint32_t heads = read_le32((const uint8_t *)&fdi->heads);
    uint32_t cyls = read_le32((const uint8_t *)&fdi->cylinders);

    /* Sanity checks */
    if (header_size < 32 || header_size > 65536) {
        return UFT_FF_ERR_NOT_DETECTED;
    }
    if (sector_size != 256 && sector_size != 512 && sector_size != 1024) {
        return UFT_FF_ERR_NOT_DETECTED;
    }
    if (spt == 0 || spt > 32 || heads == 0 || heads > 2 || cyls == 0 || cyls > 85) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    memset(result, 0, sizeof(*result));
    init_geometry(&result->geometry);

    result->geometry.cylinders = (uint16_t)cyls;
    result->geometry.heads = (uint8_t)heads;
    result->geometry.sectors_per_track = (uint8_t)spt;
    result->geometry.sector_size = (uint16_t)sector_size;
    result->geometry.data_offset = header_size;

    if (density == 0x30) {
        /* 2DD */
        result->geometry.rpm = 300;
        result->geometry.gap3 = 84;
        result->format_name = "PC98-FDI-2DD";
        result->format_desc = "PC-98 FDI (2DD)";
    } else {
        /* 2HD */
        result->geometry.rpm = 360;
        result->geometry.gap3 = 116;
        result->format_name = "PC98-FDI-2HD";
        result->format_desc = "PC-98 FDI (2HD)";
    }

    result->confidence = 90;
    return UFT_FF_OK;
}

/* ============================================================================
 * PC-98 HDM Format Detection
 * ============================================================================ */

int uft_ff_detect_pc98_hdm(size_t file_size, uft_ff_detect_result_t *result)
{
    if (!result) {
        return UFT_FF_ERR_INVALID;
    }

    /* PC-98 HDM: 77 tracks, 2 heads, 8 sectors, 1024 bytes = 1261568 bytes */
    /* Or: 80 tracks, 2 heads, 8 sectors, 1024 bytes = 1310720 bytes */
    
    memset(result, 0, sizeof(*result));
    init_geometry(&result->geometry);

    if (file_size == 1261568) {
        /* 77-track format (1.25MB) */
        result->geometry.cylinders = 77;
        result->geometry.heads = 2;
        result->geometry.sectors_per_track = 8;
        result->geometry.sector_size = 1024;
        result->geometry.rpm = 360;
        result->geometry.gap3 = 116;
        result->format_name = "PC98-HDM-77";
        result->format_desc = "PC-98 HDM 1.25MB (77-track)";
        result->confidence = 85;
        return UFT_FF_OK;
    }
    
    if (file_size == 1310720) {
        /* 80-track format (1.28MB) */
        result->geometry.cylinders = 80;
        result->geometry.heads = 2;
        result->geometry.sectors_per_track = 8;
        result->geometry.sector_size = 1024;
        result->geometry.rpm = 360;
        result->geometry.gap3 = 116;
        result->format_name = "PC98-HDM-80";
        result->format_desc = "PC-98 HDM 1.28MB (80-track)";
        result->confidence = 85;
        return UFT_FF_OK;
    }

    return UFT_FF_ERR_NOT_DETECTED;
}

/* ============================================================================
 * MSX Format Detection
 * ============================================================================ */

int uft_ff_detect_msx(const uint8_t *boot_sector, size_t file_size,
                      uft_ff_detect_result_t *result)
{
    if (!boot_sector || !result) {
        return UFT_FF_ERR_INVALID;
    }

    /* Check boot signature */
    uint16_t signature = read_le16(boot_sector + 510);
    if (signature != 0xAA55) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    /* Read BPB fields */
    uint16_t bytes_per_sector = read_le16(boot_sector + 0x0B);
    uint16_t sectors_per_track = read_le16(boot_sector + 0x18);
    uint16_t heads = read_le16(boot_sector + 0x1A);
    uint16_t total_sectors = read_le16(boot_sector + 0x13);

    /* MSX validation */
    if (bytes_per_sector != 512) {
        return UFT_FF_ERR_NOT_DETECTED;
    }
    if (sectors_per_track == 0 || sectors_per_track > 18) {
        return UFT_FF_ERR_NOT_DETECTED;
    }
    if (heads == 0 || heads > 2) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    /* Check for MSX-specific media byte */
    uint8_t media_byte = boot_sector[0x15];
    bool is_msx = false;

    /* MSX media bytes: F8, F9, FA, FB, FC, FD, FE, FF */
    if (media_byte >= 0xF8) {
        /* Check for MSX boot code patterns */
        if (boot_sector[0] == 0xEB || boot_sector[0] == 0xE9) {
            is_msx = true;
        }
    }

    if (!is_msx) {
        return UFT_FF_ERR_NOT_DETECTED;
    }

    memset(result, 0, sizeof(*result));
    init_geometry(&result->geometry);

    uint16_t cyls = total_sectors / (sectors_per_track * heads);
    
    result->geometry.cylinders = cyls;
    result->geometry.heads = (uint8_t)heads;
    result->geometry.sectors_per_track = (uint8_t)sectors_per_track;
    result->geometry.sector_size = bytes_per_sector;

    /* Determine format variant */
    if (file_size == 368640) {
        result->format_name = "MSX-360K";
        result->format_desc = "MSX 360KB (80T/1H/9S)";
        result->confidence = 85;
    } else if (file_size == 737280) {
        result->format_name = "MSX-720K";
        result->format_desc = "MSX 720KB (80T/2H/9S)";
        result->confidence = 85;
    } else {
        result->format_name = "MSX";
        result->format_desc = "MSX Disk";
        result->confidence = 70;
    }

    return UFT_FF_OK;
}

/* ============================================================================
 * MGT Format Detection (SAM Coupé / Spectrum +D)
 * ============================================================================ */

int uft_ff_detect_mgt(size_t file_size, uft_ff_detect_result_t *result)
{
    if (!result) {
        return UFT_FF_ERR_INVALID;
    }

    memset(result, 0, sizeof(*result));
    init_geometry(&result->geometry);

    /* MGT: 80 tracks, 2 sides, 10 sectors, 512 bytes = 819200 bytes */
    if (file_size == 819200) {
        result->geometry.cylinders = 80;
        result->geometry.heads = 2;
        result->geometry.sectors_per_track = 10;
        result->geometry.sector_size = 512;
        result->geometry.rpm = 300;
        result->geometry.gap3 = 36;
        result->geometry.interleave = 1;
        result->format_name = "MGT";
        result->format_desc = "SAM Coupé / Spectrum +D (MGT)";
        result->confidence = 80;
        return UFT_FF_OK;
    }

    /* SAD format: same size but different structure */
    /* We'll just report MGT for now */

    return UFT_FF_ERR_NOT_DETECTED;
}

/* ============================================================================
 * UKNC Format Detection (Soviet PDP-11 Clone)
 * ============================================================================ */

int uft_ff_detect_uknc(size_t file_size, uft_ff_detect_result_t *result)
{
    if (!result) {
        return UFT_FF_ERR_INVALID;
    }

    memset(result, 0, sizeof(*result));
    init_geometry(&result->geometry);

    /* UKNC: 80 tracks, 2 sides, 10 sectors, 512 bytes = 819200 bytes */
    /* Same size as MGT but with special track format */
    if (file_size == 819200) {
        result->geometry.cylinders = 80;
        result->geometry.heads = 2;
        result->geometry.sectors_per_track = 10;
        result->geometry.sector_size = 512;
        result->geometry.rpm = 300;
        result->geometry.gap3 = 36;
        result->geometry.interleave = 1;
        result->format_name = "UKNC";
        result->format_desc = "UKNC (Soviet PDP-11 clone)";
        result->confidence = 60; /* Lower confidence - same size as MGT */
        return UFT_FF_OK;
    }

    return UFT_FF_ERR_NOT_DETECTED;
}

/* ============================================================================
 * Auto-Detection
 * ============================================================================ */

int uft_ff_detect_auto(const uint8_t *data, size_t data_size, size_t file_size,
                       uft_ff_detect_result_t *result)
{
    if (!data || !result || data_size < 512) {
        return UFT_FF_ERR_INVALID;
    }

    uft_ff_detect_result_t best = {0};
    uft_ff_detect_result_t current;
    int rc;

    /* Try PC-98 FDI first (has distinctive header) */
    if (data_size >= sizeof(uft_pc98_fdi_header_t)) {
        rc = uft_ff_detect_pc98_fdi(data, file_size, &current);
        if (rc == UFT_FF_OK && current.confidence > best.confidence) {
            best = current;
        }
    }

    /* Try TI-99 */
    rc = uft_ff_detect_ti99(data, file_size, &current);
    if (rc == UFT_FF_OK && current.confidence > best.confidence) {
        best = current;
    }

    /* Try MSX */
    rc = uft_ff_detect_msx(data, file_size, &current);
    if (rc == UFT_FF_OK && current.confidence > best.confidence) {
        best = current;
    }

    /* Try PC-98 HDM */
    rc = uft_ff_detect_pc98_hdm(file_size, &current);
    if (rc == UFT_FF_OK && current.confidence > best.confidence) {
        best = current;
    }

    /* Try MGT */
    rc = uft_ff_detect_mgt(file_size, &current);
    if (rc == UFT_FF_OK && current.confidence > best.confidence) {
        best = current;
    }

    /* Try UKNC (lower priority, same size as MGT) */
    rc = uft_ff_detect_uknc(file_size, &current);
    if (rc == UFT_FF_OK && current.confidence > best.confidence) {
        best = current;
    }

    if (best.confidence > 0) {
        *result = best;
        return UFT_FF_OK;
    }

    return UFT_FF_ERR_NOT_DETECTED;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *uft_ff_format_name(const uft_ff_detect_result_t *result)
{
    if (!result || !result->format_name) {
        return "Unknown";
    }
    return result->format_name;
}

void uft_ff_print_geometry(const uft_ff_geometry_t *geom)
{
    if (!geom) return;
    
    printf("Geometry:\n");
    printf("  Cylinders: %u\n", geom->cylinders);
    printf("  Heads: %u\n", geom->heads);
    printf("  Sectors/Track: %u\n", geom->sectors_per_track);
    printf("  Sector Size: %u\n", geom->sector_size);
    printf("  RPM: %u\n", geom->rpm);
    printf("  GAP3: %u\n", geom->gap3);
    printf("  Interleave: %u\n", geom->interleave);
    printf("  Encoding: %s\n", geom->is_fm ? "FM" : "MFM");
}
