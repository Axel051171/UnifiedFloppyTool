// SPDX-License-Identifier: MIT
/*
 * hxc_util.c - HXC Utilities
 * 
 * Format detection and helper functions for HXC formats.
 * 
 * @version 2.7.5
 * @date 2024-12-25
 */

#include "uft/uft_error.h"
#include "../include/hxc_format.h"
#include <string.h>
#include <stdio.h>

/**
 * @brief Detect HXC format from file signature
 */
const char* hxc_detect_format(const uint8_t *file)
{
    if (!file) {
        return "Invalid";
    }
    
    /* Check HFE signature */
    if (memcmp(file, "HXCPICFE", 8) == 0) {
        return "HFE";
    }
    
    /* Check for MFM-encoded HFE variants */
    if (memcmp(file, "HXCMFMFE", 8) == 0) {
        return "MFM_HFE";
    }
    
    return "Unknown";
}

/**
 * @brief Get encoding name
 */
const char* hxc_get_encoding_name(uint8_t encoding)
{
    switch (encoding) {
        case 0x00: return "ISO/IBM MFM";
        case 0x01: return "Amiga MFM";
        case 0x02: return "ISO/IBM FM";
        case 0x03: return "EMU FM";
        default: return "Unknown";
    }
}

/**
 * @brief Free sector
 */
void hxc_free_sector(hxc_sector_t *sector)
{
    if (!sector) {
        return;
    }
    
    free(sector->data);
    sector->data = NULL;
}

/**
 * @brief Free disk
 */
void hxc_free_disk(hxc_disk_t *disk)
{
    if (!disk) {
        return;
    }
    
    if (disk->sectors) {
        for (uint32_t i = 0; i < disk->sector_count; i++) {
            hxc_free_sector(&disk->sectors[i]);
        }
        free(disk->sectors);
    }
    
    memset(disk, 0, sizeof(*disk));
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
