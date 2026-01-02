// SPDX-License-Identifier: MIT
/*
 * example_c64_protection.c - C64 Protection Detection Example
 * 
 * Demonstrates how to use the C64 protection detection API.
 * 
 * @version 2.8.3
 * @date 2024-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "c64_protection.h"

/*============================================================================*
 * EXAMPLE: Simulated D64 with RapidLok
 *============================================================================*/

static void example_rapidlok_disk(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Simulated RapidLok Disk                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Simulate track metrics for a RapidLok-protected disk */
    uft_cbm_track_metrics_t tracks[42];
    memset(tracks, 0, sizeof(tracks));
    
    /* Standard tracks 1-35 */
    for (int i = 0; i < 35; i++) {
        tracks[i].track_x2 = (i + 1) * 2;
        tracks[i].revolutions = 3;
        tracks[i].bitlen_min = 190000;
        tracks[i].bitlen_max = 200000;
        tracks[i].max_sync_bits = 320;  /* Standard */
        tracks[i].has_meaningful_data = true;
    }
    
    /* Track 36: RapidLok key track */
    tracks[35].track_x2 = 72;  /* Track 36 */
    tracks[35].revolutions = 3;
    tracks[35].bitlen_min = 195000;
    tracks[35].bitlen_max = 205000;
    tracks[35].max_sync_bits = 480;  /* Long sync! */
    tracks[35].illegal_gcr_events = 50;  /* Bad GCR in gaps */
    tracks[35].gap_non55_bytes = 100;  /* Non-standard gaps */
    tracks[35].has_meaningful_data = true;
    
    /* Some tracks with intentional errors */
    for (int i = 16; i < 20; i++) {
        tracks[i].sector_crc_failures = 5;
        tracks[i].sector_missing = 2;
    }
    
    /* Analyze */
    c64_protection_result_t result;
    if (c64_analyze_protection(tracks, 36, &result)) {
        c64_print_protection(&result, true);
    } else {
        printf("Error: Analysis failed\n");
    }
}

/*============================================================================*
 * EXAMPLE: Clean Standard Disk
 *============================================================================*/

static void example_standard_disk(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: Standard Unprotected Disk                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Simulate standard disk */
    uft_cbm_track_metrics_t tracks[35];
    memset(tracks, 0, sizeof(tracks));
    
    for (int i = 0; i < 35; i++) {
        tracks[i].track_x2 = (i + 1) * 2;
        tracks[i].revolutions = 2;
        tracks[i].bitlen_min = 195000;
        tracks[i].bitlen_max = 196000;  /* Very stable */
        tracks[i].max_sync_bits = 320;  /* Standard */
        tracks[i].illegal_gcr_events = 0;
        tracks[i].sector_crc_failures = 0;
        tracks[i].sector_missing = 0;
        tracks[i].has_meaningful_data = true;
    }
    
    /* Analyze */
    c64_protection_result_t result;
    if (c64_analyze_protection(tracks, 35, &result)) {
        c64_print_protection(&result, false);
    }
}

/*============================================================================*
 * EXAMPLE: GEOS Gap Protection
 *============================================================================*/

static void example_geos_disk(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: GEOS Gap Protection                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Simulate GEOS-protected disk */
    uft_cbm_track_metrics_t tracks[35];
    memset(tracks, 0, sizeof(tracks));
    
    for (int i = 0; i < 35; i++) {
        tracks[i].track_x2 = (i + 1) * 2;
        tracks[i].revolutions = 2;
        tracks[i].bitlen_min = 190000;
        tracks[i].bitlen_max = 200000;
        tracks[i].max_sync_bits = 320;
        tracks[i].has_meaningful_data = true;
    }
    
    /* Track 21: GEOS special track */
    tracks[20].track_x2 = 42;  /* Track 21 */
    tracks[20].gap_non55_bytes = 200;  /* Non-0x55 gap bytes */
    tracks[20].gap_length_weird = true;
    tracks[20].max_sync_bits = 500;  /* Longer sync */
    
    /* Analyze */
    c64_protection_result_t result;
    if (c64_analyze_protection(tracks, 35, &result)) {
        c64_print_protection(&result, true);
    }
}

/*============================================================================*
 * EXAMPLE: Quick Protection Check
 *============================================================================*/

static void example_quick_check(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Quick Protection Check                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Test different disks */
    const char* disk_names[] = {
        "standard.d64",
        "rapidlok.d64",
        "geos.d64",
        "unknown.d64"
    };
    
    printf("Quick protection check:\n\n");
    
    for (size_t i = 0; i < 4; i++) {
        uft_cbm_track_metrics_t tracks[35];
        memset(tracks, 0, sizeof(tracks));
        
        /* Simulate different characteristics */
        for (int j = 0; j < 35; j++) {
            tracks[j].track_x2 = (j + 1) * 2;
            tracks[j].revolutions = 2;
            tracks[j].bitlen_min = 190000;
            tracks[j].bitlen_max = 195000;
            tracks[j].max_sync_bits = 320;
            tracks[j].has_meaningful_data = true;
            
            /* Add protection for some disks */
            if (i == 1) {  /* RapidLok */
                if (j == 35) {  /* Track 36 */
                    tracks[j].max_sync_bits = 480;
                    tracks[j].illegal_gcr_events = 50;
                }
            } else if (i == 2) {  /* GEOS */
                if (j == 20) {  /* Track 21 */
                    tracks[j].gap_non55_bytes = 200;
                }
            }
        }
        
        bool has_prot = c64_has_protection(tracks, 35);
        
        printf("  %s: %s\n", disk_names[i],
               has_prot ? "⚠️  PROTECTED" : "✓ Standard");
    }
    
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char* argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  C64 PROTECTION DETECTION EXAMPLES                        ║\n");
    printf("║  UFT v2.8.3 - C64 Protection Master Edition              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    if (argc > 1) {
        int example = atoi(argv[1]);
        
        switch (example) {
            case 1:
                example_rapidlok_disk();
                break;
            case 2:
                example_standard_disk();
                break;
            case 3:
                example_geos_disk();
                break;
            case 4:
                example_quick_check();
                break;
            default:
                printf("\nUsage: %s [1-4]\n", argv[0]);
                printf("  1 - RapidLok disk example\n");
                printf("  2 - Standard disk example\n");
                printf("  3 - GEOS gap protection example\n");
                printf("  4 - Quick check example\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_standard_disk();
        example_rapidlok_disk();
        example_geos_disk();
        example_quick_check();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Examples completed! ✓                                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("INTEGRATION NOTES:\n");
    printf("  • Fill track_metrics from your D64/G64/P64 decoder\n");
    printf("  • Call c64_analyze_protection() for full analysis\n");
    printf("  • Use c64_has_protection() for quick checks\n");
    printf("  • Check result.recommendations for capture hints\n");
    printf("\n");
    printf("SUPPORTED PROTECTIONS:\n");
    printf("  • 16 Generic CBM protection methods\n");
    printf("  • 7 Named C64 protection schemes\n");
    printf("  • RapidLok family (3 variants)\n");
    printf("  • GEOS gap protection\n");
    printf("  • EA loader detection\n");
    printf("  • And more!\n");
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
