/**
 * @file uft_platform.h
 * @brief Platform-specific type definitions and compatibility
 *
 * This header provides portable type definitions that work across
 * Windows, Linux, and macOS. Include this header in any file that
 * uses platform-specific types like ssize_t.
 *
 * Copyright (C) 2025 UFT Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UFT_PLATFORM_H
#define UFT_PLATFORM_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * ssize_t compatibility
 *============================================================================*/

#ifdef _WIN32
    #ifndef _SSIZE_T_DEFINED
        #define _SSIZE_T_DEFINED
        #ifdef _WIN64
            typedef __int64 ssize_t;
        #else
            typedef int ssize_t;
        #endif
    #endif
#else
    /* POSIX systems have ssize_t in sys/types.h */
    #include <sys/types.h>
#endif

/* Include compiler macros after platform detection */
#include "uft/uft_compiler.h"

/*============================================================================
 * Boolean compatibility (for older compilers)
 *============================================================================*/

#ifndef __bool_true_false_are_defined
    #ifndef bool
        #define bool _Bool
    #endif
    #ifndef true
        #define true 1
    #endif
    #ifndef false
        #define false 0
    #endif
#endif

/*============================================================================
 * Integer type limits (supplement stdint.h)
 *============================================================================*/

#ifndef SIZE_MAX
    #ifdef _WIN64
        #define SIZE_MAX 0xFFFFFFFFFFFFFFFFULL
    #else
        #define SIZE_MAX 0xFFFFFFFFUL
    #endif
#endif

#ifndef SSIZE_MAX
    #ifdef _WIN64
        #define SSIZE_MAX 0x7FFFFFFFFFFFFFFFLL
    #else
        #define SSIZE_MAX 0x7FFFFFFFL
    #endif
#endif

/*============================================================================
 * Path separator
 *============================================================================*/

#ifdef _WIN32
    #define UFT_PATH_SEPARATOR '\\'
    #define UFT_PATH_SEPARATOR_STR "\\"
#else
    #define UFT_PATH_SEPARATOR '/'
    #define UFT_PATH_SEPARATOR_STR "/"
#endif

/*============================================================================
 * Thread-local storage
 *============================================================================*/

#if defined(_MSC_VER)
    #define UFT_THREAD_LOCAL __declspec(thread)
#elif defined(__GNUC__) || defined(__clang__)
    #define UFT_THREAD_LOCAL __thread
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define UFT_THREAD_LOCAL _Thread_local
#else
    #define UFT_THREAD_LOCAL  /* No thread-local support */
    #warning "Thread-local storage not supported"
#endif

/*============================================================================
 * Inline hints - Defined in uft_compiler.h (included above)
 * DO NOT DEFINE HERE - use the central definition!
 *============================================================================*/

/* UFT_INLINE, UFT_FORCE_INLINE are defined in uft_compiler.h */

/*============================================================================
 * Unused parameter marker - Defined in uft_compiler.h
 *============================================================================*/

/* UFT_UNUSED is defined in uft_compiler.h */

/*============================================================================
 * Endianness detection
 *============================================================================*/

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #define UFT_LITTLE_ENDIAN 1
#elif defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
    __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define UFT_BIG_ENDIAN 1
#elif defined(_WIN32) || defined(__i386__) || defined(__x86_64__) || \
      defined(__amd64__) || defined(__aarch64__)
    #define UFT_LITTLE_ENDIAN 1
#else
    /* Default to little endian */
    #define UFT_LITTLE_ENDIAN 1
#endif

/*============================================================================
 * Struct packing (with guards to prevent redefinition warnings)
 *============================================================================*/

#ifndef UFT_PACKED_BEGIN
    #if defined(_MSC_VER)
        #define UFT_PACKED_BEGIN __pragma(pack(push, 1))
        #define UFT_PACKED_END   __pragma(pack(pop))
        #define UFT_PACKED_STRUCT
    #elif defined(__GNUC__) || defined(__clang__)
        #define UFT_PACKED_BEGIN
        #define UFT_PACKED_END
        #define UFT_PACKED_STRUCT __attribute__((packed))
    #else
        #define UFT_PACKED_BEGIN
        #define UFT_PACKED_END
        #define UFT_PACKED_STRUCT
    #endif
#endif /* UFT_PACKED_BEGIN */

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLATFORM_H */
