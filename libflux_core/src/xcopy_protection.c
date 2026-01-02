// SPDX-License-Identifier: MIT
/*
 * xcopy_protection.c - Copy Protection Pattern Detection
 * 
 * Detects specific copy protection schemes used on Amiga disks.
 * Based on X-Copy Professional analysis methods.
 * 
 * Known Protection Schemes:
 *   - Rob Northen Copylock (Long tracks)
 *   - Gremlin Graphics (Modified sector count)
 *   - Hexagon (Custom sync patterns)
 *   - COPS (Variable density)
 *   - Speedlock (Weak bits)
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "xcopy_errors.h"
#include "mfm_decode.h"

/*============================================================================*
 * COPY PROTECTION PATTERNS
 *============================================================================*/

typedef enum {
    CP_PATTERN_NONE,
    CP_PATTERN_ROB_NORTHEN,      /* Rob Northen Copylock */
    CP_PATTERN_GREMLIN,          /* Gremlin Graphics */
    CP_PATTERN_HEXAGON,          /* Hexagon */
    CP_PATTERN_COPS,             /* COPS */
    CP_PATTERN_SPEEDLOCK,        /* Speedlock */
    CP_PATTERN_LONG_TRACK,       /* Generic long track */
    CP_PATTERN_NO_SECTORS,       /* Track with no sectors */
    CP_PATTERN_WEAK_BITS,        /* Weak bit protection */
    CP_PATTERN_SYNC_SHIFT,       /* Shifted sync marks */
    CP_PATTERN_FUZZY_BITS,       /* Fuzzy bits */
    CP_PATTERN_VARIABLE_DENSITY, /* Variable bit density */
    CP_PATTERN_UNKNOWN           /* Unknown protection */
} cp_pattern_t;

typedef struct {
    cp_pattern_t pattern;
    const char *name;
    const char *description;
    uint32_t confidence;  /* 0-100% */
} cp_detection_t;

/*============================================================================*
 * PATTERN NAMES
 *============================================================================*/

static const char* cp_pattern_names[] = {
    "None",
    "Rob Northen Copylock",
    "Gremlin Graphics",
    "Hexagon",
    "COPS",
    "Speedlock",
    "Long Track",
    "No Sectors",
    "Weak Bits",
    "Sync Shift",
    "Fuzzy Bits",
    "Variable Density",
    "Unknown"
};

static const char* cp_pattern_descriptions[] = {
    "No copy protection detected",
    "Rob Northen Copylock - Long track with exact length pattern",
    "Gremlin Graphics - Modified sector count and timing",
    "Hexagon - Custom sync patterns",
    "COPS - Variable bit density encoding",
    "Speedlock - Weak bit areas on track",
    "Generic long track protection",
    "Track with no valid sectors",
    "Weak bit protection scheme",
    "Shifted or modified sync marks",
    "Fuzzy bit protection",
    "Variable density encoding",
    "Unknown copy protection scheme"
};

/*============================================================================*
 * ROB NORTHEN COPYLOCK DETECTION
 *============================================================================*/

/**
 * @brief Detect Rob Northen Copylock
 * 
 * Rob Northen's protection uses precisely timed long tracks.
 * Track lengths are typically 12800-13500 bytes.
 * 
 * Characteristics:
 *   - Track length > 12500 bytes
 *   - Usually track 0 or 1
 *   - Precise timing required
 *   - Often combined with checksums
 */
bool xcopy_is_rob_northen(const xcopy_track_error_t *error)
{
    if (!error) return false;
    
    /* Long track is primary indicator */
    if (error->error_code != XCOPY_ERR_LONG_TRACK) {
        return false;
    }
    
    /* Rob Northen tracks are typically 12800-13500 bytes */
    if (error->track_length < 12500 || error->track_length > 14000) {
        return false;
    }
    
    /* High confidence if length is in sweet spot */
    if (error->track_length >= 12800 && error->track_length <= 13500) {
        return true;
    }
    
    return false;
}

/**
 * @brief Get Rob Northen confidence level
 */
static uint32_t rob_northen_confidence(const xcopy_track_error_t *error)
{
    if (!xcopy_is_rob_northen(error)) return 0;
    
    /* Perfect range = 95% confidence */
    if (error->track_length >= 12900 && error->track_length <= 13400) {
        return 95;
    }
    
    /* Good range = 80% confidence */
    if (error->track_length >= 12800 && error->track_length <= 13500) {
        return 80;
    }
    
    /* Acceptable range = 60% confidence */
    return 60;
}

