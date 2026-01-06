/**
 * @file uft_compat.h
 * @brief Cross-platform compatibility definitions for UFT
 *
 * This header must be included FIRST (before any system headers) to ensure
 * proper feature macro definitions. It provides:
 * - Feature test macros for POSIX functions
 * - Windows-compatible replacements for POSIX functions
 * - Type definitions (ssize_t, etc.)
 * - Safe string function wrappers
 *
 * Copyright (C) 2025 UFT Project
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UFT_COMPAT_H
#define UFT_COMPAT_H

/*============================================================================
 * Feature test macros - MUST be defined before ANY system headers
 *============================================================================*/

/* Enable POSIX.1-2008 features (strdup, clock_gettime, etc.) */
#ifndef _POSIX_C_SOURCE
    #define _POSIX_C_SOURCE 200809L
#endif

/* Enable BSD features (usleep, etc.) on glibc */
#ifndef _BSD_SOURCE
    #define _BSD_SOURCE 1
#endif

/* Enable default source (modern glibc) */
#ifndef _DEFAULT_SOURCE
    #define _DEFAULT_SOURCE 1
#endif

/* Enable GNU extensions */
#ifndef _GNU_SOURCE
    #define _GNU_SOURCE 1
#endif

/*============================================================================
 * Standard headers
 *============================================================================*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

/*============================================================================
 * Windows-specific compatibility
 *============================================================================*/

#ifdef _WIN32
    /* Suppress deprecation warnings for POSIX functions */
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS 1
    #endif
    #ifndef _CRT_NONSTDC_NO_WARNINGS
        #define _CRT_NONSTDC_NO_WARNINGS 1
    #endif

    #include <windows.h>
    #include <io.h>
    #include <direct.h>

    /* ssize_t for Windows */
    #ifndef _SSIZE_T_DEFINED
        #define _SSIZE_T_DEFINED
        #include <basetsd.h>
        #ifdef _WIN64
            typedef __int64 ssize_t;
        #else
            typedef int ssize_t;
        #endif
    #endif

    /* POSIX function replacements */
    #ifndef strdup
        #define strdup _strdup
    #endif
    #ifndef strcasecmp
        #define strcasecmp _stricmp
    #endif
    #ifndef strncasecmp
        #define strncasecmp _strnicmp
    #endif
    #ifndef snprintf
        #define snprintf _snprintf
    #endif
    #ifndef close
        #define close _close
    #endif
    #ifndef read
        #define read _read
    #endif
    #ifndef write
        #define write _write
    #endif
    #ifndef open
        #define open _open
    #endif
    #ifndef lseek
        #define lseek _lseek
    #endif
    #ifndef fileno
        #define fileno _fileno
    #endif
    #ifndef access
        #define access _access
    #endif
    #ifndef mkdir
        #define mkdir(path, mode) _mkdir(path)
    #endif
    #ifndef rmdir
        #define rmdir _rmdir
    #endif
    #ifndef unlink
        #define unlink _unlink
    #endif
    #ifndef getcwd
        #define getcwd _getcwd
    #endif
    #ifndef chdir
        #define chdir _chdir
    #endif
    #ifndef isatty
        #define isatty _isatty
    #endif

    /* Access mode constants */
    #ifndef F_OK
        #define F_OK 0
    #endif
    #ifndef R_OK
        #define R_OK 4
    #endif
    #ifndef W_OK
        #define W_OK 2
    #endif
    #ifndef X_OK
        #define X_OK 1
    #endif

    /* usleep replacement using Windows API */
    static inline void usleep(unsigned int usec) {
        Sleep((usec + 999) / 1000);  /* Round up to milliseconds */
    }

    /* gethostname for Windows */
    #ifndef gethostname
        #define gethostname(buf, len) \
            (GetComputerNameA((buf), &(DWORD){(len)}) ? 0 : -1)
    #endif

    /* clock_gettime replacement for Windows */
    #ifndef CLOCK_REALTIME
        #define CLOCK_REALTIME 0
    #endif
    #ifndef CLOCK_MONOTONIC
        #define CLOCK_MONOTONIC 1
    #endif

    struct timespec;  /* Forward declaration */

    static inline int clock_gettime(int clk_id, struct timespec *tp) {
        (void)clk_id;
        FILETIME ft;
        GetSystemTimeAsFileTime(&ft);
        ULARGE_INTEGER ull;
        ull.LowPart = ft.dwLowDateTime;
        ull.HighPart = ft.dwHighDateTime;
        /* FILETIME is 100-nanosecond intervals since 1601-01-01 */
        /* Convert to Unix epoch (1970-01-01) */
        ull.QuadPart -= 116444736000000000ULL;
        tp->tv_sec = (long)(ull.QuadPart / 10000000ULL);
        tp->tv_nsec = (long)((ull.QuadPart % 10000000ULL) * 100);
        return 0;
    }

