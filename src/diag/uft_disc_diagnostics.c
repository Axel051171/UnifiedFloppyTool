/**
 * @file uft_disc_diagnostics.c
 * @brief Disc Diagnostics Tool Implementation
 * 
 * EXT3-017: Comprehensive disc diagnostics
 * 
 * Features:
 * - Surface scan
 * - Bad sector detection
 * - Head alignment check
 * - Write/verify test
 * - Performance measurement
 */

#include "uft/diag/uft_disc_diagnostics.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define MAX_TRACKS          84
#define MAX_SIDES           2
#define MAX_SECTORS         36
#define MAX_RETRIES         5
#define PATTERN_COUNT       4

/* Test patterns */
static const uint8_t TEST_PATTERNS[PATTERN_COUNT] = {
    0x00,   /* All zeros */
    0xFF,   /* All ones */
    0xAA,   /* 10101010 */
    0x55    /* 01010101 */
};

/*===========================================================================
 * Diagnostic Context
 *===========================================================================*/

int uft_diag_init(uft_diag_ctx_t *ctx, uft_diag_config_t *config)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    
    if (config) {
        ctx->config = *config;
    } else {
        /* Defaults */
        ctx->config.tracks = 80;
        ctx->config.sides = 2;
        ctx->config.sectors = 18;
        ctx->config.sector_size = 512;
        ctx->config.retries = 3;
        ctx->config.verbose = true;
    }
    
    /* Allocate results */
    size_t total = ctx->config.tracks * ctx->config.sides;
    ctx->track_results = calloc(total, sizeof(uft_diag_track_result_t));
    if (!ctx->track_results) return -1;
    
    ctx->start_time = time(NULL);
    
    return 0;
}

void uft_diag_free(uft_diag_ctx_t *ctx)
{
    if (!ctx) return;
    
    free(ctx->track_results);
    memset(ctx, 0, sizeof(*ctx));
}

/*===========================================================================
 * Surface Scan
 *===========================================================================*/

int uft_diag_surface_scan(uft_diag_ctx_t *ctx, uft_diag_read_fn read_fn,
                          void *user_data)
{
    if (!ctx || !read_fn) return -1;
    
    ctx->test_type = UFT_DIAG_SURFACE_SCAN;
    ctx->total_sectors = 0;
    ctx->good_sectors = 0;
    ctx->bad_sectors = 0;
    ctx->weak_sectors = 0;
    
    uint8_t *buffer = malloc(ctx->config.sector_size);
    if (!buffer) return -1;
    
    for (int t = 0; t < ctx->config.tracks; t++) {
        for (int s = 0; s < ctx->config.sides; s++) {
            uft_diag_track_result_t *result = 
                &ctx->track_results[t * ctx->config.sides + s];
            
            result->track = t;
            result->side = s;
            result->bad_sectors = 0;
            result->weak_sectors = 0;
            result->read_errors = 0;
            result->avg_read_time_us = 0;
            
            uint64_t total_time = 0;
            
            for (int sec = 0; sec < ctx->config.sectors; sec++) {
                ctx->total_sectors++;
                
                int success = 0;
                int retries = 0;
                clock_t start = clock();
                
                while (!success && retries < ctx->config.retries) {
                    if (read_fn(t, s, sec + 1, buffer, 
                               ctx->config.sector_size, user_data) == 0) {
                        success = 1;
                    } else {
                        retries++;
                    }
                }
                
                clock_t end = clock();
                total_time += (end - start) * 1000000 / CLOCKS_PER_SEC;
                
                if (success) {
                    if (retries > 0) {
                        result->weak_sectors++;
                        ctx->weak_sectors++;
                        result->sector_status[sec] = UFT_SECTOR_WEAK;
                    } else {
                        ctx->good_sectors++;
                        result->sector_status[sec] = UFT_SECTOR_GOOD;
                    }
                } else {
                    result->bad_sectors++;
                    result->read_errors++;
                    ctx->bad_sectors++;
                    result->sector_status[sec] = UFT_SECTOR_BAD;
                }
            }
            
            result->avg_read_time_us = total_time / ctx->config.sectors;
            
            /* Calculate track quality */
            result->quality = 100.0 * 
                (ctx->config.sectors - result->bad_sectors) / ctx->config.sectors;
            
            /* Callback for progress */
            if (ctx->progress_fn) {
                int progress = (t * ctx->config.sides + s + 1) * 100 /
                              (ctx->config.tracks * ctx->config.sides);
                ctx->progress_fn(progress, result, ctx->progress_data);
            }
        }
    }
    
    free(buffer);
    
    ctx->end_time = time(NULL);
    ctx->completed = true;
    
    return 0;
}

/*===========================================================================
 * Bad Sector Map
 *===========================================================================*/

