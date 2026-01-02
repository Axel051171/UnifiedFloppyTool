/**
 * @file uft_sector_status.c
 * @brief Sector Status System Implementation
 */

#include "uft/uft_sector_status.h"
#include <string.h>

const char* uft_sector_status_string(uft_sector_status_t status)
{
    switch (status) {
        case UFT_SECTOR_OK:
            return "OK";
        case UFT_SECTOR_CRC_BAD:
            return "CRC_BAD";
        case UFT_SECTOR_MISSING:
            return "MISSING";
        case UFT_SECTOR_WEAK:
            return "WEAK";
        case UFT_SECTOR_FIXED:
            return "FIXED";
        default:
            return "UNKNOWN";
    }
}

bool uft_sector_status_is_usable(uft_sector_status_t status)
{
    return (status == UFT_SECTOR_OK || status == UFT_SECTOR_FIXED);
}
