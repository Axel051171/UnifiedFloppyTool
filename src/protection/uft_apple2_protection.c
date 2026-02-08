/**
 * @file uft_apple2_protection.c
 * @brief Apple II Copy Protection Detection Implementation
 * FIXED R18: Added stdio.h for snprintf
 */

#include "uft/protection/uft_apple2_protection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* Standard Apple II GCR marks */
static const uint8_t ADDR_PROLOGUE_STD[] = {0xD5, 0xAA, 0x96};
static const uint8_t DATA_PROLOGUE_STD[] = {0xD5, 0xAA, 0xAD};
static const uint8_t EPILOGUE_STD[] = {0xDE, 0xAA, 0xEB};

/* Expected nibble counts per track (16-sector) */
static const uint16_t EXPECTED_NIBBLES_16[] = {
    6656, 6343, 6030, 5717, 5404, 5091, 4778, 4465,  /* Tracks 0-7 */
    4465, 4465, 4465, 4465, 4465, 4465, 4465, 4465,  /* Tracks 8-15 */
    4465, 4465, 4465, 4465, 4465, 4465, 4465, 4465,  /* Tracks 16-23 */
    4465, 4465, 4465, 4465, 4465, 4465, 4465, 4465,  /* Tracks 24-31 */
    4465, 4465, 4465                                  /* Tracks 32-34 */
};

/*===========================================================================
 * Configuration
 *===========================================================================*/

void uft_apple2_config_init(uft_apple2_detect_config_t *config)
{
    if (!config) return;
    
    config->detect_nibble_count = true;
    config->detect_timing_bits = true;
    config->detect_spiral = true;
    config->detect_cross_track = true;
    config->detect_custom_marks = true;
    
    config->nibble_tolerance = UFT_APPLE2_NIBBLE_TOLERANCE;
    config->timing_threshold_ns = UFT_APPLE2_TIMING_THRESHOLD;
    config->spiral_min_tracks = UFT_APPLE2_SPIRAL_MIN_TRACKS;
}

/*===========================================================================
 * Memory Management
 *===========================================================================*/

uft_apple2_prot_result_t *uft_apple2_result_alloc(void)
{
    uft_apple2_prot_result_t *result = calloc(1, sizeof(uft_apple2_prot_result_t));
    if (!result) return NULL;
    
    result->nibble_counts = calloc(UFT_APPLE2_TRACKS, sizeof(uft_nibble_count_t));
    result->timing_bits = calloc(1024, sizeof(uft_timing_bit_t));
    result->custom_marks = calloc(256, sizeof(uft_custom_mark_t));
    
    if (!result->nibble_counts || !result->timing_bits || !result->custom_marks) {
        uft_apple2_result_free(result);
        return NULL;
    }
    
    return result;
}

void uft_apple2_result_free(uft_apple2_prot_result_t *result)
{
    if (!result) return;
    free(result->nibble_counts);
    free(result->timing_bits);
    free(result->custom_marks);
    free(result);
}

/*===========================================================================
 * Nibble Count Detection
 *===========================================================================*/

int uft_apple2_detect_nibble_count(const uint8_t *track_data, size_t track_len,
                                   uint8_t track_num, uft_nibble_count_t *result)
{
    if (!track_data || !result || track_num >= UFT_APPLE2_TRACKS) return -1;
    
    result->track = track_num;
    result->actual_nibbles = (uint16_t)track_len;
    result->expected_nibbles = EXPECTED_NIBBLES_16[track_num];
    result->difference = (int16_t)result->actual_nibbles - (int16_t)result->expected_nibbles;
    
    /* Protection detected if difference exceeds tolerance */
    result->is_protected = (abs(result->difference) > UFT_APPLE2_NIBBLE_TOLERANCE);
    
    /* Confidence based on how far from expected */
    double norm_diff = (double)abs(result->difference) / (double)result->expected_nibbles;
    result->confidence = result->is_protected ? (1.0 - norm_diff) : 0.0;
    if (result->confidence < 0.0) result->confidence = 0.0;
    if (result->confidence > 1.0) result->confidence = 1.0;
    
    return 0;
}