int uft_diag_get_bad_sectors(const uft_diag_ctx_t *ctx,
                             uft_bad_sector_t *bad_list, size_t *count)
{
    if (!ctx || !bad_list || !count) return -1;
    
    size_t max_count = *count;
    *count = 0;
    
    for (int t = 0; t < ctx->config.tracks; t++) {
        for (int s = 0; s < ctx->config.sides; s++) {
            const uft_diag_track_result_t *result =
                &ctx->track_results[t * ctx->config.sides + s];
            
            for (int sec = 0; sec < ctx->config.sectors; sec++) {
                if (result->sector_status[sec] == UFT_SECTOR_BAD) {
                    if (*count < max_count) {
                        bad_list[*count].track = t;
                        bad_list[*count].side = s;
                        bad_list[*count].sector = sec + 1;
                        bad_list[*count].type = UFT_BAD_READ_ERROR;
                    }
                    (*count)++;
                }
            }
        }
    }
    
    return 0;
}

/*===========================================================================
 * Head Alignment Test
 *===========================================================================*/

int uft_diag_head_alignment(uft_diag_ctx_t *ctx, uft_diag_read_fn read_fn,
                            void *user_data, uft_alignment_info_t *info)
{
    if (!ctx || !read_fn || !info) return -1;
    
    memset(info, 0, sizeof(*info));
    
    uint8_t *buffer = malloc(ctx->config.sector_size);
    if (!buffer) return -1;
    
    /* Test track-to-track consistency */
    int test_tracks[] = {0, 20, 40, 60, 79};
    int test_count = sizeof(test_tracks) / sizeof(test_tracks[0]);
    
    double read_times[5][2] = {0};
    int errors[5][2] = {0};
    
    for (int i = 0; i < test_count; i++) {
        int t = test_tracks[i];
        if (t >= ctx->config.tracks) continue;
        
        for (int s = 0; s < ctx->config.sides; s++) {
            clock_t start = clock();
            int err = 0;
            
            for (int sec = 0; sec < ctx->config.sectors; sec++) {
                if (read_fn(t, s, sec + 1, buffer,
                           ctx->config.sector_size, user_data) != 0) {
                    err++;
                }
            }
            
            clock_t end = clock();
            read_times[i][s] = (double)(end - start) * 1000 / CLOCKS_PER_SEC;
            errors[i][s] = err;
        }
    }
    
    free(buffer);
    
    /* Analyze results */
    double avg_time = 0;
    int total_errors = 0;
    
    for (int i = 0; i < test_count; i++) {
        for (int s = 0; s < 2; s++) {
            avg_time += read_times[i][s];
            total_errors += errors[i][s];
        }
    }
    avg_time /= (test_count * 2);
    
    /* Check for timing variations */
    double max_deviation = 0;
    for (int i = 0; i < test_count; i++) {
        for (int s = 0; s < 2; s++) {
            double dev = fabs(read_times[i][s] - avg_time) / avg_time;
            if (dev > max_deviation) {
                max_deviation = dev;
            }
        }
    }
    
    info->timing_deviation = max_deviation * 100;
    info->error_count = total_errors;
    
    /* Determine alignment status */
    if (max_deviation < 0.05 && total_errors == 0) {
        info->status = UFT_ALIGN_GOOD;
        strncpy(info->message, "Head alignment is good", sizeof(info->message));
    } else if (max_deviation < 0.15 && total_errors < 5) {
        info->status = UFT_ALIGN_FAIR;
        strncpy(info->message, "Head alignment is acceptable", sizeof(info->message));
    } else if (max_deviation < 0.25) {
        info->status = UFT_ALIGN_POOR;
        strncpy(info->message, "Head alignment needs adjustment", sizeof(info->message));
    } else {
        info->status = UFT_ALIGN_BAD;
        strncpy(info->message, "Head alignment is severely off", sizeof(info->message));
    }
    
    return 0;
}

/*===========================================================================
 * Write/Verify Test
 *===========================================================================*/

int uft_diag_write_verify(uft_diag_ctx_t *ctx,
                          uft_diag_read_fn read_fn,
                          uft_diag_write_fn write_fn,
                          void *user_data,
                          int test_track, int test_side)
{
    if (!ctx || !read_fn || !write_fn) return -1;
    
    uint8_t *write_buf = malloc(ctx->config.sector_size);
    uint8_t *read_buf = malloc(ctx->config.sector_size);
    
    if (!write_buf || !read_buf) {
        free(write_buf);
        free(read_buf);
        return -1;
    }
    
    int total_errors = 0;
    
    /* Test each pattern */
    for (int p = 0; p < PATTERN_COUNT; p++) {
        uint8_t pattern = TEST_PATTERNS[p];
        memset(write_buf, pattern, ctx->config.sector_size);
        
        /* Write all sectors */
        for (int sec = 0; sec < ctx->config.sectors; sec++) {
            if (write_fn(test_track, test_side, sec + 1, write_buf,
                        ctx->config.sector_size, user_data) != 0) {
                total_errors++;
            }
        }
        
        /* Read and verify */
        for (int sec = 0; sec < ctx->config.sectors; sec++) {
            if (read_fn(test_track, test_side, sec + 1, read_buf,
                       ctx->config.sector_size, user_data) != 0) {
                total_errors++;
            } else {
                /* Compare */
                if (memcmp(write_buf, read_buf, ctx->config.sector_size) != 0) {
                    total_errors++;
                }
            }
        }
    }
    
    free(write_buf);
    free(read_buf);
    
    return total_errors;
}

