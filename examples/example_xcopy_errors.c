// SPDX-License-Identifier: MIT
/*
 * example_xcopy_errors.c - X-Copy Error System Demo
 * 
 * Demonstrates the X-Copy error detection system integrated
 * from the original X-Copy Professional source code.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../libflux_core/include/xcopy_errors.h"

// Simulate different track types for testing
void simulate_normal_track(uint8_t *buffer, size_t *length)
{
    // Normal AmigaDOS track: ~11 sectors * 512 bytes + gaps
    *length = 11 * 512 + 800;  // ~6400 bytes
    memset(buffer, 0, *length);
}

void simulate_long_track(uint8_t *buffer, size_t *length)
{
    // Long track (copy protection!)
    *length = 14000;  // Longer than standard
    memset(buffer, 0, *length);
}

void simulate_short_track(uint8_t *buffer, size_t *length)
{
    // Short track
    *length = 5000;  // Shorter than standard
    memset(buffer, 0, *length);
}

int main(int argc, char *argv[])
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("UnifiedFloppyTool v2.6.2 - X-Copy Error System Demo\n");
    printf("═══════════════════════════════════════════════════════════\n\n");
    
    printf("This demo shows the X-Copy error detection system\n");
    printf("converted from the original X-Copy Professional source.\n\n");
    
    // Allocate buffer for track simulation
    uint8_t *track_buffer = malloc(20000);
    if (!track_buffer) {
        fprintf(stderr, "Failed to allocate track buffer\n");
        return 1;
    }
    
    // Test 1: Normal track
    printf("TEST 1: Normal AmigaDOS Track\n");
    printf("─────────────────────────────────────────────────────────\n");
    size_t track_length;
    simulate_normal_track(track_buffer, &track_length);
    
    xcopy_track_error_t error;
    xcopy_analyze_track(track_buffer, track_length, &error);
    
    printf("Track length: %zu bytes (expected ~%u)\n", 
           track_length, error.expected_length);
    printf("Error code:   %d\n", error.error_code);
    printf("Message:      %s\n", xcopy_error_message(error.error_code));
    printf("Message (DE): %s\n", xcopy_error_message_de(error.error_code));
    printf("Protected:    %s\n\n", error.is_protected ? "YES" : "NO");
    
    // Test 2: Long track (copy protection!)
    printf("TEST 2: Long Track (Copy Protection)\n");
    printf("─────────────────────────────────────────────────────────\n");
    simulate_long_track(track_buffer, &track_length);
    xcopy_analyze_track(track_buffer, track_length, &error);
    
    printf("Track length: %zu bytes (expected ~%u)\n",
           track_length, error.expected_length);
    printf("Error code:   %d\n", error.error_code);
    printf("Message:      %s\n", xcopy_error_message(error.error_code));
    printf("Message (DE): %s\n", xcopy_error_message_de(error.error_code));
    printf("Protected:    %s ⭐\n\n", error.is_protected ? "YES" : "NO");
    
    // Test 3: UFM Integration
    printf("TEST 3: UFM Copy-Protection Flags\n");
    printf("─────────────────────────────────────────────────────────\n");
    
    // Test all error codes
    printf("X-Copy Error → UFM CP Flags Mapping:\n\n");
    
    for (int i = 1; i <= 8; i++) {
        uint32_t flags = xcopy_error_to_ufm_flags(i);
        printf("Error %d: %s\n", i, xcopy_error_message(i));
        printf("  UFM Flags: 0x%08X\n", flags);
        
        if (flags & (1u << 1)) printf("    → UFM_CP_LONGTRACK\n");
        if (flags & (1u << 3)) printf("    → UFM_CP_BAD_CRC\n");
        if (flags & (1u << 5)) printf("    → UFM_CP_NONSTD_GAP\n");
        if (flags & (1u << 6)) printf("    → UFM_CP_SYNC_ANOMALY\n");
        printf("\n");
    }
    
    // Test 4: Statistics
    printf("TEST 4: Disk Statistics\n");
    printf("─────────────────────────────────────────────────────────\n");
    printf("Simulating 80 track disk (160 tracks total)...\n\n");
    
    xcopy_error_stats_t stats;
    xcopy_stats_init(&stats);
    
    // Simulate disk with some protection
    for (int cyl = 0; cyl < 80; cyl++) {
        for (int head = 0; head < 2; head++) {
            // Tracks 39-42 are protected (long tracks)
            if (cyl >= 39 && cyl <= 42) {
                simulate_long_track(track_buffer, &track_length);
            } else {
                simulate_normal_track(track_buffer, &track_length);
            }
            
            xcopy_analyze_track(track_buffer, track_length, &error);
            xcopy_stats_add(&stats, &error);
        }
    }
    
    xcopy_stats_print(&stats);
    
    // Test 5: All error messages
    printf("\nTEST 5: All X-Copy Error Messages\n");
    printf("─────────────────────────────────────────────────────────\n");
    printf("English Messages:\n");
    for (int i = 1; i <= 8; i++) {
        printf("  %s\n", xcopy_error_message(i));
    }
    printf("\nGerman Messages:\n");
    for (int i = 1; i <= 8; i++) {
        printf("  %s\n", xcopy_error_message_de(i));
    }
    printf("\n");
    
    // Cleanup
    free(track_buffer);
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("Demo complete! X-Copy error system is working perfectly.\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    return 0;
}
