/**
 * @file uft_disk_compare.h
 * @brief Cross-format disk diff — built on uft_disk_stream_pair.
 *
 * Part of Master-API Phase 5.
 */
#ifndef UFT_DISK_COMPARE_H
#define UFT_DISK_COMPARE_H

#include "uft/uft_disk_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_DIFF_NONE          = 0,
    UFT_DIFF_MISSING_IN_A  = 1,
    UFT_DIFF_MISSING_IN_B  = 2,
    UFT_DIFF_DATA          = 3,
    UFT_DIFF_STATUS        = 4,
    UFT_DIFF_SIZE          = 5,
} uft_diff_type_t;

typedef struct {
    uft_diff_type_t type;
    int             cylinder;
    int             head;
    uint8_t         sector;
    size_t          byte_offset;
    size_t          byte_count;
    /* Optional data copies (include_data=true) */
    uint8_t        *data_a;
    uint8_t        *data_b;
    size_t          data_len;
} uft_diff_entry_t;

typedef struct {
    size_t             total_tracks_compared;
    size_t             total_sectors_compared;
    size_t             identical_sectors;
    size_t             different_sectors;

    uft_diff_entry_t  *diffs;
    size_t             diff_count;
    size_t             diff_capacity;

    double             similarity;   /* 0.0-1.0 */
} uft_disk_compare_result_t;

typedef struct {
    uft_disk_op_options_t  base;

    bool                   include_data;
    bool                   compare_status;
    size_t                 max_diffs;
    bool                   stop_after_first_diff;
} uft_compare_options_t;

#define UFT_COMPARE_OPTIONS_DEFAULT \
    { UFT_DISK_OP_OPTIONS_DEFAULT, false, false, 0, false }

/* ============================================================================
 * API
 * ============================================================================ */

uft_error_t uft_disk_compare(uft_disk_t *disk_a, uft_disk_t *disk_b,
                              const uft_compare_options_t *options,
                              uft_disk_compare_result_t *result);

void uft_compare_result_free(uft_disk_compare_result_t *result);

uft_error_t uft_disk_compare_result_to_text(const uft_disk_compare_result_t *result,
                                         char *buffer, size_t buffer_size);

uft_error_t uft_disk_compare_result_to_json(const uft_disk_compare_result_t *result,
                                         char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_COMPARE_H */
