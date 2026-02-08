/**
 * @file uft_decode_score.c
 * @brief Unified Decode Score Implementation
 */

#include "uft/uft_decode_score.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

void uft_score_sector(
    uft_decode_score_t *score,
    bool crc_ok,
    int cylinder, int head, int sector,
    int max_cylinder, int max_sector,
    double timing_jitter_ns,
    double timing_threshold_ns,
    bool protection_expected,
    bool protection_found)
{
    if (!score) return;
    
    uft_score_init(score);
    
    /* CRC Score (40 points) */
    score->crc_ok = crc_ok;
    score->crc_score = crc_ok ? 40 : 0;
    
    /* ID Score (15 points) */
    bool id_valid = (cylinder >= 0 && cylinder <= max_cylinder &&
                     head >= 0 && head <= 1 &&
                     sector >= 0 && sector <= max_sector);
    score->id_valid = id_valid;
    score->id_score = id_valid ? 15 : 0;
    
    /* Sequence Score - assumed OK if ID valid (15 points) */
    score->sequence_ok = id_valid;
    score->sequence_score = id_valid ? 15 : 0;
    
    /* Header Score - assumed OK if CRC OK (10 points) */
    score->header_score = crc_ok ? 10 : 5;
    
    /* Timing Score (15 points) */
    if (timing_threshold_ns > 0 && timing_jitter_ns >= 0) {
        double ratio = 1.0 - (timing_jitter_ns / timing_threshold_ns);
        if (ratio < 0) ratio = 0;
        if (ratio > 1) ratio = 1;
        score->timing_score = (uint8_t)(15.0 * ratio);
    } else {
        score->timing_score = 10; /* Default if no timing info */
    }
    
    /* Protection Score (5 points) */
    score->has_protection = protection_found;
    if (protection_expected) {
        score->protection_score = protection_found ? 5 : 0;
    } else {
        score->protection_score = protection_found ? 3 : 5; /* Unexpected = slight penalty */
    }
    
    /* Calculate total */
    uft_score_calculate_total(score);
    
    /* Confidence based on components */
    int confident_components = 0;
    if (crc_ok) confident_components += 2;
    if (id_valid) confident_components++;
    if (score->timing_score >= 10) confident_components++;
    score->confidence = (uint8_t)(confident_components * 25);
    
    /* Build reason string */
    snprintf(score->reason, sizeof(score->reason),
             "CRC:%s ID:%s Timing:%d%% Prot:%s",
             crc_ok ? "OK" : "BAD",
             id_valid ? "OK" : "BAD",
             (score->timing_score * 100) / 15,
             protection_found ? "YES" : "NO");
}

static char score_string_buffer[512];

const char* uft_score_to_string(const uft_decode_score_t *score)
{
    if (!score) return "NULL";
    
    snprintf(score_string_buffer, sizeof(score_string_buffer),
             "Score: %d/100 (CRC:%d ID:%d Seq:%d Hdr:%d Tim:%d Prot:%d) - %s",
             score->total,
             score->crc_score, score->id_score, score->sequence_score,
             score->header_score, score->timing_score, score->protection_score,
             score->reason);
    
    return score_string_buffer;
}
