/**
 * @file uft_writer_verify.c
 * @brief UFT Writer Verification System Implementation
 * @version 3.2.0.003
 * @date 2026-01-04
 *
 * S-008: Writer Verification
 *
 * Comprehensive verification after writing to physical media.
 *
 * "Garantie dass geschriebene Daten korrekt sind"
 */

#include "uft/uft_writer_verify.h"
#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>

/*============================================================================
 * Static Helper Functions
 *============================================================================*/

/**
 * @brief Generate UUID for session
 */
static void generate_uuid(char *buffer, size_t size) {
    static const char hex[] = "0123456789abcdef";
    if (size < 37) return;
    
    srand((unsigned int)time(NULL) ^ (unsigned int)clock());
    
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            buffer[i] = '-';
        } else {
            buffer[i] = hex[rand() % 16];
        }
    }
    buffer[36] = '\0';
}

/**
 * @brief CRC32 lookup table
 */
static uint32_t crc32_table[256];
static bool crc32_initialized = false;

static void init_crc32_table(void) {
    if (crc32_initialized) return;
    
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            /* Fix C4146: avoid unary minus on unsigned - use ternary instead */
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320U : 0U);
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

/**
 * @brief Count differing bits between two bytes
 */
static int popcount8(uint8_t x) {
    int count = 0;
    while (x) {
        count += x & 1;
        x >>= 1;
    }
    return count;
}

/**
 * @brief Find track result in session
 */
static uft_track_verify_t* find_or_create_track(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head
) {
    /* Search existing */
    for (size_t i = 0; i < session->track_count; i++) {
        if (session->tracks[i].track == track &&
            session->tracks[i].head == head) {
            return &session->tracks[i];
        }
    }
    
    /* Create new */
    if (session->track_count >= UFT_VERIFY_MAX_TRACKS) {
        return NULL;
    }
    
    uft_track_verify_t *trk = &session->tracks[session->track_count++];
    memset(trk, 0, sizeof(*trk));
    trk->track = track;
    trk->head = head;
    trk->result = UFT_VRESULT_OK;
    
    /* Allocate sectors */
    trk->sectors = calloc(UFT_VERIFY_MAX_SECTORS, sizeof(uft_sector_verify_t));
    if (!trk->sectors) {
        session->track_count--;
        return NULL;
    }
    
    return trk;
}

/**
 * @brief Find sector result in track
 */
static uft_sector_verify_t* find_or_create_sector(
    uft_track_verify_t *track,
    uint8_t sector
) {
    /* Search existing */
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].sector == sector) {
            return &track->sectors[i];
        }
    }
    
    /* Create new */
    if (track->sector_count >= UFT_VERIFY_MAX_SECTORS) {
        return NULL;
    }
    
    uft_sector_verify_t *sec = &track->sectors[track->sector_count++];
    memset(sec, 0, sizeof(*sec));
    sec->track = track->track;
    sec->head = track->head;
    sec->sector = sector;
    sec->result = UFT_VRESULT_OK;
    
    return sec;
}

/*============================================================================
 * Session Management
 *============================================================================*/

uft_verify_session_t* uft_wv_session_create(
    const uft_verify_config_t *config
) {
    init_crc32_table();
    
    uft_verify_session_t *session = calloc(1, sizeof(uft_verify_session_t));
    if (!session) return NULL;
    
    generate_uuid(session->session_id, sizeof(session->session_id));
    session->start_time = time(NULL);
    
    /* Apply configuration */
    if (config) {
        session->mode = config->mode;
        session->pass_count = config->pass_count;
        session->max_retries = config->max_retries;
        session->timing_tolerance = config->timing_tolerance;
    } else {
        session->mode = UFT_VMODE_SECTOR;
        session->pass_count = 1;
        session->max_retries = 3;
        session->timing_tolerance = UFT_VERIFY_TIMING_TOLERANCE;
    }
    
    /* Allocate tracks */
    session->tracks = calloc(UFT_VERIFY_MAX_TRACKS, sizeof(uft_track_verify_t));
    if (!session->tracks) {
        free(session);
        return NULL;
    }
    
    /* Allocate error array */
    session->all_errors = calloc(1024, sizeof(uft_error_location_t));
    
    session->overall_result = UFT_VRESULT_OK;
    
    return session;
}