/*============================================================================*
 * GREMLIN GRAPHICS DETECTION
 *============================================================================*/

/**
 * @brief Detect Gremlin Graphics protection
 * 
 * Gremlin used modified sector counts and unusual gap timing.
 * 
 * Characteristics:
 *   - Sector count != 11
 *   - Often 10 or 12 sectors
 *   - Modified gap timing
 */
bool xcopy_is_gremlin_graphics(const xcopy_track_error_t *error)
{
    if (!error) return false;
    
    /* Primary indicator: wrong sector count */
    if (error->error_code != XCOPY_ERR_SECTOR_COUNT) {
        return false;
    }
    
    /* Gremlin typically uses 10 or 12 sectors */
    if (error->sector_count == 10 || error->sector_count == 12) {
        return true;
    }
    
    return false;
}

/*============================================================================*
 * HEXAGON DETECTION
 *============================================================================*/

/**
 * @brief Detect Hexagon protection
 * 
 * Hexagon used custom sync patterns.
 * 
 * Characteristics:
 *   - No standard sync marks found
 *   - Custom sync patterns
 */
bool xcopy_is_hexagon(const xcopy_track_error_t *error)
{
    if (!error) return false;
    
    /* No sync marks found */
    if (error->error_code == XCOPY_ERR_NO_SYNC && !error->sync_found) {
        return true;
    }
    
    return false;
}

/*============================================================================*
 * GENERIC PROTECTION DETECTION
 *============================================================================*/

/**
 * @brief Detect if track has copy protection
 * 
 * Generic detection based on error patterns.
 */
static cp_pattern_t detect_generic_pattern(const xcopy_track_error_t *error)
{
    if (!error) return CP_PATTERN_NONE;
    
    switch (error->error_code) {
        case XCOPY_ERR_NONE:
            return CP_PATTERN_NONE;
            
        case XCOPY_ERR_SECTOR_COUNT:
            if (error->sector_count == 0) {
                return CP_PATTERN_NO_SECTORS;
            }
            return CP_PATTERN_UNKNOWN;
            
        case XCOPY_ERR_NO_SYNC:
            return CP_PATTERN_SYNC_SHIFT;
            
        case XCOPY_ERR_GAP_SYNC:
            return CP_PATTERN_VARIABLE_DENSITY;
            
        case XCOPY_ERR_LONG_TRACK:
            return CP_PATTERN_LONG_TRACK;
            
        default:
            return CP_PATTERN_UNKNOWN;
    }
}

/*============================================================================*
 * MAIN DETECTION FUNCTION
 *============================================================================*/

/**
 * @brief Detect copy protection pattern
 * 
 * Analyzes track error and determines which protection scheme
 * is most likely being used.
 */
int xcopy_detect_protection_pattern(
    const xcopy_track_error_t *error,
    cp_detection_t *detection_out
) {
    if (!error || !detection_out) {
        return -1;
    }
    
    memset(detection_out, 0, sizeof(*detection_out));
    
    /* No protection detected */
    if (!error->is_protected || error->error_code == XCOPY_ERR_NONE) {
        detection_out->pattern = CP_PATTERN_NONE;
        detection_out->name = cp_pattern_names[CP_PATTERN_NONE];
        detection_out->description = cp_pattern_descriptions[CP_PATTERN_NONE];
        detection_out->confidence = 100;
        return 0;
    }
    
    /* Check for specific protection schemes (in order of specificity) */
    
    /* Rob Northen Copylock - very specific signature */
    if (xcopy_is_rob_northen(error)) {
        detection_out->pattern = CP_PATTERN_ROB_NORTHEN;
        detection_out->name = cp_pattern_names[CP_PATTERN_ROB_NORTHEN];
        detection_out->description = cp_pattern_descriptions[CP_PATTERN_ROB_NORTHEN];
        detection_out->confidence = rob_northen_confidence(error);
        return 0;
    }
    
    /* Gremlin Graphics - sector count based */
    if (xcopy_is_gremlin_graphics(error)) {
        detection_out->pattern = CP_PATTERN_GREMLIN;
        detection_out->name = cp_pattern_names[CP_PATTERN_GREMLIN];
        detection_out->description = cp_pattern_descriptions[CP_PATTERN_GREMLIN];
        detection_out->confidence = 75;
        return 0;
    }
    
    /* Hexagon - custom sync */
    if (xcopy_is_hexagon(error)) {
        detection_out->pattern = CP_PATTERN_HEXAGON;
        detection_out->name = cp_pattern_names[CP_PATTERN_HEXAGON];
        detection_out->description = cp_pattern_descriptions[CP_PATTERN_HEXAGON];
        detection_out->confidence = 70;
        return 0;
    }
    
    /* Generic pattern detection */
    cp_pattern_t generic = detect_generic_pattern(error);
    detection_out->pattern = generic;
    detection_out->name = cp_pattern_names[generic];
    detection_out->description = cp_pattern_descriptions[generic];
    detection_out->confidence = (generic == CP_PATTERN_UNKNOWN) ? 50 : 65;
    
    return 0;
}

