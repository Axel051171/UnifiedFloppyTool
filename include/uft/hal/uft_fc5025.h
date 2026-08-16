/**
 * @file uft_fc5025.h
 * @brief FC5025 USB Floppy Controller Interface
 * 
 * Device Software Archive (DSA) FC5025 support for reading 5.25" and 8" disks.
 * Supports FM, MFM, and various legacy formats.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PLANNED
 * PLANNED FEATURE — Hardware abstraction
 *
 * This header declares 20 public functions, of which 20 have no
 * implementation in the source tree. Callers exist but will link-fail or
 * silently no-op until the feature is implemented.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 DOCUMENT-Welle).
 * Do NOT add new call sites to functions from this header without first
 * implementing them or removing the prototype.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_FC5025_H
#define UFT_FC5025_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** FC5025 USB identifiers */
#define UFT_FC5025_VID  0x16C0
#define UFT_FC5025_PID  0x06D6

/** Supported disk formats */
typedef enum {
    UFT_FC_FMT_AUTO = 0,
    UFT_FC_FMT_APPLE_DOS32,     /**< Apple DOS 3.2 (13 sectors) */
    UFT_FC_FMT_APPLE_DOS33,     /**< Apple DOS 3.3 (16 sectors) */
    UFT_FC_FMT_APPLE_PRODOS,    /**< Apple ProDOS */
    UFT_FC_FMT_IBM_FM,          /**< IBM 3740 FM (8" SSSD) */
    UFT_FC_FMT_IBM_MFM,         /**< IBM MFM (various) */
    UFT_FC_FMT_TRS80_SSSD,      /**< TRS-80 SSSD */
    UFT_FC_FMT_TRS80_SSDD,      /**< TRS-80 SSDD */
    UFT_FC_FMT_TRS80_DSDD,      /**< TRS-80 DSDD */
    UFT_FC_FMT_KAYPRO,          /**< Kaypro */
    UFT_FC_FMT_OSBORNE,         /**< Osborne */
    UFT_FC_FMT_NORTHSTAR,       /**< North Star */
    UFT_FC_FMT_TI994A,          /**< TI-99/4A */
    UFT_FC_FMT_ATARI_FM,        /**< Atari 810 FM */
    UFT_FC_FMT_ATARI_MFM,       /**< Atari 1050 MFM */
    UFT_FC_FMT_RAW_FM,          /**< Raw FM capture */
    UFT_FC_FMT_RAW_MFM,         /**< Raw MFM capture */
} uft_fc_format_t;

/** Drive types */
typedef enum {
    UFT_FC_DRIVE_525_48TPI,     /**< 5.25" 48 TPI (40 track) */
    UFT_FC_DRIVE_525_96TPI,     /**< 5.25" 96 TPI (80 track) */
    UFT_FC_DRIVE_8_SSSD,        /**< 8" SS/SD */
    UFT_FC_DRIVE_8_DSDD,        /**< 8" DS/DD */
} uft_fc_drive_t;

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct uft_fc_config_s uft_fc_config_t;

typedef struct {
    int track;
    int side;
    uint8_t *data;
    size_t size;
    int sector_count;
    uint8_t *sector_status;  /**< Per-sector error flags */
    bool success;
    const char *error;
} uft_fc_track_t;

typedef int (*uft_fc_callback_t)(const uft_fc_track_t *track, void *user);

/*============================================================================
 * LIFECYCLE
 *============================================================================*/


int uft_fc_open(uft_fc_config_t *cfg);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/


/*============================================================================
 * DEVICE INFO
 *============================================================================*/


/*============================================================================
 * CAPTURE
 *============================================================================*/


/*============================================================================
 * UTILITIES
 *============================================================================*/


#ifdef __cplusplus
}
#endif

#endif /* UFT_FC5025_H */