void uft_wv_session_destroy(uft_verify_session_t *session) {
    if (!session) return;
    
    /* Free track data */
    if (session->tracks) {
        for (size_t t = 0; t < session->track_count; t++) {
            uft_track_verify_t *track = &session->tracks[t];
            if (track->sectors) {
                for (size_t s = 0; s < track->sector_count; s++) {
                    if (track->sectors[s].errors) {
                        free(track->sectors[s].errors);
                    }
                }
                free(track->sectors);
            }
            if (track->timing_issues) {
                free(track->timing_issues);
            }
        }
        free(session->tracks);
    }
    
    if (session->all_errors) {
        free(session->all_errors);
    }
    
    free(session);
}

int uft_wv_session_reset(uft_verify_session_t *session) {
    if (!session) return UFT_ERROR_NULL_POINTER;
    
    /* Reset tracks */
    for (size_t t = 0; t < session->track_count; t++) {
        uft_track_verify_t *track = &session->tracks[t];
        if (track->sectors) {
            for (size_t s = 0; s < track->sector_count; s++) {
                if (track->sectors[s].errors) {
                    free(track->sectors[s].errors);
                    track->sectors[s].errors = NULL;
                }
            }
            track->sector_count = 0;
        }
        if (track->timing_issues) {
            free(track->timing_issues);
            track->timing_issues = NULL;
        }
    }
    session->track_count = 0;
    
    /* Reset statistics */
    session->total_sectors = 0;
    session->sectors_passed = 0;
    session->sectors_failed = 0;
    session->sectors_retried = 0;
    session->total_errors = 0;
    session->overall_result = UFT_VRESULT_OK;
    session->overall_match = 100.0f;
    session->overall_timing = 100.0f;
    
    /* Reset multipass */
    memset(&session->multipass, 0, sizeof(session->multipass));
    
    session->start_time = time(NULL);
    session->end_time = 0;
    
    return UFT_OK;
}

/*============================================================================
 * Core Verification Functions
 *============================================================================*/

uft_verify_result_t uft_wv_verify_sector(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    const uint8_t *expected,
    size_t expected_size,
    const uint8_t *actual,
    size_t actual_size
) {
    if (!session || !expected || !actual) {
        return UFT_VRESULT_READ_ERROR;
    }
    
    /* Get or create track/sector entries */
    uft_track_verify_t *trk = find_or_create_track(session, track, head);
    if (!trk) return UFT_VRESULT_READ_ERROR;
    
    uft_sector_verify_t *sec = find_or_create_sector(trk, sector);
    if (!sec) return UFT_VRESULT_READ_ERROR;
    
    session->total_sectors++;
    
    /* Size mismatch check */
    if (expected_size != actual_size) {
        sec->result = UFT_VRESULT_MISMATCH;
        sec->differing_bits = (uint32_t)(expected_size > actual_size ? 
                               (expected_size - actual_size) * 8 :
                               (actual_size - expected_size) * 8);
        sec->match_percent = 0.0f;
        session->sectors_failed++;
        session->overall_result = UFT_VRESULT_MISMATCH;
        return UFT_VRESULT_MISMATCH;
    }
    
    /* Bit-level comparison */
    size_t compare_size = expected_size < actual_size ? expected_size : actual_size;
    uint32_t total_bits = (uint32_t)(compare_size * 8);
    uint32_t diff_bits = 0;
    
    for (size_t i = 0; i < compare_size; i++) {
        if (expected[i] != actual[i]) {
            diff_bits += popcount8(expected[i] ^ actual[i]);
        }
    }
    
    sec->total_bits = total_bits;
    sec->matching_bits = total_bits - diff_bits;
    sec->differing_bits = diff_bits;
    sec->match_percent = total_bits > 0 ? 
                         (float)(total_bits - diff_bits) / (float)total_bits * 100.0f : 0.0f;
    
    /* CRC comparison */
    sec->expected_crc = uft_wv_crc32(expected, expected_size);
    sec->actual_crc = uft_wv_crc32(actual, actual_size);
    sec->crc_match = (sec->expected_crc == sec->actual_crc);
    
    /* Determine result */
    if (sec->crc_match && diff_bits == 0) {
        sec->result = UFT_VRESULT_OK;
        session->sectors_passed++;
        trk->sectors_ok++;
    } else {
        sec->result = UFT_VRESULT_MISMATCH;
        session->sectors_failed++;
        trk->sectors_failed++;
        session->overall_result = UFT_VRESULT_MISMATCH;
        
        /* Record errors */
        sec->error_count = 0;
        sec->errors = calloc(16, sizeof(uft_error_location_t));
        if (sec->errors) {
            for (size_t i = 0; i < compare_size && sec->error_count < 16; i++) {
                if (expected[i] != actual[i]) {
                    uft_error_location_t *err = &sec->errors[sec->error_count++];
                    err->type = UFT_ELOC_DATA;
                    err->track = track;
                    err->head = head;
                    err->sector = sector;
                    err->bit_offset = (uint32_t)(i * 8);
                    err->bit_count = popcount8(expected[i] ^ actual[i]);
                    err->expected = expected[i];
                    err->actual = actual[i];
                    snprintf(err->description, sizeof(err->description),
                             "Byte %zu: expected 0x%02X, got 0x%02X",
                             i, expected[i], actual[i]);
                }
            }
        }
    }
    
    /* Update track statistics */
    trk->total_bits += total_bits;
    trk->matching_bits += sec->matching_bits;
    trk->match_percent = trk->total_bits > 0 ?
                        (float)trk->matching_bits / (float)trk->total_bits * 100.0f : 0.0f;
    
    /* Update overall match */
    session->overall_match = session->total_sectors > 0 ?
        (float)session->sectors_passed / (float)session->total_sectors * 100.0f : 0.0f;
    
    return sec->result;
}

