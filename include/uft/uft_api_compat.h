/**
 * @file uft_api_compat.h
 * @brief API Compatibility Layer for hardened format implementations
 * 
 * This header provides all necessary includes and compatibility macros
 * for the hardened format plugins to compile against the current API.
 */

#ifndef UFT_API_COMPAT_H
#define UFT_API_COMPAT_H

/* ═══════════════════════════════════════════════════════════════════════════════
 * Required Includes
 * ═══════════════════════════════════════════════════════════════════════════════ */

#include "uft/uft_error.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_capability.h"
#include "uft/uft_platform.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════════
 * Error Code Compatibility (UFT_ERROR_* → UFT_ERR_*)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_ERROR_OK
#define UFT_ERROR_OK          UFT_OK
#endif

#ifndef UFT_ERROR_FORMAT
#define UFT_ERROR_FORMAT      UFT_ERR_FORMAT
#endif

#ifndef UFT_ERROR_MEMORY
#define UFT_ERROR_MEMORY      UFT_ERR_MEMORY
#endif

#ifndef UFT_ERROR_IO
#define UFT_ERROR_IO          UFT_ERR_IO
#endif

#ifndef UFT_ERROR_FILE_READ
#define UFT_ERROR_FILE_READ   UFT_ERR_IO
#endif

#ifndef UFT_ERROR_FILE_WRITE
#define UFT_ERROR_FILE_WRITE  UFT_ERR_IO
#endif

#ifndef UFT_ERROR_FILE_OPEN
#define UFT_ERROR_FILE_OPEN   UFT_ERR_IO
#endif

#ifndef UFT_ERROR_FILE_SEEK
#define UFT_ERROR_FILE_SEEK   UFT_ERR_IO
#endif

#ifndef UFT_ERROR_NO_MEMORY
#define UFT_ERROR_NO_MEMORY   UFT_ERR_MEMORY
#endif

#ifndef UFT_ERROR_INVALID
#define UFT_ERROR_INVALID     UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_ERROR_BOUNDS
#define UFT_ERROR_BOUNDS      UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_ERROR_UNSUPPORTED
#define UFT_ERROR_UNSUPPORTED UFT_ERR_NOT_SUPPORTED
#endif

#ifndef UFT_ERROR_CRC
#define UFT_ERROR_CRC         UFT_ERR_CRC
#endif

#ifndef UFT_ERROR_CORRUPTED
#define UFT_ERROR_CORRUPTED   UFT_ERR_CORRUPTED
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Capability Flag Compatibility
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* GCR/MFM map to FLUX capability */
#ifndef UFT_CAP_GCR
#define UFT_CAP_GCR           UFT_CAP_FLUX
#endif

#ifndef UFT_CAP_MFM
#define UFT_CAP_MFM           UFT_CAP_FLUX
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * NULL-Check Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#endif

#endif

#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Safe Math (if not already defined)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_SAFE_MUL_DEFINED
#define UFT_SAFE_MUL_DEFINED
static inline bool uft_safe_mul_size(size_t a, size_t b, size_t* result) {
    if (b != 0 && a > SIZE_MAX / b) return false;
    *result = a * b;
    return true;
}
#endif


/* ═══════════════════════════════════════════════════════════════════════════════
 * UFT_FORMAT_CAP_* → UFT_CAP_* Compatibility
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_FORMAT_CAP_READ
#define UFT_FORMAT_CAP_READ    UFT_CAP_READ
#endif

#ifndef UFT_FORMAT_CAP_WRITE
#define UFT_FORMAT_CAP_WRITE   UFT_CAP_WRITE
#endif

#ifndef UFT_FORMAT_CAP_CREATE
#define UFT_FORMAT_CAP_CREATE  UFT_CAP_WRITE
#endif

#ifndef UFT_FORMAT_CAP_VERIFY
#define UFT_FORMAT_CAP_VERIFY  UFT_CAP_VERIFY
#endif

#ifndef UFT_FORMAT_CAP_ANALYZE

#ifndef UFT_FORMAT_CAP_FLUX
#define UFT_FORMAT_CAP_FLUX    UFT_CAP_FLUX
#endif
#define UFT_FORMAT_CAP_ANALYZE UFT_CAP_ANALYZE

#ifndef UFT_FORMAT_CAP_FLUX
#define UFT_FORMAT_CAP_FLUX    UFT_CAP_FLUX
#endif
#endif

#endif /* UFT_API_COMPAT_H */
