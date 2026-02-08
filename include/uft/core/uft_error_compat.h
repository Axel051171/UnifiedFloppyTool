/**
 * @file uft_error_compat.h
 * @brief Error Code Compatibility Layer
 * 
 * Uses internal _UFTEC_* names to avoid conflicts with enum-based
 * error systems in uft_unified_types.h and uft_error.h.
 */

#ifndef UFT_ERROR_COMPAT_H
#define UFT_ERROR_COMPAT_H

/* Internal base values - never conflict with enum members */
#define _UFTEC_IO          (-100)
#define _UFTEC_MEM         (-500)
#define _UFTEC_CORRUPT     (-208)
#define _UFTEC_CRC         (-300)
#define _UFTEC_INVAL       (-2)
#define _UFTEC_NOTIMPL     (-5)
#define _UFTEC_UNSUP       (-6)
#define _UFTEC_TIMEOUT     (-10)
#define _UFTEC_BUSY        (-9)
#define _UFTEC_INTERNAL    (-19)
#define _UFTEC_STATE       (-15)
#define _UFTEC_NULLPTR     (-3)
#define _UFTEC_FNOTFOUND   (-101)
#define _UFTEC_FOPEN       (-103)
#define _UFTEC_FREAD       (-104)
#define _UFTEC_FCREATE     (-108)

/* === Aliases: only define if not already present (from enum or other header) === */

/* Core error codes */
#ifndef UFT_ERR_IO
#define UFT_ERR_IO              _UFTEC_IO
#endif
#ifndef UFT_ERR_MEMORY
#define UFT_ERR_MEMORY          _UFTEC_MEM
#endif
#ifndef UFT_ERR_CORRUPT
#define UFT_ERR_CORRUPT         _UFTEC_CORRUPT
#endif
#ifndef UFT_ERR_CRC
#define UFT_ERR_CRC             _UFTEC_CRC
#endif
#ifndef UFT_ERR_INVALID_PARAM
#define UFT_ERR_INVALID_PARAM   _UFTEC_INVAL
#endif
#ifndef UFT_ERR_NOT_IMPL
#define UFT_ERR_NOT_IMPL        _UFTEC_NOTIMPL
#endif
#ifndef UFT_ERR_NOT_SUPPORTED
#define UFT_ERR_NOT_SUPPORTED   _UFTEC_UNSUP
#endif
#ifndef UFT_ERR_UNSUPPORTED
#define UFT_ERR_UNSUPPORTED     _UFTEC_UNSUP
#endif
#ifndef UFT_ERR_TIMEOUT
#define UFT_ERR_TIMEOUT         _UFTEC_TIMEOUT
#endif
#ifndef UFT_ERR_BUSY
#define UFT_ERR_BUSY            _UFTEC_BUSY
#endif
#ifndef UFT_ERR_INTERNAL
#define UFT_ERR_INTERNAL        _UFTEC_INTERNAL
#endif

/* Alternate naming aliases */
#ifndef UFT_ERR_CORRUPTED
#define UFT_ERR_CORRUPTED       _UFTEC_CORRUPT
#endif
#ifndef UFT_ERR_INVALID_ARG
#define UFT_ERR_INVALID_ARG     _UFTEC_INVAL
#endif
#ifndef UFT_ERR_NOT_IMPLEMENTED
#define UFT_ERR_NOT_IMPLEMENTED _UFTEC_NOTIMPL
#endif
#ifndef UFT_ERR_FORMAT
#define UFT_ERR_FORMAT          _UFTEC_CORRUPT
#endif
#ifndef UFT_ERC_FORMAT
#define UFT_ERC_FORMAT          _UFTEC_CORRUPT
#endif
#ifndef UFT_ERR_INVALID_STATE
#define UFT_ERR_INVALID_STATE   _UFTEC_STATE
#endif

/* File I/O aliases */
#ifndef UFT_ERR_FILE_OPEN
#define UFT_ERR_FILE_OPEN       _UFTEC_FOPEN
#endif
#ifndef UFT_ERR_FILE_READ
#define UFT_ERR_FILE_READ       _UFTEC_FREAD
#endif
#ifndef UFT_ERR_FILE_CREATE
#define UFT_ERR_FILE_CREATE     _UFTEC_FCREATE
#endif
#ifndef UFT_ERR_FILE_NOT_FOUND
#define UFT_ERR_FILE_NOT_FOUND  _UFTEC_FNOTFOUND
#endif
#ifndef UFT_ERR_NOT_FOUND
#define UFT_ERR_NOT_FOUND       _UFTEC_FNOTFOUND
#endif
#ifndef UFT_ERR_INVALID_PATH
#define UFT_ERR_INVALID_PATH    _UFTEC_INVAL
#endif
#ifndef UFT_ERR_NULL_PTR
#define UFT_ERR_NULL_PTR        _UFTEC_NULLPTR
#endif
#ifndef UFT_ERR_LIMIT
#define UFT_ERR_LIMIT           _UFTEC_MEM
#endif