uft_verify_result_t uft_wv_verify_sector_crc(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uint32_t expected_crc,
    uint32_t actual_crc
) {
    if (!session) return UFT_VRESULT_READ_ERROR;
    
    uft_track_verify_t *trk = find_or_create_track(session, track, head);
    if (!trk) return UFT_VRESULT_READ_ERROR;
    
    uft_sector_verify_t *sec = find_or_create_sector(trk, sector);
    if (!sec) return UFT_VRESULT_READ_ERROR;
    
    session->total_sectors++;
    
    sec->expected_crc = expected_crc;
    sec->actual_crc = actual_crc;
    sec->crc_match = (expected_crc == actual_crc);
    
    if (sec->crc_match) {
        sec->result = UFT_VRESULT_OK;
        sec->match_percent = 100.0f;
        session->sectors_passed++;
        trk->sectors_ok++;
    } else {
        sec->result = UFT_VRESULT_CRC_FAIL;
        sec->match_percent = 0.0f;
        session->sectors_failed++;
        trk->sectors_failed++;
        session->overall_result = UFT_VRESULT_CRC_FAIL;
    }
    
    return sec->result;
}

uft_verify_result_t uft_wv_verify_track_bitstream(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    const uint8_t *expected,
    size_t expected_bits,
    const uint8_t *actual,
    size_t actual_bits
) {
    if (!session || !expected || !actual) {
        return UFT_VRESULT_READ_ERROR;
    }
    
    uft_track_verify_t *trk = find_or_create_track(session, track, head);
    if (!trk) return UFT_VRESULT_READ_ERROR;
    
    /* Compare bit by bit */
    size_t min_bits = expected_bits < actual_bits ? expected_bits : actual_bits;
    size_t compare_bytes = (min_bits + 7) / 8;
    
    uint32_t diff_bits = 0;
    for (size_t i = 0; i < compare_bytes; i++) {
        if (expected[i] != actual[i]) {
            diff_bits += popcount8(expected[i] ^ actual[i]);
        }
    }
    
    /* Account for length difference */
    if (expected_bits != actual_bits) {
        diff_bits += (uint32_t)(expected_bits > actual_bits ? 
                     expected_bits - actual_bits :
                     actual_bits - expected_bits);
    }
    
    trk->total_bits = (uint32_t)expected_bits;
    trk->matching_bits = (uint32_t)(expected_bits - diff_bits);
    trk->match_percent = expected_bits > 0 ?
                        (float)(expected_bits - diff_bits) / (float)expected_bits * 100.0f : 0.0f;
    
    if (diff_bits == 0 && expected_bits == actual_bits) {
        trk->result = UFT_VRESULT_OK;
        return UFT_VRESULT_OK;
    } else {
        trk->result = UFT_VRESULT_MISMATCH;
        session->overall_result = UFT_VRESULT_MISMATCH;
        return UFT_VRESULT_MISMATCH;
    }
}

