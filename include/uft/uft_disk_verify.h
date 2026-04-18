/**
 * @file uft_disk_verify.h
 * @brief Disk-level verify — orchestrates per-track plugin verify hooks.
 *
 * Part of Master-API Phase 3. Nutzt uft_disk_stream_pair internally.
 */
#ifndef UFT_DISK_VERIFY_H
#define UFT_DISK_VERIFY_H

#include "uft/uft_disk_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Per-Track Verify Result
 * ============================================================================ */

typedef struct {
    int          cylinder;
    int          head;
    uft_error_t  status;              /* UFT_OK oder UFT_ERROR_VERIFY_FAILED */

    size_t       sectors_expected;
    size_t       sectors_matched;
    size_t       sectors_failed;
    size_t       sectors_missing;
    size_t       sectors_weak_skipped;
} uft_verify_track_result_t;

/* ============================================================================
 * Disk-Level Verify Result
 * ============================================================================ */

#ifndef UFT_VERIFY_RESULT_T_DEFINED
#define UFT_VERIFY_RESULT_T_DEFINED
typedef struct {
    uft_error_t  overall_status;

    size_t       tracks_total;
    size_t       tracks_ok;
    size_t       tracks_failed;

    size_t       sectors_total;
    size_t       sectors_ok;
    size_t       sectors_failed;
    size_t       sectors_weak;

    /* Pro-Track-Details (nur wenn options.detailed=true) */
    uft_verify_track_result_t *track_results;
    size_t       track_results_count;
    size_t       track_results_capacity;
} uft_verify_result_t;
#endif /* UFT_VERIFY_RESULT_T_DEFINED */

/* ============================================================================
 * Options
 * ============================================================================ */

typedef struct {
    uft_disk_op_options_t  base;

    bool                   detailed;        /* Pro-Track-Ergebnisse sammeln */
    bool                   verify_flux;     /* Flux-Daten auch vergleichen */
    bool                   tolerate_weak;   /* Weak-Bits ignorieren */
} uft_verify_options_t;

#define UFT_VERIFY_OPTIONS_DEFAULT \
    { UFT_DISK_OP_OPTIONS_DEFAULT, false, false, true }

/* ============================================================================
 * API
 * ============================================================================ */

/**
 * @brief Verifiziert ob Disk-Inhalt einer Referenz entspricht.
 *
 * Iteriert über alle Tracks, ruft pro Track plugin->verify_track auf
 * (oder fällt auf uft_generic_verify_track zurück).
 *
 * @return UFT_OK wenn identisch, UFT_ERROR_VERIFY_FAILED bei Unterschieden
 */
uft_error_t uft_disk_verify(uft_disk_t *disk,
                             uft_disk_t *reference,
                             const uft_verify_options_t *options,
                             uft_verify_result_t *result);

/**
 * @brief Self-Verify: liest jeden Track 2x und vergleicht.
 *
 * Findet intermittierende Lese-Fehler und Weak-Bit-Verhalten.
 */
uft_error_t uft_disk_verify_self(uft_disk_t *disk,
                                  const uft_verify_options_t *options,
                                  uft_verify_result_t *result);

/**
 * @brief Gibt Result-Struct frei.
 */
void uft_verify_result_free(uft_verify_result_t *result);

/**
 * @brief Exportiert als Text.
 */
uft_error_t uft_verify_result_to_text(const uft_verify_result_t *result,
                                        char *buffer, size_t buffer_size);

/**
 * @brief Exportiert als JSON.
 */
uft_error_t uft_verify_result_to_json(const uft_verify_result_t *result,
                                        char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_VERIFY_H */
