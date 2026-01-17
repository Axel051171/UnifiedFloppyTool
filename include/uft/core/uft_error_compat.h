/**
 * @file uft_error_compat.h
 * @brief Error Code Compatibility Layer
 * 
 * This header provides aliases for legacy error code names.
 * Automatically includes uft_error.h for base definitions.
 */

#ifndef UFT_ERROR_COMPAT_H
#define UFT_ERROR_COMPAT_H

/* Include base error definitions first */
#include "uft/uft_error.h"

/* 
 * This file provides compatibility aliases for legacy code.
 * All aliases point to the canonical names defined in uft_error.h
 */

/* Legacy error code aliases - only if not already defined */

/* Bridge between error systems: uft_error.h uses CORRUPTED, unified_types uses CORRUPT */
#ifndef UFT_ERR_CORRUPT
#define UFT_ERR_CORRUPT UFT_ERR_CORRUPTED
#endif

#ifndef UFT_ERR_NOT_FOUND
#define UFT_ERR_NOT_FOUND UFT_ERR_FILE_NOT_FOUND
#endif

/* UFT_ERR_INVALID_ARG is defined in uft_error.h enum - don't create circular alias */
/* Both UFT_ERR_INVALID_ARG and UFT_ERR_INVALID_PARAM should map to the same value */

/* Typo alias: UFT_ERC_FORMAT -> UFT_ERR_CORRUPT */
#ifndef UFT_ERC_FORMAT
#define UFT_ERC_FORMAT UFT_ERR_CORRUPT
#endif

#ifndef UFT_ERROR_FORMAT_NOT_SUPPORTED
#define UFT_ERROR_FORMAT_NOT_SUPPORTED UFT_ERR_NOT_SUPPORTED
#endif

#ifndef UFT_ERROR_FORMAT_UNSUPPORTED
#define UFT_ERROR_FORMAT_UNSUPPORTED UFT_ERR_NOT_SUPPORTED
#endif

#ifndef UFT_ERR_INVALID_PARAM
#define UFT_ERR_INVALID_PARAM UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_ERR_FILE_OPEN
#define UFT_ERR_FILE_OPEN UFT_ERR_IO
#endif

#ifndef UFT_ERR_FILE_READ
#define UFT_ERR_FILE_READ UFT_ERR_IO
#endif

#ifndef UFT_ERR_FILE_CREATE
#define UFT_ERR_FILE_CREATE UFT_ERR_IO
#endif

/* UFT_ERR_FORMAT is defined in uft_error.h enum - DO NOT redefine here!
 * The enum value UFT_ERR_FORMAT = -20 is the correct one.
 * Legacy code using UFT_ERR_CORRUPT should use UFT_ERR_CORRUPTED instead.
 */

#ifndef UFT_ENCODING_GCR
#define UFT_ENCODING_GCR UFT_ENCODING_GCR_COMMODORE
#endif

#ifndef UFT_ERROR_INVALID_STATE
#define UFT_ERROR_INVALID_STATE UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_ERROR_NO_DATA
#define UFT_ERROR_NO_DATA UFT_ERR_FORMAT
#endif

#ifndef UFT_IO_ERR_READ
#define UFT_IO_ERR_READ UFT_ERR_IO
#endif

#ifndef UFT_IO_ERR_WRITE
#define UFT_IO_ERR_WRITE UFT_ERR_IO
#endif

#ifndef UFT_IO_ERR_EOF
#define UFT_IO_ERR_EOF UFT_ERR_IO
#endif

#ifndef UFT_IR_ERR_READ
#define UFT_IR_ERR_READ UFT_ERR_IO
#endif

#ifndef UFT_ERR_INVALID_PATH
#define UFT_ERR_INVALID_PATH UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_ERR_LIMIT
#define UFT_ERR_LIMIT UFT_ERR_MEMORY
#endif

#ifndef UFT_ERR_NULL_PTR
#define UFT_ERR_NULL_PTR UFT_ERR_INVALID_ARG
#endif

#endif /* UFT_ERROR_COMPAT_H */

/* ============================================================================
 * New module compatibility (TransWarp/FormatID integration)
 * These modules use short error codes (UFT_EINVAL instead of UFT_ERR_INVALID_ARG)
 * ============================================================================ */

#ifndef UFT_EINVAL
#define UFT_EINVAL UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_EIO
#define UFT_EIO UFT_ERR_IO
#endif

#ifndef UFT_EFORMAT
#define UFT_EFORMAT UFT_ERR_FORMAT
#endif

#ifndef UFT_EUNSUPPORTED
#define UFT_EUNSUPPORTED UFT_ERR_NOT_SUPPORTED
#endif

#ifndef UFT_ENOMEM
#define UFT_ENOMEM UFT_ERR_MEMORY
#endif

#ifndef UFT_ECRC
#define UFT_ECRC UFT_ERR_CRC
#endif

#ifndef UFT_ENOT_IMPLEMENTED
#define UFT_ENOT_IMPLEMENTED UFT_ERR_NOT_IMPLEMENTED
#endif

#ifndef UFT_ETIMEOUT
#define UFT_ETIMEOUT UFT_ERR_TIMEOUT
#endif

/* Re-close the guard */
#undef UFT_ERROR_COMPAT_H
#define UFT_ERROR_COMPAT_H
