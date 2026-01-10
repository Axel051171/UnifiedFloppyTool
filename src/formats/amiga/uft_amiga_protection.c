/**
 * @file uft_amiga_protection.c
 * @brief Amiga Copy Protection Analysis - Ported from XCopy Pro
 * 
 * C99 port of XCopy Pro (1989-2011) 68000 Assembly algorithms:
 * - Analyse (track structure analysis)
 * - Search (multi-sync with bit rotation)
 * - gapsearch (GAP detection)
 * - Neuhaus (breakpoint detection)
 * - getracklen (track length measurement)
 * - dostest (DOS track validation)
 * 
 * Original: Frank (1989), H.V. (2011)
 * C99 Port: UFT Team (2025)
 * 
 * @version 1.0.0
 */

#include "uft_amiga_protection.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Static Data - Known Sync Patterns
 *===========================================================================*/

/* All known sync patterns for multi-sync search */
static const uint16_t KNOWN_SYNCS[] = {
    AMIGA_SYNC_STANDARD,    /* $4489 - AmigaDOS */
    AMIGA_SYNC_ARKANOID,    /* $9521 - Arkanoid */
    AMIGA_SYNC_BTIP,        /* $A245 - Beyond the Ice Palace */
    AMIGA_SYNC_MERCENARY,   /* $A89A - Mercenary, Backlash */
    AMIGA_SYNC_ALT1,        /* $448A - Alternative */
    AMIGA_SYNC_INDEX        /* $F8BC - Index copy */
};
static const int KNOWN_SYNC_COUNT = sizeof(KNOWN_SYNCS) / sizeof(KNOWN_SYNCS[0]);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Rotate 32-bit value left by 1 bit
 * Port of ROL.L #1,D0
 */
static inline uint32_t rol32(uint32_t val) {
    return (val << 1) | (val >> 31);
}

/**
 * @brief Read big-endian 16-bit word
 */
static inline uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

/**
 * @brief Read big-endian 32-bit word
 */
static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/*===========================================================================
 * Sync Pattern Identification
 *===========================================================================*/

uft_amiga_sync_type_t uft_amiga_identify_sync(uint16_t pattern) {
    switch (pattern) {
        case AMIGA_SYNC_STANDARD: return SYNC_TYPE_AMIGA_DOS;
        case AMIGA_SYNC_ARKANOID: return SYNC_TYPE_ARKANOID;
        case AMIGA_SYNC_BTIP:     return SYNC_TYPE_BTIP;
        case AMIGA_SYNC_MERCENARY: return SYNC_TYPE_MERCENARY;
        case AMIGA_SYNC_INDEX:    return SYNC_TYPE_INDEX_COPY;
        default:                  return SYNC_TYPE_CUSTOM;
    }
}

const char *uft_amiga_sync_name(uft_amiga_sync_type_t type) {
    switch (type) {
        case SYNC_TYPE_AMIGA_DOS:  return "AmigaDOS";
        case SYNC_TYPE_ARKANOID:   return "Arkanoid";
        case SYNC_TYPE_BTIP:       return "Beyond the Ice Palace";
        case SYNC_TYPE_MERCENARY:  return "Mercenary/Backlash";
        case SYNC_TYPE_INDEX_COPY: return "Index Copy";
        case SYNC_TYPE_CUSTOM:     return "Custom";
        default:                   return "Unknown";
    }
}

const char *uft_amiga_class_name(uft_amiga_track_class_t cls) {
    switch (cls) {
        case TRACK_CLASS_DOS:        return "AmigaDOS";
        case TRACK_CLASS_NIBBLE:     return "Nibble Copy";
        case TRACK_CLASS_LONG:       return "Long Track";
        case TRACK_CLASS_BREAKPOINT: return "Breakpoint";
        case TRACK_CLASS_NO_SYNC:    return "No Sync";
        case TRACK_CLASS_ERROR:      return "Error";
        default:                     return "Unknown";
    }
}

/*===========================================================================
 * Sync Search with Bit Rotation
 * Port of XCopy Pro Search routine
 *===========================================================================*/

