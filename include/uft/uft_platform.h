/**
 * @file uft_platform.h
 * @brief Cross-Platform Abstraction Layer
 * 
 * P2-005: Cross-Platform Support
 * 
 * Provides unified API for:
 * - File system operations
 * - Serial port access
 * - Memory alignment
 * - Endianness handling
 * - Thread primitives
 * - High-resolution timing
 * - Path handling
 */

#ifndef UFT_PLATFORM_H
#define UFT_PLATFORM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_PLATFORM_WINDOWS 1
    #define UFT_PLATFORM_NAME "Windows"
#elif defined(__APPLE__) && defined(__MACH__)
    #include <TargetConditionals.h>
    #if TARGET_OS_MAC
        #define UFT_PLATFORM_MACOS 1
        #define UFT_PLATFORM_NAME "macOS"
    #endif
#elif defined(__linux__)
    #define UFT_PLATFORM_LINUX 1
    #define UFT_PLATFORM_NAME "Linux"
#elif defined(__FreeBSD__)
    #define UFT_PLATFORM_FREEBSD 1
    #define UFT_PLATFORM_NAME "FreeBSD"
#else
    #define UFT_PLATFORM_UNKNOWN 1
    #define UFT_PLATFORM_NAME "Unknown"
#endif

/* POSIX-like systems */
#if defined(UFT_PLATFORM_LINUX) || defined(UFT_PLATFORM_MACOS) || defined(UFT_PLATFORM_FREEBSD)
    #define UFT_PLATFORM_POSIX 1
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compiler Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(_MSC_VER)
    #define UFT_COMPILER_MSVC 1
    #define UFT_COMPILER_NAME "MSVC"
    #define UFT_COMPILER_VERSION _MSC_VER
#elif defined(__clang__)
    #define UFT_COMPILER_CLANG 1
    #define UFT_COMPILER_NAME "Clang"
    #define UFT_COMPILER_VERSION (__clang_major__ * 10000 + __clang_minor__ * 100 + __clang_patchlevel__)
#elif defined(__GNUC__)
    #define UFT_COMPILER_GCC 1
    #define UFT_COMPILER_NAME "GCC"
    #define UFT_COMPILER_VERSION (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)
#else
    #define UFT_COMPILER_UNKNOWN 1
    #define UFT_COMPILER_NAME "Unknown"
    #define UFT_COMPILER_VERSION 0
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Architecture Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(__x86_64__) || defined(_M_X64)
    #define UFT_ARCH_X64 1
    #define UFT_ARCH_NAME "x86_64"
    #define UFT_ARCH_BITS 64
#elif defined(__i386__) || defined(_M_IX86)
    #define UFT_ARCH_X86 1
    #define UFT_ARCH_NAME "x86"
    #define UFT_ARCH_BITS 32
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define UFT_ARCH_ARM64 1
    #define UFT_ARCH_NAME "ARM64"
    #define UFT_ARCH_BITS 64
#elif defined(__arm__) || defined(_M_ARM)
    #define UFT_ARCH_ARM32 1
    #define UFT_ARCH_NAME "ARM32"
    #define UFT_ARCH_BITS 32
#else
    #define UFT_ARCH_UNKNOWN 1
    #define UFT_ARCH_NAME "Unknown"
    #define UFT_ARCH_BITS 0
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Endianness
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define UFT_BIG_ENDIAN 1
#else
    #define UFT_LITTLE_ENDIAN 1
#endif

/* Byte swap macros */
#ifdef UFT_COMPILER_GCC
    #define uft_bswap16(x) __builtin_bswap16(x)
    #define uft_bswap32(x) __builtin_bswap32(x)
    #define uft_bswap64(x) __builtin_bswap64(x)
#elif defined(UFT_COMPILER_MSVC)
    #include <stdlib.h>
    #define uft_bswap16(x) _byteswap_ushort(x)
    #define uft_bswap32(x) _byteswap_ulong(x)
    #define uft_bswap64(x) _byteswap_uint64(x)
#else
    static inline uint16_t uft_bswap16(uint16_t x) {
        return (x >> 8) | (x << 8);
    }
    static inline uint32_t uft_bswap32(uint32_t x) {
        return ((x >> 24) & 0xFF) | ((x >> 8) & 0xFF00) |
               ((x << 8) & 0xFF0000) | ((x << 24) & 0xFF000000);
    }
    static inline uint64_t uft_bswap64(uint64_t x) {
        return ((uint64_t)uft_bswap32(x) << 32) | uft_bswap32(x >> 32);
    }
