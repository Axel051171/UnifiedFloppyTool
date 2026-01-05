/**
 * @file uft_magnetic_state.c
 * @brief MAME-Compatible Magnetic State Implementation
 * 
 * Implementation of track buffer operations and weak bit detection.
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/protection/uft_magnetic_state.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Track Buffer Management
 *===========================================================================*/

/**
 * @brief Allocate track buffer
 */
int uft_tbuf_alloc(uft_track_buffer_t *tbuf, size_t capacity)
{
    if (!tbuf) return -1;
    
    uft_tbuf_init(tbuf);
    
    tbuf->cells = (uint32_t*)calloc(capacity, sizeof(uint32_t));
    if (!tbuf->cells) {
        return -1;
    }
    
    tbuf->capacity = capacity;
    return 0;
}

/**
 * @brief Free track buffer
 */
void uft_tbuf_free(uft_track_buffer_t *tbuf)
{
    if (!tbuf) return;
    
    if (tbuf->cells) {
        free(tbuf->cells);
        tbuf->cells = NULL;
    }
    
    tbuf->cell_count = 0;
    tbuf->capacity = 0;
}

/**
 * @brief Clear track buffer (keep allocation)
 */
void uft_tbuf_clear(uft_track_buffer_t *tbuf)
{
    if (!tbuf) return;
    
    if (tbuf->cells && tbuf->capacity > 0) {
        memset(tbuf->cells, 0, tbuf->capacity * sizeof(uint32_t));
    }
    
    tbuf->cell_count = 0;
    tbuf->flux_count = 0;
    tbuf->weak_count = 0;
    tbuf->damaged_count = 0;
}

/**
 * @brief Copy track buffer
 */
