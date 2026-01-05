/*
 * example_track_encoder.c
 * 
 * Demonstrates the new Track Encoder API (v2.7.0)
 * Shows how to encode logical sectors to MFM bitstream
 * 
 * FEATURES:
 *   ✅ IBM MFM encoding (PC format)
 *   ✅ Amiga MFM encoding
 *   ✅ Copy protection support (LONG TRACK!) ⭐⭐⭐
 *   ✅ Integration with X-Copy metadata
 * 
 * Compile:
 *   gcc -o track_encoder example_track_encoder.c \
 *       -I../libflux_core/include \
 *       ../build/libflux_core.a
 * 
 * Usage:
 *   ./track_encoder           # Normal track
 *   ./track_encoder --long    # Long track (copy protection!)
 * 
 * @version 2.7.0
 * @date 2024-12-25
 */

#include "track_encoder.h"
#include "flux_logical.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================*
 * HELPER: Create test UFM track
 *============================================================================*/

static void create_test_track(ufm_logical_image_t *li, int sectors)
{
    memset(li, 0, sizeof(*li));
    
    // Create test sectors
    for (int i = 0; i < sectors; i++) {
        ufm_sector_t *sec = ufm_logical_alloc_sector(li, 0, 0, i + 1);
        if (sec) {
            sec->size = 512;
            sec->data = (uint8_t*)malloc(512);
            if (sec->data) {
                // Fill with test pattern
                for (int j = 0; j < 512; j++) {
                    sec->data[j] = (uint8_t)(i * 256 + j);
                }
            }
        }
    }
}

/*============================================================================*
 * EXAMPLE 1: Normal IBM MFM Track (PC 1.44MB)
 *============================================================================*/

static void example_ibm_mfm_normal(void)
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 1: IBM MFM Normal Track (PC 1.44MB)\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    // Initialize encoder
    track_encoder_init();
    
    // Create test data (18 sectors for PC 1.44MB)
    ufm_logical_image_t li;
    create_test_track(&li, 18);
    
    // Get default parameters for IBM MFM
    track_encoder_params_t params;
    track_encoder_get_defaults(TRACK_ENC_IBM_MFM, &params);
    
    printf("Parameters:\n");
    printf("  Type: %s\n", track_encoder_type_name(params.type));
    printf("  Sectors/Track: %d\n", params.params.ibm.sectors_per_track);
    printf("  Sector Size: %d bytes\n", params.params.ibm.sector_size);
    printf("  Bitrate: %d kbps\n", params.params.ibm.bitrate_kbps);
    printf("  RPM: %d\n", params.params.ibm.rpm);
    printf("\n");
    
    // Encode track
    track_encoder_output_t output;
    int rc = track_encode(&li, &params, &output);
    
    if (rc == 0) {
        printf("✅ Encoding successful!\n");
        printf("\n");
        printf("Output:\n");
        printf("  Bitstream size: %zu bytes\n", output.bitstream_size);
        printf("  Bitstream bits: %zu bits\n", output.bitstream_bits);
        printf("  Track length: %u bytes\n", output.track_length);
        printf("  Bitrate: %u kbps\n", output.bitrate_kbps);
        printf("  Sectors encoded: %u\n", output.sectors_encoded);
        printf("\n");
        printf("First 32 bytes of bitstream:\n");
        printf("  ");
        for (int i = 0; i < 32 && i < (int)output.bitstream_size; i++) {
            printf("%02X ", output.bitstream[i]);
            if ((i + 1) % 16 == 0 && i < 31) printf("\n  ");
        }
        printf("\n");
        
        // Free output
        track_encoder_free_output(&output);
    } else {
        printf("❌ Encoding failed! rc=%d\n", rc);
    }
    
    // Cleanup
    ufm_logical_free(&li);
    track_encoder_shutdown();
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: Amiga MFM Normal Track
 *============================================================================*/

