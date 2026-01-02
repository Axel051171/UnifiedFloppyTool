// SPDX-License-Identifier: MIT
/*
 * example_xcopy_complete.c - Complete X-Copy Analysis Example
 * 
 * Demonstrates full copy protection detection with:
 *   - MFM decoding
 *   - X-Copy error analysis
 *   - Protection pattern detection
 *   - Disk-wide analysis
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "mfm_decode.h"
#include "xcopy_errors.h"
#include "xcopy_protection.h"

/*============================================================================*
 * SAMPLE TRACK DATA
 *============================================================================*/

/**
 * @brief Generate sample Amiga track with protection
 * 
 * Creates a synthetic long track with Rob Northen signature.
 */
static void generate_rob_northen_track(uint8_t **track_out, size_t *len_out)
{
    /* Rob Northen tracks are typically 13200 bytes */
    size_t track_len = 13200;
    uint8_t *track = calloc(1, track_len);
    
    if (!track) {
        *track_out = NULL;
        *len_out = 0;
        return;
    }
    
    /* Add some sync marks */
    for (int i = 0; i < 11; i++) {
        size_t pos = i * 1200;  /* Sector positions */
        if (pos + 1 < track_len) {
            track[pos] = 0x44;      /* MFM sync */
            track[pos + 1] = 0x89;  /* MFM sync */
        }
    }
    
    /* Fill with some pattern data */
    for (size_t i = 0; i < track_len; i++) {
        if (track[i] == 0) {
            track[i] = (uint8_t)(i & 0xFF);
        }
    }
    
    *track_out = track;
    *len_out = track_len;
}

/**
 * @brief Generate normal Amiga track
 */
static void generate_normal_track(uint8_t **track_out, size_t *len_out)
{
    /* Normal track ~11000 bytes */
    size_t track_len = 11000;
    uint8_t *track = calloc(1, track_len);
    
    if (!track) {
        *track_out = NULL;
        *len_out = 0;
        return;
    }
    
    /* Add 11 sync marks (standard Amiga) */
    for (int i = 0; i < 11; i++) {
        size_t pos = i * 1000;
        if (pos + 1 < track_len) {
            track[pos] = 0x44;
            track[pos + 1] = 0x89;
        }
    }
    
    /* Fill with pattern */
    for (size_t i = 0; i < track_len; i++) {
        if (track[i] == 0) {
            track[i] = (uint8_t)(i & 0xFF);
        }
    }
    
    *track_out = track;
    *len_out = track_len;
}

/**
 * @brief Generate Gremlin Graphics protected track
 */
static void generate_gremlin_track(uint8_t **track_out, size_t *len_out)
{
    /* Gremlin uses 10 sectors */
    size_t track_len = 10000;
    uint8_t *track = calloc(1, track_len);
    
    if (!track) {
        *track_out = NULL;
        *len_out = 0;
        return;
    }
    
    /* Add 10 sync marks (modified sector count) */
    for (int i = 0; i < 10; i++) {
        size_t pos = i * 1000;
        if (pos + 1 < track_len) {
            track[pos] = 0x44;
            track[pos + 1] = 0x89;
        }
    }
    
    for (size_t i = 0; i < track_len; i++) {
        if (track[i] == 0) {
            track[i] = (uint8_t)(i & 0xFF);
        }
    }
    
    *track_out = track;
    *len_out = track_len;
}

/*============================================================================*
 * EXAMPLES
 *============================================================================*/

/**
 * @brief Example 1: Basic MFM decoding
 */
static void example_mfm_decode(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: MFM Decoding                                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    uint8_t *track;
    size_t track_len;
    generate_normal_track(&track, &track_len);
    
    if (!track) {
        printf("Failed to generate track\n");
        return;
    }
    
    /* Analyze MFM track */
    mfm_track_t mfm_track;
    if (mfm_analyze_track(track, track_len, &mfm_track) == 0) {
        mfm_print_track_analysis(&mfm_track);
    }
    
    free(track);
    printf("\n");
}

/**
 * @brief Example 2: X-Copy error detection
 */
