/**
 * @file uft.h
 * @brief UnifiedFloppyTool Library - Master Header
 * 
 * Include this header to get access to all UFT library functionality.
 * 
 * Modules:
 * - Core I/O: Physical disk and image file access
 * - Geometry: CHS/LBA conversion, format detection
 * - FAT12: DOS filesystem support
 * - GCR: Commodore/Apple encoding
 * - MFM: IBM PC/Amiga encoding
 * - Flux: Raw flux data processing
 * - Disk Images: D64, ADF, IMG, etc.
 * - Protection: Copy protection detection
 * - Recovery: Data recovery tools
 * - XCopy: Disk duplication
 * 
 * @version 2.0
 * @copyright GPL-3.0-or-later
 */

#ifndef UFT_H
#define UFT_H

/* Core modules */
#include "uft_floppy_types.h"
#include "uft_floppy_io.h"
#include "uft_floppy_geometry.h"
#include "uft_fat12.h"

/* Encoding modules */
#include "encoding/uft_gcr.h"
#include "encoding/uft_mfm.h"

/* Format modules */
#include "formats/uft_diskimage.h"
#include "formats/uft_flux.h"
#include "formats/uft_protection.h"
#include "formats/uft_recovery.h"
#include "formats/uft_xcopy.h"

/**
 * @brief Library version
 */
#define UFT_VERSION_MAJOR   2
#define UFT_VERSION_MINOR   0
#define UFT_VERSION_PATCH   0
#define UFT_VERSION_STRING  "2.0.0"

/**
 * @brief Get library version string
 * @return Version string (e.g., "2.0.0")
 */
static inline const char* uft_version(void) {
    return UFT_VERSION_STRING;
}

/**
 * @brief Initialize the library
 * 
 * Call this before using any other UFT functions.
 * 
 * @return 0 on success
 */
int uft_init(void);

/**
 * @brief Cleanup the library
 * 
 * Call this before program exit.
 */
void uft_cleanup(void);

#endif /* UFT_H */
