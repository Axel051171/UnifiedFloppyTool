/**
 * @file uft_disc_diagnostics.h
 * @brief Disc diagnostics — surface scan, bad-sector detection,
 *        head-alignment check, write/verify test, performance measurement.
 *
 * Restored from UFT v3.7.0 EXT3-017. The v3.7 header was a single
 * forward-declaration stub; this header carries the full API that the
 * implementation in src/diag/uft_disc_diagnostics.c requires.
 *
 * All operations take caller-supplied read/write function pointers
 * rather than owning the device — this keeps the diagnostics module
 * HAL-agnostic.
 */

#ifndef UFT_DISC_DIAGNOSTICS_H
#define UFT_DISC_DIAGNOSTICS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Callbacks
 *===========================================================================*/

typedef int (*uft_diag_read_fn)(int track, int side, int sector,
                                  uint8_t *buffer, size_t sector_size,
                                  void *user_data);

typedef int (*uft_diag_write_fn)(int track, int side, int sector,
                                   const uint8_t *buffer, size_t sector_size,
                                   void *user_data);

/* Forward declare so progress_fn can mention track result. */
struct uft_diag_track_result;

typedef void (*uft_diag_progress_fn)(int progress,
                                       const struct uft_diag_track_result *current,
                                       void *user_data);

/*===========================================================================
 * Configuration + per-track results
 *===========================================================================*/

typedef struct {
    int  tracks;        /**< Default 80 */
    int  sides;         /**< Default 2 */
    int  sectors;       /**< Default 18 (per track) */
    int  sector_size;   /**< Default 512 */
    int  retries;       /**< Default 3 */
    bool verbose;
} uft_diag_config_t;

typedef enum {
    UFT_SECTOR_UNKNOWN = 0,
    UFT_SECTOR_GOOD    = 1,
    UFT_SECTOR_WEAK    = 2,
    UFT_SECTOR_BAD     = 3
} uft_sector_status_t;

#define UFT_DIAG_MAX_SECTORS_PER_TRACK 36

typedef struct uft_diag_track_result {
    int    track;
    int    side;
    int    bad_sectors;
    int    weak_sectors;
    int    read_errors;
    double avg_read_time_us;
    double quality;                  /**< percent 0..100 */
    uint8_t sector_status[UFT_DIAG_MAX_SECTORS_PER_TRACK];
} uft_diag_track_result_t;

typedef enum {
    UFT_BAD_UNKNOWN     = 0,
    UFT_BAD_READ_ERROR  = 1,
    UFT_BAD_CRC_ERROR   = 2,
    UFT_BAD_SECTOR_NF   = 3,   /**< sector not found */
    UFT_BAD_WEAK        = 4
} uft_bad_sector_type_t;

typedef struct {
    int                    track;
    int                    side;
    int                    sector;
    uft_bad_sector_type_t  type;
} uft_bad_sector_t;

typedef enum {
    UFT_ALIGN_UNKNOWN = 0,
    UFT_ALIGN_GOOD    = 1,
    UFT_ALIGN_FAIR    = 2,
    UFT_ALIGN_POOR    = 3,
    UFT_ALIGN_BAD     = 4
} uft_alignment_status_t;

typedef struct {
    uft_alignment_status_t status;
    double                 timing_deviation;   /**< percent */
    int                    error_count;
    char                   message[128];
} uft_alignment_info_t;

typedef enum {
    UFT_DIAG_NONE = 0,
    UFT_DIAG_SURFACE_SCAN,
    UFT_DIAG_BAD_SECTOR_MAP,
    UFT_DIAG_HEAD_ALIGNMENT,
    UFT_DIAG_WRITE_VERIFY,
    UFT_DIAG_PERFORMANCE
} uft_diag_test_type_t;

/*===========================================================================
 * Context
 *===========================================================================*/

typedef struct {
    uft_diag_config_t          config;

    int                         total_sectors;
    int                         good_sectors;
    int                         weak_sectors;
    int                         bad_sectors;

    uft_diag_track_result_t    *track_results;   /**< tracks × sides */

    time_t                      start_time;
    time_t                      end_time;
    bool                        completed;
    uft_diag_test_type_t        test_type;

    uft_diag_progress_fn        progress_fn;
    void                       *progress_data;
} uft_diag_ctx_t;

typedef struct {
    double sequential_kbps;   /**< Sequential read throughput */
    double random_kbps;       /**< Random-access throughput */
    double avg_seek_ms;       /**< Average seek time in milliseconds */
} uft_perf_result_t;

/*===========================================================================
 * Functions
 *===========================================================================*/

int  uft_diag_init(uft_diag_ctx_t *ctx, uft_diag_config_t *config);
void uft_diag_free(uft_diag_ctx_t *ctx);

int uft_diag_surface_scan(uft_diag_ctx_t *ctx,
                           uft_diag_read_fn read_fn, void *user_data);

int uft_diag_get_bad_sectors(const uft_diag_ctx_t *ctx,
                              uft_bad_sector_t *bad_list, size_t *count);

int uft_diag_head_alignment(uft_diag_ctx_t *ctx,
                             uft_diag_read_fn read_fn, void *user_data,
                             uft_alignment_info_t *info);

int uft_diag_write_verify(uft_diag_ctx_t *ctx,
                           uft_diag_read_fn read_fn,
                           uft_diag_write_fn write_fn,
                           void *user_data,
                           int test_track, int test_side);

int uft_diag_performance(uft_diag_ctx_t *ctx,
                          uft_diag_read_fn read_fn, void *user_data,
                          uft_perf_result_t *perf);

int uft_diag_report_text(const uft_diag_ctx_t *ctx,
                          char *buffer, size_t size);

int uft_diag_report_json(const uft_diag_ctx_t *ctx,
                          char *buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISC_DIAGNOSTICS_H */