static void example_amiga_mfm_normal(void)
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 2: Amiga MFM Normal Track\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    track_encoder_init();
    
    // Create test data (11 sectors for Amiga)
    ufm_logical_image_t li;
    create_test_track(&li, 11);
    
    // Get Amiga defaults
    track_encoder_params_t params;
    track_encoder_get_defaults(TRACK_ENC_AMIGA_MFM, &params);
    
    printf("Parameters:\n");
    printf("  Type: %s\n", track_encoder_type_name(params.type));
    printf("  Sectors/Track: %d\n", params.params.amiga.sectors_per_track);
    printf("  Sector Size: %d bytes\n", params.params.amiga.sector_size);
    printf("  Long Track: %s\n", params.params.amiga.long_track ? "YES" : "NO");
    printf("\n");
    
    // Encode track
    track_encoder_output_t output;
    int rc = track_encode(&li, &params, &output);
    
    if (rc == 0) {
        printf("✅ Encoding successful!\n");
        printf("\n");
        printf("Output:\n");
        printf("  Track length: %u bytes\n", output.track_length);
        printf("  Expected: ~12,668 bytes (normal Amiga track)\n");
        printf("  Bitrate: %u kbps (Amiga uses 250 kbps)\n", output.bitrate_kbps);
        printf("  Sectors encoded: %u\n", output.sectors_encoded);
        printf("\n");
        
        track_encoder_free_output(&output);
    } else {
        printf("❌ Encoding failed!\n");
    }
    
    ufm_logical_free(&li);
    track_encoder_shutdown();
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: Amiga MFM LONG TRACK (Copy Protection!) ⭐⭐⭐
 *============================================================================*/

static void example_amiga_long_track(void)
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 3: Amiga MFM LONG TRACK (Copy Protection!) ⭐\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    track_encoder_init();
    
    // Create test data
    ufm_logical_image_t li;
    create_test_track(&li, 11);
    
    // Get Amiga defaults
    track_encoder_params_t params;
    track_encoder_get_defaults(TRACK_ENC_AMIGA_MFM, &params);
    
    // ENABLE LONG TRACK! (This is what X-Copy metadata triggers!)
    params.params.amiga.long_track = true;
    // params.params.amiga.custom_length = 13200;  // Optional: exact length
    
    printf("Parameters:\n");
    printf("  Type: %s\n", track_encoder_type_name(params.type));
    printf("  Sectors/Track: %d\n", params.params.amiga.sectors_per_track);
    printf("  Long Track: %s ⭐⭐⭐\n", 
           params.params.amiga.long_track ? "YES (COPY PROTECTION!)" : "NO");
    printf("\n");
    
    // Encode NORMAL track first (for comparison)
    track_encoder_output_t output_normal;
    track_encoder_params_t params_normal = params;
    params_normal.params.amiga.long_track = false;
    
    track_encode(&li, &params_normal, &output_normal);
    
    // Encode LONG track
    track_encoder_output_t output_long;
    int rc = track_encode(&li, &params, &output_long);
    
    if (rc == 0) {
        printf("✅ Long track encoding successful!\n");
        printf("\n");
        printf("COMPARISON:\n");
        printf("  Normal track: %u bytes\n", output_normal.track_length);
        printf("  Long track:   %u bytes ⭐\n", output_long.track_length);
        printf("  Difference:   +%d bytes (+%.2f%%)\n",
               (int)output_long.track_length - (int)output_normal.track_length,
               100.0 * (float)(output_long.track_length - output_normal.track_length) / 
                       (float)output_normal.track_length);
        printf("\n");
        printf("🔒 COPY PROTECTION INFO:\n");
        printf("   This long track simulates Rob Northen Copylock,\n");
        printf("   Speedlock, and similar protection systems.\n");
        printf("\n");
        printf("   X-Copy would detect this as:\n");
        printf("     Error Code 7: Long Track! ⚠️\n");
        printf("\n");
        printf("   When written to hardware, the protection is preserved!\n");
        printf("\n");
        
        // Show statistics
        track_encoder_stats_t stats;
        track_encoder_get_stats(&stats);
        printf("Statistics:\n");
        printf("  Tracks encoded: %llu\n", (unsigned long long)stats.tracks_encoded);
        printf("  Long tracks:    %llu 🔒\n", (unsigned long long)stats.long_tracks);
        printf("\n");
        
        track_encoder_free_output(&output_normal);
        track_encoder_free_output(&output_long);
    } else {
        printf("❌ Encoding failed!\n");
    }
    
    ufm_logical_free(&li);
    track_encoder_shutdown();
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: X-Copy Integration (Simulated)
 *============================================================================*/