/*===========================================================================
 * Timing Bit Detection
 *===========================================================================*/

size_t uft_apple2_detect_timing_bits(const uint32_t *intervals, size_t count,
                                     uint8_t track_num,
                                     uft_timing_bit_t *timing_bits,
                                     size_t max_bits)
{
    if (!intervals || !timing_bits || count == 0 || max_bits == 0) return 0;
    
    /* Calculate mean interval */
    uint64_t sum = 0;
    for (size_t i = 0; i < count; i++) {
        sum += intervals[i];
    }
    double mean = (double)sum / (double)count;
    
    /* Calculate standard deviation */
    double var_sum = 0.0;
    for (size_t i = 0; i < count; i++) {
        double diff = (double)intervals[i] - mean;
        var_sum += diff * diff;
    }
    double std = sqrt(var_sum / (double)count);
    
    /* Find outliers (potential timing bits) */
    size_t found = 0;
    double threshold = mean + 3.0 * std;  /* 3 sigma */
    
    for (size_t i = 0; i < count && found < max_bits; i++) {
        if (intervals[i] > threshold) {
            timing_bits[found].track = track_num;
            timing_bits[found].sector = 0;  /* Would need more analysis */
            timing_bits[found].bit_position = (uint32_t)i;
            timing_bits[found].timing_ns = (uint16_t)(intervals[i] / 1000);
            timing_bits[found].expected_ns = (uint16_t)(mean / 1000);
            timing_bits[found].is_timing_bit = true;
            timing_bits[found].confidence = 1.0 - (mean / intervals[i]);
            found++;
        }
    }
    
    return found;
}

/*===========================================================================
 * Spiral Track Detection
 *===========================================================================*/

/**
 * @brief Find sync pattern in track data
 */
