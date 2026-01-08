/**
 * @file uft_error_compat.h
 * @brief Error Code Compatibility Aliases
 */

#ifndef UFT_ERROR_COMPAT_H
#define UFT_ERROR_COMPAT_H

/* Base error codes if not defined */
#ifndef UFT_OK
#define UFT_OK 0
#endif

#ifndef UFT_ERROR
#define UFT_ERROR (-1)
#endif

/* Common error codes */
#ifndef UFT_ERR_INVALID_ARG
#define UFT_ERR_INVALID_ARG (-1)
#endif

#ifndef UFT_ERR_IO
#define UFT_ERR_IO (-2)
#endif

#ifndef UFT_ERR_NOMEM
#define UFT_ERR_NOMEM (-3)
#endif

#ifndef UFT_ERR_TIMEOUT
#define UFT_ERR_TIMEOUT (-4)
#endif

#ifndef UFT_ERR_INVALID_PATH
#define UFT_ERR_INVALID_PATH (-5)
#endif

#ifndef UFT_ERR_LIMIT
#define UFT_ERR_LIMIT (-6)
#endif

#ifndef UFT_IO_ERR_READ
#define UFT_IO_ERR_READ (-10)
#endif

#ifndef UFT_IO_ERR_WRITE
#define UFT_IO_ERR_WRITE (-11)
#endif

#ifndef UFT_IO_ERR_EOF
#define UFT_IO_ERR_EOF (-12)
#endif

#ifndef UFT_IR_ERR_READ
#define UFT_IR_ERR_READ UFT_IO_ERR_READ
#endif

/* Format-related errors */
#ifndef UFT_ERROR_FORMAT_UNSUPPORTED
#define UFT_ERROR_FORMAT_UNSUPPORTED (-100)
#endif

#ifndef UFT_ERROR_INVALID_ARG
#define UFT_ERROR_INVALID_ARG (-101)
#endif

#ifndef UFT_ERROR_INVALID_STATE
#define UFT_ERROR_INVALID_STATE (-102)
#endif

/* Legacy aliases */
#ifndef UFT_ERROR_FORMAT_NOT_SUPPORTED
#define UFT_ERROR_FORMAT_NOT_SUPPORTED UFT_ERROR_FORMAT_UNSUPPORTED
#endif

#ifndef UFT_ERR_INVALID_PARAM
#define UFT_ERR_INVALID_PARAM UFT_ERR_INVALID_ARG
#endif

#ifndef UFT_ERR_FILE_OPEN
#define UFT_ERR_FILE_OPEN UFT_ERR_IO
#endif

#endif /* UFT_ERROR_COMPAT_H */
