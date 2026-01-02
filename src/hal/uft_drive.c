/**
 * @file uft_drive.c
 * @brief Drive Profiles Implementation
 */

#include "uft/hal/uft_drive.h"
#include <string.h>

static const uft_drive_profile_t DRIVE_PROFILES[] = {
    {DRIVE_TYPE_525_DD, "5.25\" DD", 40, 2, 300.0, 15, 500, 250000},
    {DRIVE_TYPE_525_HD, "5.25\" HD", 80, 2, 360.0, 15, 500, 500000},
    {DRIVE_TYPE_35_DD,  "3.5\" DD",  80, 2, 300.0, 15, 400, 250000},
    {DRIVE_TYPE_35_HD,  "3.5\" HD",  80, 2, 300.0, 15, 400, 500000},
    {DRIVE_TYPE_35_ED,  "3.5\" ED",  80, 2, 300.0, 15, 400, 1000000},
    {DRIVE_TYPE_1541,   "C64 1541", 35, 1, 300.0, 20, 600, 250000},
    {DRIVE_TYPE_APPLE,  "Apple II", 35, 1, 300.0, 20, 600, 250000}
};

const uft_drive_profile_t* uft_drive_get_profile(uft_drive_type_t type) {
    if (type >= 0 && type <= DRIVE_TYPE_APPLE) return &DRIVE_PROFILES[type];
    return NULL;
}

int uft_drive_list_profiles(uft_drive_profile_t* profiles, int max_count) {
    int count = sizeof(DRIVE_PROFILES) / sizeof(DRIVE_PROFILES[0]);
    if (count > max_count) count = max_count;
    memcpy(profiles, DRIVE_PROFILES, count * sizeof(uft_drive_profile_t));
    return count;
}

const char* uft_drive_type_name(uft_drive_type_t type) {
    const uft_drive_profile_t* p = uft_drive_get_profile(type);
    return p ? p->name : "Unknown";
}
