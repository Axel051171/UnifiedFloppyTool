// SPDX-License-Identifier: MIT
/*
 * example_applesauce_formats.c - Applesauce Format Examples
 * 
 * Demonstrates reading A2R3, WOZ1, and MOOF files.
 * 
 * @version 2.8.1
 * @date 2024-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "applesauce/a2r3_reader.h"
#include "applesauce/woz1_reader.h"
#include "applesauce/moof_reader.h"

/*============================================================================*
 * EXAMPLES
 *============================================================================*/

/**
 * @brief Example 1: Read A2R3 file
 */
static void example_a2r3(const char *path)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: A2R3 Flux Format                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    a2r3_image_t *image = NULL;
    
    printf("Reading A2R3 file: %s\n", path);
    if (a2r3_read(path, &image) != 0) {
        printf("Error: Failed to read A2R3 file\n");
        return;
    }
    
    /* Print info */
    a2r3_print_info(image);
    
    printf("\nCapture Details:\n");
    for (size_t i = 0; i < image->captures_len && i < 5; i++) {
        a2r3_capture_t *cap = &image->captures[i];
        printf("  Capture %zu:\n", i);
        printf("    Location:     0x%08X\n", cap->location);
        printf("    Type:         %u (1=timing, 2=bits, 3=xtiming)\n", 
               cap->capture_type);
        printf("    Resolution:   %u picoseconds/tick\n", cap->resolution_ps);
        printf("    Index marks:  %u\n", cap->index_count);
        printf("    Flux deltas:  %u\n", cap->deltas_count);
        printf("    Packed size:  %u bytes\n", cap->packed_len);
    }
    
    if (image->captures_len > 5) {
        printf("  ... and %zu more captures\n", image->captures_len - 5);
    }
    
    /* Cleanup */
    a2r3_free(image);
    printf("\n✓ A2R3 example completed\n\n");
}

/**
 * @brief Example 2: Read WOZ1 file
 */
static void example_woz1(const char *path)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: WOZ1 Bitstream Format                        ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    woz1_image_t *image = NULL;
    
    printf("Reading WOZ1 file: %s\n", path);
    if (woz1_read(path, &image) != 0) {
        printf("Error: Failed to read WOZ1 file\n");
        return;
    }
    
    /* Print info */
    woz1_print_info(image);
    
    printf("\nTrack Map:\n");
    int track_count = 0;
    for (int i = 0; i < 160; i++) {
        if (image->tmap[i] != 0xFF) {
            track_count++;
        }
    }
    printf("  Active tracks: %d / 160\n", track_count);
    
    /* Read first few tracks */
    printf("\nSample Tracks:\n");
    for (int i = 0; i < 160 && track_count > 0; i++) {
        if (image->tmap[i] != 0xFF) {
            woz1_track_t track;
            if (woz1_get_track(path, image, i, &track) == 0) {
                printf("  Track %d.%d: %u bytes, %u bits\n",
                       i / 4, (i % 4) * 25, 
                       track.bytes_used, track.bit_count);
            }
            track_count--;
            if (track_count > 155) break;  /* Show first 5 */
        }
    }
    
    /* Cleanup */
    woz1_free(image);
    printf("\n✓ WOZ1 example completed\n\n");
}

/**
 * @brief Example 3: Read MOOF file
 */
static void example_moof(const char *path)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: MOOF Hybrid Format                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    moof_image_t *image = NULL;
    
    printf("Reading MOOF file: %s\n", path);
    if (moof_read(path, &image) != 0) {
        printf("Error: Failed to read MOOF file\n");
        return;
    }
    
    /* Print info */
    moof_print_info(image);
    
    /* Check for flux */
    bool has_flux = moof_has_flux(image);
    printf("\nData Type: %s\n", has_flux ? "Bitstream + Flux" : "Bitstream only");
    
    if (has_flux) {
        printf("Flux block: %u\n", image->flux_block);
    }
    
    printf("\nOptimal bit timing: %u × 125ns = %u ns\n",
           image->optimal_bit_timing_125ns,
           image->optimal_bit_timing_125ns * 125);
    
    /* Cleanup */
    moof_free(image);
    printf("\n✓ MOOF example completed\n\n");
}

/**
 * @brief Example 4: Format comparison
 */
static void example_comparison(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Applesauce Format Comparison                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    printf("Format Characteristics:\n\n");
    
    printf("A2R3 (Applesauce Raw):\n");
    printf("  • Flux-level preservation\n");
    printf("  • Picosecond timing resolution\n");
    printf("  • Lossless flux capture\n");
    printf("  • Maximum fidelity\n");
    printf("  • Best for: Copy protection, archival\n");
    printf("  • File size: Large (flux data)\n\n");
    
    printf("WOZ1 (Woz A Day):\n");
    printf("  • Bitstream normalized to 4µs\n");
    printf("  • Quantized but accurate\n");
    printf("  • Splice point support\n");
    printf("  • Good balance: quality vs size\n");
    printf("  • Best for: Emulation, standard disks\n");
    printf("  • File size: Medium\n\n");
    
    printf("MOOF (Multi-format):\n");
    printf("  • Hybrid: bitstream OR flux\n");
    printf("  • Flexible format\n");
    printf("  • Optional flux tracks\n");
    printf("  • Block-based structure\n");
    printf("  • Best for: Mixed content disks\n");
    printf("  • File size: Variable\n\n");
    
    printf("Use Case Recommendations:\n\n");
    printf("Copy-protected disk → A2R3 (flux preservation)\n");
    printf("Standard disk       → WOZ1 (good quality, smaller)\n");
    printf("Mixed disk          → MOOF (hybrid support)\n");
    printf("Emulation           → WOZ1 (broad compatibility)\n");
    printf("Archival            → A2R3 (maximum fidelity)\n");
    
    printf("\n✓ Comparison completed\n\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  APPLESAUCE FORMATS - Complete Examples                  ║\n");
    printf("║  v2.8.1 - Apple II Complete Edition                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Run examples */
    if (argc > 1) {
        /* Read specific file */
        const char *path = argv[1];
        
        if (strstr(path, ".a2r")) {
            example_a2r3(path);
        } else if (strstr(path, ".woz")) {
            example_woz1(path);
        } else if (strstr(path, ".moof")) {
            example_moof(path);
        } else {
            printf("Usage: %s <file.a2r|file.woz|file.moof>\n", argv[0]);
            printf("   or: %s (run demonstration)\n", argv[0]);
            return 1;
        }
    } else {
        /* Run demonstration */
        example_comparison();
    }
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  All examples completed! ✓                                ║\n");
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