uft_verify_result_t uft_wv_verify_flux_timing(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    const uint32_t *expected_flux,
    size_t expected_count,
    const uint32_t *actual_flux,
    size_t actual_count,
    uint32_t sample_rate
) {
    if (!session || !expected_flux || !actual_flux) {
        return UFT_VRESULT_READ_ERROR;
    }
    
    uft_track_verify_t *trk = find_or_create_track(session, track, head);
    if (!trk) return UFT_VRESULT_READ_ERROR;
    
    float tolerance = session->timing_tolerance;
    size_t min_count = expected_count < actual_count ? expected_count : actual_count;
    
    float total_deviation = 0.0f;
    float max_deviation = 0.0f;
    size_t timing_errors = 0;
    
    /* Allocate timing issues array */
    trk->timing_issues = calloc(min_count, sizeof(uft_timing_deviation_t));
    if (!trk->timing_issues && min_count > 0) {
        return UFT_VRESULT_READ_ERROR;
    }
    
    for (size_t i = 0; i < min_count; i++) {
        float expected_us = (float)expected_flux[i] * 1000000.0f / (float)sample_rate;
        float actual_us = (float)actual_flux[i] * 1000000.0f / (float)sample_rate;
        
        float deviation = fabsf(actual_us - expected_us) / expected_us * 100.0f;
        total_deviation += deviation;
        
        if (deviation > max_deviation) {
            max_deviation = deviation;
        }
        
        if (deviation > tolerance) {
            if (trk->timing_issues) {
                uft_timing_deviation_t *td = &trk->timing_issues[trk->timing_issue_count++];
                td->track = track;
                td->head = head;
                td->flux_sample = (uint32_t)i;
                td->expected_us = expected_us;
                td->actual_us = actual_us;
                td->deviation_percent = deviation;
                td->in_tolerance = false;
            }
            timing_errors++;
        }
    }
    
    trk->flux_transitions = (uint32_t)min_count;
    trk->flux_errors = (uint32_t)timing_errors;
    trk->avg_deviation = min_count > 0 ? total_deviation / (float)min_count : 0.0f;
    trk->max_deviation = max_deviation;
    trk->flux_quality = min_count > 0 ?
                       (float)(min_count - timing_errors) / (float)min_count * 100.0f : 0.0f;
    
    /* Determine result */
    if (timing_errors == 0) {
        trk->result = UFT_VRESULT_OK;
        return UFT_VRESULT_OK;
    } else if (trk->flux_quality >= 90.0f) {
        trk->result = UFT_VRESULT_TIMING_WARN;
        return UFT_VRESULT_TIMING_WARN;
    } else {
        trk->result = UFT_VRESULT_TIMING_FAIL;
        session->overall_result = UFT_VRESULT_TIMING_FAIL;
        return UFT_VRESULT_TIMING_FAIL;
    }
}

