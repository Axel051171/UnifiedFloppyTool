/**
 * @file uft_flux_stream.c
 * @brief Flux Stream Analyzer and Display Track Implementation
 * 
 * EXT4-009: FluxStreamAnalyzer
 * EXT4-010: Display Track
 * 
 * Features:
 * - Flux stream analysis
 * - Track visualization data
 * - Histogram generation
 * - Pattern recognition
 * - ASCII track display
 */

#include "uft/flux/uft_flux_stream.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

#define HISTOGRAM_BINS      64
#define MAX_PATTERNS        128
#define DISPLAY_WIDTH       80
#define DISPLAY_HEIGHT      24

/*===========================================================================
 * Flux Stream Context
 *===========================================================================*/

int uft_flux_stream_init(uft_flux_stream_t *ctx)
{
    if (!ctx) return -1;
    
    memset(ctx, 0, sizeof(*ctx));
    return 0;
}

void uft_flux_stream_free(uft_flux_stream_t *ctx)
{
    if (ctx) {
        if (ctx->intervals) free(ctx->intervals);
        if (ctx->histogram) free(ctx->histogram);
        if (ctx->patterns) free(ctx->patterns);
        memset(ctx, 0, sizeof(*ctx));
    }
}

/*===========================================================================
 * Flux Analysis
 *===========================================================================*/

int uft_flux_stream_analyze(uft_flux_stream_t *ctx,
                            const uint32_t *flux_times, size_t count)
{
    if (!ctx || !flux_times || count < 2) return -1;
    
    /* Free previous data */
    if (ctx->intervals) free(ctx->intervals);
    if (ctx->histogram) free(ctx->histogram);
    
    ctx->flux_count = count;
    size_t interval_count = count - 1;
    
    /* Calculate intervals */
    ctx->intervals = malloc(interval_count * sizeof(uint32_t));
    if (!ctx->intervals) return -1;
    
    ctx->interval_count = interval_count;
    ctx->min_interval = UINT32_MAX;
    ctx->max_interval = 0;
    
    uint64_t sum = 0;
    
    for (size_t i = 0; i < interval_count; i++) {
        ctx->intervals[i] = flux_times[i + 1] - flux_times[i];
        sum += ctx->intervals[i];
        
        if (ctx->intervals[i] < ctx->min_interval) {
            ctx->min_interval = ctx->intervals[i];
        }
        if (ctx->intervals[i] > ctx->max_interval) {
            ctx->max_interval = ctx->intervals[i];
        }
    }
    
    ctx->avg_interval = sum / interval_count;
    
    /* Calculate standard deviation */
    double variance = 0;
    for (size_t i = 0; i < interval_count; i++) {
        double diff = (double)ctx->intervals[i] - ctx->avg_interval;
        variance += diff * diff;
    }
    ctx->std_dev = sqrt(variance / interval_count);
    
    /* Build histogram */
    ctx->histogram = calloc(HISTOGRAM_BINS, sizeof(uint32_t));
    if (!ctx->histogram) return -1;
    
    ctx->hist_bins = HISTOGRAM_BINS;
    uint32_t range = ctx->max_interval - ctx->min_interval;
    uint32_t bin_width = (range > 0) ? (range / HISTOGRAM_BINS + 1) : 1;
    ctx->hist_bin_width = bin_width;
    
    for (size_t i = 0; i < interval_count; i++) {
        int bin = (ctx->intervals[i] - ctx->min_interval) / bin_width;
        if (bin >= HISTOGRAM_BINS) bin = HISTOGRAM_BINS - 1;
        if (bin < 0) bin = 0;
        ctx->histogram[bin]++;
    }
    
    /* Find histogram peaks (likely bit cell widths) */
    ctx->peak_count = 0;
    for (int i = 1; i < HISTOGRAM_BINS - 1 && ctx->peak_count < 4; i++) {
        if (ctx->histogram[i] > ctx->histogram[i - 1] &&
            ctx->histogram[i] > ctx->histogram[i + 1] &&
            ctx->histogram[i] > interval_count / 20) {
            ctx->peaks[ctx->peak_count++] = ctx->min_interval + i * bin_width;
        }
    }
    
    /* Estimate data rate */
    if (ctx->peak_count > 0) {
        /* First peak is typically 1T cell */
        uint32_t cell_time = ctx->peaks[0];
        ctx->estimated_rate = 1000000000 / cell_time;  /* Hz */
    }
    
    ctx->is_analyzed = true;
    return 0;
}

/*===========================================================================
 * Pattern Recognition
 *===========================================================================*/

