/**
 * @file uft_error_compat.h
 * @brief Error Code Compatibility Layer
 * 
 * This header provides aliases for legacy error code names.
 * Include AFTER uft_error.h
 */

#ifndef UFT_ERROR_COMPAT_H
#define UFT_ERROR_COMPAT_H

/* 
 * This file should be included after uft_error.h
 * It only adds missing definitions, not duplicates
 */

/* Legacy error code aliases - only if not already defined */

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