int uft_tbuf_copy(uft_track_buffer_t *dst, const uft_track_buffer_t *src)
{
    if (!dst || !src) return -1;
    
    if (dst->capacity < src->cell_count) {
        uft_tbuf_free(dst);
        if (uft_tbuf_alloc(dst, src->cell_count) != 0) {
            return -1;
        }
    }
    
    memcpy(dst->cells, src->cells, src->cell_count * sizeof(uint32_t));
    dst->cell_count = src->cell_count;
    dst->track_length = src->track_length;
    dst->track_num = src->track_num;
    dst->head = src->head;
    dst->flux_count = src->flux_count;
    dst->weak_count = src->weak_count;
    dst->damaged_count = src->damaged_count;
    
    return 0;
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

/**
 * @brief Analyze track for weak bits
 */
int uft_tbuf_analyze(const uft_track_buffer_t *tbuf,
                      uint32_t *flux_count,
                      uint32_t *weak_count,
                      uint32_t *damaged_count)
{
    if (!tbuf || !tbuf->cells) return -1;
    
    uint32_t fc = 0, wc = 0, dc = 0;
    
    for (size_t i = 0; i < tbuf->cell_count; i++) {
        switch (tbuf->cells[i] & MG_MASK) {
            case MG_F: fc++; break;
            case MG_N: wc++; break;
            case MG_D: dc++; break;
            default: break;
        }
    }
    
    if (flux_count) *flux_count = fc;
    if (weak_count) *weak_count = wc;
    if (damaged_count) *damaged_count = dc;
    
    return 0;
}

/**
 * @brief Print track statistics
 */
void uft_tbuf_print_stats(FILE *out, const uft_track_buffer_t *tbuf)
{
    if (!out || !tbuf) return;
    
    uint32_t fc, wc, dc;
    uft_tbuf_analyze(tbuf, &fc, &wc, &dc);
    
    fprintf(out, "Track %d.%d Statistics:\n", tbuf->track_num, tbuf->head);
    fprintf(out, "  Cells:    %zu\n", tbuf->cell_count);
    fprintf(out, "  Flux:     %u\n", fc);
    fprintf(out, "  Weak:     %u (%.2f%%)\n", wc, 
            tbuf->cell_count ? (100.0f * wc / tbuf->cell_count) : 0);
    fprintf(out, "  Damaged:  %u\n", dc);
}

/*===========================================================================
 * Weak Bit Detection
 *===========================================================================*/

/**
 * @brief Detect weak bits from timing jitter (multi-revolution)
 * 
 * Compares multiple reads of the same track and marks cells
 * with high timing variance as weak.
 * 
 * @param revolutions Array of track buffers (one per revolution)
 * @param rev_count Number of revolutions
 * @param output Output track with weak bits marked
 * @param jitter_threshold Variance threshold (0.05 = 5%)
 * @return Number of weak bits detected
 */
size_t uft_detect_weak_from_revolutions(
    const uft_track_buffer_t *revolutions,
    size_t rev_count,
    uft_track_buffer_t *output,
    float jitter_threshold)
{
    if (!revolutions || !output || rev_count < 2) {
        return 0;
    }
    
    /* Use first revolution as reference */
    const uft_track_buffer_t *ref = &revolutions[0];
    
    if (uft_tbuf_alloc(output, ref->cell_count) != 0) {
        return 0;
    }
    
    size_t weak_detected = 0;
    uint32_t *samples = (uint32_t*)malloc(rev_count * sizeof(uint32_t));
    if (!samples) {
        return 0;
    }
    
    /* For each cell in reference */
    for (size_t i = 0; i < ref->cell_count; i++) {
        /* Collect timing from all revolutions */
        samples[0] = uft_mg_time(ref->cells[i]);
        size_t valid_samples = 1;
        
        for (size_t r = 1; r < rev_count; r++) {
            if (i < revolutions[r].cell_count) {
                samples[valid_samples++] = uft_mg_time(revolutions[r].cells[i]);
            }
        }
        
        /* Analyze jitter */
        uft_timing_stats_t stats;
        uft_analyze_timing_jitter(samples, valid_samples, &stats);
        
        /* Mark as weak if jitter exceeds threshold */
        if (uft_is_weak_from_jitter(&stats, jitter_threshold)) {
            output->cells[output->cell_count++] = uft_mg_weak((uint32_t)stats.mean);
            weak_detected++;
        } else {
            output->cells[output->cell_count++] = uft_mg_flux((uint32_t)stats.mean);
        }
    }
    
    free(samples);
    output->weak_count = (uint32_t)weak_detected;
    
    return weak_detected;
}

/*===========================================================================
 * Import/Export
 *===========================================================================*/

/**
 * @brief Import from raw flux data
 */
int uft_tbuf_from_flux(uft_track_buffer_t *tbuf,
                        const uint32_t *flux_times,
                        size_t count)
{
    if (!tbuf || !flux_times) return -1;
    
    if (uft_tbuf_alloc(tbuf, count) != 0) {
        return -1;
    }
    
    for (size_t i = 0; i < count; i++) {
        tbuf->cells[i] = uft_mg_flux(flux_times[i]);
    }
    
    tbuf->cell_count = count;
    tbuf->flux_count = (uint32_t)count;
    
    return 0;
}

/**
 * @brief Export to raw flux data (loses weak bit info)
 */
int uft_tbuf_to_flux(const uft_track_buffer_t *tbuf,
                      uint32_t *flux_times,
                      size_t *count)
{
    if (!tbuf || !flux_times || !count) return -1;
    
    size_t j = 0;
    for (size_t i = 0; i < tbuf->cell_count && j < *count; i++) {
        /* Only export flux transitions, skip weak/damaged */
        if (uft_mg_is_flux(tbuf->cells[i])) {
            flux_times[j++] = uft_mg_time(tbuf->cells[i]);
        }
    }
    
    *count = j;
    return 0;
}

/**
 * @brief Export with weak bit regions
 */
int uft_tbuf_to_flux_with_weak(
    const uft_track_buffer_t *tbuf,
    uint32_t *flux_times,
    size_t *flux_count,
    uft_weak_region_t *weak_regions,
    size_t *weak_count)
{
    if (!tbuf || !flux_times || !flux_count) return -1;
    
    /* Export flux */
    if (uft_tbuf_to_flux(tbuf, flux_times, flux_count) != 0) {
        return -1;
    }
    
    /* Export weak regions */
    if (weak_regions && weak_count) {
        *weak_count = uft_tbuf_find_weak_regions(tbuf, weak_regions, *weak_count);
    }
    
    return 0;
}