int uft_flux_stream_find_patterns(uft_flux_stream_t *ctx,
                                  uint32_t target_interval,
                                  uint32_t tolerance)
{
    if (!ctx || !ctx->intervals) return -1;
    
    /* Free previous patterns */
    if (ctx->patterns) free(ctx->patterns);
    
    ctx->patterns = malloc(MAX_PATTERNS * sizeof(uft_flux_pattern_t));
    if (!ctx->patterns) return -1;
    
    ctx->pattern_count = 0;
    size_t match_count = 0;
    size_t run_start = 0;
    bool in_run = false;
    
    for (size_t i = 0; i < ctx->interval_count; i++) {
        bool matches = (ctx->intervals[i] >= target_interval - tolerance &&
                        ctx->intervals[i] <= target_interval + tolerance);
        
        if (matches) {
            if (!in_run) {
                run_start = i;
                in_run = true;
            }
            match_count++;
        } else {
            if (in_run && ctx->pattern_count < MAX_PATTERNS) {
                /* End of run - record pattern */
                uft_flux_pattern_t *p = &ctx->patterns[ctx->pattern_count];
                p->start_index = run_start;
                p->length = i - run_start;
                p->avg_interval = target_interval;
                ctx->pattern_count++;
            }
            in_run = false;
        }
    }
    
    return match_count;
}

/*===========================================================================
 * Sync Detection
 *===========================================================================*/

int uft_flux_stream_find_syncs(uft_flux_stream_t *ctx,
                               size_t *sync_positions, size_t max_syncs,
                               size_t *sync_count)
{
    if (!ctx || !ctx->intervals || !sync_positions || !sync_count) return -1;
    
    *sync_count = 0;
    
    /* Look for MFM sync pattern: consistent 2T cells */
    uint32_t cell_1t = ctx->avg_interval;
    uint32_t cell_2t = cell_1t * 2;
    uint32_t tolerance = cell_1t / 4;
    
    int consecutive = 0;
    
    for (size_t i = 0; i < ctx->interval_count && *sync_count < max_syncs; i++) {
        if (ctx->intervals[i] >= cell_2t - tolerance &&
            ctx->intervals[i] <= cell_2t + tolerance) {
            consecutive++;
            
            /* Sync is typically 10+ consecutive 2T cells */
            if (consecutive >= 10) {
                sync_positions[*sync_count] = i - consecutive + 1;
                (*sync_count)++;
                consecutive = 0;
            }
        } else {
            consecutive = 0;
        }
    }
    
    return 0;
}

/*===========================================================================
 * Track Display
 *===========================================================================*/

int uft_track_display_init(uft_track_display_t *disp)
{
    if (!disp) return -1;
    
    memset(disp, 0, sizeof(*disp));
    disp->width = DISPLAY_WIDTH;
    disp->height = DISPLAY_HEIGHT;
    
    return 0;
}

void uft_track_display_free(uft_track_display_t *disp)
{
    if (disp) {
        memset(disp, 0, sizeof(*disp));
    }
}

int uft_track_display_set_data(uft_track_display_t *disp,
                               const uint32_t *flux_times, size_t count)
{
    if (!disp || !flux_times || count == 0) return -1;
    
    disp->flux_times = flux_times;
    disp->flux_count = count;
    
    /* Calculate range */
    disp->time_min = flux_times[0];
    disp->time_max = flux_times[count - 1];
    disp->time_range = disp->time_max - disp->time_min;
    
    return 0;
}

int uft_track_display_ascii(const uft_track_display_t *disp,
                            char *buffer, size_t size)
{
    if (!disp || !buffer || size == 0 || !disp->flux_times) return -1;
    
    /* Generate ASCII representation of flux timing */
    int width = (disp->width > 0) ? disp->width : DISPLAY_WIDTH;
    
    /* Calculate intervals and find peaks */
    if (disp->flux_count < 2) return -1;
    
    /* Create line buffer */
    char *line = malloc(width + 2);
    if (!line) return -1;
    
    size_t written = 0;
    
    /* Header */
    written += snprintf(buffer + written, size - written,
        "Track Display (%zu flux transitions)\n", disp->flux_count);
    written += snprintf(buffer + written, size - written,
        "Time range: %u - %u ns\n\n", disp->time_min, disp->time_max);
    
    /* Generate density display */
    memset(line, ' ', width);
    line[width] = '\n';
    line[width + 1] = '\0';
    
    /* Count flux per column */
    int *density = calloc(width, sizeof(int));
    if (!density) {
        free(line);
        return -1;
    }
    
    for (size_t i = 0; i < disp->flux_count; i++) {
        int col = (int)((uint64_t)(disp->flux_times[i] - disp->time_min) * 
                        (width - 1) / disp->time_range);
        if (col >= 0 && col < width) {
            density[col]++;
        }
    }
    
    /* Find max density */
    int max_density = 0;
    for (int i = 0; i < width; i++) {
        if (density[i] > max_density) max_density = density[i];
    }
    
    /* Generate display rows */
    for (int row = 7; row >= 0; row--) {
        int threshold = (max_density * row) / 8;
        
        for (int col = 0; col < width; col++) {
            if (density[col] > threshold) {
                line[col] = '#';
            } else {
                line[col] = ' ';
            }
        }
        
        written += snprintf(buffer + written, size - written, "%s", line);
    }
    
    /* Scale line */
    written += snprintf(buffer + written, size - written,
        "├");
    for (int i = 1; i < width - 1; i++) {
        written += snprintf(buffer + written, size - written,
            ((i % 10) == 0) ? "┼" : "─");
    }
    written += snprintf(buffer + written, size - written, "┤\n");
    
    /* Time labels */
    written += snprintf(buffer + written, size - written,
        "0");
    for (int i = 1; i < width - 10; i++) {
        buffer[written++] = ' ';
    }
    written += snprintf(buffer + written, size - written,
        "%uns\n", disp->time_range);
    
    free(density);
    free(line);
    
    return 0;
}

