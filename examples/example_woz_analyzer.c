// SPDX-License-Identifier: MIT
/*
 * example_woz_analyzer.c - WOZ Analysis Examples
 * 
 * Demonstrates advanced WOZ analysis inspired by wozardry
 * 
 * @version 2.8.6
 * @date 2024-12-26
 */

#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"

#include "woz_analyzer.h"
#include "uft/core/uft_safe_parse.h"

/*============================================================================*
 * EXAMPLE 1: Basic Track Analysis
 *============================================================================*/

static void example_track_quality(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Track Quality Analysis                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Simulate track data (in real use, read from WOZ file) */
    uint8_t track_data[6400];
    memset(track_data, 0xFF, sizeof(track_data));  /* Simulated sync */
    
    /* Add some data nibbles */
    for (int i = 1000; i < 2000; i++) {
        track_data[i] = 0x96 + (i % 0x6A);  /* Valid nibbles */
    }
    
    woz_track_quality_t quality;
    if (woz_analyze_track_quality(track_data, sizeof(track_data) * 8, &quality)) {
        printf("Track Quality Metrics:\n");
        printf("  Timing Quality: %.1f%%\n", quality.timing_quality * 100.0f);
        printf("  Sync Quality:   %.1f%%\n", quality.sync_quality * 100.0f);
        printf("  Data Quality:   %.1f%%\n", quality.data_quality * 100.0f);
        printf("  Sync Bytes:     %u\n", quality.sync_count);
        printf("  Errors:         %u\n", quality.error_count);
        printf("  Long Sync:      %s\n", quality.has_long_sync ? "Yes ⚠️" : "No");
        printf("  Weak Bits:      %s\n", quality.has_weak_bits ? "Yes ⚠️" : "No");
        printf("\n");
    } else {
        printf("❌ Analysis failed\n\n");
    }
}

/*============================================================================*
 * EXAMPLE 2: Protection Detection
 *============================================================================*/

static void example_protection_detection(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: Protection Detection                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Simulate protected track (long sync) */
    uint8_t track_data[6400];
    memset(track_data, 0xFF, 500);  /* Very long sync = protection! */
    memset(track_data + 500, 0x96, sizeof(track_data) - 500);
    
    woz_protection_info_t protections[10];
    size_t count = woz_detect_protections(track_data, sizeof(track_data) * 8,
                                          protections, 10);
    
    if (count > 0) {
        printf("✅ Detected %zu protection pattern(s):\n\n", count);
        
        for (size_t i = 0; i < count; i++) {
            printf("Protection #%zu:\n", i + 1);
            printf("  Type:        %s\n", 
                   woz_protection_name(protections[i].type));
            printf("  Confidence:  %.0f%%\n", 
                   protections[i].confidence * 100.0f);
            printf("  Description: %s\n", protections[i].description);
            printf("\n");
        }
    } else {
        printf("✅ No copy protection detected\n\n");
    }
}

/*============================================================================*
 * EXAMPLE 3: Nibble Decoding
 *============================================================================*/

static void example_nibble_decoding(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: Nibble Decoding                              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Create sample track data */
    uint8_t track_data[100];
    for (int i = 0; i < 100; i++) {
        track_data[i] = 0x96 + (i % 0x6A);
    }
    
    woz_nibble_data_t nibbles;
    if (woz_decode_nibbles(track_data, sizeof(track_data) * 8, &nibbles)) {
        printf("Decoded %zu nibbles from track\n\n", nibbles.count);
        
        /* Show first 32 nibbles */
        printf("First 32 nibbles (hex):\n");
        for (size_t i = 0; i < 32 && i < nibbles.count; i++) {
            printf("%02X ", nibbles.nibbles[i]);
            if ((i + 1) % 16 == 0) printf("\n");
        }
        printf("\n");
        
        /* Count valid nibbles */
        size_t valid = 0;
        for (size_t i = 0; i < nibbles.count; i++) {
            if (nibbles.valid[i]) valid++;
        }
        
        printf("Valid nibbles: %zu / %zu (%.1f%%)\n",
               valid, nibbles.count,
               (float)valid / nibbles.count * 100.0f);
        
        woz_nibbles_free(&nibbles);
    } else {
        printf("❌ Decoding failed\n");
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: Full Image Analysis
 *============================================================================*/

static void example_full_analysis(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Full WOZ Analysis                            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* This would normally analyze a real WOZ file */
    woz_analysis_t analysis;
    const char *filename = "example.woz";
    
    printf("Analyzing: %s\n", filename);
    
    if (woz_analyze(filename, &analysis)) {
        /* Print detailed report */
        woz_print_analysis(&analysis);
        
        /* Additional custom reporting */
        printf("Detailed Track Quality:\n");
        for (uint8_t i = 0; i < analysis.num_tracks && i < 5; i++) {
            printf("  Track %2d: T=%.1f%% S=%.1f%% D=%.1f%%",
                   i,
                   analysis.track_quality[i].timing_quality * 100.0f,
                   analysis.track_quality[i].sync_quality * 100.0f,
                   analysis.track_quality[i].data_quality * 100.0f);
            
            if (analysis.track_quality[i].has_long_sync ||
                analysis.track_quality[i].has_weak_bits) {
                printf(" ⚠️");
            }
            printf("\n");
        }
        if (analysis.num_tracks > 5) {
            printf("  ... (%d more tracks)\n", analysis.num_tracks - 5);
        }
        
        woz_analysis_free(&analysis);
    } else {
        printf("ℹ️  Note: This is a demonstration\n");
        printf("   In real use, provide path to actual WOZ file\n");
    }
    
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  WOZ ANALYZER - ADVANCED ANALYSIS TOOL                    ║\n");
    printf("║  UFT v2.8.6 - Inspired by wozardry (4am team)            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    if (argc > 1) {
        int32_t example = 0;
        if (!uft_parse_int32(argv[1], &example, 10)) {
            fprintf(stderr, "Invalid argument: %s\n", argv[1]);
            return 1;
        }
        
        switch (example) {
            case 1:
                example_track_quality();
                break;
            case 2:
                example_protection_detection();
                break;
            case 3:
                example_nibble_decoding();
                break;
            case 4:
                example_full_analysis();
                break;
            default:
                printf("\nUsage: %s [1-4]\n", argv[0]);
                printf("  1 - Track quality analysis\n");
                printf("  2 - Protection detection\n");
                printf("  3 - Nibble decoding\n");
                printf("  4 - Full WOZ analysis\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_track_quality();
        example_protection_detection();
        example_nibble_decoding();
        example_full_analysis();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Examples completed! ✓                                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("WOZ ANALYZER FEATURES:\n");
    printf("  ✅ Track quality metrics (timing, sync, data)\n");
    printf("  ✅ Protection pattern detection (12 types)\n");
    printf("  ✅ Nibble decoding and validation\n");
    printf("  ✅ Bit timing analysis\n");
    printf("  ✅ Comprehensive reporting\n");
    printf("\n");
    printf("PROTECTION TYPES DETECTED:\n");
    printf("  • Half-track stepping\n");
    printf("  • Spiral tracks\n");
    printf("  • Bit slip / timing errors\n");
    printf("  • Extended sync bytes\n");
    printf("  • Weak bit areas\n");
    printf("  • Custom sector formats\n");
    printf("  • Electronic Arts protection\n");
    printf("  • Optimum Resource protection\n");
    printf("  • ProLok protection\n");
    printf("  • And more...\n");
    printf("\n");
    printf("INSPIRED BY:\n");
    printf("  🏆 wozardry by 4am (legendary Apple II preservation)\n");
    printf("  🍎 Apple II preservation community\n");
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
