#define _POSIX_C_SOURCE 200809L
/**
 * @file uft_floppy_geometry.c
 * @brief Disk geometry and LBA/CHS conversion implementation
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <stdio.h>
#include <sys/types.h>
#include <string.h>

#include "uft_floppy_geometry.h"

/*===========================================================================
 * Geometry Detection
 *===========================================================================*/

uft_floppy_type_t uft_geometry_detect_type(uint64_t total_bytes) {
    /* Check against known floppy sizes */
    const uft_geometry_t *g = UFT_GEOMETRIES;
    
    while (g->description != NULL) {
        if (g->total_bytes == total_bytes) {
            return g->type;
        }
        g++;
    }
    
    /* Check with some tolerance (within 1 sector) */
    g = UFT_GEOMETRIES;
    while (g->description != NULL) {
        int64_t diff = (int64_t)total_bytes - (int64_t)g->total_bytes;
        if (diff >= 0 && diff < 512) {
            return g->type;
        }
        g++;
    }
    
    return UFT_FLOPPY_UNKNOWN;
}

uft_error_t uft_geometry_get_standard(uft_floppy_type_t type, uft_geometry_t *geom) {
    if (!geom) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    const uft_geometry_t *g = UFT_GEOMETRIES;
    
    while (g->description != NULL) {
        if (g->type == type) {
            *geom = *g;
            return UFT_OK;
        }
        g++;
    }
    
    return UFT_ERR_NOT_FOUND;
}

uft_error_t uft_geometry_create(uint16_t cylinders, uint8_t heads,
                                 uint8_t sectors, uint16_t bytes_per_sector,
                                 uft_geometry_t *geom) {
    if (!geom) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (cylinders == 0 || heads == 0 || sectors == 0 || bytes_per_sector == 0) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    geom->cylinders = cylinders;
    geom->heads = heads;
    geom->sectors_per_track = sectors;
    geom->bytes_per_sector = bytes_per_sector;
    geom->total_sectors = (uint32_t)cylinders * heads * sectors;
    geom->total_bytes = (uint32_t)geom->total_sectors * bytes_per_sector;
    geom->type = UFT_FLOPPY_UNKNOWN;
    geom->description = "Custom geometry";
    
    return UFT_OK;
}

uft_error_t uft_geometry_from_bpb(const uft_bpb_t *bpb, uft_geometry_t *geom) {
    if (!bpb || !geom) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    uint16_t bytes_per_sector = uft_le16_to_cpu(bpb->bytes_per_sector);
    uint16_t sectors_per_track = uft_le16_to_cpu(bpb->sectors_per_track);
    uint16_t heads = uft_le16_to_cpu(bpb->heads);
    uint16_t total_sectors_16 = uft_le16_to_cpu(bpb->total_sectors_16);
    uint32_t total_sectors_32 = uft_le32_to_cpu(bpb->total_sectors_32);
    
    uint32_t total_sectors = total_sectors_16 ? total_sectors_16 : total_sectors_32;
    
    if (sectors_per_track == 0 || heads == 0) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    uint32_t cylinders = total_sectors / (sectors_per_track * heads);
    
    geom->cylinders = (uint16_t)cylinders;
    geom->heads = (uint8_t)heads;
    geom->sectors_per_track = (uint8_t)sectors_per_track;
    geom->bytes_per_sector = bytes_per_sector;
    geom->total_sectors = total_sectors;
    geom->total_bytes = (uint32_t)total_sectors * bytes_per_sector;
    
    /* Try to detect type */
    geom->type = uft_geometry_detect_type(geom->total_bytes);
    if (geom->type != UFT_FLOPPY_UNKNOWN) {
        const uft_geometry_t *std;
        for (std = UFT_GEOMETRIES; std->description; std++) {
            if (std->type == geom->type) {
                geom->description = std->description;
                break;
            }
        }
    } else {
        geom->description = "Unknown geometry";
    }
    
    return UFT_OK;
}

uft_error_t uft_geometry_validate(const uft_geometry_t *geom) {
    if (!geom) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Minimum requirements based on lbacache logic */
    if (geom->sectors_per_track < 7) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    if (geom->heads < 1) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    if (geom->cylinders == 0) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    if (geom->bytes_per_sector == 0) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    /* Check for overflow */
    uint64_t calc_size = (uint64_t)geom->cylinders * geom->heads * 
                          geom->sectors_per_track * geom->bytes_per_sector;
    if (calc_size != geom->total_bytes) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    return UFT_OK;
}

/*===========================================================================
 * LBA / CHS Conversion
 *===========================================================================*/

