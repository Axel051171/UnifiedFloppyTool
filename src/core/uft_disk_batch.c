/**
 * @file uft_disk_batch.c
 * @brief Sequential batch execution (parallel deferred).
 */
#include "uft/uft_disk_batch.h"
#include "uft/uft_disk_verify.h"
#include "uft/uft_disk_stats.h"
#include "uft/uft_disk_convert.h"
#include "uft/uft_format_plugin.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* uft_disk_open/close sind in uft_core_stubs.c */
extern void* uft_disk_open(const char *path, int read_only);
extern void  uft_disk_close(void *disk);

static void detail_append(uft_batch_result_t *r, const char *path,
                            uft_error_t err, const char *msg)
{
    if (r->details_count >= r->details_capacity) {
        size_t n = r->details_capacity ? r->details_capacity * 2 : 8;
        void *p = realloc(r->details, n * sizeof(*r->details));
        if (!p) return;
        r->details = p;
        r->details_capacity = n;
    }
    struct uft_batch_detail *d = &r->details[r->details_count++];
    d->input_path = path;
    d->result = err;
    snprintf(d->message, sizeof(d->message), "%s", msg ? msg : "");
}

static uft_error_t run_verify_job(const uft_batch_job_t *job,
                                    uft_batch_result_t *result)
{
    uft_disk_t *disk = uft_disk_open(job->input_path, true);
    if (!disk) {
        detail_append(result, job->input_path,
                       UFT_ERROR_FILE_OPEN, "open failed");
        return UFT_ERROR_FILE_OPEN;
    }

    uft_verify_result_t vr;
    uft_error_t err;
    if (job->op.verify.reference_path) {
        uft_disk_t *ref = uft_disk_open(job->op.verify.reference_path, true);
        if (!ref) {
            uft_disk_close(disk);
            detail_append(result, job->input_path,
                           UFT_ERROR_FILE_OPEN, "ref open failed");
            return UFT_ERROR_FILE_OPEN;
        }
        err = uft_disk_verify(disk, ref, NULL, &vr);
        uft_disk_close(ref);
    } else {
        err = uft_disk_verify_self(disk, NULL, &vr);
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "tracks=%zu/%zu",
              vr.tracks_ok, vr.tracks_total);
    detail_append(result, job->input_path, err, msg);

    uft_verify_result_free(&vr);
    uft_disk_close(disk);
    return err;
}

static uft_error_t run_stats_job(const uft_batch_job_t *job,
                                   uft_batch_result_t *result)
{
    uft_disk_t *disk = uft_disk_open(job->input_path, true);
    if (!disk) {
        detail_append(result, job->input_path,
                       UFT_ERROR_FILE_OPEN, "open failed");
        return UFT_ERROR_FILE_OPEN;
    }

    uft_disk_stats_t stats;
    uft_error_t err = uft_disk_collect_stats(disk, NULL, &stats);

    char msg[256];
    snprintf(msg, sizeof(msg), "health=%.1f%% ok=%zu/%zu",
              stats.disk_health * 100.0, stats.ok_sectors, stats.total_sectors);
    detail_append(result, job->input_path, err, msg);

    /* Optional JSON output to file */
    if (err == UFT_OK && job->op.stats.output_json) {
        char json[2048];
        if (uft_disk_stats_to_json(&stats, json, sizeof(json)) == UFT_OK) {
            FILE *f = fopen(job->op.stats.output_json, "w");
            if (f) { fprintf(f, "%s\n", json); fclose(f); }
        }
    }

    uft_disk_stats_free(&stats);
    uft_disk_close(disk);
    return err;
}

static uft_error_t run_convert_job(const uft_batch_job_t *job,
                                     uft_batch_result_t *result)
{
    uft_disk_t *disk = uft_disk_open(job->input_path, true);
    if (!disk) {
        detail_append(result, job->input_path,
                       UFT_ERROR_FILE_OPEN, "open failed");
        return UFT_ERROR_FILE_OPEN;
    }

    uft_convert_result_t cr;
    uft_error_t err = uft_disk_convert(disk, job->op.convert.output_path,
                                         job->op.convert.target_format,
                                         NULL, &cr);

    char msg[256];
    snprintf(msg, sizeof(msg), "converted=%zu failed=%zu",
              cr.tracks_converted, cr.tracks_failed);
    detail_append(result, job->input_path, err, msg);

    uft_disk_close(disk);
    return err;
}

static uft_error_t run_custom_job(const uft_batch_job_t *job,
                                    uft_batch_result_t *result)
{
    if (!job->op.custom.fn) return UFT_ERROR_NULL_POINTER;
    uft_disk_t *disk = uft_disk_open(job->input_path, true);
    if (!disk) return UFT_ERROR_FILE_OPEN;

    uft_error_t err = job->op.custom.fn(disk);
    detail_append(result, job->input_path, err, "custom");
    uft_disk_close(disk);
    return err;
}

uft_error_t uft_batch_run(const uft_batch_job_t *jobs, size_t job_count,
                           const uft_batch_options_t *options,
                           uft_batch_result_t *result)
{
    if (!jobs || !result) return UFT_ERROR_NULL_POINTER;
    memset(result, 0, sizeof(*result));
    result->total_jobs = job_count;

    uft_batch_options_t dflt = UFT_BATCH_OPTIONS_DEFAULT;
    const uft_batch_options_t *opts = options ? options : &dflt;

    uft_error_t last_err = UFT_OK;
    for (size_t i = 0; i < job_count; i++) {
        const uft_batch_job_t *job = &jobs[i];
        uft_error_t err;

        switch (job->type) {
            case UFT_BATCH_OP_VERIFY:  err = run_verify_job(job, result); break;
            case UFT_BATCH_OP_STATS:   err = run_stats_job(job, result); break;
            case UFT_BATCH_OP_CONVERT: err = run_convert_job(job, result); break;
            case UFT_BATCH_OP_CUSTOM:  err = run_custom_job(job, result); break;
            default: err = UFT_ERROR_INVALID_ARG;
        }

        if (err == UFT_OK) result->completed++;
        else {
            result->failed++;
            last_err = err;
            if (opts->stop_on_first_failure) break;
        }

        if (opts->base.on_progress) {
            if (!opts->base.on_progress(i + 1, job_count, job->input_path,
                                          opts->base.user_data))
                return UFT_ERROR_CANCELLED;
        }
    }
    return last_err;
}

void uft_batch_result_free(uft_batch_result_t *result) {
    if (!result) return;
    free(result->details);
    result->details = NULL;
    result->details_count = 0;
    result->details_capacity = 0;
}
