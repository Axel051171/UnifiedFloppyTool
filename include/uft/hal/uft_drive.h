/**
 * @file uft_drive.h
 * @brief Drive Profile Definitions
 * @version 1.0.0
 * 
 * Defines physical drive characteristics for various floppy drive types.
 */

#ifndef UFT_DRIVE_H
#define UFT_DRIVE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_DRIVE_UNKNOWN = 0,
    UFT_DRIVE_525_DD,        /**< 5.25" Double Density (360K) */
    UFT_DRIVE_525_HD,        /**< 5.25" High Density (1.2M) */
    UFT_DRIVE_35_DD,         /**< 3.5" Double Density (720K) */
    UFT_DRIVE_35_HD,         /**< 3.5" High Density (1.44M) */
    UFT_DRIVE_35_ED,         /**< 3.5" Extended Density (2.88M) */
    UFT_DRIVE_8_SD,          /**< 8" Single Density */
    UFT_DRIVE_8_DD,          /**< 8" Double Density */
    UFT_DRIVE_1541,          /**< Commodore 1541 */
    UFT_DRIVE_APPLE,         /**< Apple II Disk II */
    UFT_DRIVE_AMIGA_DD,      /**< Amiga DD */
    UFT_DRIVE_AMIGA_HD,      /**< Amiga HD */
    UFT_DRIVE_COUNT
} uft_drive_type_t;

/* Backward compatibility aliases */
#define DRIVE_TYPE_525_DD  UFT_DRIVE_525_DD
#define DRIVE_TYPE_525_HD  UFT_DRIVE_525_HD
#define DRIVE_TYPE_35_DD   UFT_DRIVE_35_DD
#define DRIVE_TYPE_35_HD   UFT_DRIVE_35_HD
#define DRIVE_TYPE_35_ED   UFT_DRIVE_35_ED
#define DRIVE_TYPE_1541    UFT_DRIVE_1541
#define DRIVE_TYPE_APPLE   UFT_DRIVE_APPLE

/* Extended drive type aliases */
#define UFT_DRIVE_CBM_1541    UFT_DRIVE_1541
#define UFT_DRIVE_CBM_1571    UFT_DRIVE_1541  /* Use 1541 profile as base */
#define UFT_DRIVE_APPLE_525   UFT_DRIVE_APPLE

/* Extended encoding aliases */
#define DRIVE_ENC_GCR_CBM     DRIVE_ENC_GCR
#define DRIVE_ENC_GCR_APPLE   DRIVE_ENC_GCR

/* ═══════════════════════════════════════════════════════════════════════════════
 * Encoding Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    DRIVE_ENC_FM = 0,        /**< FM encoding */
    DRIVE_ENC_MFM,           /**< MFM encoding */
    DRIVE_ENC_GCR,           /**< GCR encoding (C64, Apple) */
    DRIVE_ENC_M2FM           /**< M2FM encoding (rare) */
} uft_drive_encoding_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Drive Profile Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_drive_type_t type;        /**< Drive type identifier */
    const char* name;             /**< Human-readable name */
    
    /* Physical characteristics */
    int cylinders;                /**< Number of cylinders */
    int heads;                    /**< Number of heads (sides) */
    bool double_step;             /**< Uses double stepping */
    bool half_tracks;             /**< Supports half-track stepping */
    int speed_zones;              /**< Number of speed zones (CBM) */
    
    /* Timing */
    double rpm;                   /**< Rotation speed (RPM) */
    double rpm_tolerance;         /**< RPM tolerance (%) */
    double step_time_ms;          /**< Step time (milliseconds) */
    double settle_time_ms;        /**< Head settle time (milliseconds) */
    double motor_spinup_ms;       /**< Motor spinup time (milliseconds) */
    
    /* Data rates */
    double data_rate_dd;          /**< DD data rate (kbit/s) */
    double data_rate_hd;          /**< HD data rate (kbit/s) */
    double data_rate_ed;          /**< ED data rate (kbit/s) */
    double bit_cell_dd;           /**< DD bit cell (µs) */
    double bit_cell_hd;           /**< HD bit cell (µs) */
    
    /* Track format */
    int track_length_bits;        /**< Nominal track length (bits) */
    int write_precomp_ns;         /**< Write precompensation (ns) */
    uft_drive_encoding_t default_encoding;  /**< Default encoding */
} uft_drive_profile_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Predefined Profiles
 * ═══════════════════════════════════════════════════════════════════════════════ */

extern const uft_drive_profile_t UFT_DRIVE_PROFILE_525_DD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_525_HD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_35_DD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_35_HD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_35_ED;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_8_SD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_8_DD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_1541;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_APPLE;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_AMIGA_DD;
extern const uft_drive_profile_t UFT_DRIVE_PROFILE_AMIGA_HD;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get drive profile by type
 * @param type Drive type
 * @return Profile pointer or NULL if invalid
 */
const uft_drive_profile_t* uft_drive_get_profile(uft_drive_type_t type);

/**
 * @brief List all drive profiles
 * @param profiles Array to fill
 * @param max_count Maximum entries
 * @return Number of profiles returned
 */
int uft_drive_list_profiles(uft_drive_profile_t* profiles, int max_count);

/**
 * @brief Get drive type name
 * @param type Drive type
 * @return Human-readable name
 */
const char* uft_drive_type_name(uft_drive_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DRIVE_H */
