/**
 * @file uft_limits.h
 * @brief Resource limits and memory allocation guards
 *
 * This module provides configurable resource limits to prevent
 * denial-of-service attacks and runaway allocations when processing
 * potentially malformed disk images.
 *
 * Copyright (C) 2025 UFT Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UFT_CORE_LIMITS_H
#define UFT_CORE_LIMITS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/uft_platform.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Default Limits
 *============================================================================*/

/** Maximum file size (512 MB) */
#define UFT_DEFAULT_MAX_FILE_SIZE       (512UL * 1024 * 1024)

/** Maximum single allocation (64 MB) */
#define UFT_DEFAULT_MAX_SINGLE_ALLOC    (64UL * 1024 * 1024)

/** Maximum total allocation (256 MB) */
#define UFT_DEFAULT_MAX_TOTAL_ALLOC     (256UL * 1024 * 1024)

/** Maximum tracks (200 - accommodates 100 cylinders double-sided) */
#define UFT_DEFAULT_MAX_TRACKS          200

/** Maximum sectors per track */
#define UFT_DEFAULT_MAX_SECTORS         256

/** Maximum sector size (128 KB) */
#define UFT_DEFAULT_MAX_SECTOR_SIZE     (128UL * 1024)

/** Maximum revolutions to process */
#define UFT_DEFAULT_MAX_REVOLUTIONS     20

/** Maximum flux transitions per revolution */
#define UFT_DEFAULT_MAX_FLUX_PER_REV    500000

/** Maximum string length */
#define UFT_DEFAULT_MAX_STRING_LENGTH   4096

/** Maximum path length */
#define UFT_DEFAULT_MAX_PATH_LENGTH     1024

/** Default I/O timeout (30 seconds) */
#define UFT_DEFAULT_IO_TIMEOUT_MS       30000

/*============================================================================
 * Format-Specific Limits
 *============================================================================*/

/** SCP format limits */
typedef struct {
    size_t max_file_size;   /**< Maximum SCP file size */
    size_t max_track_size;  /**< Maximum track data size */
} uft_limits_scp_t;

/** D64/D71/D81 format limits */
typedef struct {
    size_t max_file_size;   /**< Maximum file size */
} uft_limits_d64_t;

/** G64 format limits */
typedef struct {
    size_t max_file_size;   /**< Maximum file size */
    size_t max_track_size;  /**< Maximum track data size */
} uft_limits_g64_t;

/** HFE format limits */
typedef struct {
    size_t max_file_size;   /**< Maximum file size */
    size_t max_track_size;  /**< Maximum track data size */
} uft_limits_hfe_t;

/** ADF format limits */
typedef struct {
    size_t max_file_size;   /**< Maximum file size */
} uft_limits_adf_t;

/*============================================================================
 * Main Limits Structure
 *============================================================================*/

/**
 * @brief Resource limits configuration
 */
typedef struct {
    /* General limits */
    size_t max_file_size;       /**< Maximum file size to accept */
    size_t max_single_alloc;    /**< Maximum single allocation */
    size_t max_total_alloc;     /**< Maximum total memory allocation */
    
    /* Track/sector limits */
    int max_tracks;             /**< Maximum number of tracks */
    int max_sides;              /**< Maximum number of sides */
    int max_sectors;            /**< Maximum sectors per track */
    size_t max_sector_size;     /**< Maximum sector size */
    
    /* Flux/revolution limits */
    int max_revolutions;        /**< Maximum revolutions to process */
    size_t max_flux_per_rev;    /**< Maximum flux transitions per revolution */
    
    /* String/path limits */
    size_t max_string_length;   /**< Maximum string length */
    size_t max_path_length;     /**< Maximum path length */
    
    /* Timeout limits */
    uint32_t io_timeout_ms;     /**< I/O operation timeout */
    uint32_t usb_timeout_ms;    /**< USB operation timeout */
    
    /* Format-specific limits */
    uft_limits_scp_t scp;       /**< SCP format limits */
    uft_limits_d64_t d64;       /**< D64 format limits */
    uft_limits_g64_t g64;       /**< G64 format limits */
    uft_limits_hfe_t hfe;       /**< HFE format limits */
    uft_limits_adf_t adf;       /**< ADF format limits */
    
} uft_limits_t;

/*============================================================================
 * Functions
 *============================================================================*/

/**
 * @brief Get default limits
 * @param limits Output limits structure
 */
void uft_limits_get_defaults(uft_limits_t* limits);

/**
 * @brief Get embedded/low-memory limits
 * @param limits Output limits structure
 */
void uft_limits_get_embedded(uft_limits_t* limits);

/**
 * @brief Get server/high-capacity limits
 * @param limits Output limits structure
 */
void uft_limits_get_server(uft_limits_t* limits);

/**
 * @brief Get paranoid/restrictive limits
 * @param limits Output limits structure
 */
void uft_limits_get_paranoid(uft_limits_t* limits);

/**
 * @brief Set current limits
 * @param limits New limits (NULL to keep current)
 * @return Previous limits
 */
const uft_limits_t* uft_limits_set(const uft_limits_t* limits);

/**
 * @brief Get current limits
 * @return Current limits (never NULL)
 */
const uft_limits_t* uft_limits_get(void);

/**
 * @brief Reset to default limits
 */
void uft_limits_reset(void);

/*============================================================================
 * Checking Functions
 *============================================================================*/

/**
 * @brief Check if file size is within limits
 * @param size File size to check
 * @return true if within limits
 */
bool uft_check_file_size(size_t size);

/**
 * @brief Check if allocation size is within limits
 * @param size Allocation size to check
 * @return true if within limits
 */
bool uft_check_alloc_size(size_t size);

/**
 * @brief Track allocation (positive) or deallocation (negative)
 * @param size Size delta
 */
void uft_track_allocation(ssize_t size);

/**
 * @brief Get total tracked allocation
 * @return Total bytes allocated
 */
size_t uft_get_total_allocation(void);

/*============================================================================
 * Limit-Aware Memory Functions
 *============================================================================*/

/**
 * @brief Allocate memory with limit checking
 * @param size Size to allocate
 * @return Pointer or NULL if limit exceeded
 */
void* uft_malloc_limited(size_t size);

/**
 * @brief Reallocate memory with limit checking
 * @param ptr Current pointer
 * @param old_size Current size
 * @param new_size New size
 * @return New pointer or NULL if limit exceeded
 */
void* uft_realloc_limited(void* ptr, size_t old_size, size_t new_size);

/**
 * @brief Free memory and track deallocation
 * @param ptr Pointer to free
 * @param size Size being freed
 */
void uft_free_limited(void* ptr, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CORE_LIMITS_H */
