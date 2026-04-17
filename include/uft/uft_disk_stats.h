/**
 * @file uft_disk_stats.h
 * @brief Disk health and quality metrics — built on uft_disk_stream.
 *
 * Part of Master-API Phase 4.
 */
#ifndef UFT_DISK_STATS_H
#define UFT_DISK_STATS_H

#include "uft/uft_disk_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Per-Track Stats
 * ============================================================================ */

typedef struct {
    int       cylinder;
    int       head;

    size_t    sectors_total;
    size_t    sectors_ok;
    size_t    sectors_crc_error;
    size_t    sectors_missing;
    size_t    sectors_weak;
    size_t    sectors_deleted;
    size_t    sectors_duplicate;

    double    average_confidence;
    double    read_quality;

    bool      has_weak_bits;
    bool      has_timing_variance;
} uft_track_stats_entry_t;

/* ============================================================================
 * Disk-Level Stats
 * ============================================================================ */

typedef struct {
    size_t    total_tracks;
    size_t    readable_tracks;
    size_t    unreadable_tracks;

    size_t    total_sectors;
    size_t    ok_sectors;
    size_t    failed_sectors;
    size_t    weak_sectors;
    size_t    deleted_sectors;

    double    disk_health;            /* 0.0-1.0 */
    double    read_success_rate;
    double    average_confidence;

    bool      has_copy_protection;
    bool      has_weak_sectors;
    bool      has_timing_data;
    bool      has_deleted_sectors;

    uft_track_stats_entry_t *tracks;
    size_t                   tracks_count;
    size_t                   tracks_capacity;
} uft_disk_stats_t;

/* ============================================================================
 * Options
 * ============================================================================ */

typedef struct {
    uft_disk_op_options_t  base;
    bool                   detailed;
    bool                   analyze_flux;
    bool                   detect_protection;
} uft_stats_options_t;

#define UFT_STATS_OPTIONS_DEFAULT \
    { UFT_DISK_OP_OPTIONS_DEFAULT, false, false, false }

/* ============================================================================
 * API
 * ============================================================================ */

uft_error_t uft_disk_collect_stats(uft_disk_t *disk,
                                     const uft_stats_options_t *options,
                                     uft_disk_stats_t *stats);

void uft_disk_stats_free(uft_disk_stats_t *stats);

uft_error_t uft_disk_stats_to_text(const uft_disk_stats_t *stats,
                                     char *buffer, size_t buffer_size);

uft_error_t uft_disk_stats_to_json(const uft_disk_stats_t *stats,
                                     char *buffer, size_t buffer_size);

/**
 * @brief Quick-Check: Health-Score 0-100.
 */
int uft_disk_health_score(uft_disk_t *disk);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_STATS_H */
