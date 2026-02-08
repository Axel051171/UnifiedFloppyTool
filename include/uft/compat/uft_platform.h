/**
 * @file uft_platform.h
 * @brief Platform compatibility layer for Windows/macOS/Linux
 * 
 * Include this header for cross-platform I/O and POSIX compatibility.
 */
#ifndef UFT_PLATFORM_H
#define UFT_PLATFORM_H

/* Enable POSIX features on Unix-like systems */
#ifndef _WIN32
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#endif

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef _WIN32
/*===========================================================================*/
/*                         WINDOWS COMPATIBILITY                              */
/*===========================================================================*/

#include <windows.h>
#include <io.h>
#include <fcntl.h>
#include <time.h>  /* Include first so MSVC's timespec is used if available */
#include <sys/stat.h>
#include <time.h>

/* ssize_t is not defined in Windows */
#ifndef ssize_t
#ifdef _WIN64
typedef int64_t ssize_t;
#else
typedef int32_t ssize_t;
#endif
#endif

/* POSIX file I/O compatibility */
#define open    _open
#define close   _close
#define read    _read
#define write   _write
#define lseek   _lseek
#define stat    _stat
#define fstat   _fstat
#define fileno  _fileno
#define access  _access
#define unlink  _unlink
#define mkdir(path, mode) _mkdir(path)

/* Open flags */
#ifndef O_RDONLY
#define O_RDONLY _O_RDONLY
#define O_WRONLY _O_WRONLY
#define O_RDWR   _O_RDWR
#define O_CREAT  _O_CREAT
#define O_TRUNC  _O_TRUNC
#define O_APPEND _O_APPEND
#define O_BINARY _O_BINARY
#endif

/* Access flags */
#ifndef F_OK
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
#endif

/* Path separator */
#define UFT_PATH_SEP '\\'
#define UFT_PATH_SEP_STR "\\"

/* Sleep function */
#define usleep(us) Sleep((us) / 1000)
#define sleep(s)   Sleep((s) * 1000)

/* memmem - find subsequence in memory (not available on Windows) */
static inline void *memmem(const void *haystack, size_t haystack_len,
                           const void *needle, size_t needle_len) {
    if (needle_len == 0) return (void *)haystack;
    if (haystack_len < needle_len) return NULL;
    
    const unsigned char *h = (const unsigned char *)haystack;
    const unsigned char *n = (const unsigned char *)needle;
    const unsigned char *end = h + haystack_len - needle_len + 1;
    
    while (h < end) {
        if (*h == *n && memcmp(h, n, needle_len) == 0) {
            return (void *)h;
        }
        h++;
    }
    return NULL;
}

/* clock_gettime emulation for Windows */
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#define CLOCK_REALTIME  0

/* MSVC 2015+ (version 1900+) provides struct timespec in time.h
 * For older MSVC or MinGW without it, we define our own */
#if defined(_MSC_VER) && _MSC_VER >= 1900
    /* MSVC 2015+ has timespec - do nothing */
#elif !defined(_TIMESPEC_DEFINED) && !defined(__struct_timespec_defined) && !defined(__timespec_defined)
#define _TIMESPEC_DEFINED 1
#define __struct_timespec_defined 1
#define __timespec_defined 1
struct timespec {
    time_t tv_sec;
    long   tv_nsec;
};
#endif

static inline int clock_gettime(int clk_id, struct timespec *tp) {
    (void)clk_id;
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    tp->tv_sec = (time_t)(count.QuadPart / freq.QuadPart);
    tp->tv_nsec = (long)((count.QuadPart % freq.QuadPart) * 1000000000LL / freq.QuadPart);
    return 0;
}
#endif /* CLOCK_MONOTONIC */

/* Thread-local storage */
#define UFT_THREAD_LOCAL __declspec(thread)

/* Packed struct */
#define UFT_PACKED __pragma(pack(push, 1))
#define UFT_PACKED_END __pragma(pack(pop))

