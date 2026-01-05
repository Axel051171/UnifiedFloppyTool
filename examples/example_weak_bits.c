/*
 * example_weak_bits.c
 * 
 * Demonstrates WEAK BIT DETECTION - The Copy Protection Finder! 🔒⭐
 * 
 * Shows how to detect weak/unstable bits in floppy tracks that indicate
 * copy protection schemes like Rob Northen, Speedlock, etc.
 * 
 * Compile:
 *   gcc -o weak_bits example_weak_bits.c \
 *       -I../libflux_core/include \
 *       ../build/libflux_core.a
 * 
 * Usage:
 *   ./weak_bits           # All examples
 *   ./weak_bits --simple  # Just basic detection
 * 
 * @version 2.7.1
 * @date 2024-12-25
 */

#include "weak_bits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*============================================================================*
 * TEST DATA GENERATION
 *============================================================================*/

/**
 * @brief Create a normal stable track (no weak bits)
 */
static void create_stable_track(uint8_t *track, size_t size) {
    // Fill with repeating pattern
    for (size_t i = 0; i < size; i++) {
        track[i] = (uint8_t)(i & 0xFF);
    }
}

/**
 * @brief Create a track with Rob Northen-style weak bits
 * 
 * Rob Northen Copylock uses weak bits in specific patterns!
 */
static void create_weak_track_rob_northen(
    uint8_t **tracks,
    size_t track_count,
    size_t track_size,
    uint32_t weak_bit_offset,
    uint8_t weak_bit_pos
) {
    for (size_t rev = 0; rev < track_count; rev++) {
        create_stable_track(tracks[rev], track_size);
        
        // Add weak bit at specific location
        // Rob Northen: bit alternates between 0 and 1
        uint8_t weak_value = (rev % 2);  // Alternates!
        
        if (weak_value) {
            tracks[rev][weak_bit_offset] |= (1 << (7 - weak_bit_pos));
        } else {
            tracks[rev][weak_bit_offset] &= ~(1 << (7 - weak_bit_pos));
        }
    }
}

/**
 * @brief Create a track with Speedlock-style weak bits
 * 
 * Speedlock uses multiple weak bits in a sector!
 */
static void create_weak_track_speedlock(
    uint8_t **tracks,
    size_t track_count,
    size_t track_size
) {
    for (size_t rev = 0; rev < track_count; rev++) {
        create_stable_track(tracks[rev], track_size);
        
        // Add multiple weak bits in "protection zone"
        uint32_t protection_start = track_size / 2;  // Middle of track
        
        for (uint32_t offset = protection_start; offset < protection_start + 10; offset++) {
            // Random weak bits
            for (uint8_t bit = 0; bit < 8; bit += 2) {
                uint8_t weak_value = (rand() % 2);
                
                if (weak_value) {
                    tracks[rev][offset] |= (1 << (7 - bit));
                } else {
                    tracks[rev][offset] &= ~(1 << (7 - bit));
                }
            }
        }
    }
}

/*============================================================================*
 * EXAMPLE 1: Basic Weak Bit Detection
 *============================================================================*/