uft_verify_result_t uft_wv_multipass_verify(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    const uint8_t *expected,
    size_t size,
    uint8_t passes,
    uft_wv_read_cb read_cb,
    void *user_data
) {
    if (!session || !expected || !read_cb || passes == 0) {
        return UFT_VRESULT_READ_ERROR;
    }
    
    if (passes > UFT_VERIFY_MAX_PASSES) {
        passes = UFT_VERIFY_MAX_PASSES;
    }
    
    uint8_t *read_buffer = malloc(size);
    if (!read_buffer) return UFT_VRESULT_READ_ERROR;
    
    /* Tracking for weak bit detection */
    uint8_t *vote_zeros = calloc(size, 1);
    uint8_t *vote_ones = calloc(size, 1);
    if (!vote_zeros || !vote_ones) {
        free(read_buffer);
        free(vote_zeros);
        free(vote_ones);
        return UFT_VRESULT_READ_ERROR;
    }
    
    uft_multipass_stats_t *stats = &session->multipass;
    stats->pass_count = passes;
    stats->avg_match_percent = 0.0f;
    stats->min_match_percent = 100.0f;
    stats->max_match_percent = 0.0f;
    
    /* Perform passes */
    for (uint8_t p = 0; p < passes; p++) {
        int rc = read_cb(track, head, read_buffer, size, user_data);
        if (rc != 0) {
            stats->passes[p].result = UFT_VRESULT_READ_ERROR;
            stats->passes[p].errors = 1;
            continue;
        }
        
        /* Compare with expected */
        uint32_t diff_bits = 0;
        for (size_t i = 0; i < size; i++) {
            if (expected[i] != read_buffer[i]) {
                diff_bits += popcount8(expected[i] ^ read_buffer[i]);
            }
            
            /* Track voting for weak bit detection */
            for (int b = 0; b < 8; b++) {
                if (read_buffer[i] & (1 << b)) {
                    vote_ones[i] |= (1 << b);
                } else {
                    vote_zeros[i] |= (1 << b);
                }
            }
        }
        
        float match = (float)(size * 8 - diff_bits) / (float)(size * 8) * 100.0f;
        
        stats->passes[p].match_percent = match;
        stats->passes[p].errors = diff_bits;
        stats->passes[p].result = (diff_bits == 0) ? UFT_VRESULT_OK : UFT_VRESULT_MISMATCH;
        
        stats->avg_match_percent += match;
        if (match < stats->min_match_percent) stats->min_match_percent = match;
        if (match > stats->max_match_percent) stats->max_match_percent = match;
    }
    
    stats->avg_match_percent /= (float)passes;
    
    /* Detect weak bits (positions that flip between passes) */
    stats->weak_bit_positions = 0;
    for (size_t i = 0; i < size; i++) {
        uint8_t both = vote_zeros[i] & vote_ones[i];
        stats->weak_bit_positions += popcount8(both);
    }
    stats->has_weak_bits = (stats->weak_bit_positions > 0);
    
    /* Calculate consistency */
    float spread = stats->max_match_percent - stats->min_match_percent;
    stats->consistency = 100.0f - spread;
    
    free(read_buffer);
    free(vote_zeros);
    free(vote_ones);
    
    /* Determine overall result */
    if (stats->min_match_percent >= 99.9f && !stats->has_weak_bits) {
        return UFT_VRESULT_OK;
    } else if (stats->has_weak_bits) {
        return UFT_VRESULT_WEAK_BITS;
    } else if (stats->avg_match_percent >= 95.0f) {
        return UFT_VRESULT_TIMING_WARN;
    } else {
        return UFT_VRESULT_MISMATCH;
    }
}

/*============================================================================
 * Retry Functions
 *============================================================================*/

uft_verify_result_t uft_wv_retry_sector(
    uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    const uint8_t *data,
    size_t size,
    uft_wv_write_cb write_cb,
    uft_wv_read_sector_cb read_cb,
    void *user_data
) {
    if (!session || !data || !write_cb || !read_cb) {
        return UFT_VRESULT_READ_ERROR;
    }
    
    uint8_t *read_buffer = malloc(size);
    if (!read_buffer) return UFT_VRESULT_READ_ERROR;
    
    uft_track_verify_t *trk = find_or_create_track(session, track, head);
    uft_sector_verify_t *sec = trk ? find_or_create_sector(trk, sector) : NULL;
    
    uft_verify_result_t result = UFT_VRESULT_RETRY_FAIL;
    
    for (uint8_t retry = 0; retry < session->max_retries; retry++) {
        /* Write */
        int rc = write_cb(track, head, sector, data, size, user_data);
        if (rc != 0) continue;
        
        /* Read back */
        rc = read_cb(track, head, sector, read_buffer, size, user_data);
        if (rc != 0) continue;
        
        /* Verify */
        bool match = (memcmp(data, read_buffer, size) == 0);
        
        if (match) {
            result = UFT_VRESULT_RETRY_OK;
            if (sec) {
                sec->retry_count = retry + 1;
                sec->retry_successful = true;
                sec->result = UFT_VRESULT_RETRY_OK;
            }
            session->sectors_retried++;
            break;
        }
    }
    
    if (result != UFT_VRESULT_RETRY_OK && sec) {
        sec->retry_count = session->max_retries;
        sec->retry_successful = false;
        sec->result = UFT_VRESULT_RETRY_FAIL;
    }
    
    free(read_buffer);
    return result;
}

