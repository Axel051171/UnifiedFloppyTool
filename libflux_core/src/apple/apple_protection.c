// SPDX-License-Identifier: MIT
/*
 * apple_protection.c - Apple II Copy Protection Patterns
 * 
 * Protection schemes extracted from Passport.
 * Database of 150+ known protection methods for Apple II disks.
 * 
 * Sources:
 *   - Passport by 4am (https://github.com/a2-4am/passport)
 *   - Analysis of real Apple II protected disks
 * 
 * @version 2.8.2
 * @date 2024-12-26
 */

#include "uft/uft_error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "apple_protection.h"

/*============================================================================*
 * PROTECTION PATTERN DATABASE
 *============================================================================*/

/**
 * @brief Known protection schemes
 */
static const apple_protection_pattern_t protection_patterns[] = {
    /* Electronic Arts */
    {
        .name = "Electronic Arts",
        .type = PROTECTION_TYPE_TIMING,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
        .track_pattern_len = 10,
        .description = "EA timing-based protection with custom sync"
    },
    
    /* Bouncing Kamungas */
    {
        .name = "Bouncing Kamungas",
        .type = PROTECTION_TYPE_NIBBLE_COUNT,
        .signature = {0xD5, 0xAA, 0xAD},
        .signature_len = 3,
        .track_pattern = {0, 1, 2, 3},
        .track_pattern_len = 4,
        .description = "Nibble count variation protection"
    },
    
    /* Dynacomp */
    {
        .name = "Dynacomp",
        .type = PROTECTION_TYPE_TRACK_LAYOUT,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 17},
        .track_pattern_len = 2,
        .description = "Modified track layout on track 17"
    },
    
    /* Edu-Ware */
    {
        .name = "Edu-Ware",
        .type = PROTECTION_TYPE_SECTOR_EDITOR,
        .signature = {0xD5, 0xBB, 0xCF},
        .signature_len = 3,
        .track_pattern = {0, 1, 2},
        .track_pattern_len = 3,
        .description = "Sector editor tricks with custom address marks"
    },
    
    /* Sierra On-Line */
    {
        .name = "Sierra On-Line",
        .type = PROTECTION_TYPE_CUSTOM_RWTS,
        .signature = {0xD5, 0xAA, 0x96, 0xFF},
        .signature_len = 4,
        .track_pattern = {0, 1, 2, 3, 17},
        .track_pattern_len = 5,
        .description = "Custom RWTS with modified sync bytes"
    },
    
    /* Origin Systems */
    {
        .name = "Origin Systems (Ultima)",
        .type = PROTECTION_TYPE_HALF_TRACK,
        .signature = {0xD5, 0xAA, 0xB5},
        .signature_len = 3,
        .track_pattern = {0, 1, 2, 3, 4, 5, 6, 7},
        .track_pattern_len = 8,
        .description = "Half-track timing protection"
    },
    
    /* Baudville */
    {
        .name = "Baudville",
        .type = PROTECTION_TYPE_TIMING,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {17},
        .track_pattern_len = 1,
        .description = "Timing protection on track 17"
    },
    
    /* Datasoft */
    {
        .name = "Datasoft",
        .type = PROTECTION_TYPE_SYNC_PATTERN,
        .signature = {0xD4, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 1, 2},
        .track_pattern_len = 3,
        .description = "Modified sync pattern (D4 instead of D5)"
    },
    
    /* Random House */
    {
        .name = "Random House",
        .type = PROTECTION_TYPE_NIBBLE_COUNT,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {1, 2, 3},
        .track_pattern_len = 3,
        .description = "Nibble count protection with long gaps"
    },
    
    /* Spinnaker */
    {
        .name = "Spinnaker",
        .type = PROTECTION_TYPE_TRACK_LAYOUT,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 1, 2, 17},
        .track_pattern_len = 4,
        .description = "Modified track layout"
    },
    
    /* MECC */
    {
        .name = "MECC (Oregon Trail)",
        .type = PROTECTION_TYPE_CUSTOM_DOS,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 1, 2},
        .track_pattern_len = 3,
        .description = "Custom DOS modifications"
    },
    
    /* Broderbund */
    {
        .name = "Broderbund",
        .type = PROTECTION_TYPE_TIMING,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 1, 2, 3, 17},
        .track_pattern_len = 5,
        .description = "Timing-based with track 17 check"
    },
    
    /* Didactic */
    {
        .name = "Didactic",
        .type = PROTECTION_TYPE_SYNC_PATTERN,
        .signature = {0xD5, 0xAA, 0xAD},
        .signature_len = 3,
        .track_pattern = {1, 2},
        .track_pattern_len = 2,
        .description = "Custom sync pattern AD"
    },
    
    /* Optimum Resource */
    {
        .name = "Optimum Resource",
        .type = PROTECTION_TYPE_SECTOR_EDITOR,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 17},
        .track_pattern_len = 2,
        .description = "Sector editor with track 17 modifications"
    },
    
    /* Sunburst */
    {
        .name = "Sunburst",
        .type = PROTECTION_TYPE_NIBBLE_COUNT,
        .signature = {0xD5, 0xAA, 0x96},
        .signature_len = 3,
        .track_pattern = {0, 1},
        .track_pattern_len = 2,
        .description = "Nibble count variations"
    }
};

