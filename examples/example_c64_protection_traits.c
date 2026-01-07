// SPDX-License-Identifier: MIT
/*
 * example_c64_protection_traits.c - C64 Protection Traits Examples
 * 
 * Demonstrates all 10 C64 protection trait detectors.
 * 
 * @version 2.8.6.1
 * @date 2024-12-26
 */

#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <stdbool.h>
#include "uft/core/uft_safe_parse.h"

#include "c64_protection_traits.h"
#include "uft/core/uft_safe_parse.h"

/*============================================================================*
 * EXAMPLE 1: Weak Bits Detection
 *============================================================================*/

static void example_weak_bits(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Weak Bits Detection                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Weak Bits Detection:\n");
    printf("  • Detects intentional weak/random bits\n");
    printf("  • Common in RapidLok, V-MAX protections\n");
    printf("  • Requires multi-revolution capture\n");
    printf("  • Indicates protection: HIGH\n");
    printf("\n");
    
    printf("Example: Simulated weak bit detection\n");
    printf("  Track 18: Weak bits detected (confidence: 85%%)\n");
    printf("  Track 19: Weak bits detected (confidence: 92%%)\n");
    printf("  → Protection scheme: RapidLok likely\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: Long Track Detection
 *============================================================================*/

static void example_long_track(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: Long Track Detection                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Long Track Detection:\n");
    printf("  • Detects tracks longer than standard\n");
    printf("  • Normal: ~7692 bytes\n");
    printf("  • Long: >8000 bytes\n");
    printf("  • Used in EA, Vorpal protections\n");
    printf("\n");
    
    printf("Example: Simulated long track detection\n");
    printf("  Track 17: 8234 bytes (107%% of normal)\n");
    printf("  Track 18: 8156 bytes (106%% of normal)\n");
    printf("  → Protection scheme: EA Fat Track likely\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: Half-Track Detection
 *============================================================================*/

static void example_half_tracks(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: Half-Track Detection                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Half-Track Detection:\n");
    printf("  • Detects data on half-tracks (e.g., 18.5)\n");
    printf("  • Requires special hardware/firmware\n");
    printf("  • Common in advanced protections\n");
    printf("  • Indicates protection: VERY HIGH\n");
    printf("\n");
    
    printf("Example: Simulated half-track detection\n");
    printf("  Track 18.5: Data detected (1024 bytes)\n");
    printf("  Track 19.5: Data detected (512 bytes)\n");
    printf("  → Protection scheme: Advanced custom protection\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: Long Sync Run Detection
 *============================================================================*/

static void example_long_sync(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Long Sync Run Detection                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Long Sync Run Detection:\n");
    printf("  • Detects unusually long sync sequences\n");
    printf("  • Normal: ~10-40 sync bytes\n");
    printf("  • Long: >100 sync bytes\n");
    printf("  • Used for timing-based protection\n");
    printf("\n");
    
    printf("Example: Simulated long sync detection\n");
    printf("  Track 18: Sync run of 156 bytes detected\n");
    printf("  Track 19: Sync run of 203 bytes detected\n");
    printf("  → Protection uses timing verification\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 5: Illegal GCR Detection
 *============================================================================*/

static void example_illegal_gcr(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 5: Illegal GCR Detection                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Illegal GCR Detection:\n");
    printf("  • Detects invalid GCR sequences\n");
    printf("  • Used to defeat simple copiers\n");
    printf("  • Common in many protections\n");
    printf("  • Requires flux-level capture\n");
    printf("\n");
    
    printf("Example: Simulated illegal GCR detection\n");
    printf("  Track 18: 23 illegal GCR sequences\n");
    printf("  Track 19: 18 illegal GCR sequences\n");
    printf("  → Protection scheme: Standard copy protection\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 6: Marker Bytes Detection
 *============================================================================*/

static void example_marker_bytes(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 6: Marker Bytes Detection                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Marker Bytes Detection:\n");
    printf("  • Detects specific signature bytes\n");
    printf("  • Used to identify protection scheme\n");
    printf("  • Common in RapidLok family\n");
    printf("  • Helps with scheme identification\n");
    printf("\n");
    
    printf("Example: Simulated marker detection\n");
    printf("  Track 18: RapidLok signature detected (0xA5A5)\n");
    printf("  Track 19: Protection marker detected (0x5A5A)\n");
    printf("  → Protection scheme: RapidLok 6 confirmed\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 7: Bit Length Jitter Detection
 *============================================================================*/

static void example_bitlen_jitter(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 7: Bit Length Jitter Detection                  ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Bit Length Jitter Detection:\n");
    printf("  • Detects intentional timing variations\n");
    printf("  • Requires flux-level analysis\n");
    printf("  • Used in sophisticated protections\n");
    printf("  • Indicates mastering vs. copy\n");
    printf("\n");
    
    printf("Example: Simulated jitter detection\n");
    printf("  Track 18: Jitter variance: 12.3%%\n");
    printf("  Track 19: Jitter variance: 15.7%%\n");
    printf("  → Timing-based protection detected\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 8: Directory Track Anomaly
 *============================================================================*/

static void example_dirtrack_anomaly(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 8: Directory Track Anomaly                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Directory Track Anomaly Detection:\n");
    printf("  • Detects unusual directory track structure\n");
    printf("  • Track 18 is standard directory track\n");
    printf("  • Anomalies indicate protection\n");
    printf("  • Common in many schemes\n");
    printf("\n");
    
    printf("Example: Simulated directory anomaly\n");
    printf("  Track 18: Non-standard sector count (22 vs 19)\n");
    printf("  Track 18: Invalid BAM structure detected\n");
    printf("  → Directory-based protection present\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 9: Multi-Revolution Recommendation
 *============================================================================*/

static void example_multirev(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 9: Multi-Revolution Recommendation              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Multi-Revolution Recommendation:\n");
    printf("  • Analyzes if multi-rev capture needed\n");
    printf("  • Based on detected protection traits\n");
    printf("  • Critical for weak bits preservation\n");
    printf("  • Improves capture quality\n");
    printf("\n");
    
    printf("Example: Simulated multi-rev analysis\n");
    printf("  Weak bits detected: YES\n");
    printf("  Timing variation: HIGH\n");
    printf("  → RECOMMENDATION: 5 revolutions minimum\n");
    printf("  → Capture settings: multi-rev, preserve gaps\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 10: Speed Anomaly Detection
 *============================================================================*/

static void example_speed_anomaly(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 10: Speed Anomaly Detection                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Speed Anomaly Detection:\n");
    printf("  • Detects non-standard rotation speeds\n");
    printf("  • Normal C64: 4 speed zones\n");
    printf("  • Anomalies indicate mastering tricks\n");
    printf("  • Requires flux-level timing\n");
    printf("\n");
    
    printf("Example: Simulated speed anomaly\n");
    printf("  Track 17: Speed zone mismatch\n");
    printf("  Track 18: Non-standard bitrate detected\n");
    printf("  → Custom mastering / protection present\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 11: Comprehensive Analysis
 *============================================================================*/

static void example_comprehensive(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 11: Comprehensive Protection Analysis           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("Comprehensive Analysis Results:\n");
    printf("\n");
    
    printf("DETECTED TRAITS:\n");
    printf("  ✓ Weak bits (Track 18-19)       Confidence: 90%%\n");
    printf("  ✓ Long tracks (Track 17-18)      Confidence: 85%%\n");
    printf("  ✓ Illegal GCR (Track 18)         Confidence: 75%%\n");
    printf("  ✓ Marker bytes (Track 18)        Confidence: 95%%\n");
    printf("  ✓ Directory anomaly (Track 18)   Confidence: 80%%\n");
    printf("\n");
    
    printf("PROTECTION SCHEME IDENTIFICATION:\n");
    printf("  Primary:   RapidLok 6         (95%% confidence)\n");
    printf("  Secondary: EA Fat Track       (45%% confidence)\n");
    printf("\n");
    
    printf("PRESERVATION RECOMMENDATIONS:\n");
    printf("  • Use multi-revolution capture (5+ revs)\n");
    printf("  • Capture half-tracks if possible\n");
    printf("  • Preserve all gaps and sync runs\n");
    printf("  • Do not attempt error correction\n");
    printf("  • Save in G64 or P64 format\n");
    printf("\n");
    
    printf("CAPTURE QUALITY:\n");
    printf("  Signal Quality:    EXCELLENT (95%%)\n");
    printf("  Protection Info:   COMPLETE\n");
    printf("  Archival Status:   MUSEUM QUALITY ✓\n");
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  C64 PROTECTION TRAITS - COMPREHENSIVE EXAMPLES           ║\n");
    printf("║  UFT v2.8.6.1 - C64 Traits Edition                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    if (argc > 1) {
        int32_t example = 0;
        if (!uft_parse_int32(argv[1], &example, 10)) {
            fprintf(stderr, "Invalid argument: %s\n", argv[1]);
            return 1;
        }
        
        switch (example) {
            case 1:  example_weak_bits(); break;
            case 2:  example_long_track(); break;
            case 3:  example_half_tracks(); break;
            case 4:  example_long_sync(); break;
            case 5:  example_illegal_gcr(); break;
            case 6:  example_marker_bytes(); break;
            case 7:  example_bitlen_jitter(); break;
            case 8:  example_dirtrack_anomaly(); break;
            case 9:  example_multirev(); break;
            case 10: example_speed_anomaly(); break;
            case 11: example_comprehensive(); break;
            default:
                printf("\nUsage: %s [1-11]\n", argv[0]);
                printf("  1  - Weak bits detection\n");
                printf("  2  - Long track detection\n");
                printf("  3  - Half-track detection\n");
                printf("  4  - Long sync run detection\n");
                printf("  5  - Illegal GCR detection\n");
                printf("  6  - Marker bytes detection\n");
                printf("  7  - Bit length jitter\n");
                printf("  8  - Directory track anomaly\n");
                printf("  9  - Multi-rev recommendation\n");
                printf("  10 - Speed anomaly detection\n");
                printf("  11 - Comprehensive analysis\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_weak_bits();
        example_long_track();
        example_half_tracks();
        example_long_sync();
        example_illegal_gcr();
        example_marker_bytes();
        example_bitlen_jitter();
        example_dirtrack_anomaly();
        example_multirev();
        example_speed_anomaly();
        example_comprehensive();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  All examples completed! ✓                                ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("C64 PROTECTION TRAITS SUMMARY:\n");
    printf("  Available Traits: %d\n", c64_traits_count());
    printf("\n");
    
    printf("  1. Weak Bits               - Random/intentional weak bits\n");
    printf("  2. Long Track              - Tracks longer than standard\n");
    printf("  3. Half-Tracks             - Data on half-track positions\n");
    printf("  4. Long Sync Run           - Unusually long sync sequences\n");
    printf("  5. Illegal GCR             - Invalid GCR sequences\n");
    printf("  6. Marker Bytes            - Protection signature bytes\n");
    printf("  7. Bit Length Jitter       - Intentional timing variations\n");
    printf("  8. Directory Track Anomaly - Non-standard directory structure\n");
    printf("  9. Multi-Rev Recommended   - Multi-revolution capture needed\n");
    printf("  10. Speed Anomaly          - Non-standard rotation speeds\n");
    printf("\n");
    
    printf("PROTECTION SCHEMES DETECTED BY TRAITS:\n");
    printf("  • RapidLok (1, 2, 6)       - Weak bits, markers, illegal GCR\n");
    printf("  • EA Fat Track             - Long tracks, custom format\n");
    printf("  • GEOS Gap Data            - Directory anomaly, gap signatures\n");
    printf("  • Vorpal                   - Long tracks, sync counting\n");
    printf("  • V-MAX                    - Weak bits, speed anomaly\n");
    printf("\n");
    
    printf("PRESERVATION BENEFITS:\n");
    printf("  ✓ Identify protection automatically\n");
    printf("  ✓ Recommend optimal capture settings\n");
    printf("  ✓ Ensure museum-quality preservation\n");
    printf("  ✓ Detect protection traits accurately\n");
    printf("  ✓ Professional archival quality\n");
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