/* I/O subsystem aliases */
#ifndef UFT_IO_ERR_READ
#define UFT_IO_ERR_READ         _UFTEC_IO
#endif
#ifndef UFT_IO_ERR_WRITE
#define UFT_IO_ERR_WRITE        _UFTEC_IO
#endif
#ifndef UFT_IO_ERR_EOF
#define UFT_IO_ERR_EOF          _UFTEC_IO
#endif
#ifndef UFT_IR_ERR_READ
#define UFT_IR_ERR_READ         _UFTEC_IO
#endif

/* Legacy ERROR_ prefix */
#ifndef UFT_ERROR_FORMAT_NOT_SUPPORTED
#define UFT_ERROR_FORMAT_NOT_SUPPORTED _UFTEC_UNSUP
#endif
#ifndef UFT_ERROR_FORMAT_UNSUPPORTED
#define UFT_ERROR_FORMAT_UNSUPPORTED   _UFTEC_UNSUP
#endif
#ifndef UFT_ERROR_INVALID_STATE
#define UFT_ERROR_INVALID_STATE        _UFTEC_STATE
#endif
#ifndef UFT_ERROR_NO_DATA
#define UFT_ERROR_NO_DATA              _UFTEC_CORRUPT
#endif
#ifndef UFT_ERROR_FILE_NOT_FOUND
#define UFT_ERROR_FILE_NOT_FOUND       _UFTEC_FNOTFOUND
#endif
#ifndef UFT_ERR_INVALID_PARAM_VALUE
#define UFT_ERR_INVALID_PARAM_VALUE    _UFTEC_INVAL
#endif

/* Encoding aliases */
#ifndef UFT_ENCODING_GCR
#define UFT_ENCODING_GCR               3
#endif

/* FloppyDevice short-form codes */
#ifndef UFT_EINVAL
#define UFT_EINVAL              _UFTEC_INVAL
#endif
#ifndef UFT_EIO
#define UFT_EIO                 _UFTEC_IO
#endif
#ifndef UFT_EFORMAT
#define UFT_EFORMAT             _UFTEC_CORRUPT
#endif
#ifndef UFT_EUNSUPPORTED
#define UFT_EUNSUPPORTED        _UFTEC_UNSUP
#endif
#ifndef UFT_ENOMEM
#define UFT_ENOMEM              _UFTEC_MEM
#endif
#ifndef UFT_ECRC
#define UFT_ECRC                _UFTEC_CRC
#endif
#ifndef UFT_ENOT_IMPLEMENTED
#define UFT_ENOT_IMPLEMENTED    _UFTEC_NOTIMPL
#endif
#ifndef UFT_ETIMEOUT
#define UFT_ETIMEOUT            _UFTEC_TIMEOUT
#endif

/* Legacy E_ prefix */
#ifndef UFT_E_IO
#define UFT_E_IO                _UFTEC_IO
#endif
#ifndef UFT_E_CORRUPT
#define UFT_E_CORRUPT           _UFTEC_CORRUPT
#endif
#ifndef UFT_E_INVALID_ARG
#define UFT_E_INVALID_ARG       _UFTEC_INVAL
#endif
#ifndef UFT_E_MEMORY
#define UFT_E_MEMORY            _UFTEC_MEM
#endif

#endif /* UFT_ERROR_COMPAT_H */

/* ============================================================================
 * Phase 2 Compatibility: Format Types, Encoding, Structure Field Aliases
 * ============================================================================ */

/* Format type aliases */
#ifndef UFT_FMT_RAW
#define UFT_FMT_RAW             UFT_FORMAT_RAW
#endif
#ifndef UFT_FMT_IMG
#define UFT_FMT_IMG             UFT_FORMAT_IMG
#endif

/* Encoding type aliases (UFT_ENCODING_* → UFT_ENC_*) */
#ifndef UFT_ENCODING_MFM
#define UFT_ENCODING_MFM        UFT_ENC_MFM
#endif
#ifndef UFT_ENCODING_FM
#define UFT_ENCODING_FM         UFT_ENC_FM
#endif
#ifndef UFT_ENCODING_GCR
#define UFT_ENCODING_GCR        UFT_ENC_GCR_C64
#endif
#ifndef UFT_ENC_GCR
#define UFT_ENC_GCR             UFT_ENC_GCR_C64
#endif

/* Encoding type for files using typedef - only if uft_types.h not included */
#if !defined(_UFT_ENCODING_T_DEFINED) && !defined(UFT_TYPES_H)
#define _UFT_ENCODING_T_DEFINED
#ifdef UINT8_MAX
typedef uint8_t uft_encoding_t;
#else
typedef unsigned char uft_encoding_t;
#endif
#endif

/* Error code aliases for Phase 2 files */
#ifndef UFT_ERR_UNKNOWN_FORMAT
#define UFT_ERR_UNKNOWN_FORMAT  _UFTEC_CORRUPT
#endif
#ifndef UFT_ERR_INCOMPLETE
#define UFT_ERR_INCOMPLETE      _UFTEC_IO
#endif

