/**
 * @file uft_progress.h
 * @brief Einheitlicher Progress-Callback für UFT.
 *
 * Ersetzt die 25 parallelen Legacy-Callback-Typen (uft_batch_progress_cb,
 * uft_gw_progress_cb, uft_hal_progress_cb, ...). Diese bleiben mit
 * UFT_DEPRECATED-Marker funktional, neuer Code nutzt nur noch diesen Header.
 *
 * Typischer Einsatz:
 *
 *   static bool my_cb(const uft_progress_t *p, void *ud) {
 *       fprintf(stderr, "\r%s: %zu/%zu", p->stage, p->current, p->total);
 *       return true;  // weitermachen
 *   }
 *   uft_progress_report(my_cb, NULL, 5, 100, "verifying");
 */
#ifndef UFT_PROGRESS_H
#define UFT_PROGRESS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kanonische Progress-Info-Struct.
 *
 * Alle neuen Module füllen diese Struct; Module-spezifische Daten gehen
 * in @ref extra.
 */
typedef struct {
    size_t        current;          /* aktueller Fortschritt */
    size_t        total;            /* Gesamtumfang (0 = unbekannt) */
    const char   *stage;            /* "reading" / "writing" / "verifying" */
    const char   *detail;           /* optional: "Track 35 Head 0" */
    double        throughput;       /* optional: ops/s oder bytes/s (0 = n/a) */
    double        eta_seconds;      /* optional: -1.0 = unbekannt */
    void         *extra;            /* Modul-spezifisch, kann NULL sein */
} uft_progress_t;

/**
 * @brief Kanonischer Progress-Callback (neue Variante).
 *
 * Heißt "uft_unified_progress_fn" um Kollisionen mit den 5 existierenden
 * (alten, inkompatiblen) uft_progress_fn Typdefinitionen zu vermeiden.
 *
 * @param p Progress-Info (lifetime: nur während dieser Aufruf)
 * @param user_data vom Aufrufer weitergereicht
 * @return true = weitermachen, false = User will abbrechen
 */
typedef bool (*uft_unified_progress_fn)(const uft_progress_t *p, void *user_data);

/**
 * @brief Kurzform — füllt Struct und ruft Callback.
 *
 * @return true wenn Callback NULL oder weitermachen will
 */
static inline bool uft_progress_report(uft_unified_progress_fn fn, void *ud,
                                         size_t current, size_t total,
                                         const char *stage)
{
    if (!fn) return true;
    uft_progress_t p = { current, total, stage, NULL, 0.0, -1.0, NULL };
    return fn(&p, ud);
}

/**
 * @brief Wie uft_progress_report, aber mit Detail-Text.
 */
static inline bool uft_progress_report_detail(uft_unified_progress_fn fn, void *ud,
                                                size_t current, size_t total,
                                                const char *stage,
                                                const char *detail)
{
    if (!fn) return true;
    uft_progress_t p = { current, total, stage, detail, 0.0, -1.0, NULL };
    return fn(&p, ud);
}

#ifdef __cplusplus
}
#endif
#endif /* UFT_PROGRESS_H */
