/**
 * @file uft_disk.h
 * @brief Disk Handle Definition
 * 
 * P0-002: Complete disk structure with writer backend support
 */

#ifndef UFT_DISK_H
#define UFT_DISK_H

/*
 * NOTE (Phase 1 consolidation): The struct uft_disk definition has moved
 * to uft_format_plugin.h. That version is now the single canonical
 * definition with all legacy fields (image_data, tracks, reader_backend,
 * hw_provider, etc.) merged in. This header remains for API compatibility
 * and re-exports the struct + legacy API declarations.
 */

#include "uft_types.h"
#include "uft_error.h"
#include "uft_format_plugin.h"   /* provides struct uft_disk */

#ifdef __cplusplus
extern "C" {
#endif

/* Writer-backend forward decl kept for callers that still reference it */
typedef struct uft_writer_backend uft_writer_backend_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Disk API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create new disk handle
 */
uft_disk_t* uft_disk_create(void);

/* NOTE: the 3-arg form `uft_error_t uft_disk_open(uft_disk_t*, const char*,
 * bool)` used to live here but conflicted with the canonical handle-returning
 * form in uft_core.h:
 *
 *     uft_disk_t* uft_disk_open(const char *path, bool read_only);
 *
 * The canonical form wins (6 callers vs 1). Include uft/uft_core.h for it.
 * This header retains only the non-conflicting entries below. */

/**
 * @brief Close disk
 */
void uft_disk_close(uft_disk_t *disk);

/**
 * @brief Free disk handle — DEKLARIERT, NICHT DEFINIERT (MF-1158)
 *
 * Hier stand `uft_disk_free(uft_disk_t *)`, und der Name kollidierte mit
 * `uft_disk_free(uft_disk_image_t *)` in `core/uft_unified_types.h`, das
 * als einziges eine Definition hat (`uft_unified_types.c:455`). Der
 * C-Binder kennt keine Parametertypen: wer diesen Header einbindet und
 * `uft_disk_free()` ruft, bindet an die Fassung fuer den ANDEREN Typ und
 * gibt einen Zeiger frei, den die Funktion als etwas anderes
 * dereferenziert. `uft_disk_t` ist gemessen ein UNDURCHSICHTIGER Typ
 * (`uft_types.h:34`), also gerade nicht dasselbe struct.
 *
 * Umbenannt statt entfernt (MF-1077). Zwei Dinge gehoeren dazu gesagt:
 *   - Fuer DIESEN Namen gibt es im ganzen Baum keine Definition und
 *     keinen Aufrufer. Er verspricht etwas, das es nicht gibt.
 *   - Inhaltlich tut er dasselbe wie `uft_disk_close()` fuenf Zeilen
 *     hoeher. Ob er ueberhaupt bleiben soll, ist eine
 *     Eigentuemer-Entscheidung (P3-413) — nicht meine.
 */
void uft_disk_handle_free(uft_disk_t *disk);

/**
 * @brief Get disk geometry
 */
uft_error_t uft_disk_get_geometry(const uft_disk_t *disk, uft_geometry_t *geom);




#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_H */
