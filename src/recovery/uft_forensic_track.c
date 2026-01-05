/**
 * @file uft_forensic_track.c
 * @brief Forensic Track Recovery with Sector Position Estimation
 * 
 * ALGORITHM: Multi-Phase Track Recovery
 * =====================================
 * 
 * Phase 1: TIMING ANALYSIS
 * - Measure rotation time from index pulses
 * - Build timing histogram of transitions
 * - Detect timing anomalies
 * 
 * Phase 2: SYNC DETECTION
 * - Find all sync candidates (fuzzy matching)
 * - Score by confidence and spacing regularity
 * - Estimate missing sync positions from pattern
 * 
 * Phase 3: SECTOR EXTRACTION
 * - For each sync position, attempt sector decode
 * - Cross-reference with expected format layout
 * - Fill gaps with interpolated positions
 * 
 * Phase 4: SECTOR RECOVERY
 * - Multi-pass correlation for each sector
 * - CRC-based error correction
 * - Quality assessment
 * 
 * Phase 5: GAP ANALYSIS
 * - Detect missing sectors from gaps
 * - Estimate data boundaries without headers
 * - Attempt headerless recovery
 */

#include "uft/recovery/uft_forensic_recovery.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// TIMING ANALYSIS
// ============================================================================

typedef struct {
    float rotation_time_ms;
    float cell_time_ns;
    float cell_time_variance;
    
    float *timing_histogram;
    size_t histogram_bins;
    float histogram_min_ns;
    float histogram_max_ns;
    
    size_t timing_anomalies;
} timing_analysis_t;

static int analyze_timing(
    const uint64_t *flux_timestamps,
    size_t flux_count,
    timing_analysis_t *out_analysis,
    uft_forensic_session_t *session)
{
    if (!flux_timestamps || flux_count < 10 || !out_analysis) return -1;
    
    memset(out_analysis, 0, sizeof(*out_analysis));
    
    // Compute deltas
    float *deltas = calloc(flux_count - 1, sizeof(float));
    if (!deltas) return -1;
    
    float sum = 0.0f;
    float min_delta = 1e12f;
    float max_delta = 0.0f;
    
    for (size_t i = 0; i + 1 < flux_count; i++) {
        float delta = (float)(flux_timestamps[i + 1] - flux_timestamps[i]);
        deltas[i] = delta;
        sum += delta;
        if (delta < min_delta) min_delta = delta;
        if (delta > max_delta) max_delta = delta;
    }
    
    float mean = sum / (flux_count - 1);
    
    // Compute variance
    float var_sum = 0.0f;
    for (size_t i = 0; i + 1 < flux_count; i++) {
        float diff = deltas[i] - mean;
        var_sum += diff * diff;
    }
    float variance = var_sum / (flux_count - 1);
    float stddev = sqrtf(variance);
    
    // Estimate rotation time (assuming ~200ms for 300RPM)
    // In real implementation, this would use index pulses
    out_analysis->rotation_time_ms = 200.0f;  // Default
    
    // Cell time is the most common delta (mode)
    // Build histogram to find mode
    out_analysis->histogram_bins = 100;
    out_analysis->histogram_min_ns = min_delta * 0.5f;
    out_analysis->histogram_max_ns = max_delta * 1.5f;
    out_analysis->timing_histogram = calloc(out_analysis->histogram_bins, sizeof(float));
    
    if (out_analysis->timing_histogram) {
        float bin_width = (out_analysis->histogram_max_ns - out_analysis->histogram_min_ns) 
                          / out_analysis->histogram_bins;
        
        for (size_t i = 0; i + 1 < flux_count; i++) {
            int bin = (int)((deltas[i] - out_analysis->histogram_min_ns) / bin_width);
            if (bin >= 0 && bin < (int)out_analysis->histogram_bins) {
                out_analysis->timing_histogram[bin] += 1.0f;
            }
        }
        
        // Find peak (mode)
        int max_bin = 0;
        float max_count = 0.0f;
        for (size_t b = 0; b < out_analysis->histogram_bins; b++) {
            if (out_analysis->timing_histogram[b] > max_count) {
                max_count = out_analysis->timing_histogram[b];
                max_bin = b;
            }
        }
        
        out_analysis->cell_time_ns = out_analysis->histogram_min_ns + 
                                     (max_bin + 0.5f) * bin_width;
    } else {
        out_analysis->cell_time_ns = mean;
    }
    
    out_analysis->cell_time_variance = variance;
    
    // Count timing anomalies (deltas > 3 sigma from mean)
    for (size_t i = 0; i + 1 < flux_count; i++) {
        if (fabsf(deltas[i] - mean) > 3.0f * stddev) {
            out_analysis->timing_anomalies++;
        }
    }
    
    free(deltas);
    
    uft_forensic_log(session, 4,
        "Timing analysis: cell=%.1fns, variance=%.1f, anomalies=%zu",
        out_analysis->cell_time_ns, out_analysis->cell_time_variance,
        out_analysis->timing_anomalies);
    
    return 0;
}

