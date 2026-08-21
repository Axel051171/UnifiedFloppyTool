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
 * MSVC Compatibility - MUST BE FIRST
 *===========================================================================*/
#ifdef _MSC_VER
    /* MSVC doesn't support __attribute__, disable it */
    #ifndef __attribute__
        #define __attribute__(x)
    #endif
#endif

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
#include "uft/uft_error.h"

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

/* MF-451: das war die gefährlichste der sieben Definitionen.
 *
 * Auf GCC definierte sie UFT_PACKED_BEGIN und UFT_PACKED_END **leer** — wer
 * dieses Paar benutzte und diesen Header zuerst sah, bekam gar kein Packing,
 * während `UFT_PACKED` gleichzeitig `__attribute__((packed))` war. Zwei
 * Schreibweisen desselben Zwecks, in derselben Datei, mit gegensätzlicher
 * Wirkung. Der Kommentar „only define if not already defined by uft_packed.h"
 * beschreibt genau die Abhängigkeit von der Include-Reihenfolge, die das
 * Problem ist.
 *
 * Packing kommt aus uft_packed.h, dort sind UFT_PACKED_BEGIN/_END Aliase des
 * echten pragma-Paars. */
#include "uft/uft_packed.h"

/* Alignment */
/* MF-456: UFT_ALIGNED, UFT_INLINE und UFT_RESTRICT standen hier ein
 * weiteres Mal, hinter #ifndef — also abhaengig von der
 * Include-Reihenfolge. UFT_INLINE hiess hier
 * `static inline __attribute__((always_inline))`, in uft_compiler.h
 * `static inline`. Eigentuemer ist uft_compiler.h. */
#include "uft/uft_compiler.h"

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