#else
    /* POSIX systems */
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <unistd.h>
    #include <strings.h>  /* strcasecmp, strncasecmp */
    #include <time.h>     /* clock_gettime */
#endif

/*============================================================================
 * Safe string functions (cross-platform)
 *============================================================================*/

/**
 * Safe string copy (ensures null-termination).
 * Works like strncpy but always null-terminates.
 */
static inline char *uft_strncpy_safe(char *dest, const char *src, size_t n) {
    if (n == 0 || !dest) return dest;
    strncpy(dest, src ? src : "", n - 1);
    dest[n - 1] = '\0';
    return dest;
}

/**
 * Safe string concatenation.
 */
static inline char *uft_strncat_safe(char *dest, const char *src, size_t n) {
    if (n == 0 || !dest) return dest;
    size_t dest_len = strlen(dest);
    if (dest_len >= n - 1) return dest;
    strncat(dest, src ? src : "", n - dest_len - 1);
    dest[n - 1] = '\0';
    return dest;
}

/**
 * Safe strdup with malloc.
 */
static inline char *uft_strdup_safe(const char *src) {
    if (!src) return NULL;
    size_t len = strlen(src) + 1;
    char *dest = (char *)malloc(len);
    if (dest) {
        memcpy(dest, src, len);
    }
    return dest;
}

/*============================================================================
 * Integer type safety
 *============================================================================*/

/* Safe cast from size_t to smaller types */
static inline uint32_t uft_size_to_u32(size_t val) {
    return (val > UINT32_MAX) ? UINT32_MAX : (uint32_t)val;
}

static inline uint16_t uft_size_to_u16(size_t val) {
    return (val > UINT16_MAX) ? UINT16_MAX : (uint16_t)val;
}

static inline uint8_t uft_size_to_u8(size_t val) {
    return (val > UINT8_MAX) ? UINT8_MAX : (uint8_t)val;
}

/* Safe cast from int to unsigned */
static inline uint32_t uft_int_to_u32(int val) {
    return (val < 0) ? 0 : (uint32_t)val;
}

/*============================================================================
 * Compiler-specific attributes
 *============================================================================*/

/* Unused parameter/variable marker */
#ifdef __GNUC__
    #define UFT_UNUSED __attribute__((unused))
#else
    #define UFT_UNUSED
#endif

/* Printf-like format checking */
#ifdef __GNUC__
    #define UFT_PRINTF_LIKE(fmt_idx, args_idx) \
        __attribute__((format(printf, fmt_idx, args_idx)))
#else
    #define UFT_PRINTF_LIKE(fmt_idx, args_idx)
#endif

/* Likely/unlikely branch hints */
#ifdef __GNUC__
    #define UFT_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UFT_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define UFT_LIKELY(x)   (x)
    #define UFT_UNLIKELY(x) (x)
#endif

/* Inline hint */
#ifdef _MSC_VER
    #define UFT_INLINE __forceinline
#elif defined(__GNUC__)
    #define UFT_INLINE inline __attribute__((always_inline))
#else
    #define UFT_INLINE inline
#endif

/* No-inline hint */
#ifdef _MSC_VER
    #define UFT_NOINLINE __declspec(noinline)
#elif defined(__GNUC__)
    #define UFT_NOINLINE __attribute__((noinline))
#else
    #define UFT_NOINLINE
#endif

#endif /* UFT_COMPAT_H */
