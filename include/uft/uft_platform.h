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

/* ══════════════════════════════════════════════════════════════════════════ *
 * UFT_SKELETON_PARTIAL
 * PARTIALLY IMPLEMENTED — Root-level API
 *
 * This header declares 32 public functions; 27 are NOT implemented
 * in the source tree (only 5 have a definition). Callers exist
 * for some of the unimplemented prototypes, so this file is a live hazard:
 * compile passes but link may fail depending on call pattern.
 *
 * Status: tracked in docs/KNOWN_ISSUES.md under "Planned APIs".
 * Scope: see docs/MASTER_PLAN.md (M1/MF-011 IMPLEMENT-Welle).
 * Decision per function: IMPLEMENT (finish it), or DELETE prototype + all
 * call sites. Do NOT add new call sites until each prototype is resolved.
 * ══════════════════════════════════════════════════════════════════════════ */


#ifndef UFT_PLATFORM_H
#define UFT_PLATFORM_H

/* Include compatibility layer first for POSIX functions on Windows */
/* MF-455: die BASIS, nicht die Shims.
 *
 * Hier stand `#include "uft/compat/uft_platform.h"` mit dem Kommentar „Include
 * compatibility layer first for POSIX functions on Windows" — und bewirkte
 * nichts, weil compat denselben Guard UFT_PLATFORM_H trug wie diese Datei und
 * deshalb bei jedem Mal uebersprungen wurde. Die Absicht war nie wirksam.
 *
 * Jetzt kommt uft_platform_base.h: Plattform-/Compiler-Erkennung, Endianness,
 * UFT_PATH_SEP, UFT_THREAD_LOCAL, UFT_INLINE, POSIX-Konstanten — rund 40 Makros,
 * die bisher bei keinem Nutzer dieses Headers ankamen.
 *
 * Die POSIX-Shims (close/read/mkdir/...) bleiben in compat und damit Opt-in.
 * Sie baumweit sichtbar zu machen brach 2026-08-18 (MF-416) 187 von 197 Tests. */
#include "uft/compat/uft_platform_base.h"

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
    /* MF-431: no UFT_PLATFORM_UNKNOWN flag here. The name is already an enum
     * constant in uft_protection.h / uft_integration.h / protection/
     * uft_protection_classify.h, where it means "the DISK's target platform is
     * unknown" and is 0 — while this one meant "the HOST OS is not one we
     * recognise" and was 1. Nothing ever tested the OS flag (grep: zero
     * `defined(UFT_PLATFORM_UNKNOWN)`), but on an unrecognised build host it
     * would have silently turned every `= UFT_PLATFORM_UNKNOWN` in the
     * protection layer into ordinal 1. UFT_PLATFORM_NAME carries the same
     * information for the only purpose it had. */
    #define UFT_PLATFORM_NAME "Unknown"
#endif

/* POSIX-like systems */
#if defined(UFT_PLATFORM_LINUX) || defined(UFT_PLATFORM_MACOS) || defined(UFT_PLATFORM_FREEBSD)
    #define UFT_PLATFORM_POSIX 1
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Compiler Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Compiler identity lives in uft_compiler.h — one owner, one value (MF-428).
 * The block that used to stand here defined UFT_COMPILER_VERSION with the
 * patchlevel while uft_compiler.h defined it without; the #ifndef guard here
 * only decided who lost, it did not make the two agree. The patchlevel form
 * moved over there, so nothing is lost by including it. */
#include "uft/uft_compiler.h"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Architecture Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* MF-456: Architektur-Erkennung kommt aus uft/compat/uft_platform_base.h,
 * das dieser Header oben einbindet. Hier stand die zweite Fassung. */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Endianness
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* MF-455: UFT_BIG_ENDIAN und UFT_LITTLE_ENDIAN kommen aus
 * uft/compat/uft_platform_base.h und sind dort IMMER 0 oder 1.
 *
 * Hier stand `#define UFT_BIG_ENDIAN 1` auf Big-Endian und sonst
 * `#define UFT_LITTLE_ENDIAN 1` — der Name UFT_BIG_ENDIAN war auf Little-Endian
 * also gar nicht definiert, waehrend compat ihn dort auf 0 setzte.
 * `#ifdef UFT_BIG_ENDIAN` antwortete damit je nach gesehenem Header
 * entgegengesetzt. Latent, weil es im Baum kein `#ifdef` darauf gibt; jetzt
 * gegenstandslos, weil es nur noch eine Definition gibt. */