uft_error_t uft_chs_to_lba(const uft_geometry_t *geom, const uft_chs_t *chs,
                            uint32_t *lba) {
    if (!geom || !chs || !lba) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    /* Validate CHS values */
    if (chs->head >= geom->heads) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    if (chs->sector < 1 || chs->sector > geom->sectors_per_track) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    if (chs->cylinder >= geom->cylinders) {
        return UFT_ERR_CHS_OVERFLOW;
    }
    
    /* LBA = (C × heads + H) × sectors_per_track + (S - 1) */
    *lba = ((uint32_t)chs->cylinder * geom->heads + chs->head) * 
            geom->sectors_per_track + (chs->sector - 1);
    
    return UFT_OK;
}

uft_error_t uft_lba_to_chs(const uft_geometry_t *geom, uint32_t lba,
                            uft_chs_t *chs) {
    if (!geom || !chs) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (geom->sectors_per_track == 0 || geom->heads == 0) {
        return UFT_ERR_GEOMETRY_INVALID;
    }
    
    /* S = (LBA mod sectors_per_track) + 1 */
    chs->sector = (uint8_t)((lba % geom->sectors_per_track) + 1);
    
    /* temp = LBA / sectors_per_track */
    uint32_t temp = lba / geom->sectors_per_track;
    
    /* H = temp mod heads */
    chs->head = (uint8_t)(temp % geom->heads);
    
    /* C = temp / heads */
    uint32_t cylinder = temp / geom->heads;
    
    /* Check for overflow (CHS can only address cylinders 0-1023) */
    if (cylinder >= 1024) {
        chs->cylinder = 1023;
        chs->head = (uint8_t)(geom->heads - 1);
        chs->sector = geom->sectors_per_track;
        return UFT_ERR_CHS_OVERFLOW;
    }
    
    chs->cylinder = (uint16_t)cylinder;
    
    return UFT_OK;
}

/*===========================================================================
 * BIOS Int 13h Encoding
 *===========================================================================*/

uft_error_t uft_chs_to_bios(const uft_chs_t *chs, uint8_t drive_num,
                             uint16_t *cx, uint16_t *dx) {
    if (!chs || !cx || !dx) {
        return UFT_ERR_INVALID_PARAM;
    }
    
    if (chs->cylinder > 1023) {
        return UFT_ERR_CHS_OVERFLOW;
    }
    
    /* CX format: CCCCCCCC CCSSSSSS
     * - Bits 15-8: Low 8 bits of cylinder
     * - Bits 7-6: High 2 bits of cylinder
     * - Bits 5-0: Sector number (1-63)
     */
    uint8_t cyl_low = (uint8_t)(chs->cylinder & 0xFF);
    uint8_t cyl_high = (uint8_t)((chs->cylinder >> 8) & 0x03);
    
    *cx = ((uint16_t)cyl_low << 8) | (cyl_high << 6) | (chs->sector & 0x3F);
    
    /* DX format: HHHHHHHH DDDDDDDD
     * - Bits 15-8: Head number
     * - Bits 7-0: Drive number
     */
    *dx = ((uint16_t)chs->head << 8) | drive_num;
    
    return UFT_OK;
}

void uft_bios_to_chs(uint16_t cx, uint16_t dx, uft_chs_t *chs, uint8_t *drive_num) {
    if (!chs) return;
    
    /* Decode CX */
    uint8_t cyl_low = (uint8_t)(cx >> 8);
    uint8_t cyl_high = (uint8_t)((cx >> 6) & 0x03);
    
    chs->cylinder = ((uint16_t)cyl_high << 8) | cyl_low;
    chs->sector = (uint8_t)(cx & 0x3F);
    
    /* Decode DX */
    chs->head = (uint8_t)(dx >> 8);
    
    if (drive_num) {
        *drive_num = (uint8_t)(dx & 0xFF);
    }
}

/*===========================================================================
 * String Formatting
 *===========================================================================*/

int uft_geometry_to_string(const uft_geometry_t *geom, char *buffer, size_t size) {
    if (!geom || !buffer || size == 0) {
        return 0;
    }
    
    return snprintf(buffer, size, 
                    "C:%u H:%u S:%u (%u sectors, %u bytes)",
                    geom->cylinders, geom->heads, geom->sectors_per_track,
                    geom->total_sectors, geom->total_bytes);
}

int uft_chs_to_string(const uft_chs_t *chs, char *buffer, size_t size) {
    if (!chs || !buffer || size == 0) {
        return 0;
    }
    
    return snprintf(buffer, size, "C:%u H:%u S:%u",
                    chs->cylinder, chs->head, chs->sector);
}

const char* uft_floppy_type_name(uft_floppy_type_t type) {
    const uft_geometry_t *g = UFT_GEOMETRIES;
    
    while (g->description != NULL) {
        if (g->type == type) {
            return g->description;
        }
        g++;
    }
    
    return "Unknown";
}
