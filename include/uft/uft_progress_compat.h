/**
 * @file uft_progress_compat.h
 * @brief Legacy-Progress-Callback Typ-Aliase.
 *
 * Stellt alte Callback-Typen als Aliase auf den kanonischen
 * uft_progress_fn bereit. Bestehender Code compiliert weiter,
 * bekommt aber Deprecation-Warnings.
 *
 * Ausnahme: Legacy-Typen mit abweichenden Signaturen (z.B. 3-Parameter-
 * Callbacks ohne Struct) bleiben hier als eigenständige Typen bestehen —
 * sie müssen manuell in Aufrufstellen zum neuen Typ geändert werden.
 *
 * Neuer Code inkludiert immer nur uft_progress.h, nie diese Datei.
 */
#ifndef UFT_PROGRESS_COMPAT_H
#define UFT_PROGRESS_COMPAT_H

#include "uft/uft_progress.h"
#include "uft/uft_compiler.h"   /* UFT_DEPRECATED */

#ifdef __cplusplus
extern "C" {
#endif

/* Bridge-Helper: alten Legacy-Callback-Typ wrappen in neue Struct */

typedef struct {
    void  *legacy_cb;     /* Pointer zum alten Callback */
    void  *legacy_ud;     /* User-Data für alten Callback */
} uft_progress_bridge_t;

/* Bridge-Funktion: 3-int-Signatur → uft_progress_fn */
static inline bool uft_progress_bridge_int3(const uft_progress_t *p, void *ud)
{
    typedef bool (*legacy_t)(int current, int total, const char *msg, void *ud);
    uft_progress_bridge_t *b = (uft_progress_bridge_t *)ud;
    if (!b || !b->legacy_cb) return true;
    legacy_t fn = (legacy_t)b->legacy_cb;
    return fn((int)p->current, (int)p->total, p->detail, b->legacy_ud);
}

/* Bridge-Funktion: void-return → uft_progress_fn (liefert immer true) */
static inline bool uft_progress_bridge_void_intint(const uft_progress_t *p, void *ud)
{
    typedef void (*legacy_t)(void *ud, int current, int total);
    uft_progress_bridge_t *b = (uft_progress_bridge_t *)ud;
    if (b && b->legacy_cb) {
        legacy_t fn = (legacy_t)b->legacy_cb;
        fn(b->legacy_ud, (int)p->current, (int)p->total);
    }
    return true;
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PROGRESS_COMPAT_H */
