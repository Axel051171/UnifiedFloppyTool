/**
 * @file uft_core.h
 * @brief UFT Core Types Master Header
 * 
 * Consolidated include for all unified core types.
 * Include this single header to get access to:
 * - uft_disk_unified_t
 * - uft_track_base_t
 * - uft_sector_unified_t  
 * - uft_disk_encoding_t
 * - uft_crc_* functions
 * - uft_error_t / UFT_E_* error codes
 * - uft_format_id_t / format registry
 * 
 * @version 1.1.0
 * @date 2026-01-05
 */

#ifndef UFT_CORE_H
#define UFT_CORE_H

/* Core unified types */
#include "uft_error_codes.h"    /* P2-ARCH-006: Unified error codes */
#include "uft_encoding.h"       /* P2-ARCH-003: Unified encoding */
#include "uft_crc_unified.h"    /* P2-ARCH-002: Unified CRC */
#include "uft_sector.h"         /* P2-ARCH-004: Unified sector */
#include "uft_track_base.h"     /* P2-ARCH-001: Unified track */
/* Note: uft_track_compat.h is optional - include separately if needed */
#include "uft_disk.h"           /* P2-ARCH-005: Unified disk */
#include "uft_format_registry.h"/* P2-ARCH-007: Format registry */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UFT Core version
 */
#define UFT_CORE_VERSION_MAJOR  1
#define UFT_CORE_VERSION_MINOR  1
#define UFT_CORE_VERSION_PATCH  0
#define UFT_CORE_VERSION_STR    "1.1.0"

/**
 * @brief Get core library version string
 */
static inline const char* uft_core_version(void) {
    return UFT_CORE_VERSION_STR;
}

/**
 * @brief Initialize core library (currently a no-op)
 */
static inline int uft_core_init(void) {
    return 0;
}

/**
 * @brief Cleanup core library (currently a no-op)
 */
static inline void uft_core_cleanup(void) {
    /* Nothing to do */
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_CORE_H */
