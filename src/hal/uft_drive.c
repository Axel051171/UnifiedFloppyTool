/**
 * @file uft_drive.c
 * @brief Drive Profiles Implementation
 */

#include "uft/hal/uft_drive.h"
#include <string.h>

/* Simple profiles table - basic functionality */
static const uft_drive_profile_t DRIVE_PROFILES[] = {
    { .type = UFT_DRIVE_525_DD, .name = "5.25\" DD", .cylinders = 40, .heads = 2,
      .rpm = 300.0, .step_time_ms = 15, .settle_time_ms = 15, .data_rate_dd = 250.0,
      .default_encoding = DRIVE_ENC_MFM },
    { .type = UFT_DRIVE_525_HD, .name = "5.25\" HD", .cylinders = 80, .heads = 2,
      .rpm = 360.0, .step_time_ms = 15, .settle_time_ms = 15, .data_rate_hd = 500.0,
      .default_encoding = DRIVE_ENC_MFM },
    { .type = UFT_DRIVE_35_DD, .name = "3.5\" DD", .cylinders = 80, .heads = 2,
      .rpm = 300.0, .step_time_ms = 15, .settle_time_ms = 15, .data_rate_dd = 250.0,
      .default_encoding = DRIVE_ENC_MFM },
    { .type = UFT_DRIVE_35_HD, .name = "3.5\" HD", .cylinders = 80, .heads = 2,
      .rpm = 300.0, .step_time_ms = 15, .settle_time_ms = 15, .data_rate_hd = 500.0,
      .default_encoding = DRIVE_ENC_MFM },
    { .type = UFT_DRIVE_35_ED, .name = "3.5\" ED", .cylinders = 80, .heads = 2,
      .rpm = 300.0, .step_time_ms = 15, .settle_time_ms = 15, .data_rate_hd = 1000.0,
      .default_encoding = DRIVE_ENC_MFM },
    { .type = UFT_DRIVE_1541, .name = "C64 1541", .cylinders = 35, .heads = 1,
      .rpm = 300.0, .step_time_ms = 20, .settle_time_ms = 20, .data_rate_dd = 250.0,
      .default_encoding = DRIVE_ENC_GCR },
    { .type = UFT_DRIVE_APPLE, .name = "Apple II", .cylinders = 35, .heads = 1,
      .rpm = 300.0, .step_time_ms = 20, .settle_time_ms = 20, .data_rate_dd = 250.0,
      .default_encoding = DRIVE_ENC_GCR }
};

#define PROFILE_COUNT (sizeof(DRIVE_PROFILES) / sizeof(DRIVE_PROFILES[0]))

const uft_drive_profile_t* uft_drive_get_profile(uft_drive_type_t type) {
    for (size_t i = 0; i < PROFILE_COUNT; i++) {
        if (DRIVE_PROFILES[i].type == type) return &DRIVE_PROFILES[i];
    }
    return NULL;
}

int uft_drive_list_profiles(uft_drive_profile_t* profiles, int max_count) {
    int count = (int)PROFILE_COUNT;
    if (count > max_count) count = max_count;
    memcpy(profiles, DRIVE_PROFILES, count * sizeof(uft_drive_profile_t));
    return count;
}

const char* uft_drive_type_name(uft_drive_type_t type) {
    const uft_drive_profile_t* p = uft_drive_get_profile(type);
    return p ? p->name : "Unknown";
}
