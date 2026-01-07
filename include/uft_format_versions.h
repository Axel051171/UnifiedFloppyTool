/**
 * @file uft_format_versions.h
 * @brief Format Versioning API Header
 * @version 5.31.0
 */

#ifndef UFT_FORMAT_VERSIONS_H
#define UFT_FORMAT_VERSIONS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Version Enums for All Formats
 * ============================================================================ */

/* D64 (Commodore 1541) */
typedef enum {
    D64_35_TRACK = 0, D64_35_TRACK_ERRORS, D64_40_TRACK, 
    D64_40_TRACK_ERRORS, D64_42_TRACK, D64_VERSION_COUNT
} d64_version_t;

/* ADF (Amiga) */
typedef enum {
    ADF_OFS_DD = 0, ADF_FFS_DD, ADF_OFS_HD, ADF_FFS_HD,
    ADF_INTL_DD, ADF_INTL_HD, ADF_DIRC_DD, ADF_DIRC_HD, ADF_VERSION_COUNT
} adf_version_t;

/* WOZ (Apple II) */
typedef enum { WOZ_V1 = 0, WOZ_V2, WOZ_VERSION_COUNT } woz_version_t;

/* ATR (Atari 8-Bit) */
typedef enum { 
    ATR_SD_90K = 0, ATR_ED_130K, ATR_DD_180K, ATR_QD_360K, ATR_VERSION_COUNT 
} atr_version_t;

/* HFE (UFT HFE Format) */
typedef enum { HFE_V1 = 0, HFE_V3, HFE_VERSION_COUNT } hfe_version_t;

/* NFD (PC-98) */
typedef enum { NFD_R0 = 0, NFD_R1, NFD_VERSION_COUNT } nfd_version_t;

/* D88 (PC-98/PC-88/X1) */
typedef enum { D88_2D = 0, D88_2DD, D88_2HD, D88_VERSION_COUNT } d88_version_t;

/* TD0 (Teledisk) */
typedef enum { TD0_NORMAL = 0, TD0_ADVANCED, TD0_VERSION_COUNT } td0_version_t;

/* DMS (Amiga Disk Masher) */
typedef enum {
    DMS_NONE = 0, DMS_SIMPLE, DMS_QUICK, DMS_MEDIUM, 
    DMS_DEEP, DMS_HEAVY1, DMS_HEAVY2, DMS_VERSION_COUNT
} dms_version_t;

/* IMG/IMA (PC Raw) */
typedef enum {
    IMG_160K = 0, IMG_180K, IMG_320K, IMG_360K,
    IMG_720K, IMG_1200K, IMG_1440K, IMG_2880K, IMG_VERSION_COUNT
} img_version_t;

/* SSD/DSD (BBC Micro) */
typedef enum { 
    SSD_40T = 0, SSD_80T, DSD_40T, DSD_80T, SSD_VERSION_COUNT 
} ssd_version_t;

/* TRD (ZX Spectrum) */
typedef enum { 
    TRD_SS_40T = 0, TRD_DS_80T, TRD_DS_40T, TRD_VERSION_COUNT 
} trd_version_t;

/* DSK/EDSK (CPC) */
typedef enum { DSK_STANDARD = 0, DSK_EXTENDED, DSK_VERSION_COUNT } dsk_version_t;

/* G64/G71 (Commodore GCR) */
typedef enum { 
    G64_1541 = 0, G64_1541_40T, G71_1571, G64_VERSION_COUNT 
} g64_version_t;

/* ============================================================================
 * Version Info Structure
 * ============================================================================ */

typedef struct {
    int          version_id;
    const char  *name;
    const char  *description;
    uint64_t     typical_size;
    int          tracks;
    int          sides;
    int          sectors;
    int          sector_size;
} version_info_t;

/* ============================================================================
 * Disk Image Structure
 * ============================================================================ */

typedef struct {
    uint8_t *data;
    size_t   size;
    int      tracks;
    int      sides;
    int      sectors_per_track;
    int      sector_size;
    int      version;
    char     format[16];
    bool     write_protected;
    bool     has_errors;
    uint8_t *error_info;
} uft_disk_image_t;

/* ============================================================================
 * API Functions
 * ============================================================================ */

/**
 * @brief Get available versions for a format
 * 
 * @param format    Format name (e.g., "d64", "adf", "atr")
 * @param versions  Output: pointer to version array
 * @param count     Output: number of versions
 * @return          0 on success, -1 if format not found
 */
int uft_get_versions(const char *format, const version_info_t **versions, int *count);

/**
 * @brief Print available versions for a format
 */
void uft_print_versions(const char *format);

/**
 * @brief Print all supported formats and their versions
 */
void uft_print_all_versions(void);

/**
 * @brief Load disk image with automatic version detection
 * 
 * @param path              Path to disk image
 * @param image             Output: loaded image
 * @param detected_version  Output: detected version ID
 * @return                  0 on success
 */
int uft_load_image(const char *path, uft_disk_image_t *image, int *detected_version);

/**
 * @brief Save disk image with specific version
 * 
 * @param path      Output path
 * @param image     Image to save
 * @param format    Format name (e.g., "d64")
 * @param version   Version ID (e.g., D64_35_TRACK)
 * @return          0 on success
 */
int uft_save_image(const char *path, const uft_disk_image_t *image, 
                   const char *format, int version);

/* Format-specific save functions */
int uft_save_d64(const char *path, const uft_disk_image_t *image, d64_version_t version);
int uft_save_adf(const char *path, const uft_disk_image_t *image, adf_version_t version);
int uft_save_atr(const char *path, const uft_disk_image_t *image, atr_version_t version);

/**
 * @brief Free disk image
 */
void uft_free_image(uft_disk_image_t *image);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_VERSIONS_H */