int uft_amiga_find_syncs_rotated(const uint8_t *data, size_t size,
                                 const uint16_t *patterns, int pattern_count,
                                 uft_amiga_sync_pos_t *positions, int max_positions) {
    if (!data || !patterns || !positions || size < 4) {
        return 0;
    }
    
    int found = 0;
    size_t pos = 0;
    
    /* Start with first 32-bit word */
    uint32_t d0 = read_be32(data);
    pos = 4;
    
    while (pos < size - 2 && found < max_positions) {
        /* Shift in next word */
        /* swap D0 / move.w (A2)+,D0 / swap D0 */
        d0 = (d0 >> 16) | ((uint32_t)read_be16(data + pos) << 16);
        pos += 2;
        
        uint32_t rotated = d0;
        
        /* Try all 16 bit rotations */
        for (int bit = 0; bit < 16; bit++) {
            uint16_t word = (uint16_t)(rotated >> 16);
            
            /* Check against all patterns */
            for (int p = 0; p < pattern_count; p++) {
                if (word == patterns[p]) {
                    positions[found].position = pos - 4;
                    positions[found].bit_offset = bit;
                    positions[found].pattern = patterns[p];
                    positions[found].type = uft_amiga_identify_sync(patterns[p]);
                    found++;
                    
                    if (found >= max_positions) {
                        return found;
                    }
                    
                    /* Skip minimal sector length before next search */
                    pos += 0x100;  /* $100 = 256 bytes minimum */
                    if (pos < size - 2) {
                        d0 = read_be32(data + pos - 4);
                    }
                    break;
                }
            }
            
            /* ROL.L #1,D0 */
            rotated = rol32(rotated);
        }
    }
    
    return found;
}

int uft_amiga_find_sync_standard(const uint8_t *data, size_t size,
                                 size_t *positions, int max_positions) {
    if (!data || !positions || size < 2) {
        return 0;
    }
    
    int found = 0;
    
    for (size_t i = 0; i < size - 1 && found < max_positions; i++) {
        uint16_t word = read_be16(data + i);
        if (word == AMIGA_SYNC_STANDARD) {
            positions[found++] = i;
            i += 0x100;  /* Skip minimum sector */
        }
    }
    
    return found;
}

/*===========================================================================
 * Track Length Measurement
 * Port of XCopy Pro getracklen routine
 *===========================================================================*/

size_t uft_amiga_measure_track_length(const uint8_t *data, size_t size,
                                      size_t *end_pos) {
    if (!data || size < 2) {
        return 0;
    }
    
    /* 
     * Original algorithm from XCopy Pro:
     * getracklen:
     *   move.w  #RLEN-2,D0      ; Read LEN in Bytes
     *   lea     RLEN*2(A3),A1   ; Ende des Puffers
     * 1$ tst.w   -(A1)          ; letztes Word im Puffer suchen
     *   dbne    D0,1$
     *   addq.l  #2,A1
     *   move.l  A1,D0
     *   sub.l   A3,D0           ; 2*Tracklaenge in Bytes
     *   lsr.w   #1,D0           ; durch 2 => Mitte
     */
    
    /* Find last non-zero word from end of buffer */
    size_t pos = size;
    while (pos >= 2) {
        pos -= 2;
        uint16_t word = read_be16(data + pos);
        if (word != 0) {
            pos += 2;  /* Point past last data */
            break;
        }
    }
    
    if (end_pos) {
        *end_pos = pos;
    }
    
    /* For 2-rotation read, track length is half */
    size_t track_len = pos / 2;
    
    /* Ensure even alignment */
    track_len &= ~1;
    
    return track_len;
}

bool uft_amiga_is_long_track(size_t track_length) {
    return track_length >= AMIGA_TRACKLEN_LONG;
}

/*===========================================================================
 * Breakpoint Detection (Neuhaus Algorithm)
 * Port of XCopy Pro Neuhaus routine
 *===========================================================================*/

