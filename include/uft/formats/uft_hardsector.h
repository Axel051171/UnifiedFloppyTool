/**
 * @file uft_hardsector.h
 * @brief Hard-sector floppy disk format support
 * @version 3.9.0
 * 
 * Supports hard-sectored floppy disk formats:
 * - 8" SSSD (IBM 3740): 77 tracks, 1 side, 26 sectors, 128 bytes = 256KB
 * - 8" DSSD: 77 tracks, 2 sides, 26 sectors, 128 bytes = 512KB
 * - 8" DSDD (IBM System/34): 77 tracks, 2 sides, 26 sectors, 256 bytes = 1MB
 * - 5.25" hard-sector (early): 35/40 tracks, various configurations
 * 
 * Hard-sector disks have physical index holes for each sector.
 * The number of holes determines sectors per track.
 * 
 * Reference: Catweasel hard-sector support, various 8" format specs
 */

#ifndef UFT_HARDSECTOR_H
#define UFT_HARDSECTOR_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 8" disk standard geometries */
#define HS_8IN_SSSD_CYLS        77
#define HS_8IN_SSSD_HEADS       1
#define HS_8IN_SSSD_SECTORS     26
#define HS_8IN_SSSD_SECSIZE     128
#define HS_8IN_SSSD_SIZE        (77 * 1 * 26 * 128)  /* 256,256 bytes */

#define HS_8IN_DSSD_CYLS        77
#define HS_8IN_DSSD_HEADS       2
#define HS_8IN_DSSD_SECTORS     26
#define HS_8IN_DSSD_SECSIZE     128
#define HS_8IN_DSSD_SIZE        (77 * 2 * 26 * 128)  /* 512,512 bytes */

#define HS_8IN_DSDD_CYLS        77
#define HS_8IN_DSDD_HEADS       2
#define HS_8IN_DSDD_SECTORS     26
#define HS_8IN_DSDD_SECSIZE     256
#define HS_8IN_DSDD_SIZE        (77 * 2 * 26 * 256)  /* 1,025,024 bytes */

/* 5.25" hard-sector geometries */
#define HS_525_10SEC_CYLS       35
#define HS_525_10SEC_HEADS      1
#define HS_525_10SEC_SECTORS    10
#define HS_525_10SEC_SECSIZE    256

#define HS_525_16SEC_CYLS       40
#define HS_525_16SEC_HEADS      1
#define HS_525_16SEC_SECTORS    16
#define HS_525_16SEC_SECSIZE    256

/* Hard-sector disk types */
typedef enum {
    HS_TYPE_8IN_SSSD = 0,        /* IBM 3740 compatible */
    HS_TYPE_8IN_DSSD,            /* 8" double-sided single-density */
    HS_TYPE_8IN_DSDD,            /* IBM System/34 compatible */
    HS_TYPE_525_10SEC,           /* 5.25" 10-sector hard-sector */
    HS_TYPE_525_16SEC,           /* 5.25" 16-sector hard-sector */
    HS_TYPE_CUSTOM,              /* User-defined geometry */
} hardsector_type_t;

/* Data encoding for hard-sector disks */
typedef enum {
    HS_ENC_FM = 0,               /* FM (Single Density) */
    HS_ENC_MFM,                  /* MFM (Double Density) */
    HS_ENC_GCR,                  /* GCR (rare for hard-sector) */
} hardsector_encoding_t;

/**
 * @brief Hard-sector disk geometry
 */
typedef struct {
    hardsector_type_t type;
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors;             /* Determined by physical holes */
    uint16_t sector_size;
    hardsector_encoding_t encoding;
    uint8_t first_sector;        /* First sector number (0 or 1) */
    bool double_step;            /* For 40->80 track drives */
} hardsector_geometry_t;

/**
 * @brief Hard-sector read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Detected geometry */
    hardsector_geometry_t geometry;
    
    /* Statistics */
    size_t image_size;
    uint32_t total_sectors;
    uint32_t bad_sectors;
    
} hardsector_read_result_t;

/**
 * @brief Hard-sector write options
 */
typedef struct {
    hardsector_geometry_t geometry;
    uint8_t fill_byte;           /* For unformatted sectors */
    bool create_index_marks;     /* Include index marks in raw output */
} hardsector_write_options_t;

/* ============================================================================
 * Geometry Functions
 * ============================================================================ */

/**
 * @brief Get standard geometry for hard-sector type
 */
void hardsector_get_geometry(hardsector_type_t type, 
                             hardsector_geometry_t *geometry);

/**
 * @brief Detect hard-sector type from image size
 */
hardsector_type_t hardsector_detect_type(size_t image_size);

/**
 * @brief Calculate total image size
 */
size_t hardsector_calc_size(const hardsector_geometry_t *geometry);

/* ============================================================================
 * Hard-Sector File I/O
 * ============================================================================ */

/**
 * @brief Read hard-sector disk image
 */
uft_error_t uft_hardsector_read(const char *path,
                                uft_disk_image_t **out_disk,
                                hardsector_read_result_t *result);

/**
 * @brief Read hard-sector from memory
 */
uft_error_t uft_hardsector_read_mem(const uint8_t *data, size_t size,
                                    const hardsector_geometry_t *geometry,
                                    uft_disk_image_t **out_disk,
                                    hardsector_read_result_t *result);

/**
 * @brief Write hard-sector disk image
 */
uft_error_t uft_hardsector_write(const uft_disk_image_t *disk,
                                 const char *path,
                                 const hardsector_write_options_t *opts);

/**
 * @brief Initialize write options with defaults
 */
void uft_hardsector_write_options_init(hardsector_write_options_t *opts,
                                       hardsector_type_t type);

/**
 * @brief Probe if data is hard-sector format (by size)
 */
bool uft_hardsector_probe(const uint8_t *data, size_t size, int *confidence);

/* ============================================================================
 * IBM 3740 Specific Functions
 * ============================================================================ */

/**
 * @brief Read IBM 3740 format (8" SSSD)
 */
uft_error_t uft_ibm3740_read(const char *path,
                             uft_disk_image_t **out_disk,
                             hardsector_read_result_t *result);

/**
 * @brief Write IBM 3740 format
 */
uft_error_t uft_ibm3740_write(const uft_disk_image_t *disk,
                              const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HARDSECTOR_H */