static int find_sync_pattern(const uint8_t *data, size_t len, size_t start)
{
    for (size_t i = start; i + 2 < len; i++) {
        /* Look for 10 consecutive FF bytes (sync) */
        int sync_count = 0;
        for (size_t j = i; j < len && data[j] == 0xFF; j++) {
            sync_count++;
        }
        if (sync_count >= 10) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * @brief Calculate rotation offset between tracks
 */
static double calc_rotation_offset(const uint8_t *track_a, size_t len_a,
                                   const uint8_t *track_b, size_t len_b)
{
    /* Find first sync in each track */
    int sync_a = find_sync_pattern(track_a, len_a, 0);
    int sync_b = find_sync_pattern(track_b, len_b, 0);
    
    if (sync_a < 0 || sync_b < 0) return 0.0;
    
    /* Calculate offset as fraction of track */
    double pos_a = (double)sync_a / (double)len_a;
    double pos_b = (double)sync_b / (double)len_b;
    
    return fmod(pos_b - pos_a + 1.0, 1.0);
}

int uft_apple2_detect_spiral(const uint8_t **tracks, const size_t *track_lens,
                             uint8_t track_count, uint8_t start_track,
                             uft_spiral_track_t *result)
{
    if (!tracks || !track_lens || !result || track_count < 3) return -1;
    
    memset(result, 0, sizeof(*result));
    result->start_track = start_track;
    result->end_track = start_track + track_count - 1;
    result->track_count = track_count;
    
    /* Calculate rotation offsets between consecutive tracks */
    double total_offset = 0.0;
    double offsets[8] = {0};
    uint8_t max_offsets = (track_count > 8) ? 8 : track_count;
    
    for (uint8_t i = 0; i < track_count - 1 && i < max_offsets; i++) {
        offsets[i] = calc_rotation_offset(tracks[i], track_lens[i],
                                          tracks[i + 1], track_lens[i + 1]);
        total_offset += offsets[i];
    }
    
    result->rotation_offset = total_offset / (track_count - 1);
    
    /* Spiral detected if consistent rotation offset */
    double variance = 0.0;
    for (uint8_t i = 0; i < track_count - 1 && i < max_offsets; i++) {
        double diff = offsets[i] - result->rotation_offset;
        variance += diff * diff;
    }
    variance /= (track_count - 1);
    
    /* Consistent offset = spiral track */
    double std = sqrt(variance);
    result->detected = (result->rotation_offset > 0.05 && std < 0.1);
    result->confidence = result->detected ? (1.0 - std) : 0.0;
    
    /* Record data start positions */
    for (uint8_t i = 0; i < track_count && i < 8; i++) {
        int sync = find_sync_pattern(tracks[i], track_lens[i], 0);
        result->data_start[i] = (sync >= 0) ? (uint32_t)sync : 0;
    }
    
    return 0;
}

/*===========================================================================
 * Cross-Track Sync Detection
 *===========================================================================*/

/**
 * @brief Find matching pattern between tracks
 */
static int find_cross_pattern(const uint8_t *track_a, size_t len_a,
                              const uint8_t *track_b, size_t len_b,
                              uint32_t *pos_a, uint32_t *pos_b,
                              uint8_t *pattern, uint8_t *pattern_len)
{
    const int MIN_MATCH = 8;
    
    /* Search for matching byte sequences */
    for (size_t i = 0; i + MIN_MATCH < len_a; i++) {
        /* Skip sync bytes */
        if (track_a[i] == 0xFF) continue;
        
        for (size_t j = 0; j + MIN_MATCH < len_b; j++) {
            if (track_b[j] == 0xFF) continue;
            
            /* Check for match */
            int match_len = 0;
            while (i + match_len < len_a && j + match_len < len_b &&
                   track_a[i + match_len] == track_b[j + match_len] &&
                   match_len < 16) {
                match_len++;
            }
            
            if (match_len >= MIN_MATCH) {
                *pos_a = (uint32_t)i;
                *pos_b = (uint32_t)j;
                *pattern_len = (uint8_t)match_len;
                memcpy(pattern, &track_a[i], match_len);
                return match_len;
            }
        }
    }
    
    return 0;
}

int uft_apple2_detect_cross_track(const uint8_t *track_a, size_t len_a,
                                  const uint8_t *track_b, size_t len_b,
                                  uint8_t track_num_a, uint8_t track_num_b,
                                  uft_cross_track_t *result)
{
    if (!track_a || !track_b || !result) return -1;
    
    memset(result, 0, sizeof(*result));
    result->track_a = track_num_a;
    result->track_b = track_num_b;
    
    /* Find cross-track pattern */
    int match = find_cross_pattern(track_a, len_a, track_b, len_b,
                                   &result->sync_position_a,
                                   &result->sync_position_b,
                                   result->sync_pattern,
                                   &result->pattern_length);
    
    if (match >= 8) {
        result->detected = true;
        result->sync_offset = result->sync_position_b - result->sync_position_a;
        result->confidence = (double)match / 16.0;
        if (result->confidence > 1.0) result->confidence = 1.0;
    }
    
    return 0;
}

/*===========================================================================
 * Custom Mark Detection
 *===========================================================================*/

/**
 * @brief Find next address field
 */
static int find_address_field(const uint8_t *data, size_t len, size_t start,
                              uint8_t prologue[3], uint8_t epilogue[3])
{
    for (size_t i = start; i + 10 < len; i++) {
        /* Look for D5 AA xx (address prologue) */
        if (data[i] == 0xD5 && data[i + 1] == 0xAA) {
            prologue[0] = data[i];
            prologue[1] = data[i + 1];
            prologue[2] = data[i + 2];
            
            /* Find epilogue (DE AA xx) */
            for (size_t j = i + 3; j + 2 < len && j < i + 20; j++) {
                if (data[j] == 0xDE && data[j + 1] == 0xAA) {
                    epilogue[0] = data[j];
                    epilogue[1] = data[j + 1];
                    epilogue[2] = data[j + 2];
                    return (int)i;
                }
            }
        }
    }
    return -1;
}

size_t uft_apple2_detect_custom_marks(const uint8_t *track_data, size_t track_len,
                                      uint8_t track_num,
                                      uft_custom_mark_t *marks,
                                      size_t max_marks)
{
    if (!track_data || !marks || track_len == 0 || max_marks == 0) return 0;
    
    size_t found = 0;
    size_t pos = 0;
    uint8_t sector = 0;
    
    while (pos < track_len && found < max_marks) {
        uint8_t addr_pro[3] = {0}, addr_epi[3] = {0};
        
        int addr_pos = find_address_field(track_data, track_len, pos,
                                          addr_pro, addr_epi);
        if (addr_pos < 0) break;
        
        /* Initialize mark */
        marks[found].track = track_num;
        marks[found].sector = sector++;
        
        memcpy(marks[found].std_addr_prologue, ADDR_PROLOGUE_STD, 3);
        memcpy(marks[found].std_data_prologue, DATA_PROLOGUE_STD, 3);
        memcpy(marks[found].addr_prologue, addr_pro, 3);
        memcpy(marks[found].addr_epilogue, addr_epi, 3);
        
        /* Check for custom address mark */
        marks[found].custom_addr = (memcmp(addr_pro, ADDR_PROLOGUE_STD, 3) != 0);
        
        /* Look for data field after address */
        size_t data_search = addr_pos + 10;
        for (size_t i = data_search; i + 3 < track_len && i < data_search + 50; i++) {
            if (track_data[i] == 0xD5 && track_data[i + 1] == 0xAA) {
                marks[found].data_prologue[0] = track_data[i];
                marks[found].data_prologue[1] = track_data[i + 1];
                marks[found].data_prologue[2] = track_data[i + 2];
                
                marks[found].custom_data = 
                    (memcmp(marks[found].data_prologue, DATA_PROLOGUE_STD, 3) != 0);
                break;
            }
        }
        
        /* Calculate confidence */
        marks[found].confidence = 1.0;
        if (marks[found].custom_addr || marks[found].custom_data) {
            found++;
        }
        
        pos = addr_pos + 20;
    }
    
    return found;
}

/*===========================================================================
 * Full Detection
 *===========================================================================*/

int uft_apple2_detect_all(const uint8_t **tracks, const size_t *track_lens,
                          const uint32_t **intervals, const size_t *interval_counts,
                          uint8_t track_count,
                          const uft_apple2_detect_config_t *config,
                          uft_apple2_prot_result_t *result)
{
    if (!tracks || !track_lens || !config || !result) return -1;
    
    result->primary_type = UFT_APPLE2_PROT_NONE;
    result->type_flags = 0;
    result->overall_confidence = 0.0;
    
    double total_confidence = 0.0;
    int detection_count = 0;
    
    /* Detect nibble count protection */
    if (config->detect_nibble_count) {
        result->nibble_count_len = 0;
        for (uint8_t t = 0; t < track_count && t < UFT_APPLE2_TRACKS; t++) {
            uft_apple2_detect_nibble_count(tracks[t], track_lens[t], t,
                                           &result->nibble_counts[t]);
            if (result->nibble_counts[t].is_protected) {
                result->type_flags |= (1u << UFT_APPLE2_PROT_NIBBLE_COUNT);
                total_confidence += result->nibble_counts[t].confidence;
                detection_count++;
            }
            result->nibble_count_len++;
        }
    }
    
    /* Detect timing bits */
    if (config->detect_timing_bits && intervals && interval_counts) {
        result->timing_bit_count = 0;
        for (uint8_t t = 0; t < track_count; t++) {
            size_t found = uft_apple2_detect_timing_bits(intervals[t], 
                                                          interval_counts[t], t,
                                                          result->timing_bits + result->timing_bit_count,
                                                          1024 - result->timing_bit_count);
            if (found > 0) {
                result->type_flags |= (1u << UFT_APPLE2_PROT_TIMING_BITS);
                result->timing_bit_count += (uint16_t)found;
                total_confidence += 0.8;
                detection_count++;
            }
        }
    }
    
    /* Detect spiral track */
    if (config->detect_spiral && track_count >= config->spiral_min_tracks) {
        uft_apple2_detect_spiral(tracks, track_lens, track_count, 0, &result->spiral);
        if (result->spiral.detected) {
            result->type_flags |= (1u << UFT_APPLE2_PROT_SPIRAL_TRACK);
            total_confidence += result->spiral.confidence;
            detection_count++;
        }
    }
    
    /* Detect cross-track sync */
    if (config->detect_cross_track && track_count >= 2) {
        uft_apple2_detect_cross_track(tracks[0], track_lens[0],
                                      tracks[1], track_lens[1],
                                      0, 1, &result->cross_track);
        if (result->cross_track.detected) {
            result->type_flags |= (1u << UFT_APPLE2_PROT_CROSS_TRACK);
            total_confidence += result->cross_track.confidence;
            detection_count++;
        }
    }
    
    /* Detect custom marks */
    if (config->detect_custom_marks) {
        result->custom_mark_count = 0;
        for (uint8_t t = 0; t < track_count; t++) {
            size_t found = uft_apple2_detect_custom_marks(tracks[t], track_lens[t], t,
                                                          result->custom_marks + result->custom_mark_count,
                                                          256 - result->custom_mark_count);
            if (found > 0) {
                result->type_flags |= (1u << UFT_APPLE2_PROT_CUSTOM_ADDR);
                result->custom_mark_count += (uint8_t)found;
                total_confidence += 0.9;
                detection_count++;
            }
        }
    }
    
    /* Determine primary type */
    if (result->type_flags & (1u << UFT_APPLE2_PROT_SPIRAL_TRACK)) {
        result->primary_type = UFT_APPLE2_PROT_SPIRAL_TRACK;
    } else if (result->type_flags & (1u << UFT_APPLE2_PROT_CROSS_TRACK)) {
        result->primary_type = UFT_APPLE2_PROT_CROSS_TRACK;
    } else if (result->type_flags & (1u << UFT_APPLE2_PROT_TIMING_BITS)) {
        result->primary_type = UFT_APPLE2_PROT_TIMING_BITS;
    } else if (result->type_flags & (1u << UFT_APPLE2_PROT_NIBBLE_COUNT)) {
        result->primary_type = UFT_APPLE2_PROT_NIBBLE_COUNT;
    } else if (result->type_flags & (1u << UFT_APPLE2_PROT_CUSTOM_ADDR)) {
        result->primary_type = UFT_APPLE2_PROT_CUSTOM_ADDR;
    }
    
    /* Multiple protections? */
    int pop_count = 0;
    uint32_t flags = result->type_flags;
    while (flags) {
        pop_count += (flags & 1);
        flags >>= 1;
    }
    if (pop_count > 1) {
        result->primary_type = UFT_APPLE2_PROT_MULTIPLE;
    }
    
    /* Calculate overall confidence */
    if (detection_count > 0) {
        result->overall_confidence = total_confidence / detection_count;
    }
    
    /* Generate description */
    snprintf(result->description, sizeof(result->description),
             "Apple II Protection: %s (confidence: %.1f%%)",
             uft_apple2_prot_name(result->primary_type),
             result->overall_confidence * 100.0);
    
    return 0;
}

/*===========================================================================
 * Utilities
 *===========================================================================*/

const char *uft_apple2_prot_name(uft_apple2_prot_type_t type)
{
    switch (type) {
        case UFT_APPLE2_PROT_NONE:         return "None";
        case UFT_APPLE2_PROT_NIBBLE_COUNT: return "Nibble Count";
        case UFT_APPLE2_PROT_TIMING_BITS:  return "Timing Bits";
        case UFT_APPLE2_PROT_SPIRAL_TRACK: return "Spiral Track";
        case UFT_APPLE2_PROT_CROSS_TRACK:  return "Cross-Track Sync";
        case UFT_APPLE2_PROT_CUSTOM_ADDR:  return "Custom Address Mark";
        case UFT_APPLE2_PROT_CUSTOM_DATA:  return "Custom Data Mark";
        case UFT_APPLE2_PROT_HALF_TRACK:   return "Half-Track";
        case UFT_APPLE2_PROT_SYNC_PATTERN: return "Custom Sync";
        case UFT_APPLE2_PROT_MULTIPLE:     return "Multiple Protections";
        default:                           return "Unknown";
    }
}

int uft_apple2_result_to_json(const uft_apple2_prot_result_t *result,
                              char *buffer, size_t buffer_size)
{
    if (!result || !buffer || buffer_size < 256) return -1;
    
    int written = snprintf(buffer, buffer_size,
        "{\n"
        "  \"protection_type\": \"%s\",\n"
        "  \"type_flags\": %u,\n"
        "  \"confidence\": %.4f,\n"
        "  \"spiral_detected\": %s,\n"
        "  \"cross_track_detected\": %s,\n"
        "  \"nibble_anomalies\": %u,\n"
        "  \"timing_bits\": %u,\n"
        "  \"custom_marks\": %u,\n"
        "  \"description\": \"%s\"\n"
        "}",
        uft_apple2_prot_name(result->primary_type),
        result->type_flags,
        result->overall_confidence,
        result->spiral.detected ? "true" : "false",
        result->cross_track.detected ? "true" : "false",
        result->nibble_count_len,
        result->timing_bit_count,
        result->custom_mark_count,
        result->description);
    
    return written;
}

/* ============================================================================
 * SPIRADISC PROTECTION - Sierra On-Line (Mark Duchaineau)
 * 
 * Spiradisc was developed by Mark Duchaineau for Sierra On-Line.
 * Instead of writing data in concentric circles (normal disk format),
 * Spiradisc writes data in a spiral pattern across the disk surface.
 * 
 * Key characteristics:
 * - Data spans multiple tracks in a continuous spiral
 * - Track-to-track synchronization is critical
 * - Standard nibble copiers fail because they read track by track
 * - Eventually defeated by Copy II Plus v5.0
 * 
 * References:
 * - Steven Levy "Hackers" (2010 ed.), Chapter 19 "Applefest"
 * - Computist Magazine Issue 25, 41, 82, 83 (softkeys)
 * ============================================================================ */

/**
 * @brief Known Spiradisc-protected titles
 */
typedef struct {
    const char *title;
    const char *publisher;
    int year;
    const char *notes;
} spiradisc_title_t;

static const spiradisc_title_t g_spiradisc_titles[] = {
    /* Sierra On-Line titles - primary Spiradisc user */
    {"Lunar Leepers", "Sierra On-Line", 1982, "Early Spiradisc title"},
    {"Frogger", "Sierra On-Line", 1982, "Sega license"},
    {"Jawbreaker", "Sierra On-Line", 1981, "Early Spiradisc title"},
    {"Ultima II", "Sierra On-Line", 1982, "Very early versions only"},
    {"Maze Craze Construction Set", "Sierra On-Line", 1983, "Spiradisc protected"},
    {"Pest Patrol", "Sierra On-Line", 1982, "Spiradisc protected"},
    {"Crossfire", "Sierra On-Line", 1981, "Spiradisc protected"},
    {"Threshold", "Sierra On-Line", 1981, "Spiradisc protected"},
    {"Cannonball Blitz", "Sierra On-Line", 1982, "Spiradisc protected"},
    {"Missile Defense", "Sierra On-Line", 1981, "Spiradisc protected"},
    {"Marauder", "Sierra On-Line", 1982, "Spiradisc protected"},
    {"Mousie", "Sierra On-Line", 1983, "Spiradisc protected"},
    {"Oil's Well", "Sierra On-Line", 1983, "Spiradisc protected"},
    {"Screenwriter II", "Sierra On-Line", 1982, "Spiradisc protected"},
    {"The General Manager", "Sierra On-Line", 1982, "Spiradisc protected"},
    
    /* End marker */
    {NULL, NULL, 0, NULL}
};

#define SPIRADISC_TITLE_COUNT (sizeof(g_spiradisc_titles) / sizeof(g_spiradisc_titles[0]) - 1)

/**
 * @brief Detect Spiradisc protection characteristics
 * 
 * Spiradisc detection looks for:
 * 1. Consistent rotation offset between adjacent tracks
 * 2. Data patterns that span track boundaries
 * 3. Unusual sync patterns at track edges
 * 4. Cross-track byte sequences
 */
typedef struct {
    bool detected;
    double confidence;
    int spiral_tracks;              /* Number of tracks in spiral */
    double rotation_offset;         /* Rotation offset per track */
    int cross_track_sequences;      /* Number of cross-track byte sequences */
    uint8_t spiral_start_track;     /* First track of spiral */
    uint8_t spiral_end_track;       /* Last track of spiral */
    char matched_title[64];         /* Matched known title (if any) */
} spiradisc_result_t;

/**
 * @brief Lookup title in Spiradisc database
 */
static const spiradisc_title_t *lookup_spiradisc_title(const char *title) {
    if (!title) return NULL;
    
    for (size_t i = 0; i < SPIRADISC_TITLE_COUNT; i++) {
        /* Case-insensitive substring match */
        const char *db_title = g_spiradisc_titles[i].title;
        const char *t1 = title;
        const char *t2 = db_title;
        
        while (*t1 && *t2) {
            char c1 = (*t1 >= 'A' && *t1 <= 'Z') ? *t1 + 32 : *t1;
            char c2 = (*t2 >= 'A' && *t2 <= 'Z') ? *t2 + 32 : *t2;
            if (c1 != c2) break;
            t1++; t2++;
        }
        
        if (*t2 == '\0') {
            return &g_spiradisc_titles[i];
        }
    }
    
    return NULL;
}

/**
 * @brief Detect cross-track data sequences characteristic of Spiradisc
 * 
 * Spiradisc writes data continuously across track boundaries.
 * We look for byte sequences that appear at the end of one track
 * and continue at the beginning of the next track.
 */
static int detect_cross_track_sequences(const uint8_t **tracks, 
                                         const size_t *track_lens,
                                         uint8_t track_count,
                                         int *sequence_count) {
    *sequence_count = 0;
    const int MATCH_LEN = 8;  /* Minimum bytes to match */
    
    for (uint8_t t = 0; t < track_count - 1; t++) {
        if (!tracks[t] || !tracks[t + 1]) continue;
        if (track_lens[t] < MATCH_LEN || track_lens[t + 1] < MATCH_LEN) continue;
        
        /* Look for data at end of track t that continues at start of track t+1 */
        size_t end_pos = track_lens[t] - MATCH_LEN;
        
        /* Skip sync bytes at track boundaries */
        while (end_pos > 0 && tracks[t][end_pos] == 0xFF) end_pos--;
        if (end_pos < MATCH_LEN) continue;
        
        /* Find where non-sync data starts on next track */
        size_t start_pos = 0;
        while (start_pos < track_lens[t + 1] && tracks[t + 1][start_pos] == 0xFF) {
            start_pos++;
        }
        if (start_pos + MATCH_LEN > track_lens[t + 1]) continue;
        
        /* Check for matching sequence */
        /* In Spiradisc, data flows continuously, so there should be
           a correlation between track end and next track start */
        int matches = 0;
        for (int i = 0; i < MATCH_LEN; i++) {
            /* Look for XOR correlation (Spiradisc uses XOR scrambling) */
            uint8_t b1 = tracks[t][end_pos - MATCH_LEN + i];
            uint8_t b2 = tracks[t + 1][start_pos + i];
            
            /* Check for direct match or XOR pattern */
            if (b1 == b2 || (b1 ^ b2) == 0xFF || (b1 ^ b2) == 0xAA) {
                matches++;
            }
        }
        
        if (matches >= MATCH_LEN / 2) {
            (*sequence_count)++;
        }
    }
    
    return (*sequence_count > 0) ? 0 : -1;
}

/**
 * @brief Detect Spiradisc protection
 * 
 * @param tracks Array of track data
 * @param track_lens Array of track lengths
 * @param track_count Number of tracks
 * @param title Optional title for database lookup
 * @param result Output Spiradisc result
 * @return 0 on success
 */
int uft_apple2_detect_spiradisc(const uint8_t **tracks, const size_t *track_lens,
                                uint8_t track_count, const char *title,
                                spiradisc_result_t *result) {
    if (!tracks || !track_lens || !result || track_count < 3) return -1;
    
    memset(result, 0, sizeof(*result));
    
    /* First check if title is in known Spiradisc database */
    if (title) {
        const spiradisc_title_t *known = lookup_spiradisc_title(title);
        if (known) {
            result->detected = true;
            result->confidence = 0.95;  /* High confidence for known title */
            strncpy(result->matched_title, known->title, 
                    sizeof(result->matched_title) - 1);
        }
    }
    
    /* Detect spiral track characteristics */
    uft_spiral_track_t spiral;
    int spiral_err = uft_apple2_detect_spiral(tracks, track_lens, track_count, 
                                               0, &spiral);
    
    if (spiral_err == 0 && spiral.detected) {
        result->spiral_tracks = spiral.track_count;
        result->rotation_offset = spiral.rotation_offset;
        result->spiral_start_track = spiral.start_track;
        result->spiral_end_track = spiral.end_track;
        
        /* Adjust confidence based on spiral characteristics */
        if (!result->detected) {
            result->detected = true;
            result->confidence = spiral.confidence;
        } else {
            /* Combine confidences */
            result->confidence = 1.0 - (1.0 - result->confidence) * 
                                       (1.0 - spiral.confidence);
        }
    }
    
    /* Detect cross-track sequences */
    int cross_sequences = 0;
    detect_cross_track_sequences(tracks, track_lens, track_count, &cross_sequences);
    result->cross_track_sequences = cross_sequences;
    
    if (cross_sequences > 3) {
        if (!result->detected) {
            result->detected = true;
            result->confidence = 0.7 + (cross_sequences * 0.05);
            if (result->confidence > 0.95) result->confidence = 0.95;
        } else {
            result->confidence += 0.1;
            if (result->confidence > 1.0) result->confidence = 1.0;
        }
    }
    
    return 0;
}

/**
 * @brief Get number of known Spiradisc titles
 */
int uft_apple2_get_spiradisc_title_count(void) {
    return (int)SPIRADISC_TITLE_COUNT;
}

/**
 * @brief Get Spiradisc title by index
 */
const char *uft_apple2_get_spiradisc_title(int index) {
    if (index < 0 || index >= (int)SPIRADISC_TITLE_COUNT) return NULL;
    return g_spiradisc_titles[index].title;
}

/**
 * @brief Generate Spiradisc analysis report
 */
int uft_apple2_spiradisc_report(const spiradisc_result_t *result,
                                 char *buffer, size_t buffer_size) {
    if (!result || !buffer || buffer_size < 256) return -1;
    
    int written = snprintf(buffer, buffer_size,
        "╔══════════════════════════════════════════════════════════════════╗\n"
        "║       SPIRADISC PROTECTION ANALYSIS (Sierra On-Line)            ║\n"
        "╚══════════════════════════════════════════════════════════════════╝\n\n"
        "Detected: %s\n"
        "Confidence: %.1f%%\n\n",
        result->detected ? "YES" : "No",
        result->confidence * 100.0);
    
    if (result->detected) {
        if (result->matched_title[0]) {
            written += snprintf(buffer + written, buffer_size - written,
                "Matched Title: %s\n\n", result->matched_title);
        }
        
        written += snprintf(buffer + written, buffer_size - written,
            "Spiral Characteristics:\n"
            "  Tracks in Spiral: %d\n"
            "  Rotation Offset: %.4f\n"
            "  Start Track: %d\n"
            "  End Track: %d\n"
            "  Cross-Track Sequences: %d\n\n",
            result->spiral_tracks,
            result->rotation_offset,
            result->spiral_start_track,
            result->spiral_end_track,
            result->cross_track_sequences);
        
        written += snprintf(buffer + written, buffer_size - written,
            "Notes:\n"
            "  - Spiradisc developed by Mark Duchaineau (Sierra On-Line)\n"
            "  - Data written in spiral pattern across disk surface\n"
            "  - Defeated by Copy II Plus v5.0 (1983)\n");
    }
    
    return written;
}
