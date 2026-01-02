/**
 * @file uft_flux_buffer.c
 * @brief Flux Buffer Layer Implementation
 * 
 * Hardware-unabhängige Abstraktion für Flux-Daten mit
 * Multi-Revolution Support und Normalisierung.
 */

#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Constants
// ============================================================================

#define FLUX_DEFAULT_SAMPLE_RATE    24000000    // 24 MHz (SCP default)
#define FLUX_DEFAULT_REV_CAPACITY   8
#define FLUX_DEFAULT_TRANS_CAPACITY 100000

// ============================================================================
// Revolution Management
// ============================================================================

static uft_error_t revolution_init(uft_flux_revolution_t* rev, size_t capacity) {
    if (!rev) return UFT_ERROR_NULL_POINTER;
    
    memset(rev, 0, sizeof(*rev));
    
    if (capacity > 0) {
        rev->transitions = calloc(capacity, sizeof(uft_flux_transition_t));
        if (!rev->transitions) return UFT_ERROR_NO_MEMORY;
        rev->capacity = capacity;
    }
    
    return UFT_OK;
}

static void revolution_cleanup(uft_flux_revolution_t* rev) {
    if (!rev) return;
    free(rev->transitions);
    memset(rev, 0, sizeof(*rev));
}

static uft_error_t revolution_append(uft_flux_revolution_t* rev, 
                                      uint32_t delta_ns, uint8_t flags) {
    if (!rev) return UFT_ERROR_NULL_POINTER;
    
    // Grow if needed
    if (rev->count >= rev->capacity) {
        size_t new_cap = rev->capacity ? rev->capacity * 2 : FLUX_DEFAULT_TRANS_CAPACITY;
        uft_flux_transition_t* new_trans = realloc(rev->transitions, 
                                                    new_cap * sizeof(uft_flux_transition_t));
        if (!new_trans) return UFT_ERROR_NO_MEMORY;
        rev->transitions = new_trans;
        rev->capacity = new_cap;
    }
    
    rev->transitions[rev->count].delta_ns = delta_ns;
    rev->transitions[rev->count].flags = flags;
    rev->count++;
    rev->total_time_ns += delta_ns;
    
    return UFT_OK;
}

// ============================================================================
// Flux Track Creation / Destruction
// ============================================================================

uft_flux_track_data_t* uft_flux_track_alloc(int cyl, int head) {
    uft_flux_track_data_t* track = calloc(1, sizeof(uft_flux_track_data_t));
    if (!track) return NULL;
    
    track->cylinder = cyl;
    track->head = head;
    track->source_sample_rate_hz = FLUX_DEFAULT_SAMPLE_RATE;
    
    track->revolutions = calloc(FLUX_DEFAULT_REV_CAPACITY, sizeof(uft_flux_revolution_t));
    if (!track->revolutions) {
        free(track);
        return NULL;
    }
    track->revolution_capacity = FLUX_DEFAULT_REV_CAPACITY;
    
    return track;
}

void uft_flux_track_free(uft_flux_track_data_t* track) {
    if (!track) return;
    
    if (track->revolutions) {
        for (size_t i = 0; i < track->revolution_count; i++) {
            revolution_cleanup(&track->revolutions[i]);
        }
        free(track->revolutions);
    }
    
    free(track);
}

// ============================================================================
// Revolution Management
// ============================================================================

uft_error_t uft_flux_track_begin_revolution(uft_flux_track_data_t* track) {
    if (!track) return UFT_ERROR_NULL_POINTER;
    
    // Grow array if needed
    if (track->revolution_count >= track->revolution_capacity) {
        size_t new_cap = track->revolution_capacity * 2;
        uft_flux_revolution_t* new_revs = realloc(track->revolutions,
                                                   new_cap * sizeof(uft_flux_revolution_t));
        if (!new_revs) return UFT_ERROR_NO_MEMORY;
        
        // Zero new entries
        memset(&new_revs[track->revolution_capacity], 0,
               (new_cap - track->revolution_capacity) * sizeof(uft_flux_revolution_t));
        
        track->revolutions = new_revs;
        track->revolution_capacity = new_cap;
    }
    
    // Initialize new revolution
    return revolution_init(&track->revolutions[track->revolution_count], 
                          FLUX_DEFAULT_TRANS_CAPACITY);
}