/*===========================================================================
 * Performance Test
 *===========================================================================*/

int uft_diag_performance(uft_diag_ctx_t *ctx, uft_diag_read_fn read_fn,
                         void *user_data, uft_perf_result_t *perf)
{
    if (!ctx || !read_fn || !perf) return -1;
    
    memset(perf, 0, sizeof(*perf));
    
    uint8_t *buffer = malloc(ctx->config.sector_size);
    if (!buffer) return -1;
    
    /* Sequential read test */
    clock_t seq_start = clock();
    int seq_sectors = 0;
    
    for (int t = 0; t < 10 && t < ctx->config.tracks; t++) {
        for (int sec = 0; sec < ctx->config.sectors; sec++) {
            if (read_fn(t, 0, sec + 1, buffer,
                       ctx->config.sector_size, user_data) == 0) {
                seq_sectors++;
            }
        }
    }
    
    clock_t seq_end = clock();
    double seq_time = (double)(seq_end - seq_start) / CLOCKS_PER_SEC;
    
    if (seq_time > 0) {
        perf->sequential_kbps = (seq_sectors * ctx->config.sector_size) / 
                                (seq_time * 1024);
    }
    
    /* Random read test */
    clock_t rand_start = clock();
    int rand_sectors = 0;
    
    for (int i = 0; i < 100; i++) {
        int t = rand() % ctx->config.tracks;
        int s = rand() % ctx->config.sides;
        int sec = (rand() % ctx->config.sectors) + 1;
        
        if (read_fn(t, s, sec, buffer,
                   ctx->config.sector_size, user_data) == 0) {
            rand_sectors++;
        }
    }
    
    clock_t rand_end = clock();
    double rand_time = (double)(rand_end - rand_start) / CLOCKS_PER_SEC;
    
    if (rand_time > 0) {
        perf->random_kbps = (rand_sectors * ctx->config.sector_size) / 
                            (rand_time * 1024);
        perf->avg_seek_ms = (rand_time * 1000) / rand_sectors;
    }
    
    free(buffer);
    
    return 0;
}

/*===========================================================================
 * Report Generation
 *===========================================================================*/

int uft_diag_report_json(const uft_diag_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer) return -1;
    
    double duration = difftime(ctx->end_time, ctx->start_time);
    double quality = (ctx->total_sectors > 0) ? 
        100.0 * ctx->good_sectors / ctx->total_sectors : 0;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"diagnostics\": {\n"
        "    \"completed\": %s,\n"
        "    \"duration_seconds\": %.1f,\n"
        "    \"tracks\": %d,\n"
        "    \"sides\": %d,\n"
        "    \"sectors_per_track\": %d,\n"
        "    \"total_sectors\": %d,\n"
        "    \"good_sectors\": %d,\n"
        "    \"weak_sectors\": %d,\n"
        "    \"bad_sectors\": %d,\n"
        "    \"quality_percent\": %.2f\n"
        "  }\n"
        "}",
        ctx->completed ? "true" : "false",
        duration,
        ctx->config.tracks,
        ctx->config.sides,
        ctx->config.sectors,
        ctx->total_sectors,
        ctx->good_sectors,
        ctx->weak_sectors,
        ctx->bad_sectors,
        quality
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}

int uft_diag_report_text(const uft_diag_ctx_t *ctx, char *buffer, size_t size)
{
    if (!ctx || !buffer) return -1;
    
    double quality = (ctx->total_sectors > 0) ?
        100.0 * ctx->good_sectors / ctx->total_sectors : 0;
    
    int written = snprintf(buffer, size,
        "=== Disc Diagnostics Report ===\n\n"
        "Configuration:\n"
        "  Tracks: %d  Sides: %d  Sectors: %d\n"
        "  Sector Size: %d bytes\n\n"
        "Results:\n"
        "  Total Sectors: %d\n"
        "  Good: %d (%.1f%%)\n"
        "  Weak: %d (%.1f%%)\n"
        "  Bad:  %d (%.1f%%)\n\n"
        "Overall Quality: %.1f%%\n"
        "Status: %s\n",
        ctx->config.tracks, ctx->config.sides, ctx->config.sectors,
        ctx->config.sector_size,
        ctx->total_sectors,
        ctx->good_sectors, 100.0 * ctx->good_sectors / ctx->total_sectors,
        ctx->weak_sectors, 100.0 * ctx->weak_sectors / ctx->total_sectors,
        ctx->bad_sectors, 100.0 * ctx->bad_sectors / ctx->total_sectors,
        quality,
        quality >= 99 ? "EXCELLENT" :
        quality >= 95 ? "GOOD" :
        quality >= 80 ? "FAIR" :
        quality >= 50 ? "POOR" : "BAD"
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