int uft_wv_get_retry_stats(
    const uft_verify_session_t *session,
    uint32_t *total_retries,
    uint32_t *successful_retries
) {
    if (!session) return UFT_ERROR_NULL_POINTER;
    
    uint32_t total = 0;
    uint32_t successful = 0;
    
    for (size_t t = 0; t < session->track_count; t++) {
        const uft_track_verify_t *track = &session->tracks[t];
        for (size_t s = 0; s < track->sector_count; s++) {
            const uft_sector_verify_t *sector = &track->sectors[s];
            if (sector->retry_count > 0) {
                total += sector->retry_count;
                if (sector->retry_successful) {
                    successful++;
                }
            }
        }
    }
    
    if (total_retries) *total_retries = total;
    if (successful_retries) *successful_retries = successful;
    
    return UFT_OK;
}

/*============================================================================
 * Analysis Functions
 *============================================================================*/

const uft_sector_verify_t* uft_wv_get_sector_result(
    const uft_verify_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector
) {
    if (!session) return NULL;
    
    for (size_t t = 0; t < session->track_count; t++) {
        const uft_track_verify_t *trk = &session->tracks[t];
        if (trk->track == track && trk->head == head) {
            for (size_t s = 0; s < trk->sector_count; s++) {
                if (trk->sectors[s].sector == sector) {
                    return &trk->sectors[s];
                }
            }
        }
    }
    return NULL;
}

const uft_track_verify_t* uft_wv_get_track_result(
    const uft_verify_session_t *session,
    uint8_t track,
    uint8_t head
) {
    if (!session) return NULL;
    
    for (size_t t = 0; t < session->track_count; t++) {
        if (session->tracks[t].track == track &&
            session->tracks[t].head == head) {
            return &session->tracks[t];
        }
    }
    return NULL;
}

size_t uft_wv_get_failed_sectors(
    const uft_verify_session_t *session,
    const uft_sector_verify_t **sectors,
    size_t max_sectors
) {
    if (!session || !sectors) return 0;
    
    size_t count = 0;
    
    for (size_t t = 0; t < session->track_count && count < max_sectors; t++) {
        const uft_track_verify_t *track = &session->tracks[t];
        for (size_t s = 0; s < track->sector_count && count < max_sectors; s++) {
            if (track->sectors[s].result != UFT_VRESULT_OK &&
                track->sectors[s].result != UFT_VRESULT_RETRY_OK) {
                sectors[count++] = &track->sectors[s];
            }
        }
    }
    
    return count;
}

size_t uft_wv_get_all_errors(
    const uft_verify_session_t *session,
    const uft_error_location_t **errors,
    size_t max_errors
) {
    if (!session || !errors) return 0;
    
    size_t count = 0;
    
    for (size_t t = 0; t < session->track_count && count < max_errors; t++) {
        const uft_track_verify_t *track = &session->tracks[t];
        for (size_t s = 0; s < track->sector_count && count < max_errors; s++) {
            const uft_sector_verify_t *sector = &track->sectors[s];
            for (size_t e = 0; e < sector->error_count && count < max_errors; e++) {
                errors[count++] = &sector->errors[e];
            }
        }
    }
    
    return count;
}

float uft_wv_calculate_score(const uft_verify_session_t *session) {
    if (!session || session->total_sectors == 0) return 0.0f;
    
    float sector_score = (float)session->sectors_passed / 
                        (float)session->total_sectors * 100.0f;
    
    /* Weight timing if available */
    float timing_score = session->overall_timing;
    
    /* Combine scores (70% sector, 30% timing) */
    return sector_score * 0.7f + timing_score * 0.3f;
}

/*============================================================================
 * Export Functions
 *============================================================================*/

