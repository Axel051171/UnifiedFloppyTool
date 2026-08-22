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
      .default_encoding = DRIVE_ENC_GCR },

    /* Atari 8-bit — die einzigen Laufwerke hier, die NICHT mit 300 min^-1
     * drehen (MF-471). 288 belegt in src/a8rawconv/a8rawconv.cpp:133
     * ("Atari disk timing produces 250,000 clocks per second at 288 RPM"),
     * analyze.cpp:25 und encode.cpp:4.
     *
     * Warum das zaehlt: liest man eine dieser Disketten in einem
     * 300-min^-1-Laufwerk — der Normalfall beim Sichern mit Greaseweazle,
     * SCP oder KryoFlux — laufen ihre Bitzellen um 288/300 = 4 % schneller
     * vorbei. Die Korrektur rechnet uft_media_cell_ns_from_rev()
     * (include/uft/flux/uft_media_profile.h) aus der GEMESSENEN
     * Umdrehungsdauer; dieses Feld hier sagt, was beim SCHREIBEN galt.
     *
     * Geometrien aus src/a8rawconv/diskatr.cpp:42-56:
     *   SD 18x128 FM, ED 26x128 MFM, DD 18x256 MFM (zweiseitig = 1440). */
    { .type = UFT_DRIVE_ATARI_810, .name = "Atari 810 (SD)", .cylinders = 40, .heads = 1,
      .rpm = 288.0, .step_time_ms = 20, .settle_time_ms = 20, .data_rate_dd = 250.0,
      .default_encoding = DRIVE_ENC_FM },
    { .type = UFT_DRIVE_ATARI_1050, .name = "Atari 1050 (SD/ED)", .cylinders = 40, .heads = 1,
      .rpm = 288.0, .step_time_ms = 20, .settle_time_ms = 20, .data_rate_dd = 250.0,
      .default_encoding = DRIVE_ENC_FM },
    { .type = UFT_DRIVE_ATARI_XF551, .name = "Atari XF551 (DS/DD)", .cylinders = 40, .heads = 2,
      .rpm = 288.0, .step_time_ms = 20, .settle_time_ms = 20, .data_rate_dd = 500.0,
      .default_encoding = DRIVE_ENC_MFM }
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
