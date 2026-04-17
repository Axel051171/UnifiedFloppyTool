/**
 * @file uft_capture.h
 * @brief Unified disk capture / restore over the HAL dispatcher.
 *
 * Phase P2b of the API consolidation. Sits on top of uft_hw_read_tracks
 * (HAL H1/H3) so any registered backend's batch fast-path or the generic
 * fallback can be selected transparently.
 *
 * This layer owns NO format coupling. It produces / consumes a
 * uft_disk_t populated with uft_track_t entries; format-specific
 * serialization stays in the format-plugin layer.
 */
#ifndef UFT_CAPTURE_H
#define UFT_CAPTURE_H

#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/uft_hardware.h"
#include "uft/uft_progress.h"
#include "uft/uft_format_plugin.h"  /* uft_track_t */

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int                      cyl_start;     /**< inclusive */
    int                      cyl_end;       /**< exclusive */
    int                      head_mask;     /**< 0x1=h0, 0x2=h1, 0x3=both */
    uint8_t                  revolutions;   /**< 0 = backend default */
    uft_unified_progress_fn  progress;      /**< optional */
    void                    *user_data;
} uft_capture_options_t;

typedef struct {
    uft_disk_t  *disk;              /**< populated on success, caller frees */
    size_t       tracks_attempted;
    size_t       tracks_succeeded;
    size_t       tracks_failed;
    uint64_t     total_bytes;       /**< sum of raw_size over all tracks */
    double       elapsed_seconds;
} uft_capture_result_t;

/**
 * @brief Capture a physical disk into a newly allocated uft_disk_t.
 *
 * Uses uft_hw_read_tracks() — the backend's native batch is used if it
 * exposes one, otherwise the generic per-track fallback runs.
 *
 * @param device Open HAL device.
 * @param opts   Must not be NULL.
 * @param out    Populated on success; caller must uft_capture_free() afterwards.
 * @return UFT_OK or error. On any error, out->disk is NULL.
 */
uft_error_t uft_capture_disk(uft_hw_device_t *device,
                              const uft_capture_options_t *opts,
                              uft_capture_result_t *out);

/**
 * @brief Restore a disk's tracks to a physical device.
 *
 * Loops over backend->write_track for the disk's tracks matching the
 * cylinder/head range in @p opts. Returns the first non-OK error.
 */
uft_error_t uft_restore_disk(uft_hw_device_t *device,
                              const uft_disk_t *disk,
                              const uft_capture_options_t *opts,
                              uft_capture_result_t *out);

/**
 * @brief Free a capture result's disk + internal arrays.
 */
void uft_capture_free(uft_capture_result_t *result);

#ifdef __cplusplus
}
#endif
#endif /* UFT_CAPTURE_H */