size_t uft_wv_export_json(
    const uft_verify_session_t *session,
    char *buffer,
    size_t buffer_size
) {
    if (!session || !buffer) return 0;
    
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
        "{\n"
        "  \"session_id\": \"%s\",\n"
        "  \"start_time\": %ld,\n"
        "  \"mode\": %d,\n"
        "  \"overall_result\": \"%s\",\n"
        "  \"statistics\": {\n"
        "    \"total_sectors\": %zu,\n"
        "    \"passed\": %zu,\n"
        "    \"failed\": %zu,\n"
        "    \"retried\": %zu,\n"
        "    \"overall_match\": %.2f,\n"
        "    \"score\": %.2f\n"
        "  },\n"
        "  \"tracks\": [\n",
        session->session_id,
        (long)session->start_time,
        (int)session->mode,
        uft_wv_result_name(session->overall_result),
        session->total_sectors,
        session->sectors_passed,
        session->sectors_failed,
        session->sectors_retried,
        session->overall_match,
        uft_wv_calculate_score(session)
    );
    
    for (size_t t = 0; t < session->track_count; t++) {
        const uft_track_verify_t *track = &session->tracks[t];
        written += snprintf(buffer + written, buffer_size - written,
            "    {\n"
            "      \"track\": %u,\n"
            "      \"head\": %u,\n"
            "      \"result\": \"%s\",\n"
            "      \"match_percent\": %.2f,\n"
            "      \"sectors_ok\": %zu,\n"
            "      \"sectors_failed\": %zu\n"
            "    }%s\n",
            track->track,
            track->head,
            uft_wv_result_name(track->result),
            track->match_percent,
            track->sectors_ok,
            track->sectors_failed,
            t < session->track_count - 1 ? "," : ""
        );
    }
    
    written += snprintf(buffer + written, buffer_size - written,
        "  ]\n"
        "}\n"
    );
    
    return (size_t)written;
}

size_t uft_wv_export_markdown(
    const uft_verify_session_t *session,
    char *buffer,
    size_t buffer_size
) {
    if (!session || !buffer) return 0;
    
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
        "# Writer Verification Report\n\n"
        "**Session ID:** %s  \n"
        "**Result:** %s  \n"
        "**Score:** %.1f%%\n\n"
        "## Summary\n\n"
        "| Metric | Value |\n"
        "|--------|-------|\n"
        "| Total Sectors | %zu |\n"
        "| Passed | %zu |\n"
        "| Failed | %zu |\n"
        "| Retried | %zu |\n"
        "| Match %% | %.2f%% |\n\n",
        session->session_id,
        uft_wv_result_name(session->overall_result),
        uft_wv_calculate_score(session),
        session->total_sectors,
        session->sectors_passed,
        session->sectors_failed,
        session->sectors_retried,
        session->overall_match
    );
    
    /* Failed sectors */
    if (session->sectors_failed > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "## Failed Sectors\n\n");
        
        const uft_sector_verify_t *failed[32];
        size_t failed_count = uft_wv_get_failed_sectors(session, failed, 32);
        
        for (size_t i = 0; i < failed_count; i++) {
            written += snprintf(buffer + written, buffer_size - written,
                "- Track %u, Head %u, Sector %u: %s (%.1f%% match)\n",
                failed[i]->track,
                failed[i]->head,
                failed[i]->sector,
                uft_wv_result_name(failed[i]->result),
                failed[i]->match_percent
            );
        }
        written += snprintf(buffer + written, buffer_size - written, "\n");
    }
    
    /* Multi-pass info */
    if (session->multipass.pass_count > 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "## Multi-Pass Analysis\n\n"
            "| Pass | Match %% | Errors |\n"
            "|------|----------|--------|\n"
        );
        
        for (uint8_t p = 0; p < session->multipass.pass_count; p++) {
            written += snprintf(buffer + written, buffer_size - written,
                "| %u | %.2f%% | %u |\n",
                p + 1,
                session->multipass.passes[p].match_percent,
                session->multipass.passes[p].errors
            );
        }
        
        written += snprintf(buffer + written, buffer_size - written,
            "\n**Consistency:** %.1f%%  \n"
            "**Weak Bits:** %u\n\n",
            session->multipass.consistency,
            session->multipass.weak_bit_positions
        );
    }
    
    return (size_t)written;
}

size_t uft_wv_export_error_report(
    const uft_verify_session_t *session,
    char *buffer,
    size_t buffer_size
) {
    if (!session || !buffer) return 0;
    
    int written = 0;
    written += snprintf(buffer + written, buffer_size - written,
        "# Verification Error Report\n\n");
    
    const uft_error_location_t *errors[256];
    size_t error_count = uft_wv_get_all_errors(session, errors, 256);
    
    if (error_count == 0) {
        written += snprintf(buffer + written, buffer_size - written,
            "No errors detected.\n");
        return (size_t)written;
    }
    
    written += snprintf(buffer + written, buffer_size - written,
        "## Errors (%zu total)\n\n"
        "| Location | Type | Details |\n"
        "|----------|------|----------|\n",
        error_count
    );
    
    for (size_t i = 0; i < error_count && i < 50; i++) {
        written += snprintf(buffer + written, buffer_size - written,
            "| T%u/H%u/S%u | %s | %s |\n",
            errors[i]->track,
            errors[i]->head,
            errors[i]->sector,
            uft_wv_error_type_name(errors[i]->type),
            errors[i]->description
        );
    }
    
    if (error_count > 50) {
        written += snprintf(buffer + written, buffer_size - written,
            "\n... and %zu more errors.\n", error_count - 50);
    }
    
    return (size_t)written;
}