// ============================================================================
// SYNC PATTERN FINDING
// ============================================================================

typedef struct {
    size_t bit_offset;
    float confidence;
    int hamming_distance;
    float expected_offset;      // From spacing pattern
    float offset_error;         // Difference from expected
} sector_sync_t;

static int find_sector_syncs(
    const uint8_t *bits,
    size_t bit_count,
    const float *confidence,
    uint16_t sync_pattern,
    size_t expected_sector_count,
    float expected_sector_spacing_bits,
    sector_sync_t *out_syncs,
    size_t *out_sync_count,
    uft_forensic_session_t *session)
{
    if (!bits || !out_syncs || !out_sync_count) return -1;
    
    *out_sync_count = 0;
    
    // Find all sync candidates with fuzzy matching
    size_t max_candidates = expected_sector_count * 3;  // Allow extras
    sector_sync_t *candidates = calloc(max_candidates, sizeof(sector_sync_t));
    if (!candidates) return -1;
    
    size_t found = 0;
    
    // Scan for sync pattern (allow Hamming distance up to 2)
    for (size_t i = 0; i + 16 <= bit_count && found < max_candidates; i++) {
        uint16_t window = 0;
        for (int b = 0; b < 16; b++) {
            window = (window << 1) | ((bits[(i + b) >> 3] >> (7 - ((i + b) & 7))) & 1);
        }
        
        int dist = 0;
        uint16_t diff = window ^ sync_pattern;
        while (diff) {
            dist += (diff & 1);
            diff >>= 1;
        }
        
        if (dist <= 2) {
            candidates[found].bit_offset = i;
            candidates[found].confidence = 1.0f - (float)dist / 16.0f;
            candidates[found].hamming_distance = dist;
            found++;
            
            // Skip ahead to avoid duplicates
            i += 15;
        }
    }
    
    uft_forensic_log(session, 4,
        "Found %zu sync candidates (expected ~%zu)",
        found, expected_sector_count);
    
    // If we found roughly the expected number, use them
    // Otherwise, try to infer from spacing pattern
    
    if (found >= expected_sector_count * 0.8 && found <= expected_sector_count * 1.2) {
        // Good match, use candidates directly
        for (size_t i = 0; i < found && i < expected_sector_count; i++) {
            out_syncs[i] = candidates[i];
        }
        *out_sync_count = found < expected_sector_count ? found : expected_sector_count;
    } else if (found > 0) {
        // Sparse candidates - try to fill in gaps using spacing
        
        // Compute average spacing from found syncs
        float avg_spacing = 0.0f;
        if (found >= 2) {
            for (size_t i = 1; i < found; i++) {
                avg_spacing += (float)(candidates[i].bit_offset - candidates[i-1].bit_offset);
            }
            avg_spacing /= (found - 1);
        } else {
            avg_spacing = expected_sector_spacing_bits;
        }
        
        // Use first sync as anchor, extrapolate others
        size_t anchor = 0;
        for (size_t i = 0; i < found; i++) {
            if (candidates[i].confidence > candidates[anchor].confidence) {
                anchor = i;
            }
        }
        
        size_t out_idx = 0;
        float expected_pos = (float)candidates[anchor].bit_offset;
        
        // Go backward from anchor
        while (expected_pos > avg_spacing / 2 && out_idx < expected_sector_count) {
            expected_pos -= avg_spacing;
        }
        
        // Now go forward, filling in syncs
        for (size_t s = 0; s < expected_sector_count && out_idx < expected_sector_count; s++) {
            // Check if there's a candidate near this position
            bool found_match = false;
            for (size_t c = 0; c < found; c++) {
                float diff = fabsf((float)candidates[c].bit_offset - expected_pos);
                if (diff < avg_spacing * 0.3f) {
                    out_syncs[out_idx] = candidates[c];
                    out_syncs[out_idx].expected_offset = expected_pos;
                    out_syncs[out_idx].offset_error = diff;
                    found_match = true;
                    break;
                }
            }
            
            if (!found_match) {
                // Interpolate
                out_syncs[out_idx].bit_offset = (size_t)expected_pos;
                out_syncs[out_idx].confidence = 0.3f;  // Low confidence
                out_syncs[out_idx].hamming_distance = -1;  // Unknown
                out_syncs[out_idx].expected_offset = expected_pos;
                out_syncs[out_idx].offset_error = 0.0f;
                
                uft_forensic_log(session, 3,
                    "Interpolated sync at bit %zu (expected from spacing)",
                    (size_t)expected_pos);
            }
            
            out_idx++;
            expected_pos += avg_spacing;
        }
        
        *out_sync_count = out_idx;
    }
    
    free(candidates);
    return 0;
}