/* uft_bswap16/32/64 kommen aus uft/compat/uft_platform_base.h (MF-455).
 *
 * Hier stand eine zweite Fassung, nach UFT_COMPILER_GCC/MSVC unterschieden.
 * Der Fallback dort hatte ausserdem einen Fehler: uft_bswap64 rief
 * uft_bswap32(x) mit dem vollen uint64_t auf, ohne Cast auf uint32_t — die
 * oberen 32 Bit gingen in die Maskenrechnung ein. Die Basisfassung castet. */

/* Little-endian read/write.
 *
 * MF-455: `#if`, nicht `#ifdef`.
 *
 * Hier stand `#ifdef UFT_LITTLE_ENDIAN`, und der Name war bis MF-455 nur auf
 * Little-Endian ueberhaupt definiert — die Abfrage war also zufaellig richtig.
 * Seit die Basis ihn IMMER setzt (0 oder 1), waere `#ifdef` immer wahr und
 * uft_le16() auf einer Big-Endian-Maschine die Identitaet, also falsch.
 * scripts/platform_header_gate.py hat genau diese Stelle gefunden. */
#if UFT_LITTLE_ENDIAN
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

/* MF-451: hier standen UFT_PACKED/UFT_PACK_BEGIN/UFT_PACK_END ein weiteres
 * Mal — unerreichbar, weil dieser Header zehn Zeilen weiter oben
 * uft_compiler.h einbindet, das die Namen vorher definiert, und der ganze
 * Block hinter `#ifndef UFT_PACKED` stand. Eine tote Definition, die beim
 * Lesen wie die maßgebliche aussah. Packing kommt aus uft_packed.h. */
#include "uft/uft_packed.h"

/* MF-456: UFT_ALIGNED kommt aus uft_compiler.h, UFT_CACHE_LINE_SIZE aus
 * uft/compat/uft_platform_base.h (dort architekturabhaengig).
 *
 * Hier stand beides ein weiteres Mal, das Cache-Line-Mass sogar UNGESCHUETZT
 * auf 64 — auf ARM32, wo die Basis 32 kennt, haette diese Zeile den richtigen
 * Wert ueberschrieben. */

/* Page size (typical) */
#define UFT_PAGE_SIZE 4096

/* ═══════════════════════════════════════════════════════════════════════════════
 * Export/Import Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_API
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
#endif /* UFT_API */

/* ═══════════════════════════════════════════════════════════════════════════════
 * POSIX Timer Compatibility (Windows fallback)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* MF-435. What stood here was:
 *
 *     #ifndef CLOCK_MONOTONIC
 *     #define CLOCK_MONOTONIC 1
 *     #include <time.h>
 *     static inline int uft_clock_gettime(int clk, struct timespec *ts) {
 *         time_t t = time(NULL);
 *         ts->tv_sec = t;
 *         ts->tv_nsec = 0;          // one-SECOND resolution
 *         return 0;
 *     }
 *     #define clock_gettime uft_clock_gettime
 *     #endif
 *
 * Three things were wrong with it.
 *
 * It replaced a function that WORKS. MinGW-w64 ships a real clock_gettime
 * with 100 ns granularity — measured, not assumed. The shim was installed
 * anyway.
 *
 * Whether it was installed depended on INCLUDE ORDER. The guard tested
 * CLOCK_MONOTONIC, which <time.h> defines; a translation unit that included
 * <time.h> first kept the real clock, one that reached this header first got
 * the shim. Two files in the tree, two different clocks.
 *
 * And the replacement reported whole seconds. Every elapsed-time measurement
 * shorter than a second came out as exactly 0. src/core/uft_capture.c times
 * disk captures with it and fills uft_capture_result_t::elapsed_seconds — so
 * that field has been quantised to whole seconds on Windows. It is also why
 * this project had no benchmarks: timing inside UFT silently did not work.
 *
 * A clock that reports zero elapsed time is the timing equivalent of invented
 * data, which the first principle forbids. Nothing is substituted here now:
 * every toolchain this project builds with — GCC, Clang, MinGW-w64 — provides
 * clock_gettime. A toolchain that does not will fail to link, loudly, which is
 * the correct outcome and far better than a silent one-second clock.
 */

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






/* ═══════════════════════════════════════════════════════════════════════════════
 * File System
 * ═══════════════════════════════════════════════════════════════════════════════ */



/**
 * @brief Get file size
 */
int64_t uft_file_size(const char *path);





/* ═══════════════════════════════════════════════════════════════════════════════
 * High Resolution Timing
 * ═══════════════════════════════════════════════════════════════════════════════ */






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









/* ═══════════════════════════════════════════════════════════════════════════════
 * Thread Primitives
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Mutex handle
 */
typedef struct uft_mutex uft_mutex_t;






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



#ifdef __cplusplus
}
#endif

#endif /* UFT_PLATFORM_H */