bool uft_amiga_detect_breakpoints(const uint8_t *data, size_t size,
                                  int *breakpoint_count,
                                  size_t *positions, int max_positions) {
    if (!data || !breakpoint_count || size < 16) {
        return false;
    }
    
    /*
     * Original algorithm from XCopy Pro:
     * Neuhaus:
     *   moveq   #0,D6           ; Bruchstellenzaehler
     *   move.l  A3,A0           ; A3=Puffer Anfang
     *   move.l  A3,A2
     * vergleich:
     *   move.b  (A0)+,D1
     * comp:
     *   cmp.b   (A0)+,D1
     *   bne.s   ungleich
     *   subq.w  #1,D7
     *   bne     comp
     *   ...
     * ungleich:
     *   addq.w  #1,D6           ; Bruchstelle gefunden
     *   move.l  A0,A2
     *   subq.l  #8,A2
     *   cmp.w   #5,D6           ; max 5 Bruchstellen
     *   ble     vergleich
     */
    
    int bp_count = 0;
    size_t bytes_left = size - 8;
    const uint8_t *p = data;
    
    while (bytes_left > 0) {
        uint8_t val = *p++;
        bytes_left--;
        
        /* Count consecutive identical bytes */
        while (bytes_left > 0 && *p == val) {
            p++;
            bytes_left--;
        }
        
        if (bytes_left > 0 && *p != val) {
            /* Breakpoint found */
            bp_count++;
            
            if (positions && bp_count <= max_positions) {
                positions[bp_count - 1] = (size_t)(p - data);
            }
            
            if (bp_count > MAX_BREAKPOINTS) {
                /* Too many breakpoints - not a valid protection */
                *breakpoint_count = bp_count;
                return false;
            }
        }
    }
    
    *breakpoint_count = bp_count;
    return (bp_count > 0 && bp_count <= MAX_BREAKPOINTS);
}

/*===========================================================================
 * GAP Analysis
 * Port of XCopy Pro gapsearch routine
 *===========================================================================*/

bool uft_amiga_find_gap(const size_t *sync_positions, int sync_count,
                        int *gap_index, size_t *gap_length) {
    if (!sync_positions || !gap_index || sync_count < 2) {
        return false;
    }
    
    /*
     * Original algorithm from XCopy Pro:
     * gapsearch:
     *   lea     coutab(A0),A0   ; die Laenge,die am wenigsten vorkommt
     *   moveq   #$7f,D2         ; wird als GAP betrachtet. D2=MAX
     * 1$ move.l  (A0)+,D1       ; Minimum suchen
     *   beq.s   gap_fou         ; TABEND
     *   cmp.w   D2,D1
     *   bgt.s   1$              ; letzten kleinen nehmen
     *   move.w  D1,D2
     *   swap    D1
     *   move.w  D1,D5           ; Laenge des Minimums nach D5
     */
    
    /* Calculate sector lengths */
    uint16_t lengths[MAX_SYNC_POSITIONS];
    uint16_t counts[MAX_SYNC_POSITIONS] = {0};
    int unique_count = 0;
    
    for (int i = 0; i < sync_count - 1; i++) {
        uint16_t len = (uint16_t)(sync_positions[i + 1] - sync_positions[i]);
        lengths[i] = len;
        
        /* Count occurrences with tolerance */
        bool found = false;
        for (int j = 0; j < unique_count; j++) {
            int16_t diff = (int16_t)len - (int16_t)lengths[j];
            if (diff < 0) diff = -diff;
            
            if (diff <= SECTOR_LEN_TOLERANCE) {
                counts[j]++;
                found = true;
                break;
            }
        }
        
        if (!found && unique_count < MAX_SYNC_POSITIONS) {
            lengths[unique_count] = len;
            counts[unique_count] = 1;
            unique_count++;
        }
    }
    
    /* Find minimum occurrence (GAP) */
    int min_count = 0x7F;
    uint16_t gap_len = 0;
    
    for (int i = 0; i < unique_count; i++) {
        if (counts[i] < min_count) {
            min_count = counts[i];
            gap_len = lengths[i];
        }
    }
    
    /* Find sector index with this length */
    for (int i = 0; i < sync_count - 1; i++) {
        int16_t diff = (int16_t)lengths[i] - (int16_t)gap_len;
        if (diff < 0) diff = -diff;
        
        if (diff <= SECTOR_LEN_TOLERANCE) {
            *gap_index = i + 1;  /* Sector AFTER GAP */
            if (gap_length) {
                *gap_length = lengths[i];
            }
            return true;
        }
    }
    
    return false;
}