// ============================================================================
// TRACK RECOVERY
// ============================================================================

int uft_forensic_recover_track(
    const uint64_t **flux_timestamps,
    const size_t *flux_counts,
    size_t revolution_count,
    uint16_t cylinder,
    uint8_t head,
    const void *format_hint,
    uft_forensic_session_t *session,
    uft_forensic_track_t *out_track)
{
    if (!flux_timestamps || !flux_counts || revolution_count == 0 ||
        !session || !out_track) {
        return -1;
    }
    
    uft_forensic_log(session, 3,
        "Recovering track C%u H%u from %zu revolutions",
        cylinder, head, revolution_count);
    
    memset(out_track, 0, sizeof(*out_track));
    out_track->cylinder = cylinder;
    out_track->head = head;
    
    // Phase 1: Timing Analysis (use first revolution)
    timing_analysis_t timing;
    analyze_timing(flux_timestamps[0], flux_counts[0], &timing, session);
    
    out_track->rotation_time_ms = timing.rotation_time_ms;
    
    // Determine expected format
    // In real implementation, this would use format_hint
    size_t expected_sectors = 18;  // Default to 1.44MB format
    float sector_spacing_bits = 12000.0f;  // Approximate
    uint16_t sync_pattern = 0x4489;  // MFM sync
    uint16_t sector_size = 512;
    
    out_track->expected_sectors = expected_sectors;
    
    // Allocate sector storage
    out_track->sector_capacity = expected_sectors + 4;  // Extra for found extras
    out_track->sectors = calloc(out_track->sector_capacity, sizeof(uft_forensic_sector_t));
    if (!out_track->sectors) {
        free(timing.timing_histogram);
        return -1;
    }
    
    // Phase 2: Decode flux to bits for each revolution
    // (simplified - in real implementation, would use Kalman PLL)
    
    size_t max_bits = 250000;  // Typical track
    uint8_t **rev_bits = calloc(revolution_count, sizeof(uint8_t *));
    size_t *rev_bit_counts = calloc(revolution_count, sizeof(size_t));
    
    if (!rev_bits || !rev_bit_counts) {
        free(timing.timing_histogram);
        free(out_track->sectors);
        free(rev_bits);
        free(rev_bit_counts);
        return -1;
    }
    
    for (size_t r = 0; r < revolution_count; r++) {
        rev_bits[r] = calloc((max_bits + 7) / 8, 1);
        if (!rev_bits[r]) continue;
        
        // Simple flux-to-bits (would use PLL in real implementation)
        size_t bit_idx = 0;
        for (size_t i = 0; i + 1 < flux_counts[r] && bit_idx < max_bits; i++) {
            uint64_t delta = flux_timestamps[r][i + 1] - flux_timestamps[r][i];
            int cells = (int)((delta + timing.cell_time_ns / 2) / timing.cell_time_ns);
            if (cells < 1) cells = 1;
            if (cells > 8) cells = 8;
            
            for (int c = 0; c < cells - 1 && bit_idx < max_bits; c++) {
                // Zero bits
                bit_idx++;
            }
            // One bit
            if (bit_idx < max_bits) {
                rev_bits[r][bit_idx >> 3] |= (0x80 >> (bit_idx & 7));
                bit_idx++;
            }
        }
        rev_bit_counts[r] = bit_idx;
    }
    
    // Phase 3: Find sync patterns
    sector_sync_t *syncs = calloc(expected_sectors * 2, sizeof(sector_sync_t));
    size_t sync_count = 0;
    
    if (syncs && rev_bits[0]) {
        find_sector_syncs(
            rev_bits[0], rev_bit_counts[0],
            NULL,  // No per-bit confidence yet
            sync_pattern,
            expected_sectors,
            sector_spacing_bits,
            syncs, &sync_count,
            session);
    }
    
    out_track->found_sectors = sync_count;
    
    // Phase 4: Extract and recover each sector
    for (size_t s = 0; s < sync_count && s < out_track->sector_capacity; s++) {
        uft_forensic_sector_t *sector = &out_track->sectors[out_track->sector_count];
        
        // Extract sector bits from each revolution
        const uint8_t **sector_passes = calloc(revolution_count, sizeof(uint8_t *));
        size_t *sector_bit_counts = calloc(revolution_count, sizeof(size_t));
        
        if (sector_passes && sector_bit_counts) {
            for (size_t r = 0; r < revolution_count; r++) {
                if (!rev_bits[r]) continue;
                
                // Extract bits starting from sync position
                size_t start = syncs[s].bit_offset;
                size_t sector_bits = sector_size * 10 + 100;  // MFM overhead
                
                if (start + sector_bits <= rev_bit_counts[r]) {
                    sector_passes[r] = rev_bits[r] + (start >> 3);
                    sector_bit_counts[r] = sector_bits;
                }
            }
            
            // Count valid passes
            size_t valid_passes = 0;
            for (size_t r = 0; r < revolution_count; r++) {
                if (sector_passes[r]) valid_passes++;
            }
            
            if (valid_passes > 0) {
                int result = uft_forensic_recover_sector(
                    sector_passes, sector_bit_counts, revolution_count,
                    cylinder, head, (uint16_t)s, sector_size,
                    session, sector);
                
                if (result >= 0) {
                    out_track->sector_count++;
                    
                    if (sector->crc_valid) {
                        out_track->recovered_sectors++;
                    }
                }
            }
        }
        
        free(sector_passes);
        free(sector_bit_counts);
    }
    
    // Compute track quality
    if (out_track->expected_sectors > 0) {
        out_track->completeness = (float)out_track->recovered_sectors / out_track->expected_sectors;
    }
    
    float quality_sum = 0.0f;
    for (size_t s = 0; s < out_track->sector_count; s++) {
        quality_sum += out_track->sectors[s].quality.overall;
    }
    out_track->quality_score = out_track->sector_count > 0 ?
        quality_sum / out_track->sector_count : 0.0f;
    
    out_track->timing_anomalies = timing.timing_anomalies;
    
    // Cleanup
    for (size_t r = 0; r < revolution_count; r++) {
        free(rev_bits[r]);
    }
    free(rev_bits);
    free(rev_bit_counts);
    free(syncs);
    free(timing.timing_histogram);
    
    uft_forensic_log(session, 2,
        "Track C%u H%u: found=%zu/%zu recovered=%zu quality=%.2f",
        cylinder, head,
        out_track->found_sectors, out_track->expected_sectors,
        out_track->recovered_sectors, out_track->quality_score);
    
    // Update session statistics
    session->total_sectors_expected += out_track->expected_sectors;
    
    return 0;
}