#endif

/* Little-endian read/write */
#ifdef UFT_LITTLE_ENDIAN
    #define uft_le16(x) (x)
    #define uft_le32(x) (x)
    #define uft_le64(x) (x)
    #define uft_be16(x) uft_bswap16(x)
    #define uft_be32(x) uft_bswap32(x)
    #define uft_be64(x) uft_bswap64(x)
#else
    #define uft_le16(x) uft_bswap16(x)
    #define uft_le32(x) uft_bswap32(x)
    #define uft_le64(x) uft_bswap64(x)
    #define uft_be16(x) (x)
    #define uft_be32(x) (x)
    #define uft_be64(x) (x)
#endif

/* Guard against redefinition */
#ifndef UFT_ENDIAN_FUNCTIONS_DEFINED
#define UFT_ENDIAN_FUNCTIONS_DEFINED
/* Unaligned access */
static inline uint16_t uft_read_le16(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return (uint16_t)b[0] | ((uint16_t)b[1] << 8);
}

static inline uint32_t uft_read_le32(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
           ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static inline uint16_t uft_read_be16(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return ((uint16_t)b[0] << 8) | (uint16_t)b[1];
}

static inline uint32_t uft_read_be32(const void *p) {
    const uint8_t *b = (const uint8_t*)p;
    return ((uint32_t)b[0] << 24) | ((uint32_t)b[1] << 16) |
           ((uint32_t)b[2] << 8) | (uint32_t)b[3];
}

static inline void uft_write_le16(void *p, uint16_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
}

static inline void uft_write_le32(void *p, uint32_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = v & 0xFF;
    b[1] = (v >> 8) & 0xFF;
    b[2] = (v >> 16) & 0xFF;
    b[3] = (v >> 24) & 0xFF;
}

static inline void uft_write_be16(void *p, uint16_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = (v >> 8) & 0xFF;
    b[1] = v & 0xFF;
}

static inline void uft_write_be32(void *p, uint32_t v) {
    uint8_t *b = (uint8_t*)p;
    b[0] = (v >> 24) & 0xFF;
    b[1] = (v >> 16) & 0xFF;
    b[2] = (v >> 8) & 0xFF;
    b[3] = v & 0xFF;
}

#endif /* UFT_ENDIAN_FUNCTIONS_DEFINED */
/* ═══════════════════════════════════════════════════════════════════════════════
 * Alignment & Memory
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_COMPILER_MSVC
    #define UFT_ALIGNED(n) __declspec(align(n))
    #define UFT_PACKED
    #pragma pack(push, 1)
    #define UFT_PACK_BEGIN
    #define UFT_PACK_END
    #pragma pack(pop)
#else
    #define UFT_ALIGNED(n) __attribute__((aligned(n)))
    #define UFT_PACKED __attribute__((packed))
    #define UFT_PACK_BEGIN
    #define UFT_PACK_END
#endif

/* Cache line size (typical) */
#define UFT_CACHE_LINE_SIZE 64

/* Page size (typical) */
#define UFT_PAGE_SIZE 4096

/* ═══════════════════════════════════════════════════════════════════════════════
 * Export/Import Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS
    #ifdef UFT_BUILDING_DLL
        #define UFT_API __declspec(dllexport)
    #elif defined(UFT_USING_DLL)
        #define UFT_API __declspec(dllimport)
    #else
        #define UFT_API
    #endif
#else
    #ifdef UFT_BUILDING_SHARED
        #define UFT_API __attribute__((visibility("default")))
    #else
        #define UFT_API
    #endif
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Path Handling
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifdef UFT_PLATFORM_WINDOWS
    #define UFT_PATH_SEPARATOR '\\'
    #define UFT_PATH_SEPARATOR_STR "\\"
    #define UFT_PATH_MAX 260
#else
    #define UFT_PATH_SEPARATOR '/'
    #define UFT_PATH_SEPARATOR_STR "/"
    #define UFT_PATH_MAX 4096
#endif

/**
 * @brief Normalize path separators
 */
void uft_path_normalize(char *path);

/**
 * @brief Join path components
 */
int uft_path_join(char *dest, size_t dest_size, const char *base, const char *rel);

/**
 * @brief Get file extension
 */
const char* uft_path_extension(const char *path);

/**
 * @brief Get base name (filename without directory)
 */
const char* uft_path_basename(const char *path);

/**
 * @brief Get directory part
 */
int uft_path_dirname(const char *path, char *dir, size_t dir_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * File System
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Check if file exists
 */
bool uft_file_exists(const char *path);

/**
 * @brief Check if directory exists
 */
bool uft_dir_exists(const char *path);

/**
 * @brief Get file size
 */
int64_t uft_file_size(const char *path);

/**
 * @brief Create directory (with parents)
 */
int uft_mkdir_p(const char *path);

/**
 * @brief Get user home directory
 */
int uft_get_home_dir(char *path, size_t path_size);

/**
 * @brief Get application data directory
 */
int uft_get_app_data_dir(char *path, size_t path_size, const char *app_name);

/**
 * @brief Get temp directory
 */
int uft_get_temp_dir(char *path, size_t path_size);

/* ═══════════════════════════════════════════════════════════════════════════════
 * High Resolution Timing
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief High-resolution timestamp (nanoseconds)
 */
uint64_t uft_time_ns(void);

/**
 * @brief High-resolution timestamp (microseconds)
 */
uint64_t uft_time_us(void);

/**
 * @brief High-resolution timestamp (milliseconds)
 */
uint64_t uft_time_ms(void);

/**
 * @brief Sleep for specified milliseconds
 */
void uft_sleep_ms(uint32_t ms);

/**
 * @brief Sleep for specified microseconds
 */
void uft_sleep_us(uint32_t us);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Serial Port
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Serial port handle
 */
typedef struct uft_serial uft_serial_t;

/**
 * @brief Serial port configuration
 */
typedef struct {
    uint32_t baud_rate;     /* 9600, 115200, etc. */
    uint8_t data_bits;      /* 5, 6, 7, 8 */
    uint8_t stop_bits;      /* 1, 2 */
    char parity;            /* 'N', 'E', 'O' */
    bool flow_control;      /* Hardware flow control */
    uint32_t timeout_ms;    /* Read timeout */
} uft_serial_config_t;

#define UFT_SERIAL_CONFIG_DEFAULT { \
    .baud_rate = 115200, \
    .data_bits = 8, \
    .stop_bits = 1, \
    .parity = 'N', \
    .flow_control = false, \
    .timeout_ms = 1000 \
}

/**
 * @brief Open serial port
 */
uft_serial_t* uft_serial_open(const char *port, const uft_serial_config_t *config);

/**
 * @brief Close serial port
 */
void uft_serial_close(uft_serial_t *serial);

/**
 * @brief Read from serial port
 */
int uft_serial_read(uft_serial_t *serial, void *buffer, size_t size);

/**
 * @brief Write to serial port
 */
int uft_serial_write(uft_serial_t *serial, const void *buffer, size_t size);

/**
 * @brief Flush serial port buffers
 */
int uft_serial_flush(uft_serial_t *serial);

/**
 * @brief Set serial timeout
 */
int uft_serial_set_timeout(uft_serial_t *serial, uint32_t timeout_ms);

/**
 * @brief Enumerate available serial ports
 */
int uft_serial_enumerate(char **ports, int max_ports);

/**
 * @brief Free enumerated ports list
 */
void uft_serial_free_list(char **ports, int count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Thread Primitives
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Mutex handle
 */
typedef struct uft_mutex uft_mutex_t;

/**
 * @brief Create mutex
 */
uft_mutex_t* uft_mutex_create(void);

/**
 * @brief Destroy mutex
 */
void uft_mutex_destroy(uft_mutex_t *mutex);

/**
 * @brief Lock mutex
 */
void uft_mutex_lock(uft_mutex_t *mutex);

/**
 * @brief Try lock mutex (non-blocking)
 */
bool uft_mutex_trylock(uft_mutex_t *mutex);

/**
 * @brief Unlock mutex
 */
void uft_mutex_unlock(uft_mutex_t *mutex);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Platform Info
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Platform information
 */
typedef struct {
    const char *os_name;
    const char *os_version;
    const char *arch_name;
    const char *compiler_name;
    int compiler_version;
    int cpu_count;
    uint64_t total_memory;
    bool is_little_endian;
} uft_platform_info_t;

/**
 * @brief Get platform information
 */
void uft_platform_get_info(uft_platform_info_t *info);

/**
 * @brief Print platform info to stdout
 */
void uft_platform_print_info(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PLATFORM_H */
