/**
 * @file uft_disk_batch.h
 * @brief Batch operations on multiple disks (Master-API Phase 8).
 *
 * Parallel execution deferred — current impl is sequential.
 * Threading choice (pthread vs C11 vs OpenMP) requires project consensus.
 */
#ifndef UFT_DISK_BATCH_H
#define UFT_DISK_BATCH_H

#include "uft/uft_disk_api_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_BATCH_OP_VERIFY   = 0,
    UFT_BATCH_OP_CONVERT  = 1,
    UFT_BATCH_OP_STATS    = 2,
    UFT_BATCH_OP_CUSTOM   = 3,
} uft_batch_op_type_t;

typedef struct {
    uft_batch_op_type_t  type;
    const char          *input_path;

    union {
        struct { const char *reference_path; }     verify;
        struct { const char *output_path;
                 uft_format_t target_format; }     convert;
        struct { const char *output_json; }        stats;
        struct { void *user_data;
                 uft_error_t (*fn)(uft_disk_t*); } custom;
    } op;
} uft_batch_job_t;

typedef struct {
    size_t    total_jobs;
    size_t    completed;
    size_t    failed;

    struct uft_batch_detail {
        const char   *input_path;
        uft_error_t   result;
        char          message[256];
    } *details;
    size_t    details_count;
    size_t    details_capacity;
} uft_batch_result_t;

typedef struct {
    uft_disk_op_options_t  base;

    int                    parallel_jobs;    /* 1 = sequential */
    bool                   stop_on_first_failure;
} uft_batch_options_t;

#define UFT_BATCH_OPTIONS_DEFAULT \
    { UFT_DISK_OP_OPTIONS_DEFAULT, 1, false }

uft_error_t uft_batch_run(const uft_batch_job_t *jobs, size_t job_count,
                           const uft_batch_options_t *options,
                           uft_batch_result_t *result);

void uft_batch_result_free(uft_batch_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_BATCH_H */