int uft_track_display_histogram(const uft_flux_stream_t *stream,
                                char *buffer, size_t size)
{
    if (!stream || !buffer || size == 0 || !stream->histogram) return -1;
    
    size_t written = 0;
    
    /* Find max bin */
    uint32_t max_bin = 0;
    for (int i = 0; i < stream->hist_bins; i++) {
        if (stream->histogram[i] > max_bin) max_bin = stream->histogram[i];
    }
    
    if (max_bin == 0) return -1;
    
    /* Header */
    written += snprintf(buffer + written, size - written,
        "Interval Histogram (%zu intervals)\n", stream->interval_count);
    written += snprintf(buffer + written, size - written,
        "Range: %u - %u ns (bin width: %u ns)\n\n",
        stream->min_interval, stream->max_interval, stream->hist_bin_width);
    
    /* Histogram bars */
    for (int row = 10; row > 0; row--) {
        uint32_t threshold = (max_bin * row) / 10;
        
        written += snprintf(buffer + written, size - written, "%5u │", threshold);
        
        for (int bin = 0; bin < stream->hist_bins && bin < 60; bin++) {
            if (stream->histogram[bin] >= threshold) {
                buffer[written++] = '█';
            } else {
                buffer[written++] = ' ';
            }
        }
        buffer[written++] = '\n';
    }
    
    /* Axis */
    written += snprintf(buffer + written, size - written, "      └");
    for (int i = 0; i < 60; i++) {
        buffer[written++] = '─';
    }
    buffer[written++] = '\n';
    
    /* Labels */
    written += snprintf(buffer + written, size - written,
        "       %u", stream->min_interval);
    for (int i = 0; i < 45; i++) {
        buffer[written++] = ' ';
    }
    written += snprintf(buffer + written, size - written,
        "%u ns\n", stream->max_interval);
    
    /* Peaks */
    if (stream->peak_count > 0) {
        written += snprintf(buffer + written, size - written, "\nPeaks: ");
        for (int i = 0; i < stream->peak_count; i++) {
            written += snprintf(buffer + written, size - written,
                "%u%s", stream->peaks[i], 
                i < stream->peak_count - 1 ? ", " : " ns\n");
        }
    }
    
    return 0;
}

/*===========================================================================
 * Statistics Report
 *===========================================================================*/

int uft_flux_stream_report_json(const uft_flux_stream_t *ctx,
                                char *buffer, size_t size)
{
    if (!ctx || !buffer || size == 0) return -1;
    
    int written = snprintf(buffer, size,
        "{\n"
        "  \"flux_count\": %zu,\n"
        "  \"interval_count\": %zu,\n"
        "  \"min_interval\": %u,\n"
        "  \"max_interval\": %u,\n"
        "  \"avg_interval\": %u,\n"
        "  \"std_deviation\": %.2f,\n"
        "  \"estimated_rate\": %u,\n"
        "  \"peak_count\": %d,\n"
        "  \"peaks\": [",
        ctx->flux_count,
        ctx->interval_count,
        ctx->min_interval,
        ctx->max_interval,
        ctx->avg_interval,
        ctx->std_dev,
        ctx->estimated_rate,
        ctx->peak_count
    );
    
    for (int i = 0; i < ctx->peak_count && written < (int)size - 20; i++) {
        written += snprintf(buffer + written, size - written,
            "%u%s", ctx->peaks[i], i < ctx->peak_count - 1 ? ", " : "");
    }
    
    written += snprintf(buffer + written, size - written,
        "],\n"
        "  \"pattern_count\": %zu\n"
        "}",
        ctx->pattern_count
    );
    
    return (written > 0 && (size_t)written < size) ? 0 : -1;
}