size_t uft_amiga_calc_write_start(const uint8_t *track_data, size_t track_length,
                                  int gap_sector_index, const size_t *sync_positions) {
    (void)track_data;   /* Reserved for future use */
    (void)track_length; /* Reserved for future use */
    
    if (!sync_positions || gap_sector_index <= 0) {
        return 0;
    }
    
    /*
     * From XCopy Pro:
     * - Write starts 10 bytes before sync after GAP
     * - If too close to buffer start, use second rotation
     */
    
    size_t start = sync_positions[gap_sector_index];
    
    /* Subtract 10 bytes (0x0A) for pre-sync gap */
    if (start >= 10) {
        start -= 10;
    }
    
    return start;
}

/*===========================================================================
 * DOS Track Validation
 * Port of XCopy Pro dostest
 *===========================================================================*/

bool uft_amiga_is_dos_track(const uft_amiga_track_analysis_t *result) {
    if (!result) return false;
    
    /*
     * From XCopy Pro dostest:
     * - Sync must be $4489
     * - Must have 11 sectors
     * - Each sector $440 bytes
     */
    
    if (result->sync_pattern != AMIGA_SYNC_STANDARD) {
        return false;
    }
    
    if (result->sector_count != AMIGA_SECTORS_DD &&
        result->sector_count != AMIGA_SECTORS_HD) {
        return false;
    }
    
    /* Check for uniform sector length of $440 */
    for (int i = 0; i < result->unique_lengths; i++) {
        if (result->sector_lengths[i].count >= 9) {
            /* Most sectors should be standard size */
            int16_t diff = (int16_t)result->sector_lengths[i].length - 
                           AMIGA_SECTOR_MFM_SIZE;
            if (diff < 0) diff = -diff;
            
            if (diff <= SECTOR_LEN_TOLERANCE) {
                return true;
            }
        }
    }
    
    return false;
}

bool uft_amiga_is_pseudo_dos(const uft_amiga_track_analysis_t *result) {
    if (!result) return false;
    
    /*
     * Pseudo-DOS: Looks like DOS (sync=$4489, 11 sectors) but
     * sector lengths are not uniform $440
     */
    
    if (result->sync_pattern != AMIGA_SYNC_STANDARD) {
        return false;
    }
    
    if (result->sector_count != AMIGA_SECTORS_DD) {
        return false;
    }
    
    /* Check if NOT all sectors are $440 */
    for (int i = 0; i < result->unique_lengths; i++) {
        int16_t diff = (int16_t)result->sector_lengths[i].length - 
                       AMIGA_SECTOR_MFM_SIZE;
        if (diff < 0) diff = -diff;
        
        if (diff > SECTOR_LEN_TOLERANCE && result->sector_lengths[i].count > 0) {
            return true;  /* Found non-standard sector */
        }
    }
    
    return false;
}

/*===========================================================================
 * Protection Identification
 *===========================================================================*/

bool uft_amiga_identify_protection(const uft_amiga_track_analysis_t *result,
                                   char *name, size_t name_size) {
    if (!result || !name || name_size == 0) {
        return false;
    }
    
    name[0] = '\0';
    
    /* Check by sync pattern */
    switch (result->sync_type) {
        case SYNC_TYPE_ARKANOID:
            snprintf(name, name_size, "Arkanoid Protection");
            return true;
            
        case SYNC_TYPE_BTIP:
            snprintf(name, name_size, "Ocean/Imagine Protection");
            return true;
            
        case SYNC_TYPE_MERCENARY:
            snprintf(name, name_size, "Novagen Protection");
            return true;
            
        default:
            break;
    }
    
    /* Check by track characteristics */
    if (result->is_long_track) {
        snprintf(name, name_size, "Long Track Protection");
        return true;
    }
    
    if (result->has_breakpoints) {
        snprintf(name, name_size, "Breakpoint Protection (Neuhaus)");
        return true;
    }
    
    if (result->is_pseudo_dos) {
        snprintf(name, name_size, "Pseudo-DOS Protection");
        return true;
    }
    
    if (result->sync_type == SYNC_TYPE_CUSTOM) {
        snprintf(name, name_size, "Custom Sync ($%04X)", result->sync_pattern);
        return true;
    }
    
    return false;
}

/*===========================================================================
 * Main Analysis Function
 * Port of XCopy Pro Analyse routine
 *===========================================================================*/

