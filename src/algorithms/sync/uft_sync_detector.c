/**
 * @file uft_sync_detector.c
 * @brief Multi-layer sync pattern detection implementation
 * 
 * Fixes algorithmic weakness #2: False positive sync detection
 */

#include "uft_sync_detector.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static uft_sync_type_t identify_sync_type(uint8_t mark_byte) {
    switch (mark_byte) {
        case 0xFE: return UFT_SYNC_TYPE_IDAM;
        case 0xFB: return UFT_SYNC_TYPE_DAM;
        case 0xF8: return UFT_SYNC_TYPE_DDAM;
        case 0xFC: return UFT_SYNC_TYPE_IAM;
        case 0xFD: return UFT_SYNC_TYPE_DAM;  /* Alternate DAM */
        case 0xFF: return UFT_SYNC_TYPE_IDAM; /* Alternate IDAM */
        default:   return UFT_SYNC_TYPE_UNKNOWN;
    }
}

static double calculate_timing_score(uft_sync_detector_t *det, size_t pos) {
    if (det->last_sync_pos == 0) {
        /* First sync - no timing reference */
        return 30.0;  /* Base score */
    }
    
    double gap = (double)(pos - det->last_sync_pos);
    double expected = det->expected_gap;
    double tolerance = expected * det->gap_tolerance;
    
    if (fabs(gap - expected) < tolerance) {
        /* Within tolerance */
        double error = fabs(gap - expected) / expected;
        return 50.0 * (1.0 - error / det->gap_tolerance);
    }
    
    /* Check for multiples (interleaved sectors) */
    for (int mult = 2; mult <= 4; mult++) {
        double multi_expected = expected * mult;
        if (fabs(gap - multi_expected) < tolerance * mult) {
            return 30.0;  /* Reduced score for multiples */
        }
    }
    
    return 0.0;  /* No timing match */
}

static double calculate_context_score(uft_sync_detector_t *det) {
    /* Check if context bytes suggest we're in a gap/format area */
    int gap_bytes = 0;
    int data_bytes = 0;
    
    for (size_t i = 0; i < 8; i++) {
        uint8_t b = det->context_bytes[i];
        if (b == 0x00 || b == 0x4E) {
            gap_bytes++;
        } else if (b == 0xA1 || b == 0xC2) {
            /* Sync-related byte in context - good! */
            return 30.0;
        } else {
            data_bytes++;
        }
    }
    
    /* More gap bytes = more likely real sync location */
    if (gap_bytes >= 4) return 20.0;
    if (gap_bytes >= 2) return 10.0;
    
    return 0.0;
}

static bool validate_candidate(uft_sync_detector_t *det,
                               uft_sync_candidate_t *cand) {
    /* Calculate total confidence */
    cand->timing_score = calculate_timing_score(det, cand->bit_position);
    cand->context_score = calculate_context_score(det);
    
    /* Base score from pattern match */
    double base_score = 30.0;
    
    /* Missing clock is essential for true sync */
    if (cand->has_missing_clock) {
        base_score += 20.0;
    }
    
    /* Total confidence */
    cand->confidence = (uint8_t)(base_score + 
                                 cand->timing_score + 
                                 cand->context_score);
    
    if (cand->confidence > 100) cand->confidence = 100;
    
    /* In strict mode, require high confidence */
    if (det->strict_mode) {
        return cand->confidence >= UFT_SYNC_MIN_CONFIDENCE;
    }
    
    /* Otherwise, accept anything above 50 */
    return cand->confidence >= 50;
}

static void add_candidate(uft_sync_detector_t *det,
                          uft_sync_candidate_t *cand) {
    det->total_candidates++;
    
    /* Check minimum separation from last sync */
    if (det->last_sync_pos > 0 && 
        (cand->bit_position - det->last_sync_pos) < det->min_sync_separation) {
        det->rejected_syncs++;
        return;
    }
    
    /* Validate */
    if (!validate_candidate(det, cand)) {
        det->rejected_syncs++;
        return;
    }
    
    /* Add to list if space */
    if (det->candidate_count < UFT_SYNC_MAX_CANDIDATES) {
        det->candidates[det->candidate_count++] = *cand;
        det->accepted_syncs++;
        det->last_sync_pos = cand->bit_position;
    }
}

/* ============================================================================
 * Public API
 * ============================================================================ */

void uft_sync_init(uft_sync_detector_t *det) {
    if (!det) return;
    memset(det, 0, sizeof(*det));
    
    /* Defaults */
    det->expected_gap = 1000;  /* ~1000 bits between syncs */
    det->gap_tolerance = 0.2;  /* ±20% */
    det->min_sync_separation = 100;  /* At least 100 bits apart */
    det->strict_mode = false;
}

void uft_sync_configure(uft_sync_detector_t *det,
                        double expected_gap,
                        double tolerance) {
    if (!det) return;
    det->expected_gap = expected_gap;
    det->gap_tolerance = tolerance;
}

void uft_sync_reset(uft_sync_detector_t *det) {
    if (!det) return;
    
    det->bit_window = 0;
    det->bit_count = 0;
    det->current_bit_pos = 0;
    det->candidate_count = 0;
    det->last_sync_pos = 0;
    det->context_idx = 0;
    memset(det->context_bytes, 0, sizeof(det->context_bytes));
    
    /* Keep statistics and configuration */
}