static void example_xcopy_analysis(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: X-Copy Error Analysis                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Test clean track */
    printf("Testing normal track:\n");
    uint8_t *normal_track;
    size_t normal_len;
    generate_normal_track(&normal_track, &normal_len);
    
    if (normal_track) {
        xcopy_track_error_t error;
        xcopy_analyze_track(normal_track, normal_len, &error);
        
        printf("  Error code: %d (%s)\n", error.error_code,
               xcopy_error_message(error.error_code));
        printf("  Sectors:    %d (expected %d)\n", 
               error.sector_count, error.expected_sectors);
        printf("  Protected:  %s\n", error.is_protected ? "YES" : "NO");
        
        free(normal_track);
    }
    
    printf("\n");
    
    /* Test Rob Northen track */
    printf("Testing Rob Northen protected track:\n");
    uint8_t *rn_track;
    size_t rn_len;
    generate_rob_northen_track(&rn_track, &rn_len);
    
    if (rn_track) {
        xcopy_track_error_t error;
        xcopy_analyze_track(rn_track, rn_len, &error);
        
        printf("  Error code: %d (%s)\n", error.error_code,
               xcopy_error_message(error.error_code));
        printf("  Track len:  %u (expected %u)\n",
               error.track_length, error.expected_length);
        printf("  Protected:  %s\n", error.is_protected ? "YES" : "NO");
        
        free(rn_track);
    }
    
    printf("\n");
}

/**
 * @brief Example 3: Protection pattern detection
 */
static void example_protection_detection(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: Copy Protection Detection                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Test Rob Northen */
    printf("Testing Rob Northen Copylock:\n");
    uint8_t *rn_track;
    size_t rn_len;
    generate_rob_northen_track(&rn_track, &rn_len);
    
    if (rn_track) {
        xcopy_track_error_t error;
        xcopy_analyze_track(rn_track, rn_len, &error);
        
        cp_detection_t detection;
        xcopy_detect_protection_pattern(&error, &detection);
        
        printf("  Pattern:     %s\n", detection.name);
        printf("  Description: %s\n", detection.description);
        printf("  Confidence:  %u%%\n", detection.confidence);
        
        free(rn_track);
    }
    
    printf("\n");
    
    /* Test Gremlin */
    printf("Testing Gremlin Graphics:\n");
    uint8_t *gremlin_track;
    size_t gremlin_len;
    generate_gremlin_track(&gremlin_track, &gremlin_len);
    
    if (gremlin_track) {
        xcopy_track_error_t error;
        xcopy_analyze_track(gremlin_track, gremlin_len, &error);
        
        cp_detection_t detection;
        xcopy_detect_protection_pattern(&error, &detection);
        
        printf("  Pattern:     %s\n", detection.name);
        printf("  Description: %s\n", detection.description);
        printf("  Confidence:  %u%%\n", detection.confidence);
        
        free(gremlin_track);
    }
    
    printf("\n");
}

/**
 * @brief Example 4: Full disk analysis
 */
static void example_disk_analysis(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Full Disk Analysis                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Simulate disk with mixed protection */
    #define NUM_TRACKS 160
    xcopy_track_error_t track_errors[NUM_TRACKS];
    
    printf("Analyzing simulated disk (80 tracks, 2 sides)...\n\n");
    
    /* Generate tracks */
    for (int i = 0; i < NUM_TRACKS; i++) {
        uint8_t *track;
        size_t track_len;
        
        /* Track 0 = Rob Northen */
        if (i == 0) {
            generate_rob_northen_track(&track, &track_len);
        }
        /* Tracks 1-2 = Gremlin */
        else if (i >= 1 && i <= 2) {
            generate_gremlin_track(&track, &track_len);
        }
        /* Rest = normal */
        else {
            generate_normal_track(&track, &track_len);
        }
        
        if (track) {
            xcopy_analyze_track(track, track_len, &track_errors[i]);
            free(track);
        } else {
            memset(&track_errors[i], 0, sizeof(xcopy_track_error_t));
        }
    }
    
    /* Analyze whole disk */
    disk_protection_t disk;
    xcopy_analyze_disk_protection(track_errors, NUM_TRACKS, &disk);
    
    xcopy_print_disk_protection(&disk);
    
    printf("\n");
}

/**
 * @brief Example 5: Error statistics
 */
static void example_statistics(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 5: Error Statistics                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    xcopy_error_stats_t stats;
    xcopy_stats_init(&stats);
    
    /* Analyze multiple tracks */
    for (int i = 0; i < 10; i++) {
        uint8_t *track;
        size_t track_len;
        
        if (i < 2) {
            generate_rob_northen_track(&track, &track_len);
        } else {
            generate_normal_track(&track, &track_len);
        }
        
        if (track) {
            xcopy_track_error_t error;
            xcopy_analyze_track(track, track_len, &error);
            xcopy_stats_add(&stats, &error);
            free(track);
        }
    }
    
    xcopy_stats_print(&stats);
    
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  X-COPY COMPLETE - Full Implementation Demo               ║\n");
    printf("║  Version 2.8.0                                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Run examples */
    example_mfm_decode();
    example_xcopy_analysis();
    example_protection_detection();
    example_disk_analysis();
    example_statistics();
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  All examples completed successfully! ✓                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return 0;
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
