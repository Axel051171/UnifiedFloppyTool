/**
 * @file uft_geos_protection.h
 * @brief GEOS Copy Protection Detection Interface
 * @version 4.1.1
 */

#ifndef UFT_GEOS_PROTECTION_H
#define UFT_GEOS_PROTECTION_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of protections that can be detected */
#define GEOS_MAX_PROTECTIONS  16

/* Protection types */
typedef enum {
    GEOS_PROT_NONE = 0,
    GEOS_PROT_V1_KEY_DISK,      /* Original GEOS key disk */
    GEOS_PROT_V2_ENHANCED,      /* GEOS 2.0+ enhanced protection */
    GEOS_PROT_SERIAL_CHECK,     /* Serial number verification */
    GEOS_PROT_HALF_TRACK,       /* Half-track data */
    GEOS_PROT_BAM_SIGNATURE,    /* Modified BAM entries */
    GEOS_PROT_INTERLEAVE,       /* Non-standard interleave */
    GEOS_PROT_SYNC_MARK,        /* Custom sync marks */
} geos_protection_type_t;

/* Protection severity */
typedef enum {
    GEOS_SEV_NONE = 0,
    GEOS_SEV_TRIVIAL,           /* Easily bypassed */
    GEOS_SEV_STANDARD,          /* Requires nibbler */
    GEOS_SEV_DIFFICULT,         /* May require flux capture */
} geos_severity_t;

/* Protection info */
typedef struct {
    geos_protection_type_t type;
    const char *name;
    const char *description;
    geos_severity_t severity;
    bool copyable_with_nibbler;
    bool requires_original;
} geos_protection_info_t;

/* Analysis result */
typedef struct {
    bool is_geos_disk;
    int geos_version;
    int protection_count;
    geos_protection_type_t protections[GEOS_MAX_PROTECTIONS];
} geos_analysis_result_t;

/* GEOS file info */
typedef struct {
    bool is_geos_file;
    uint8_t dos_file_type;
    uint8_t geos_file_type;
    uint8_t structure_type;
    uint16_t load_address;
    uint16_t end_address;
    uint16_t start_address;
    char class_name[21];
    char author[25];
    char parent_app[21];
    char description[33];
    uint8_t icon_data[63];
} geos_file_info_t;

/* Detection functions */
bool uft_geos_detect_boot_signature(const uint8_t *sector_data, size_t size);
bool uft_geos_detect_extended_signature(const uint8_t *sector_data, size_t size);
int uft_geos_detect_file_type(const uint8_t *info_sector);

/* Main analysis function */
int uft_geos_analyze_disk(const uft_disk_image_t *disk, 
                          geos_analysis_result_t *result);

/* Information functions */
const geos_protection_info_t* uft_geos_get_protection_info(
    geos_protection_type_t type);
int uft_geos_get_report(const geos_analysis_result_t *result,
                        char *buffer, size_t buffer_size);

/* File analysis */
int uft_geos_analyze_file(const uint8_t *info_sector, size_t size,
                          geos_file_info_t *info);
const char* uft_geos_file_type_name(int type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GEOS_PROTECTION_H */