uft_error_t uft_flux_track_end_revolution(uft_flux_track_data_t* track, uint32_t index_pos) {
    if (!track || track->revolution_count >= track->revolution_capacity) {
        return UFT_ERROR_INVALID_STATE;
    }
    
    uft_flux_revolution_t* rev = &track->revolutions[track->revolution_count];
    rev->index_position = index_pos;
    
    // Calculate RPM
    if (rev->total_time_ns > 0) {
        double seconds = (double)rev->total_time_ns / 1000000000.0;
        rev->rpm = 60.0 / seconds;
    }
    
    track->revolution_count++;
    
    // Update track statistics
    double sum_rpm = 0;
    for (size_t i = 0; i < track->revolution_count; i++) {
        sum_rpm += track->revolutions[i].rpm;
    }
    track->avg_rpm = sum_rpm / track->revolution_count;
    
    // Calculate stddev
    if (track->revolution_count > 1) {
        double sum_sq = 0;
        for (size_t i = 0; i < track->revolution_count; i++) {
            double diff = track->revolutions[i].rpm - track->avg_rpm;
            sum_sq += diff * diff;
        }
        track->rpm_stddev = sqrt(sum_sq / (track->revolution_count - 1));
    }
    
    return UFT_OK;
}

uft_error_t uft_flux_track_add_transition(uft_flux_track_data_t* track,
                                           uint32_t delta_ns, uint8_t flags) {
    if (!track || track->revolution_count == 0) {
        return UFT_ERROR_INVALID_STATE;
    }
    
    // Add to current (last) revolution
    uft_flux_revolution_t* rev = &track->revolutions[track->revolution_count - 1];
    return revolution_append(rev, delta_ns, flags);
}

// ============================================================================
// Import from Raw Samples
// ============================================================================