int uft_amiga_analyze_track(const uint8_t *track_data, size_t track_size,
                            uft_amiga_track_analysis_t *result) {
    uft_amiga_analysis_ctx_t ctx = {
        .track_data = track_data,
        .track_size = track_size,
        .detect_custom_sync = true,
        .detect_breakpoints = true,
        .force_sync = 0
    };
    
    return uft_amiga_analyze_track_ex(&ctx, result);
}

int uft_amiga_analyze_track_ex(uft_amiga_analysis_ctx_t *ctx,
                               uft_amiga_track_analysis_t *result) {
    if (!ctx || !ctx->track_data || !result || ctx->track_size < 100) {
        return -1;
    }
    
    memset(result, 0, sizeof(*result));
    
    const uint8_t *data = ctx->track_data;
    size_t size = ctx->track_size;
    
    /* Step 1: Measure track length */
    size_t end_pos = 0;
    result->track_length = uft_amiga_measure_track_length(data, size, &end_pos);
    result->read_length = end_pos;
    
    /* Check for long track */
    result->is_long_track = uft_amiga_is_long_track(result->track_length);
    if (result->is_long_track) {
        result->classification = TRACK_CLASS_LONG;
    }
    
    /* Step 2: Search for sync patterns */
    if (ctx->force_sync != 0) {
        /* Use forced sync */
        result->sync_count = uft_amiga_find_sync_standard(
            data, result->track_length, ctx->sync_pos_buffer, MAX_SYNC_POSITIONS);
        result->sync_pattern = ctx->force_sync;
        result->sync_type = uft_amiga_identify_sync(ctx->force_sync);
    } else if (ctx->detect_custom_sync) {
        /* Search for all known sync patterns with bit rotation */
        result->sync_count = uft_amiga_find_syncs_rotated(
            data, result->track_length,
            KNOWN_SYNCS, KNOWN_SYNC_COUNT,
            result->sync_positions, MAX_SYNC_POSITIONS);
        
        if (result->sync_count > 0) {
            result->sync_pattern = result->sync_positions[0].pattern;
            result->sync_type = result->sync_positions[0].type;
            
            /* Copy positions to simple array for GAP analysis */
            for (int i = 0; i < result->sync_count; i++) {
                ctx->sync_pos_buffer[i] = result->sync_positions[i].position;
            }
        }
    } else {
        /* Standard sync only */
        result->sync_count = uft_amiga_find_sync_standard(
            data, result->track_length, ctx->sync_pos_buffer, MAX_SYNC_POSITIONS);
        result->sync_pattern = AMIGA_SYNC_STANDARD;
        result->sync_type = SYNC_TYPE_AMIGA_DOS;
    }
    
    /* Step 3: Handle no sync case */
    if (result->sync_count == 0) {
        /* Try breakpoint detection */
        if (ctx->detect_breakpoints) {
            result->has_breakpoints = uft_amiga_detect_breakpoints(
                data, result->track_length,
                &result->breakpoint_count, NULL, 0);
            
            if (result->has_breakpoints) {
                result->classification = TRACK_CLASS_BREAKPOINT;
                result->confidence = 0.6f;
                return 0;
            }
        }
        
        result->classification = TRACK_CLASS_NO_SYNC;
        result->confidence = 0.0f;
        return 0;
    }
    
    result->sector_count = result->sync_count;
    
    /* Step 4: Analyze sector lengths and find GAP */
    /* Calculate sector lengths */
    int unique_lens = 0;
    for (int i = 0; i < result->sync_count - 1; i++) {
        uint16_t len = (uint16_t)(ctx->sync_pos_buffer[i + 1] - 
                                  ctx->sync_pos_buffer[i]);
        
        /* Add to unique lengths */
        bool found = false;
        for (int j = 0; j < unique_lens; j++) {
            int16_t diff = (int16_t)len - (int16_t)result->sector_lengths[j].length;
            if (diff < 0) diff = -diff;
            
            if (diff <= SECTOR_LEN_TOLERANCE) {
                result->sector_lengths[j].count++;
                found = true;
                break;
            }
        }
        
        if (!found && unique_lens < MAX_SYNC_POSITIONS) {
            result->sector_lengths[unique_lens].length = len;
            result->sector_lengths[unique_lens].count = 1;
            unique_lens++;
        }
    }
    result->unique_lengths = unique_lens;
    
    /* Find GAP */
    uft_amiga_find_gap(ctx->sync_pos_buffer, result->sync_count,
                       &result->gap_sector_index, &result->gap_length);
    
    /* Calculate write start */
    result->write_start_offset = uft_amiga_calc_write_start(
        data, result->track_length,
        result->gap_sector_index, ctx->sync_pos_buffer);
    
    /* Step 5: Classify track */
    if (uft_amiga_is_dos_track(result)) {
        result->classification = TRACK_CLASS_DOS;
        result->confidence = 0.95f;
    } else if (uft_amiga_is_pseudo_dos(result)) {
        result->is_pseudo_dos = true;
        result->classification = TRACK_CLASS_NIBBLE;
        result->confidence = 0.8f;
    } else if (result->is_long_track) {
        result->classification = TRACK_CLASS_LONG;
        result->confidence = 0.9f;
    } else {
        result->classification = TRACK_CLASS_NIBBLE;
        result->confidence = 0.7f;
    }
    
    /* Step 6: Identify protection */
    uft_amiga_identify_protection(result, result->protection_name,
                                  sizeof(result->protection_name));
    
    return 0;
}