static void example_xcopy_integration(void)
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  EXAMPLE 4: X-Copy Integration (Complete Workflow)\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    
    printf("WORKFLOW:\n");
    printf("\n");
    printf("1. Read Flux from Hardware:\n");
    printf("   disk.scp (from Greaseweazle)\n");
    printf("   ↓\n");
    printf("\n");
    printf("2. X-Copy Analysis (v2.6.2):\n");
    printf("   Track 0 length: 13,200 bytes\n");
    printf("   Expected: 12,668 bytes\n");
    printf("   → ERROR CODE 7: LONG TRACK! 🔒\n");
    printf("   → Set: UFM.tracks[0].cp_flags |= UFM_CP_LONGTRACK\n");
    printf("   ↓\n");
    printf("\n");
    printf("3. Bootblock Detection (v2.6.3):\n");
    printf("   Bootblock: Rob Northen Copylock\n");
    printf("   Category: Protection\n");
    printf("   → Confirm: Long track protection\n");
    printf("   ↓\n");
    printf("\n");
    printf("4. Store UFM with Metadata:\n");
    printf("   disk.ufm:\n");
    printf("     tracks[0].cp_flags = UFM_CP_LONGTRACK\n");
    printf("     tracks[0].actual_length = 13200\n");
    printf("     tracks[0].bootblock = \"Rob Northen\"\n");
    printf("   ↓\n");
    printf("\n");
    printf("5. Later: Write to Hardware (v2.7.0!):\n");
    printf("   Load: disk.ufm\n");
    printf("   ↓\n");
    printf("   Track Encoder:\n");
    printf("     if (UFM.tracks[0].cp_flags & UFM_CP_LONGTRACK)\n");
    printf("       params.amiga.long_track = true;\n");
    printf("   ↓\n");
    printf("   Encoder Output:\n");
    printf("     MFM bitstream: 13,200 bytes (LONG!)\n");
    printf("   ↓\n");
    printf("   Hardware Writer:\n");
    printf("     Write to /dev/fd0\n");
    printf("   ↓\n");
    printf("   Result:\n");
    printf("     ✅ REAL FLOPPY WITH COPY PROTECTION! 🔒⭐\n");
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  TRACK ENCODER DEMO - v2.7.0 Writer Edition              ║\n");
    printf("║  HxC Integration + Copy Protection Support! ⭐            ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    bool demo_long = false;
    
    if (argc > 1 && strcmp(argv[1], "--long") == 0) {
        demo_long = true;
    }
    
    if (demo_long) {
        // Just demonstrate long track
        example_amiga_long_track();
        example_xcopy_integration();
    } else {
        // Full demo
        example_ibm_mfm_normal();
        example_amiga_mfm_normal();
        example_amiga_long_track();
        example_xcopy_integration();
    }
    
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  KEY TAKEAWAY\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Track Encoder v2.7.0:\n");
    printf("  ✅ Converts logical sectors → MFM bitstream\n");
    printf("  ✅ IBM MFM (PC formats)\n");
    printf("  ✅ Amiga MFM (with long track support!)\n");
    printf("  ✅ Integrates with X-Copy metadata (v2.6.2)\n");
    printf("  ✅ Integrates with Bootblock DB (v2.6.3)\n");
    printf("  ✅ PRESERVES COPY PROTECTION! 🔒⭐\n");
    printf("\n");
    printf("Next: Hardware Writer (Phase 2)\n");
    printf("  → Write MFM bitstream to /dev/fd0\n");
    printf("  → Complete preservation workflow!\n");
    printf("\n");
    
    return 0;
}
