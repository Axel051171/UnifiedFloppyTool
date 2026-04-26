/* =====================================================================
 * GENERATED FILE — DO NOT EDIT BY HAND.
 * Source of truth: data/errors.tsv
 * Regenerate with: make generate  (or scripts/verify_errors_ssot.sh)
 * Any manual edits will be overwritten on the next generator run.
 * ===================================================================== */
#ifndef UFT_ERROR_H
#define UFT_ERROR_H

/**
 * @file uft_error.h
 * @brief Unified error handling for UnifiedFloppyTool (GENERATED).
 *
 * This header is generated from data/errors.tsv by
 * scripts/generators/gen_errors_h.py. Edit the TSV, not this file.
 *
 * The runtime surface (struct uft_error_ctx, uft_strerror, helper
 * macros) lives in the hand-maintained sibling header
 * include/uft/core/uft_error_ext.h which this file `#include`s at
 * the bottom. Legacy alias spellings (UFT_ERROR_*, UFT_E_*, etc.)
 * are provided by the generated header
 * include/uft/core/uft_error_compat_gen.h.
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UFT_ERROR_ENUM_DEFINED
#define UFT_ERROR_ENUM_DEFINED
typedef enum {

    /* --- MISC --- */
    UFT_SUCCESS = 0, /**< Operation completed successfully */

    /* --- ARG --- */
    UFT_ERR_INVALID_ARG = -1, /**< Invalid argument (NULL pointer, out of range, etc.) */
    UFT_ERR_BUFFER_TOO_SMALL = -2, /**< Required buffer too small */
    UFT_ERR_INVALID_PATH = -3, /**< Invalid path or filename */
    UFT_ERR_NULL_POINTER = -5, /**< Null pointer passed where non-null required (distinct diagnostic from generic INVALID_ARG; 86 call sites) */

    /* --- IO --- */
    UFT_ERR_IO = -10, /**< General I/O error */
    UFT_ERR_FILE_NOT_FOUND = -11, /**< File not found */
    UFT_ERR_PERMISSION = -12, /**< Permission denied */
    UFT_ERR_FILE_EXISTS = -13, /**< File already exists */
    UFT_ERR_EOF = -14, /**< End of file reached */

    /* --- FORMAT --- */
    UFT_ERR_FORMAT = -20, /**< Unknown or invalid format */
    UFT_ERR_FORMAT_DETECT = -21, /**< Format detection failed */
    UFT_ERR_FORMAT_VARIANT = -22, /**< Unsupported format variant */
    UFT_ERR_CORRUPTED = -23, /**< Corrupted or invalid data */
    UFT_ERR_CRC = -24, /**< CRC/checksum mismatch */

    /* --- RESOURCE --- */
    UFT_ERR_MEMORY = -30, /**< Memory allocation failed */
    UFT_ERR_RESOURCE = -31, /**< Resource limit exceeded */
    UFT_ERR_BUSY = -32, /**< Resource busy */

    /* --- FEATURE --- */
    UFT_ERR_NOT_SUPPORTED = -40, /**< Feature not supported */
    UFT_ERR_NOT_IMPLEMENTED = -41, /**< Feature not implemented */
    UFT_ERR_NOT_PERMITTED = -42, /**< Operation not permitted in current state */

    /* --- HARDWARE --- */
    UFT_ERR_HARDWARE = -50, /**< Hardware communication error */
    UFT_ERR_USB = -51, /**< USB error */
    UFT_ERR_DEVICE_NOT_FOUND = -52, /**< Device not found */
    UFT_ERR_TIMEOUT = -53, /**< Operation timed out */

    /* --- INTERNAL --- */
    UFT_ERR_INTERNAL = -90, /**< Internal error (should not happen) */
    UFT_ERR_ASSERTION = -91, /**< Assertion failed */

    /* --- MISC --- */
    UFT_ERR_UNKNOWN = -100, /**< Unknown error */

    /* --- FORMAT --- */
    UFT_ERR_FORMAT_INVALID = -25, /**< Format structurally invalid (magic/header mismatch) */
    UFT_ERR_UNKNOWN_FORMAT = -26, /**< Format could not be identified from content */
    UFT_ERR_ENCODING = -27, /**< Encoding error (MFM/FM/GCR bit-level decode failed) */
    UFT_ERR_LONG_TRACK = -28, /**< Track exceeds standard length (possible protection) */
    UFT_ERR_NON_STANDARD = -29, /**< Non-standard format variant */
    UFT_ERR_SYNC_LOST = -60, /**< PLL/sync lost during decode */
    UFT_ERR_WEAK_BITS = -61, /**< Weak bits detected in data field */
    UFT_ERR_TIMING = -62, /**< Timing anomaly (splice, density-mix) */
    UFT_ERR_ID_MISMATCH = -63, /**< Sector ID header mismatch */
    UFT_ERR_DELETED_DATA = -64, /**< Deleted-data address mark (0xF8) */
    UFT_ERR_MISSING_SECTOR = -65, /**< Expected sector not found on track */
    UFT_ERR_INCOMPLETE = -66, /**< Incomplete data (truncated sector/track) */
    UFT_ERR_PLL_UNLOCK = -67, /**< PLL never achieved lock */

    /* --- PROTECTION --- */
    UFT_ERR_PROTECTION = -80, /**< Copy-protection scheme triggered */
    UFT_ERR_COPY_DENIED = -81, /**< Copy refused due to protection policy */

    /* --- HARDWARE --- */
    UFT_ERR_WRITE_PROTECT = -54, /**< Disk is write-protected */
    UFT_ERR_WRITE_FAULT = -55, /**< Drive reports write fault */
    UFT_ERR_TRACK_OVERFLOW = -56, /**< Track buffer overflow during write */

    /* --- ARG --- */
    UFT_ERR_VERIFY_FAIL = -4, /**< Verification compare failed */

    /* --- IO --- */
    UFT_ERR_FILE_OPEN = -15, /**< Cannot open file */
    UFT_ERR_FILE_READ = -16, /**< File read error */
    UFT_ERR_FILE_CREATE = -17, /**< Cannot create file */
    UFT_ERR_FILE_SEEK = -18, /**< File seek error (offset out of range or device failure) */
    UFT_ERR_FILE_WRITE = -19, /**< File write error */

    /* --- MISC --- */
    UFT_ERR_CANCELLED = -92, /**< Operation cancelled by user or policy */

    /* --- INTERNAL --- */
    UFT_ERR_INVALID_STATE = -93, /**< Object/context in invalid state for operation */

    /* --- MISC --- */
    UFT_ERR_VERSION = -94, /**< Version mismatch */

    /* --- RESOURCE --- */
    UFT_ERR_PLUGIN_LOAD = -95, /**< Plugin load failed (ABI / dlopen / init error) */
    UFT_ERR_PLUGIN_NOT_FOUND = -96, /**< Plugin not found by name or format id */
} uft_rc_t;

#ifndef UFT_ERROR_T_DEFINED
#define UFT_ERROR_T_DEFINED
typedef uft_rc_t uft_error_t;
#endif
#else
/* Enum already defined by another header — provide opaque int typedef. */
typedef int uft_rc_t;
#ifndef UFT_ERROR_T_DEFINED
#define UFT_ERROR_T_DEFINED
typedef int uft_error_t;
#endif
#ifndef UFT_SUCCESS
#define UFT_SUCCESS 0
#endif
#endif /* UFT_ERROR_ENUM_DEFINED */

#ifndef UFT_OK
#define UFT_OK UFT_SUCCESS
#endif

#ifdef __cplusplus
}
#endif

/* Runtime surface (struct uft_error_ctx, uft_strerror prototype, helper
 * macros, inline predicates) lives in the hand-maintained sibling header.
 * Pulled in last so every consumer of <uft/uft_error.h> transitively gets
 * the full API without a second include. */
#include "uft/core/uft_error_ext.h"

#endif /* UFT_ERROR_H */
