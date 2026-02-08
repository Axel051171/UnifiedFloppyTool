/**
 * @file uft_posix.h
 * @brief POSIX disk format support
 * @version 3.9.0
 * 
 * POSIX format stores raw disk data with geometry in a separate .geom file.
 * This is useful for Unix/Linux environments where disk images need
 * explicit geometry information.
 * 
 * Format: imagename.dsk + imagename.dsk.geom
 * The .geom file contains: cylinders heads sectors secsize
 * 
 * Reference: libdsk drvposix.c
 */

#ifndef UFT_POSIX_H
#define UFT_POSIX_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/core/uft_error_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

/* POSIX geometry file extension */
#define POSIX_GEOM_EXTENSION    ".geom"
#define POSIX_GEOM_MAX_LINE     128

/**
 * @brief POSIX geometry data
 */
typedef struct {
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    uint8_t  first_sector;      /* Usually 0 or 1 */
    uft_encoding_t encoding;    /* FM or MFM */
} posix_geometry_t;

/**
 * @brief POSIX read options
 */
typedef struct {
    bool     require_geom;      /* Require .geom file (fail if missing) */
    posix_geometry_t fallback;  /* Fallback geometry if no .geom */
} posix_read_options_t;

/**
 * @brief POSIX read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    bool geom_found;            /* Was .geom file found? */
    posix_geometry_t geometry;  /* Detected/used geometry */
    size_t image_size;
    
} posix_read_result_t;

/* ============================================================================
 * POSIX Disk Functions
 * ============================================================================ */

/**
 * @brief Initialize read options with defaults
 */
void uft_posix_read_options_init(posix_read_options_t *opts);

/**
 * @brief Read POSIX disk image
 */
uft_error_t uft_posix_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const posix_read_options_t *opts,
                           posix_read_result_t *result);

/**
 * @brief Write POSIX disk image (creates .geom file)
 */
uft_error_t uft_posix_write(const uft_disk_image_t *disk,
                            const char *path);

/**
 * @brief Read geometry file
 */
uft_error_t uft_posix_read_geometry(const char *geom_path,
                                    posix_geometry_t *geometry);

/**
 * @brief Write geometry file
 */
uft_error_t uft_posix_write_geometry(const char *geom_path,
                                     const posix_geometry_t *geometry);

/**
 * @brief Probe for POSIX format (checks for .geom file)
 */
bool uft_posix_probe(const char *path, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_POSIX_H */