/* Function attributes */
#ifdef _MSC_VER
#define UFT_INLINE __forceinline
#define UFT_NOINLINE __declspec(noinline)
/* Disable specific MSVC warnings */
#pragma warning(disable: 4996)  /* deprecated POSIX names */
#pragma warning(disable: 4244)  /* conversion warnings */
#pragma warning(disable: 4267)  /* size_t to int */
#else
/* MinGW/GCC on Windows */
#define UFT_INLINE static inline __attribute__((always_inline))
#define UFT_NOINLINE __attribute__((noinline))
#endif

#else
/*===========================================================================*/
/*                         POSIX (Linux/macOS)                                */
/*===========================================================================*/

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/time.h>

/* Path separator */
#define UFT_PATH_SEP '/'
#define UFT_PATH_SEP_STR "/"

/* Thread-local storage */
#define UFT_THREAD_LOCAL __thread

/* Packed struct */
#define UFT_PACKED 
#define UFT_PACKED_END

/* Function attributes */
#define UFT_INLINE inline __attribute__((always_inline))
#define UFT_NOINLINE __attribute__((noinline))

/* O_BINARY doesn't exist on POSIX */
#ifndef O_BINARY
#define O_BINARY 0
#endif

#endif /* _WIN32 */

/*===========================================================================*/
/*                         COMMON DEFINITIONS                                 */
/*===========================================================================*/

/* Byte order detection */
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define UFT_BIG_ENDIAN 1
#else
#define UFT_BIG_ENDIAN 0
#endif

/* Byte swap macros */
#ifdef _WIN32
#include <stdlib.h>
#define uft_bswap16(x) _byteswap_ushort(x)
#define uft_bswap32(x) _byteswap_ulong(x)
#define uft_bswap64(x) _byteswap_uint64(x)
#elif defined(__GNUC__) || defined(__clang__)
#define uft_bswap16(x) __builtin_bswap16(x)
#define uft_bswap32(x) __builtin_bswap32(x)
#define uft_bswap64(x) __builtin_bswap64(x)
#else
static inline uint16_t uft_bswap16(uint16_t x) {
    return (x >> 8) | (x << 8);
}
static inline uint32_t uft_bswap32(uint32_t x) {
    return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
           ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
}
static inline uint64_t uft_bswap64(uint64_t x) {
    return ((uint64_t)uft_bswap32((uint32_t)x) << 32) |
           uft_bswap32((uint32_t)(x >> 32));
}
#endif

/* Endian conversion (host to little-endian and back) */
#if UFT_BIG_ENDIAN
#define uft_htole16(x) uft_bswap16(x)
#define uft_htole32(x) uft_bswap32(x)
#define uft_htole64(x) uft_bswap64(x)
#define uft_le16toh(x) uft_bswap16(x)
#define uft_le32toh(x) uft_bswap32(x)
#define uft_le64toh(x) uft_bswap64(x)
#define uft_htobe16(x) (x)
#define uft_htobe32(x) (x)
#define uft_htobe64(x) (x)
#define uft_be16toh(x) (x)
#define uft_be32toh(x) (x)
#define uft_be64toh(x) (x)
#else
#define uft_htole16(x) (x)
#define uft_htole32(x) (x)
#define uft_htole64(x) (x)
#define uft_le16toh(x) (x)
#define uft_le32toh(x) (x)
#define uft_le64toh(x) (x)
#define uft_htobe16(x) uft_bswap16(x)
#define uft_htobe32(x) uft_bswap32(x)
#define uft_htobe64(x) uft_bswap64(x)
#define uft_be16toh(x) uft_bswap16(x)
#define uft_be32toh(x) uft_bswap32(x)
#define uft_be64toh(x) uft_bswap64(x)
#endif

/* Safe string functions */
#ifdef _WIN32
#define uft_snprintf _snprintf
#define uft_vsnprintf _vsnprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#define uft_snprintf snprintf
#define uft_vsnprintf vsnprintf
#endif

/* Time functions */
#ifdef _WIN32
static inline int gettimeofday(struct timeval* tv, void* tz) {
    (void)tz;
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    uint64_t time = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    time -= 116444736000000000ULL;  /* Convert to Unix epoch */
    tv->tv_sec = (long)(time / 10000000);
    tv->tv_usec = (long)((time % 10000000) / 10);
    return 0;
}
#endif

#endif /* UFT_PLATFORM_H */