/*===========================================================================
 * Reporting
 *===========================================================================*/

void uft_amiga_print_analysis(const uft_amiga_track_analysis_t *result) {
    if (!result) return;
    
    printf("=== Amiga Track Analysis ===\n");
    printf("Classification: %s\n", uft_amiga_class_name(result->classification));
    printf("Sync Pattern:   $%04X (%s)\n", 
           result->sync_pattern, uft_amiga_sync_name(result->sync_type));
    printf("Sync Count:     %d\n", result->sync_count);
    printf("Track Length:   %zu bytes (0x%zX)\n", 
           result->track_length, result->track_length);
    printf("Sector Count:   %d\n", result->sector_count);
    printf("GAP at sector:  %d (length: %zu)\n", 
           result->gap_sector_index, result->gap_length);
    printf("Write Offset:   %zu\n", result->write_start_offset);
    printf("Long Track:     %s\n", result->is_long_track ? "Yes" : "No");
    printf("Breakpoints:    %s (%d)\n", 
           result->has_breakpoints ? "Yes" : "No", result->breakpoint_count);
    printf("Confidence:     %.1f%%\n", result->confidence * 100.0f);
    
    if (result->protection_name[0]) {
        printf("Protection:     %s\n", result->protection_name);
    }
    
    if (result->unique_lengths > 0) {
        printf("Sector Lengths:\n");
        for (int i = 0; i < result->unique_lengths; i++) {
            printf("  $%04X: %d sectors\n",
                   result->sector_lengths[i].length,
                   result->sector_lengths[i].count);
        }
    }
}

int uft_amiga_analysis_to_json(const uft_amiga_track_analysis_t *result,
                               char *buffer, size_t size) {
    if (!result || !buffer || size == 0) return -1;
    
    return snprintf(buffer, size,
        "{\n"
        "  \"classification\": \"%s\",\n"
        "  \"sync_pattern\": \"0x%04X\",\n"
        "  \"sync_type\": \"%s\",\n"
        "  \"sync_count\": %d,\n"
        "  \"track_length\": %zu,\n"
        "  \"sector_count\": %d,\n"
        "  \"gap_sector\": %d,\n"
        "  \"gap_length\": %zu,\n"
        "  \"write_offset\": %zu,\n"
        "  \"is_long_track\": %s,\n"
        "  \"has_breakpoints\": %s,\n"
        "  \"breakpoint_count\": %d,\n"
        "  \"is_pseudo_dos\": %s,\n"
        "  \"confidence\": %.2f,\n"
        "  \"protection\": \"%s\"\n"
        "}",
        uft_amiga_class_name(result->classification),
        result->sync_pattern,
        uft_amiga_sync_name(result->sync_type),
        result->sync_count,
        result->track_length,
        result->sector_count,
        result->gap_sector_index,
        result->gap_length,
        result->write_start_offset,
        result->is_long_track ? "true" : "false",
        result->has_breakpoints ? "true" : "false",
        result->breakpoint_count,
        result->is_pseudo_dos ? "true" : "false",
        result->confidence,
        result->protection_name[0] ? result->protection_name : "none"
    );
}