/*============================================================================*
 * MULTI-TRACK ANALYSIS
 *============================================================================*/

/**
 * @brief Disk protection analysis
 */
typedef struct {
    int total_tracks;
    int protected_tracks;
    int clean_tracks;
    
    cp_pattern_t detected_patterns[10];
    int pattern_count;
    
    cp_pattern_t primary_pattern;
    uint32_t primary_confidence;
    
} disk_protection_t;

/**
 * @brief Analyze entire disk for copy protection
 * 
 * Looks at all tracks and determines overall protection scheme.
 */
int xcopy_analyze_disk_protection(
    const xcopy_track_error_t *track_errors,
    int track_count,
    disk_protection_t *disk_out
) {
    if (!track_errors || !disk_out || track_count <= 0) {
        return -1;
    }
    
    memset(disk_out, 0, sizeof(*disk_out));
    disk_out->total_tracks = track_count;
    
    /* Count protection patterns */
    int pattern_votes[13] = {0};  /* One per CP_PATTERN_* */
    
    for (int i = 0; i < track_count; i++) {
        const xcopy_track_error_t *error = &track_errors[i];
        
        if (!error->is_protected) {
            disk_out->clean_tracks++;
            continue;
        }
        
        disk_out->protected_tracks++;
        
        /* Detect pattern */
        cp_detection_t detection;
        if (xcopy_detect_protection_pattern(error, &detection) == 0) {
            pattern_votes[detection.pattern]++;
            
            /* Store unique patterns */
            bool already_stored = false;
            for (int j = 0; j < disk_out->pattern_count; j++) {
                if (disk_out->detected_patterns[j] == detection.pattern) {
                    already_stored = true;
                    break;
                }
            }
            
            if (!already_stored && disk_out->pattern_count < 10) {
                disk_out->detected_patterns[disk_out->pattern_count++] = 
                    detection.pattern;
            }
        }
    }
    
    /* Determine primary pattern (most votes) */
    int max_votes = 0;
    cp_pattern_t primary = CP_PATTERN_NONE;
    
    for (int i = 0; i < 13; i++) {
        if (pattern_votes[i] > max_votes) {
            max_votes = pattern_votes[i];
            primary = (cp_pattern_t)i;
        }
    }
    
    disk_out->primary_pattern = primary;
    
    /* Calculate confidence based on consistency */
    if (disk_out->protected_tracks > 0) {
        disk_out->primary_confidence = 
            (max_votes * 100) / disk_out->protected_tracks;
    } else {
        disk_out->primary_confidence = 100;
    }
    
    return 0;
}

/**
 * @brief Print disk protection analysis
 */
void xcopy_print_disk_protection(const disk_protection_t *disk)
{
    if (!disk) return;
    
    printf("Disk Protection Analysis:\n");
    printf("  Total tracks:     %d\n", disk->total_tracks);
    printf("  Protected tracks: %d\n", disk->protected_tracks);
    printf("  Clean tracks:     %d\n", disk->clean_tracks);
    
    if (disk->protected_tracks > 0) {
        printf("\n  Primary Protection: %s\n", 
               cp_pattern_names[disk->primary_pattern]);
        printf("  Confidence:         %u%%\n", disk->primary_confidence);
        
        if (disk->pattern_count > 1) {
            printf("\n  Multiple patterns detected:\n");
            for (int i = 0; i < disk->pattern_count; i++) {
                printf("    - %s\n", 
                       cp_pattern_names[disk->detected_patterns[i]]);
            }
        }
    } else {
        printf("\n  No copy protection detected.\n");
    }
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
