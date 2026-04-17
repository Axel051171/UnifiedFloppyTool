/**
 * @file uft_disk_api_types.h
 * @brief Shared types for the UFT Disk Orchestrator API
 *
 * Provides progress/error callbacks and common option struct used by
 * uft_disk_verify, uft_disk_compare, uft_disk_convert, uft_disk_stats,
 * uft_disk_stream, uft_disk_transaction, uft_disk_batch.
 *
 * Part of Master-API specification (Phase 1).
 */
#ifndef UFT_DISK_API_TYPES_H
#define UFT_DISK_API_TYPES_H

#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Progress & Error Callbacks
 * ============================================================================ */

/**
 * @brief Progress-Report für langlaufende Operationen.
 *
 * @param current   Aktueller Fortschritt (z.B. Track-Nummer)
 * @param total     Gesamtumfang
 * @param message   Optional human-readable Status (kann NULL sein)
 * @param user_data Weitergereicht an Callback
 * @return true wenn weitermachen, false wenn abbrechen
 */
typedef bool (*uft_progress_cb)(size_t current, size_t total,
                                 const char *message, void *user_data);

/**
 * @brief Error-Report für nicht-abbrechende Fehler.
 *
 * Wird aufgerufen wenn z.B. ein einzelner Sektor nicht lesbar ist,
 * die Operation aber weiterläuft (continue_on_error=true).
 */
typedef void (*uft_error_cb)(uft_error_t err, int cyl, int head,
                              const char *detail, void *user_data);

/* ============================================================================
 * Common Operation Options
 * ============================================================================ */

/**
 * @brief Gemeinsame Options für alle API-Module.
 *
 * Module erweitern diese Struct um modulspezifische Felder.
 */
typedef struct uft_disk_op_options {
    uft_progress_cb   on_progress;
    uft_error_cb      on_error;
    void             *user_data;

    /* Verhalten bei Fehlern */
    bool              continue_on_error;   /* false = abort beim ersten Fehler */
    int               max_retries;         /* 0 = kein Retry */

    /* Scope — Teilbereich der Disk bearbeiten */
    int               cyl_start;           /* -1 = alle */
    int               cyl_end;             /* -1 = bis Ende */
    int               head_filter;         /* -1 = alle Köpfe */
} uft_disk_op_options_t;

/**
 * @brief Default-Options — alle Tracks, abort bei Fehler, kein Retry.
 *
 * Verwendung:
 *   uft_disk_op_options_t opts = UFT_DISK_OP_OPTIONS_DEFAULT;
 */
#define UFT_DISK_OP_OPTIONS_DEFAULT \
    { NULL, NULL, NULL, false, 0, -1, -1, -1 }

/**
 * @brief Initialisiert Options-Struct mit Defaults (für Heap-Allocated).
 */
static inline void uft_disk_op_options_init(uft_disk_op_options_t *opts) {
    if (!opts) return;
    opts->on_progress = NULL;
    opts->on_error = NULL;
    opts->user_data = NULL;
    opts->continue_on_error = false;
    opts->max_retries = 0;
    opts->cyl_start = -1;
    opts->cyl_end = -1;
    opts->head_filter = -1;
}

/**
 * @brief Berechnet den tatsächlichen cyl_start/cyl_end basierend auf Options.
 *
 * @param disk_cyls Anzahl Zylinder der Disk
 * @param opts      Options (NULL = Default)
 * @param out_start [out] Effective cyl_start (0 wenn -1 in opts)
 * @param out_end   [out] Effective cyl_end (disk_cyls wenn -1 in opts)
 */
static inline void uft_disk_op_options_resolve_range(
    int disk_cyls,
    const uft_disk_op_options_t *opts,
    int *out_start, int *out_end)
{
    int start = (opts && opts->cyl_start >= 0) ? opts->cyl_start : 0;
    int end   = (opts && opts->cyl_end >= 0)   ? opts->cyl_end   : disk_cyls;
    if (start < 0) start = 0;
    if (end > disk_cyls) end = disk_cyls;
    if (end < start) end = start;
    if (out_start) *out_start = start;
    if (out_end) *out_end = end;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_API_TYPES_H */
