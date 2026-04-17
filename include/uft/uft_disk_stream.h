/**
 * @file uft_disk_stream.h
 * @brief Generic track/sector iteration over any Plugin-B disk.
 *
 * Provides callback-based streaming so that higher-level APIs
 * (verify, compare, convert, stats) can iterate uniformly over
 * any disk regardless of format.
 *
 * Part of Master-API specification (Phase 2 — foundation for
 * verify/compare/convert/stats).
 */
#ifndef UFT_DISK_STREAM_H
#define UFT_DISK_STREAM_H

#include "uft/uft_disk_api_types.h"
#include "uft/uft_format_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Visitor Callbacks
 * ============================================================================ */

/**
 * @brief Callback pro Track.
 *
 * Der Visitor empfängt den GELESENEN Track. Er darf den Track lesen;
 * wenn modify_allowed=true in options gesetzt ist, darf er ihn auch
 * modifizieren (wird dann am Ende des Visits via write_track zurückgeschrieben).
 *
 * @return UFT_OK = weitermachen
 *         UFT_ERROR_CANCELLED = Iteration beenden (kein Fehler)
 *         anderer Fehler-Code = Fehler, wird an on_error callback gemeldet
 */
typedef uft_error_t (*uft_track_visitor_t)(uft_disk_t *disk,
                                             int cyl, int head,
                                             uft_track_t *track,
                                             void *user_data);

/**
 * @brief Callback pro Sektor.
 */
typedef uft_error_t (*uft_sector_visitor_t)(uft_disk_t *disk,
                                              int cyl, int head,
                                              const uft_sector_t *sector,
                                              void *user_data);

/**
 * @brief Callback für paralleles Iterieren über zwei Disks.
 */
typedef uft_error_t (*uft_pair_visitor_t)(uft_disk_t *a, uft_disk_t *b,
                                            int cyl, int head,
                                            uft_track_t *track_a,
                                            uft_track_t *track_b,
                                            void *user_data);

/* ============================================================================
 * Iteration-Order
 * ============================================================================ */

typedef enum {
    UFT_ITER_LINEAR       = 0,   /* cyl 0/head 0, cyl 0/head 1, cyl 1/head 0, ... */
    UFT_ITER_BY_HEAD      = 1,   /* alle Tracks head 0, dann alle head 1 */
    UFT_ITER_INTERLEAVED  = 2,   /* ungerade vor gerade (Seek-Optimierung) */
} uft_iter_order_t;

/* ============================================================================
 * Stream Options
 * ============================================================================ */

typedef struct {
    uft_disk_op_options_t  base;

    uft_iter_order_t       order;
    bool                   modify_allowed;  /* Visitor darf Track modifizieren */
} uft_stream_options_t;

#define UFT_STREAM_OPTIONS_DEFAULT \
    { UFT_DISK_OP_OPTIONS_DEFAULT, UFT_ITER_LINEAR, false }

/* ============================================================================
 * Public API
 * ============================================================================ */

/**
 * @brief Iteriert über alle Tracks, ruft Visitor pro Track auf.
 *
 * Der Track wird via plugin->read_track gelesen, an den Visitor übergeben,
 * und danach freigegeben. Wenn options.modify_allowed=true gesetzt ist und
 * der Visitor UFT_OK zurückgibt, wird der Track via plugin->write_track
 * zurückgeschrieben (Disk darf nicht read_only sein).
 *
 * @param disk      Geöffnete Disk (plugin_data gesetzt)
 * @param visitor   Callback pro Track
 * @param options   Options (NULL = Defaults)
 * @param user_data wird an Visitor weitergereicht
 * @return UFT_OK bei Erfolg, UFT_ERROR_CANCELLED bei User-Abbruch
 */
uft_error_t uft_disk_stream_tracks(uft_disk_t *disk,
                                     uft_track_visitor_t visitor,
                                     const uft_stream_options_t *options,
                                     void *user_data);

/**
 * @brief Iteriert über alle Sektoren, ruft Visitor pro Sektor auf.
 *
 * Liest jeden Track einmal und iteriert über track->sectors[]. Der
 * Sektor-Visitor sieht read-only Sektoren (kein Write-Back).
 */
uft_error_t uft_disk_stream_sectors(uft_disk_t *disk,
                                      uft_sector_visitor_t visitor,
                                      const uft_stream_options_t *options,
                                      void *user_data);

/**
 * @brief Iteriert parallel über zwei Disks.
 *
 * Liest für jedes (cyl, head) beide Tracks, ruft Visitor mit beiden
 * Tracks. Funktioniert auch für unterschiedliche Formate — solange
 * die Geometrie kompatibel ist.
 *
 * @return UFT_ERROR_GEOMETRY_MISMATCH wenn cylinders/heads differieren
 */
uft_error_t uft_disk_stream_pair(uft_disk_t *disk_a, uft_disk_t *disk_b,
                                   uft_pair_visitor_t visitor,
                                   const uft_stream_options_t *options,
                                   void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_STREAM_H */