static void example_basic_detection(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 1: Basic Weak Bit Detection\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Initialize
    weak_bits_init();
    
    // Create test data: 5 revolutions of a track
    const size_t track_size = 512;
    const size_t rev_count = 5;
    
    uint8_t *tracks[10];
    for (size_t i = 0; i < rev_count; i++) {
        tracks[i] = malloc(track_size);
    }
    
    // Create track with ONE weak bit at offset 256, bit 3
    create_weak_track_rob_northen(
        (const uint8_t**)tracks,
        rev_count,
        track_size,
        256,  // offset
        3     // bit position
    );
    
    printf("Test Setup:\n");
    printf("  Track size: %zu bytes\n", track_size);
    printf("  Revolutions: %zu\n", rev_count);
    printf("  Weak bit: Offset 256, Bit 3 (Rob Northen style)\n");
    printf("\n");
    
    // Get default parameters
    weak_bit_params_t params;
    weak_bits_get_default_params(0, &params);  // Amiga format
    
    printf("Detection Parameters:\n");
    printf("  Revolution count: %u\n", params.revolution_count);
    printf("  Variation threshold: %u%%\n", params.variation_threshold);
    printf("  Byte-level detection: %s\n", params.enable_byte_level ? "Yes" : "No");
    printf("  Pattern analysis: %s\n", params.enable_pattern_analysis ? "Yes" : "No");
    printf("\n");
    
    // Detect weak bits!
    weak_bit_result_t result;
    int rc = weak_bits_detect(
        (const uint8_t**)tracks,
        rev_count,
        track_size,
        &params,
        &result
    );
    
    if (rc == 0) {
        printf("✅ Detection successful!\n");
        weak_bits_print_results(&result);
        
        // Check if would trigger X-Copy error
        if (weak_bits_triggers_xcopy_error(&result, 1)) {
            char message[256];
            weak_bits_format_xcopy_message(&result, message, sizeof(message));
            printf("\n");
            printf("🔒 X-COPY INTEGRATION:\n");
            printf("   This would trigger X-Copy Error Code 8 (Verify):\n");
            printf("   %s\n", message);
        }
        
        weak_bits_free_result(&result);
    } else {
        printf("❌ Detection failed!\n");
    }
    
    // Cleanup
    for (size_t i = 0; i < rev_count; i++) {
        free(tracks[i]);
    }
    
    weak_bits_shutdown();
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: Rob Northen Copylock Detection
 *============================================================================*/

static void example_rob_northen(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 2: Rob Northen Copylock Detection ⭐\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    weak_bits_init();
    
    const size_t track_size = 12668;  // Amiga track size
    const size_t rev_count = 5;
    
    uint8_t *tracks[10];
    for (size_t i = 0; i < rev_count; i++) {
        tracks[i] = malloc(track_size);
    }
    
    // Rob Northen uses weak bit at specific location
    create_weak_track_rob_northen(
        (const uint8_t**)tracks,
        rev_count,
        track_size,
        6000,  // Typical Rob Northen location
        4      // Bit position
    );
    
    printf("Simulating Rob Northen Copylock:\n");
    printf("  Track size: %zu bytes (Amiga DD)\n", track_size);
    printf("  Protection location: Offset 6000, Bit 4\n");
    printf("  Pattern: Alternating (0,1,0,1,...)\n");
    printf("\n");
    
    weak_bit_params_t params;
    weak_bits_get_default_params(0, &params);
    
    weak_bit_result_t result;
    weak_bits_detect(
        (const uint8_t**)tracks,
        rev_count,
        track_size,
        &params,
        &result
    );
    
    weak_bits_print_results(&result);
    
    // Export to JSON
    char json[2048];
    if (weak_bits_export_json(&result, json, sizeof(json)) == 0) {
        printf("\n");
        printf("JSON Export (first 400 chars):\n");
        printf("%.400s\n", json);
        printf("...\n");
    }
    
    weak_bits_free_result(&result);
    
    for (size_t i = 0; i < rev_count; i++) {
        free(tracks[i]);
    }
    
    weak_bits_shutdown();
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: Speedlock Detection
 *============================================================================*/

static void example_speedlock(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 3: Speedlock Protection Detection ⭐\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    weak_bits_init();
    srand(time(NULL));
    
    const size_t track_size = 12668;
    const size_t rev_count = 5;
    
    uint8_t *tracks[10];
    for (size_t i = 0; i < rev_count; i++) {
        tracks[i] = malloc(track_size);
    }
    
    // Speedlock uses MULTIPLE weak bits!
    create_weak_track_speedlock((const uint8_t**)tracks, rev_count, track_size);
    
    printf("Simulating Speedlock Protection:\n");
    printf("  Track size: %zu bytes (Amiga DD)\n", track_size);
    printf("  Protection: Multiple random weak bits\n");
    printf("  Location: Mid-track (bytes 6334-6344)\n");
    printf("\n");
    
    weak_bit_params_t params;
    weak_bits_get_default_params(0, &params);
    params.variation_threshold = 20;  // Lower threshold for Speedlock
    
    weak_bit_result_t result;
    weak_bits_detect(
        (const uint8_t**)tracks,
        rev_count,
        track_size,
        &params,
        &result
    );
    
    weak_bits_print_results(&result);
    
    printf("\n");
    printf("Speedlock Characteristics:\n");
    printf("  • Multiple weak bits in cluster\n");
    printf("  • Random patterns (not alternating)\n");
    printf("  • High density in protection zone\n");
    printf("  • Typical density: >10 per 1000 bits\n");
    printf("\n");
    
    weak_bits_free_result(&result);
    
    for (size_t i = 0; i < rev_count; i++) {
        free(tracks[i]);
    }
    
    weak_bits_shutdown();
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: X-Copy Integration
 *============================================================================*/

static void example_xcopy_integration(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 4: X-Copy Integration ⭐⭐⭐\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    printf("COMPLETE WORKFLOW:\n");
    printf("\n");
    printf("1. Read Track with Greaseweazle:\n");
    printf("   → Multiple revolutions (5-10)\n");
    printf("   → Store each revolution separately\n");
    printf("   ↓\n");
    printf("\n");
    printf("2. X-Copy Analysis (v2.6.2):\n");
    printf("   → Error Code 7: Long Track (13,200 bytes)\n");
    printf("   → UFM flag: UFM_CP_LONGTRACK\n");
    printf("   ↓\n");
    printf("\n");
    printf("3. Weak Bit Detection (v2.7.1!):\n");
    printf("   → Compare all revolutions\n");
    printf("   → Find varying bits\n");
    printf("   → Result: 15 weak bits detected!\n");
    printf("   → UFM flag: UFM_CP_WEAKBITS\n");
    printf("   ↓\n");
    printf("\n");
    printf("4. Bootblock Detection (v2.6.3):\n");
    printf("   → Bootblock: \"Rob Northen Copylock\"\n");
    printf("   → Confirms protection type!\n");
    printf("   ↓\n");
    printf("\n");
    printf("5. Combined Result:\n");
    printf("   → cp_flags = UFM_CP_LONGTRACK | UFM_CP_WEAKBITS\n");
    printf("   → bootblock = \"Rob Northen Copylock\"\n");
    printf("   → weak_bit_count = 15\n");
    printf("   → track_length = 13200\n");
    printf("   ↓\n");
    printf("\n");
    printf("6. Track Encoder (v2.7.0):\n");
    printf("   → Recreate long track: 13,200 bytes ✅\n");
    printf("   → Recreate weak bits: 15 positions ✅\n");
    printf("   → PERFECT COPY PROTECTION! 🔒⭐\n");
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("SYNERGY POWER:\n");
    printf("\n");
    printf("v2.6.2 X-Copy:      Detects LONG TRACK ⭐\n");
    printf("v2.7.1 Weak Bits:   Detects WEAK BITS ⭐\n");
    printf("v2.6.3 Bootblock:   Identifies SYSTEM ⭐\n");
    printf("v2.7.0 Encoder:     Recreates BOTH! ⭐⭐⭐\n");
    printf("\n");
    printf("= COMPLETE COPY PROTECTION PRESERVATION! 🏆💎\n");
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 5: Statistics
 *============================================================================*/

static void example_statistics(void) {
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 5: Detection Statistics\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    weak_bits_init();
    
    // Simulate analyzing multiple tracks
    printf("Analyzing 10 tracks...\n");
    printf("\n");
    
    for (int track_num = 0; track_num < 10; track_num++) {
        const size_t track_size = 12668;
        const size_t rev_count = 5;
        
        uint8_t *tracks[10];
        for (size_t i = 0; i < rev_count; i++) {
            tracks[i] = malloc(track_size);
        }
        
        // Tracks 3, 5, 7 have weak bits (copy protection)
        if (track_num == 3 || track_num == 5 || track_num == 7) {
            create_weak_track_rob_northen(
                (const uint8_t**)tracks, rev_count, track_size,
                6000, 4
            );
        } else {
            // Normal track
            for (size_t i = 0; i < rev_count; i++) {
                create_stable_track(tracks[i], track_size);
            }
        }
        
        weak_bit_params_t params;
        weak_bits_get_default_params(0, &params);
        
        weak_bit_result_t result;
        weak_bits_detect(
            (const uint8_t**)tracks,
            rev_count,
            track_size,
            &params,
            &result
        );
        
        printf("  Track %2d: %zu weak bits%s\n",
               track_num,
               result.weak_bit_count,
               result.weak_bit_count > 0 ? " 🔒" : "");
        
        weak_bits_free_result(&result);
        
        for (size_t i = 0; i < rev_count; i++) {
            free(tracks[i]);
        }
    }
    
    printf("\n");
    
    // Get statistics
    weak_bits_stats_t stats;
    weak_bits_get_stats(&stats);
    
    printf("STATISTICS:\n");
    printf("  Tracks analyzed:        %llu\n", (unsigned long long)stats.tracks_analyzed);
    printf("  Weak bits found:        %llu\n", (unsigned long long)stats.weak_bits_found);
    printf("  Protections detected:   %llu 🔒\n", (unsigned long long)stats.protections_detected);
    printf("  Average density:        %.2f per track\n", stats.avg_density);
    printf("  Total analysis time:    %llu ms\n", (unsigned long long)stats.total_time_ms);
    printf("\n");
    printf("Protection rate:          %.1f%%\n",
           (stats.protections_detected * 100.0) / stats.tracks_analyzed);
    printf("\n");
    
    weak_bits_shutdown();
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[]) {
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  WEAK BIT DETECTION - v2.7.1                             ║\n");
    printf("║  The Copy Protection Finder! 🔒⭐                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    bool simple = false;
    if (argc > 1 && strcmp(argv[1], "--simple") == 0) {
        simple = true;
    }
    
    if (simple) {
        example_basic_detection();
    } else {
        example_basic_detection();
        example_rob_northen();
        example_speedlock();
        example_xcopy_integration();
        example_statistics();
    }
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  KEY TAKEAWAYS\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Weak Bit Detection v2.7.1:\n");
    printf("  ✅ Multi-revolution analysis\n");
    printf("  ✅ Bit-level variation detection\n");
    printf("  ✅ Pattern analysis (alternating, random, custom)\n");
    printf("  ✅ X-Copy integration (Error Code 8)\n");
    printf("  ✅ Density calculation\n");
    printf("  ✅ JSON export for archival\n");
    printf("\n");
    printf("Copy Protection Coverage:\n");
    printf("  ✅ Rob Northen Copylock\n");
    printf("  ✅ Speedlock\n");
    printf("  ✅ And many more weak-bit based protections!\n");
    printf("\n");
    printf("Synergies:\n");
    printf("  💎 X-Copy (v2.6.2) + Weak Bits = Complete detection!\n");
    printf("  💎 Bootblock (v2.6.3) + Weak Bits = Protection ID!\n");
    printf("  💎 Track Encoder (v2.7.0) + Weak Bits = Perfect recreation!\n");
    printf("\n");
    printf("Next: KryoFlux/XUM1541 Hardware (v2.7.2+)\n");
    printf("  → Read flux with weak bit analysis!\n");
    printf("\n");
    
    return 0;
}
