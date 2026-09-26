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

/* Limits of a scan configuration. uft_diag_init() and
 * uft_diag_surface_scan() return -1 for anything outside them instead of
 * reading (door stage 4): before, sectors > 36 wrote past sector_status[],
 * sectors == 0 divided by zero, and retries == 0 marked every sector BAD
 * without a single read attempt. */
#define UFT_DIAG_MAX_SECTORS_PER_TRACK 36
#define UFT_DIAG_MAX_TRACKS            84   /**< cylinders 0..83 */
#define UFT_DIAG_MAX_SIDES             2

typedef struct {
    int  tracks;        /**< Default 80; 1..UFT_DIAG_MAX_TRACKS */
    int  sides;         /**< Default 2; 1..UFT_DIAG_MAX_SIDES */
    int  sectors;       /**< Default 18 (per track); 1..UFT_DIAG_MAX_SECTORS_PER_TRACK */
    int  sector_size;   /**< Default 512; >= 1 */
    int  retries;       /**< Default 3; read ATTEMPTS per sector, >= 1 */
    bool verbose;

    /* Appended (door stage 4) — the fields above keep their offsets.
     *
     * ABI, stated: this is append-only for uft_diag_config_t itself, NOT
     * for uft_diag_ctx_t, which embeds the config BY VALUE as its first
     * member. The config grows by 40 bytes (24 -> 64), so every ctx field
     * behind it moves by 40 (sizeof(uft_diag_ctx_t) 88 -> 136: the 40
     * from the config plus the newly appended 8-byte track_results_count,
     * total_sectors 24 -> 64, track_results 40 -> 80; measured with
     * offsetof against the header of 0095b756). Code compiled against the
     * old header must be rebuilt. Translated users outside src/diag,
     * include/uft/diag and tests: 0 (git grep for the header and both
     * type names; the only other hit is the HEADERS list of
     * UnifiedFloppyTool.pro).
     *
     * The sector numbers the scan asks read_fn for, in scan order.
     * sector_id_count == 0 keeps the old behaviour: 1..sectors. Otherwise
     * it must equal `sectors`, and the numbers must be distinct (one
     * sector, one result slot) — else init/scan return -1.
     *
     * Needed for every format whose numbers do not start at 1: Amstrad CPC
     * data 0xC1..0xC9, JV1 0..9 (MF-1016), MYZ80 0..127 (MF-1029). Asked
     * for 1..n, such a disk scans as all BAD.
     *
     * A fixed array, not a pointer: the config is COPIED into the context,
     * and a borrowed pointer would have to outlive it.
     *
     * Limits, stated: one list for all tracks (zoned formats with a
     * different count per track cannot be described); and
     * uft_diag_head_alignment / uft_diag_performance / uft_diag_write_verify
     * still ask for 1..sectors — only the surface scan and
     * uft_diag_get_bad_sectors use this list. */
    uint8_t sector_ids[UFT_DIAG_MAX_SECTORS_PER_TRACK];
    int     sector_id_count;
} uft_diag_config_t;

/*
 * Result of one sector in a surface scan. Own name since MF-1341: this
 * enum used to be called `uft_sector_status_t` with enumerators
 * `UFT_SECTOR_*`, which uft_types.h also defines with a different meaning
 * (a bit mask, WEAK = 16), so the two headers could not be compiled in
 * one translation unit. Values are unchanged; `sector_status[]` below
 * stores them as uint8_t.
 */
typedef enum {
    UFT_DIAG_SECTOR_UNKNOWN = 0,
    UFT_DIAG_SECTOR_GOOD    = 1,
    UFT_DIAG_SECTOR_WEAK    = 2,   /**< read, but only after a retry */
    UFT_DIAG_SECTOR_BAD     = 3
} uft_diag_sector_status_t;

typedef struct uft_diag_track_result {
    int    track;
    int    side;
    int    bad_sectors;
    int    weak_sectors;
    int    read_errors;
    double avg_read_time_us;
    double quality;                  /**< percent 0..100 */
    /** Indexed by POSITION in the scan order, not by sector number: with
     *  config.sector_ids set, sector_status[i] belongs to sector_ids[i]. */
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

    /* Appended (door stage 4): how many entries uft_diag_init() allocated
     * in track_results. `config` is public and can be changed after init;
     * the scan checks tracks x sides against THIS number instead of
     * writing past the allocation. */
    size_t                      track_results_count;
} uft_diag_ctx_t;

typedef struct {
    double sequential_kbps;   /**< Sequential read throughput */
    double random_kbps;       /**< Random-access throughput */
    double avg_seek_ms;       /**< Average seek time in milliseconds */
} uft_perf_result_t;

/*===========================================================================
 * Functions
 *===========================================================================*/

/**
 * Copies @p config (or installs the defaults 80/2/18/512/3 when NULL) and
 * allocates tracks x sides results. Returns -1 — with @p ctx zeroed, so
 * uft_diag_free() stays safe — when the configuration is outside the limits
 * above or its sector_ids list is malformed.
 */
int  uft_diag_init(uft_diag_ctx_t *ctx, uft_diag_config_t *config);
void uft_diag_free(uft_diag_ctx_t *ctx);

/**
 * Reads every sector of every track and side with up to config.retries
 * attempts each; GOOD = first attempt, WEAK = a later attempt, BAD = none.
 * Read-only: the scan has no write path.
 *
 * Checks the configuration again (ctx->config is public) and returns -1
 * WITHOUT a single read when it is outside the limits or tracks x sides
 * exceeds what uft_diag_init() allocated.
 *
 * A retry only means something when read_fn measures anew each time
 * (hardware). Over an image file every attempt returns the same bytes.
 */
int uft_diag_surface_scan(uft_diag_ctx_t *ctx,
                           uft_diag_read_fn read_fn, void *user_data);

/**
 * Lists the BAD sectors of the last scan. `sector` is the sector NUMBER
 * that was asked for (config.sector_ids when set, else position + 1),
 * `side` the side it was read on. Returns -1 under the same conditions as
 * uft_diag_surface_scan().
 */
int uft_diag_get_bad_sectors(const uft_diag_ctx_t *ctx,
                              uft_bad_sector_t *bad_list, size_t *count);

int uft_diag_head_alignment(uft_diag_ctx_t *ctx,
                             uft_diag_read_fn read_fn, void *user_data,
                             uft_alignment_info_t *info);

/**
 * DESTRUCTIVE. Overwrites every sector 1..config.sectors of
 * @p test_track / @p test_side with the patterns 0x00, 0xFF, 0xAA, 0x55
 * via @p write_fn and reads them back. Whatever the disk held there is
 * gone afterwards. Never call it on a disk that is being preserved.
 *
 * No surface-scan path reaches this function: uft_diag_surface_scan()
 * takes no write_fn, and the Greaseweazle adapter (uft_diag_gw.h) has no
 * write parameter at all.
 */
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