void uft_wv_print_summary(const uft_verify_session_t *session) {
    if (!session) return;
    
    printf("\n=== Writer Verification Summary ===\n");
    printf("Session: %s\n", session->session_id);
    printf("Result:  %s\n", uft_wv_result_name(session->overall_result));
    printf("Score:   %.1f%%\n", uft_wv_calculate_score(session));
    printf("\nSectors: %zu total, %zu passed, %zu failed, %zu retried\n",
           session->total_sectors,
           session->sectors_passed,
           session->sectors_failed,
           session->sectors_retried);
    printf("Match:   %.2f%%\n", session->overall_match);
    
    if (session->multipass.has_weak_bits) {
        printf("\n⚠ Weak bits detected: %u positions\n",
               session->multipass.weak_bit_positions);
    }
    
    printf("=====================================\n\n");
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char* uft_wv_result_name(uft_verify_result_t result) {
    switch (result) {
        case UFT_VRESULT_OK:           return "OK";
        case UFT_VRESULT_MISMATCH:     return "Mismatch";
        case UFT_VRESULT_TIMING_WARN:  return "Timing Warning";
        case UFT_VRESULT_TIMING_FAIL:  return "Timing Fail";
        case UFT_VRESULT_READ_ERROR:   return "Read Error";
        case UFT_VRESULT_CRC_FAIL:     return "CRC Fail";
        case UFT_VRESULT_WEAK_BITS:    return "Weak Bits";
        case UFT_VRESULT_PARTIAL:      return "Partial";
        case UFT_VRESULT_RETRY_OK:     return "Retry OK";
        case UFT_VRESULT_RETRY_FAIL:   return "Retry Fail";
        default:                        return "Unknown";
    }
}

const char* uft_wv_error_type_name(uft_error_location_type_t type) {
    switch (type) {
        case UFT_ELOC_NONE:   return "None";
        case UFT_ELOC_TRACK:  return "Track";
        case UFT_ELOC_SECTOR: return "Sector";
        case UFT_ELOC_GAP:    return "Gap";
        case UFT_ELOC_SYNC:   return "Sync";
        case UFT_ELOC_HEADER: return "Header";
        case UFT_ELOC_DATA:   return "Data";
        case UFT_ELOC_CRC:    return "CRC";
        case UFT_ELOC_TIMING: return "Timing";
        default:              return "Unknown";
    }
}

void uft_wv_config_defaults(uft_verify_config_t *config) {
    if (!config) return;
    
    config->mode = UFT_VMODE_SECTOR;
    config->pass_count = 1;
    config->max_retries = 3;
    config->timing_tolerance = UFT_VERIFY_TIMING_TOLERANCE;
    config->min_match_percent = UFT_VERIFY_MIN_CONFIDENCE;
    config->abort_on_fail = false;
    config->verify_gaps = false;
    config->verify_sync = true;
    config->collect_timing = false;
    config->enable_retry = true;
    config->log_progress = false;
}

uint32_t uft_wv_crc32(const uint8_t *data, size_t size) {
    init_crc32_table();
    
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return crc ^ 0xFFFFFFFF;
}

size_t uft_wv_compare_bytes(
    const uint8_t *a,
    const uint8_t *b,
    size_t size,
    uint32_t *diff_positions,
    size_t max_diffs
) {
    if (!a || !b) return 0;
    
    size_t diff_count = 0;
    
    for (size_t i = 0; i < size; i++) {
        if (a[i] != b[i]) {
            if (diff_positions && diff_count < max_diffs) {
                diff_positions[diff_count] = (uint32_t)i;
            }
            diff_count++;
        }
    }
    
    return diff_count;
}

/* End of uft_writer_verify.c */