uft_error_t uft_flux_track_from_samples(uft_flux_track_data_t* track,
                                         const uint32_t* samples,
                                         size_t sample_count,
                                         uint32_t sample_rate_hz,
                                         const uint32_t* index_positions,
                                         size_t index_count) {
    if (!track || !samples || sample_count == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    track->source_sample_rate_hz = sample_rate_hz;
    double ns_per_tick = 1000000000.0 / (double)sample_rate_hz;
    
    // If no index positions, treat as single revolution
    if (!index_positions || index_count == 0) {
        uft_error_t err = uft_flux_track_begin_revolution(track);
        if (err != UFT_OK) return err;
        
        for (size_t i = 0; i < sample_count; i++) {
            uint32_t delta_ns = (uint32_t)(samples[i] * ns_per_tick);
            err = uft_flux_track_add_transition(track, delta_ns, 0);
            if (err != UFT_OK) return err;
        }
        
        return uft_flux_track_end_revolution(track, 0);
    }
    
    // Multiple revolutions based on index positions
    size_t sample_idx = 0;
    
    for (size_t rev_idx = 0; rev_idx < index_count; rev_idx++) {
        uft_error_t err = uft_flux_track_begin_revolution(track);
        if (err != UFT_OK) return err;
        
        uint32_t end_pos = (rev_idx + 1 < index_count) ? 
                           index_positions[rev_idx + 1] : (uint32_t)sample_count;
        
        while (sample_idx < end_pos && sample_idx < sample_count) {
            uint32_t delta_ns = (uint32_t)(samples[sample_idx] * ns_per_tick);
            uint8_t flags = (sample_idx == index_positions[rev_idx]) ? 
                            UFT_FLUX_FLAG_INDEX : 0;
            
            err = uft_flux_track_add_transition(track, delta_ns, flags);
            if (err != UFT_OK) return err;
            
            sample_idx++;
        }
        
        err = uft_flux_track_end_revolution(track, index_positions[rev_idx]);
        if (err != UFT_OK) return err;
    }
    
    return UFT_OK;
}

// ============================================================================
// Export to Raw Samples
// ============================================================================

uft_error_t uft_flux_track_to_samples(const uft_flux_track_data_t* track,
                                       int revolution,
                                       uint32_t target_rate_hz,
                                       uint32_t** samples,
                                       size_t* sample_count) {
    if (!track || !samples || !sample_count) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    if (revolution < 0 || (size_t)revolution >= track->revolution_count) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    const uft_flux_revolution_t* rev = &track->revolutions[revolution];
    
    *samples = malloc(rev->count * sizeof(uint32_t));
    if (!*samples) return UFT_ERROR_NO_MEMORY;
    
    double scale = (double)target_rate_hz / 1000000000.0;  // ns to ticks
    
    for (size_t i = 0; i < rev->count; i++) {
        (*samples)[i] = (uint32_t)(rev->transitions[i].delta_ns * scale);
    }
    
    *sample_count = rev->count;
    return UFT_OK;
}

// ============================================================================
// Statistics
// ============================================================================

void uft_flux_track_get_stats(const uft_flux_track_data_t* track,
                               double* avg_rpm,
                               double* rpm_stddev,
                               size_t* total_transitions) {
    if (!track) return;
    
    if (avg_rpm) *avg_rpm = track->avg_rpm;
    if (rpm_stddev) *rpm_stddev = track->rpm_stddev;
    
    if (total_transitions) {
        size_t total = 0;
        for (size_t i = 0; i < track->revolution_count; i++) {
            total += track->revolutions[i].count;
        }
        *total_transitions = total;
    }
}

double uft_flux_track_get_bit_rate(const uft_flux_track_data_t* track) {
    if (!track || track->revolution_count == 0) return 0.0;
    
    // Use first revolution
    const uft_flux_revolution_t* rev = &track->revolutions[0];
    if (rev->total_time_ns == 0) return 0.0;
    
    // Estimate bit rate: transitions per second * 2 (MFM has 2 cells per bit)
    double seconds = (double)rev->total_time_ns / 1000000000.0;
    return (double)rev->count / seconds / 2.0;
}

// ============================================================================
// Histogram Analysis
// ============================================================================

uft_error_t uft_flux_track_histogram(const uft_flux_track_data_t* track,
                                      int revolution,
                                      uint32_t bin_width_ns,
                                      uint32_t max_bins,
                                      uint32_t* histogram,
                                      size_t* bin_count) {
    if (!track || !histogram || !bin_count || bin_width_ns == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    if (revolution < 0 || (size_t)revolution >= track->revolution_count) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    memset(histogram, 0, max_bins * sizeof(uint32_t));
    
    const uft_flux_revolution_t* rev = &track->revolutions[revolution];
    uint32_t max_used_bin = 0;
    
    for (size_t i = 0; i < rev->count; i++) {
        uint32_t bin = rev->transitions[i].delta_ns / bin_width_ns;
        if (bin < max_bins) {
            histogram[bin]++;
            if (bin > max_used_bin) max_used_bin = bin;
        }
    }
    
    *bin_count = max_used_bin + 1;
    return UFT_OK;
}

// ============================================================================
// Merge Revolutions (for better decoding)
// ============================================================================

uft_error_t uft_flux_track_merge_revolutions(const uft_flux_track_data_t* track,
                                              uft_flux_revolution_t* merged) {
    if (!track || !merged || track->revolution_count == 0) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    // Calculate total transitions needed
    size_t total = 0;
    for (size_t i = 0; i < track->revolution_count; i++) {
        total += track->revolutions[i].count;
    }
    
    uft_error_t err = revolution_init(merged, total);
    if (err != UFT_OK) return err;
    
    // Copy all transitions
    for (size_t r = 0; r < track->revolution_count; r++) {
        const uft_flux_revolution_t* rev = &track->revolutions[r];
        for (size_t i = 0; i < rev->count; i++) {
            merged->transitions[merged->count] = rev->transitions[i];
            merged->count++;
            merged->total_time_ns += rev->transitions[i].delta_ns;
        }
    }
    
    // Average RPM
    merged->rpm = track->avg_rpm;
    
    return UFT_OK;
}
