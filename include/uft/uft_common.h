/**
 * @file uft_common.h
 * @brief UFT Common Includes - Bündelt alle häufig benötigten Header
 * 
 * VERWENDUNG:
 * Statt vieler einzelner Includes einfach:
 *   #include "uft_common.h"
 * 
 * Dies verhindert:
 * - Fehlende stdbool.h für 'bool'
 * - Fehlende stdint.h für uint8_t etc.
 * - MSVC atomic Probleme
 * - Legacy Error-Code Probleme
 */

#ifndef UFT_COMMON_H
#define UFT_COMMON_H

/*===========================================================================
 * Standard C Headers
 *===========================================================================*/

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * UFT Core Headers
 *===========================================================================*/

/* Error handling */
#include "uft_error.h"
#include "uft_error_compat.h"

/* Type definitions */
#include "uft_types.h"

/* Track/Sector types */
#include "uft_track.h"

/* Portable atomics (MSVC-kompatibel) */
#include "uft_atomics.h"

/*===========================================================================
 * Common Macros
 *===========================================================================*/

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* Unused parameter marker */
#ifndef UNUSED
#define UNUSED(x) (void)(x)
#endif

/* Static assertions */
#ifndef STATIC_ASSERT
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
    #define STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#else
    #define STATIC_ASSERT(cond, msg) typedef char static_assert_##msg[(cond)?1:-1]
#endif
#endif

/*===========================================================================
 * Compiler Compatibility
 *===========================================================================*/

/* Packed structs */
#ifdef _MSC_VER
    #define UFT_PACKED_BEGIN __pragma(pack(push, 1))
    #define UFT_PACKED_END   __pragma(pack(pop))
    #define UFT_PACKED
#else
    #define UFT_PACKED_BEGIN
    #define UFT_PACKED_END
    #define UFT_PACKED __attribute__((packed))
#endif

/* Alignment */
#ifdef _MSC_VER
    #define UFT_ALIGNED(n) __declspec(align(n))
#else
    #define UFT_ALIGNED(n) __attribute__((aligned(n)))
#endif

/* Inline */
#ifdef _MSC_VER
    #define UFT_INLINE __forceinline
#else
    #define UFT_INLINE static inline __attribute__((always_inline))
#endif

/* Restrict */
#ifdef _MSC_VER
    #define UFT_RESTRICT __restrict
#else
    #define UFT_RESTRICT __restrict__
#endif

/*===========================================================================
 * Diagnostic Structure (for TransWarp/FormatID modules)
 *===========================================================================*/

/**
 * @brief Simple diagnostic structure for detailed error messages
 * Used by newer modules (TransWarp, FormatID) for error reporting.
 */
typedef struct uft_diag {
    char msg[256];
} uft_diag_t;

/**
 * @brief Set diagnostic message
 */
static inline void uft_diag_set(uft_diag_t *d, const char *s)
{
    if (!d) return;
    if (!s) { d->msg[0] = '\0'; return; }
    size_t i = 0;
    for (; i + 1 < sizeof(d->msg) && s[i]; i++) d->msg[i] = s[i];
    d->msg[i] = '\0';
}

/**
 * @brief Clear diagnostic message
 */
static inline void uft_diag_clear(uft_diag_t *d)
{
    if (d) d->msg[0] = '\0';
}

#endif /* UFT_COMMON_H */