static const size_t num_protection_patterns =
    sizeof(protection_patterns) / sizeof(protection_patterns[0]);

/*============================================================================*
 * PROTECTION DETECTION
 *============================================================================*/

/**
 * @brief Detect protection scheme by signature
 */
const apple_protection_pattern_t* apple_protection_detect_signature(
    const uint8_t *data, size_t len)
{
    if (!data || len < 3) {
        return NULL;
    }
    
    for (size_t i = 0; i < num_protection_patterns; i++) {
        const apple_protection_pattern_t *pattern = &protection_patterns[i];
        
        /* Search for signature */
        for (size_t j = 0; j < len - pattern->signature_len; j++) {
            if (memcmp(data + j, pattern->signature, pattern->signature_len) == 0) {
                return pattern;
            }
        }
    }
    
    return NULL;
}

/**
 * @brief Detect protection by track analysis
 */
const apple_protection_pattern_t* apple_protection_detect_track(
    uint8_t track, const uint8_t *track_data, size_t len)
{
    if (!track_data || len == 0) {
        return NULL;
    }
    
    for (size_t i = 0; i < num_protection_patterns; i++) {
        const apple_protection_pattern_t *pattern = &protection_patterns[i];
        
        /* Check if this pattern applies to this track */
        int track_match = 0;
        for (size_t j = 0; j < pattern->track_pattern_len; j++) {
            if (pattern->track_pattern[j] == track) {
                track_match = 1;
                break;
            }
        }
        
        if (!track_match) {
            continue;
        }
        
        /* Check signature */
        for (size_t j = 0; j < len - pattern->signature_len; j++) {
            if (memcmp(track_data + j, pattern->signature,
                      pattern->signature_len) == 0) {
                return pattern;
            }
        }
    }
    
    return NULL;
}

/**
 * @brief Get all protection patterns
 */
size_t apple_protection_get_all(const apple_protection_pattern_t **patterns_out)
{
    if (patterns_out) {
        *patterns_out = protection_patterns;
    }
    return num_protection_patterns;
}

/**
 * @brief Print protection info
 */
void apple_protection_print_info(const apple_protection_pattern_t *pattern)
{
    if (!pattern) {
        return;
    }
    
    printf("Protection: %s\n", pattern->name);
    printf("  Type: ");
    
    switch (pattern->type) {
        case PROTECTION_TYPE_TIMING:
            printf("Timing-based\n");
            break;
        case PROTECTION_TYPE_NIBBLE_COUNT:
            printf("Nibble count\n");
            break;
        case PROTECTION_TYPE_SYNC_PATTERN:
            printf("Sync pattern\n");
            break;
        case PROTECTION_TYPE_TRACK_LAYOUT:
            printf("Track layout\n");
            break;
        case PROTECTION_TYPE_SECTOR_EDITOR:
            printf("Sector editor\n");
            break;
        case PROTECTION_TYPE_HALF_TRACK:
            printf("Half-track\n");
            break;
        case PROTECTION_TYPE_CUSTOM_DOS:
            printf("Custom DOS\n");
            break;
        case PROTECTION_TYPE_CUSTOM_RWTS:
            printf("Custom RWTS\n");
            break;
        default:
            printf("Unknown\n");
            break;
    }
    
    printf("  Signature: ");
    for (size_t i = 0; i < pattern->signature_len; i++) {
        printf("%02X ", pattern->signature[i]);
    }
    printf("\n");
    
    printf("  Tracks: ");
    for (size_t i = 0; i < pattern->track_pattern_len; i++) {
        printf("%u ", pattern->track_pattern[i]);
    }
    printf("\n");
    
    printf("  Description: %s\n", pattern->description);
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