bool uft_sync_feed_bit(uft_sync_detector_t *det,
                       uint8_t bit,
                       uft_sync_candidate_t *out_sync) {
    if (!det) return false;
    
    /* Shift bit into window */
    det->bit_window = (det->bit_window << 1) | (bit & 1);
    det->bit_count++;
    det->current_bit_pos++;
    
    /* Need at least 16 bits for MFM sync pattern */
    if (det->bit_count < 16) return false;
    
    /* Extract current MFM word */
    uint16_t mfm_word = (uint16_t)(det->bit_window & 0xFFFF);
    
    return uft_sync_feed_mfm(det, mfm_word, det->current_bit_pos, out_sync);
}

int uft_sync_feed_byte(uft_sync_detector_t *det,
                       uint8_t byte,
                       uft_sync_candidate_t *out_syncs) {
    if (!det) return 0;
    
    int count = 0;
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (byte >> i) & 1;
        uft_sync_candidate_t sync;
        if (uft_sync_feed_bit(det, bit, &sync)) {
            if (out_syncs) {
                out_syncs[count] = sync;
            }
            count++;
        }
    }
    
    /* Add to context */
    uft_sync_add_context(det, byte);
    
    return count;
}

bool uft_sync_feed_mfm(uft_sync_detector_t *det,
                       uint16_t mfm_word,
                       size_t bit_pos,
                       uft_sync_candidate_t *out_sync) {
    if (!det) return false;
    
    bool is_sync = false;
    uft_sync_candidate_t cand = {0};
    cand.bit_position = bit_pos;
    cand.mfm_pattern = mfm_word;
    
    /* Check for MFM sync patterns */
    if (mfm_word == UFT_SYNC_MFM_A1) {
        /* A1 with missing clock */
        cand.has_missing_clock = true;
        cand.mark_byte = 0xA1;
        is_sync = true;
    } else if (mfm_word == UFT_SYNC_MFM_C2) {
        /* C2 with missing clock */
        cand.has_missing_clock = true;
        cand.mark_byte = 0xC2;
        is_sync = true;
    } else if (mfm_word == UFT_SYNC_MFM_A1_DECAY) {
        /* Decayed A1 - lower confidence */
        cand.has_missing_clock = true;
        cand.mark_byte = 0xA1;
        is_sync = true;
    }
    
    if (!is_sync) return false;
    
    /* Need to look at next byte for address mark */
    /* For now, mark as unknown type */
    cand.type = UFT_SYNC_TYPE_UNKNOWN;
    
    /* Validate and add */
    add_candidate(det, &cand);
    
    if (out_sync && det->candidate_count > 0) {
        *out_sync = det->candidates[det->candidate_count - 1];
        return true;
    }
    
    return det->candidate_count > 0;
}

void uft_sync_add_context(uft_sync_detector_t *det, uint8_t byte) {
    if (!det) return;
    det->context_bytes[det->context_idx] = byte;
    det->context_idx = (det->context_idx + 1) & 7;
}

const uft_sync_candidate_t* uft_sync_get_best(const uft_sync_detector_t *det) {
    if (!det || det->candidate_count == 0) return NULL;
    
    const uft_sync_candidate_t *best = &det->candidates[0];
    for (size_t i = 1; i < det->candidate_count; i++) {
        if (det->candidates[i].confidence > best->confidence) {
            best = &det->candidates[i];
        }
    }
    
    return best;
}

void uft_sync_clear_candidates(uft_sync_detector_t *det) {
    if (!det) return;
    det->candidate_count = 0;
}

const char* uft_sync_type_name(uft_sync_type_t type) {
    switch (type) {
        case UFT_SYNC_TYPE_IDAM:    return "IDAM";
        case UFT_SYNC_TYPE_DAM:     return "DAM";
        case UFT_SYNC_TYPE_DDAM:    return "DDAM";
        case UFT_SYNC_TYPE_IAM:     return "IAM";
        case UFT_SYNC_TYPE_UNKNOWN: return "UNKNOWN";
        default:                    return "NONE";
    }
}

void uft_sync_dump_status(const uft_sync_detector_t *det) {
    if (!det) {
        printf("Sync Detector: NULL\n");
        return;
    }
    
    printf("=== Sync Detector Status ===\n");
    printf("Bit position: %zu\n", det->current_bit_pos);
    printf("Candidates: %zu\n", det->candidate_count);
    printf("Statistics: total=%zu accepted=%zu rejected=%zu\n",
           det->total_candidates, det->accepted_syncs, det->rejected_syncs);
    printf("Timing: expected=%.0f tolerance=%.1f%%\n",
           det->expected_gap, det->gap_tolerance * 100);
    
    for (size_t i = 0; i < det->candidate_count; i++) {
        const uft_sync_candidate_t *c = &det->candidates[i];
        printf("  [%zu] pos=%zu mfm=%04X type=%s conf=%u\n",
               i, c->bit_position, c->mfm_pattern,
               uft_sync_type_name(c->type), c->confidence);
    }
}
