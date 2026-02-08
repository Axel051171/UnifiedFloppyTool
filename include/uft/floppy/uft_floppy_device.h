/**
 * @file uft_floppy_device.h
 * @brief Legacy FloppyDevice API for sector-level disk image access
 * 
 * This provides the FloppyDevice structure used by format modules in
 * src/formats/commodore/, src/formats/amstrad/, src/formats/apple/, etc.
 * 
 * Each format module implements uft_floppy_open/close/read_sector/write_sector
 * with static linkage or through the format registry.
 */

#ifndef UFT_FLOPPY_DEVICE_H
#define UFT_FLOPPY_DEVICE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Error Code Aliases (short form used by format modules)
 * ============================================================================ */

#include "uft/core/uft_error_codes.h"
#include "uft/core/uft_error_compat.h"

#ifndef UFT_EBOUNDS
#define UFT_EBOUNDS     UFT_E_BOUNDS
#endif

#ifndef UFT_ENOENT
#define UFT_ENOENT      UFT_E_FILE_NOT_FOUND
#endif

#ifndef UFT_ENOTSUP
#define UFT_ENOTSUP     UFT_E_NOT_SUPPORTED
#endif

#ifndef UFT_ECORRUPT
#define UFT_ECORRUPT    UFT_E_CORRUPT
#endif

#ifndef UFT_EINVAL
#define UFT_EINVAL      UFT_E_INVALID_ARG
#endif

#ifndef UFT_EIO
#define UFT_EIO         UFT_E_IO
#endif

#ifndef UFT_ENOMEM
#define UFT_ENOMEM      UFT_E_MEMORY
#endif

#ifndef UFT_EUNSUPPORTED
#define UFT_EUNSUPPORTED UFT_E_NOT_SUPPORTED
#endif

/* ============================================================================
 * FloppyDevice Structure
 * ============================================================================ */

/**
 * @brief Legacy sector-level disk image device
 * 
 * Format modules fill in geometry fields on open and provide
 * read/write through the standard uft_floppy_* functions.
 */
typedef struct FloppyDevice {
    /* Geometry */
    uint32_t    tracks;             /**< Number of tracks (cylinders) */
    uint32_t    heads;              /**< Number of heads/sides */
    uint32_t    sectors;            /**< Sectors per track (0 = variable) */
    uint32_t    sectorSize;         /**< Bytes per sector (typically 256 or 512) */
    
    /* Capabilities */
    bool        flux_supported;     /**< Device supports flux-level access */
    bool        read_only;          /**< Image opened read-only */
    
    /* Internal state */
    void       *internal_ctx;       /**< Format-specific context (opaque) */
    
    /* Callbacks */
    void      (*log_callback)(const char *message);  /**< Optional log callback */
} FloppyDevice;

/* ============================================================================
 * Standard Format Module API
 * 
 * Each format module (d64.c, d71.c, etc.) implements these functions.
 * They are not globally unique — each module provides its own implementation
 * that gets registered through the format registry or linked directly.
 * ============================================================================ */

/**
 * @brief Open a disk image file
 * @param dev Device structure to fill in
 * @param path Path to disk image file
 * @return 0 on success, error code on failure
 */
int uft_floppy_open(FloppyDevice *dev, const char *path);

/**
 * @brief Close disk image and free resources
 * @param dev Device structure
 * @return 0 on success
 */
int uft_floppy_close(FloppyDevice *dev);

/**
 * @brief Read a sector from the disk image
 * @param dev Device structure
 * @param track Track number (1-based for CBM, 0-based for others)
 * @param head Head/side number
 * @param sector Sector number
 * @param buf Output buffer (must be at least sectorSize bytes)
 * @return 0 on success
 */
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t track, uint32_t head,
                            uint32_t sector, uint8_t *buf);

/**
 * @brief Write a sector to the disk image
 * @param dev Device structure
 * @param track Track number
 * @param head Head/side number
 * @param sector Sector number
 * @param buf Input buffer
 * @return 0 on success
 */
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t track, uint32_t head,
                              uint32_t sector, const uint8_t *buf);

/**
 * @brief Analyze copy protection schemes
 * @param dev Device structure
 * @return 0 on success, or error code
 */
int uft_floppy_analyze_protection(FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_DEVICE_H */

/* Struct packing macros */
#ifndef UFT_PACK_BEGIN
#if defined(_MSC_VER)
#define UFT_PACK_BEGIN __pragma(pack(push, 1))
#define UFT_PACK_END   __pragma(pack(pop))
#elif defined(__GNUC__) || defined(__clang__)
#define UFT_PACK_BEGIN _Pragma("pack(push, 1)")
#define UFT_PACK_END   _Pragma("pack(pop)")
#else
#define UFT_PACK_BEGIN
#define UFT_PACK_END
#endif
#endif
