// SPDX-License-Identifier: MIT
/*
 * example_msa.c - MSA Format Examples
 * 
 * Demonstrates MSA format usage in UFT v2.8.6
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

#include "msa.h"
#include "uft/core/uft_safe_parse.h"

/*============================================================================*
 * EXAMPLE 1: Create MSA Image
 *============================================================================*/

static void example_create_msa(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Create Atari ST MSA Image                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    msa_image_t image;
    
    /* Create standard Atari ST format (9 sectors, DS, 80 tracks) */
    if (!msa_init(&image, 9, 2, 80)) {
        printf("❌ Initialization failed\n");
        return;
    }
    
    printf("✅ Created Atari ST MSA image\n");
    printf("   Sectors/track: %d\n", image.header.sectors_per_track);
    printf("   Sides:         %d\n", image.num_sides);
    printf("   Tracks:        %d\n", image.num_tracks);
    printf("   Track size:    %zu bytes\n", 
           msa_track_size(image.header.sectors_per_track));
    printf("\n");
    
    /* Fill first track with data */
    uint8_t *track;
    size_t size;
    if (msa_get_track(&image, 0, 0, &track, &size)) {
        snprintf((char*)track, 256, "Atari ST MSA Test - Track 0, Side 0");
        printf("✅ Wrote data to track 0, side 0\n");
    }
    
    /* Save */
    const char *filename = "atari_st_test.msa";
    if (msa_write(filename, &image)) {
        printf("✅ Saved to '%s'\n", filename);
    } else {
        printf("❌ Failed to save\n");
    }
    
    msa_free(&image);
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: Read and Analyze MSA
 *============================================================================*/

static void example_read_msa(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: Read and Analyze MSA                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    msa_image_t image;
    const char *filename = "atari_st_test.msa";
    
    printf("Reading: %s\n", filename);
    
    if (msa_read(filename, &image)) {
        printf("✅ Read successful!\n\n");
        
        /* Display info */
        printf("Image Information:\n");
        printf("  Magic:         0x%04X\n", image.header.magic);
        printf("  Sectors/track: %d\n", image.header.sectors_per_track);
        printf("  Sides:         %d\n", image.num_sides);
        printf("  Tracks:        %d-%d (%d total)\n",
               image.header.starting_track,
               image.header.ending_track,
               image.num_tracks);
        printf("  Track size:    %zu bytes\n",
               msa_track_size(image.header.sectors_per_track));
        printf("\n");
        
        /* Validate */
        char errors[1024];
        if (msa_validate(&image, errors, sizeof(errors))) {
            printf("✅ Validation: PASSED\n");
        } else {
            printf("❌ Validation: FAILED\n");
            printf("Errors:\n%s\n", errors);
        }
        
        /* Read track */
        uint8_t *data;
        size_t size;
        if (msa_get_track(&image, 0, 0, &data, &size)) {
            printf("\nTrack 0, Side 0 contents:\n");
            printf("  Size: %zu bytes\n", size);
            printf("  Data: %s\n", data);
        }
        
        msa_free(&image);
    } else {
        printf("❌ Read failed\n");
        printf("Note: Make sure '%s' exists\n", filename);
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: Convert MSA to ST
 *============================================================================*/

static void example_msa_to_st(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: Convert MSA to ST (raw)                      ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    const char *msa_file = "atari_st_test.msa";
    const char *st_file = "atari_st_test.st";
    
    printf("Converting: %s → %s\n", msa_file, st_file);
    
    if (msa_to_st(msa_file, st_file)) {
        printf("✅ Conversion successful!\n");
        printf("\nMSA vs ST:\n");
        printf("  MSA: Compressed Atari ST disk image\n");
        printf("  ST:  Raw Atari ST disk image (uncompressed)\n");
    } else {
        printf("❌ Conversion failed\n");
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: Different Atari ST Formats
 *============================================================================*/

static void example_atari_formats(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Different Atari ST Formats                   ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* Format 1: Standard 9 sector */
    msa_image_t img1;
    if (msa_init(&img1, 9, 2, 80)) {
        printf("✅ Created 9-sector format (standard)\n");
        printf("   Capacity: %zu KB\n",
               (80 * 2 * 9 * 512) / 1024);
        msa_write("atari_9sector.msa", &img1);
        msa_free(&img1);
    }
    
    /* Format 2: 10 sector */
    msa_image_t img2;
    if (msa_init(&img2, 10, 2, 80)) {
        printf("✅ Created 10-sector format\n");
        printf("   Capacity: %zu KB\n",
               (80 * 2 * 10 * 512) / 1024);
        msa_write("atari_10sector.msa", &img2);
        msa_free(&img2);
    }
    
    /* Format 3: 11 sector (extended) */
    msa_image_t img3;
    if (msa_init(&img3, 11, 2, 80)) {
        printf("✅ Created 11-sector format (extended)\n");
        printf("   Capacity: %zu KB\n",
               (80 * 2 * 11 * 512) / 1024);
        msa_write("atari_11sector.msa", &img3);
        msa_free(&img3);
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
    printf("║  MSA FORMAT EXAMPLES                                      ║\n");
    printf("║  UFT v2.8.6 - Atari ST Edition                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    if (argc > 1) {
        int32_t example = 0;
        if (!uft_parse_int32(argv[1], &example, 10)) {
            fprintf(stderr, "Invalid argument: %s\n", argv[1]);
            return 1;
        }
        
        switch (example) {
            case 1:
                example_create_msa();
                break;
            case 2:
                example_read_msa();
                break;
            case 3:
                example_msa_to_st();
                break;
            case 4:
                example_atari_formats();
                break;
            default:
                printf("\nUsage: %s [1-4]\n", argv[0]);
                printf("  1 - Create MSA image\n");
                printf("  2 - Read and analyze MSA\n");
                printf("  3 - Convert MSA to ST\n");
                printf("  4 - Different Atari ST formats\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_create_msa();
        example_read_msa();
        example_msa_to_st();
        example_atari_formats();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Examples completed! ✓                                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("MSA FORMAT INFORMATION:\n");
    printf("  • MSA (Magic Shadow Archiver) by David Lawrence\n");
    printf("  • Compressed Atari ST disk image format\n");
    printf("  • RLE compression algorithm\n");
    printf("  • Supports 9, 10, 11, and 18 sector formats\n");
    printf("  • Standard for Atari ST emulators\n");
    printf("\n");
    printf("ATARI ST INFORMATION:\n");
    printf("  • 16/32-bit Motorola 68000 CPU\n");
    printf("  • Legendary in music production (Cubase, Logic)\n");
    printf("  • Important in graphics (Degas, NEOchrome)\n");
    printf("  • Rich gaming library\n");
    printf("  • Active preservation community\n");
    printf("\n");
    printf("INTEGRATION NOTES:\n");
    printf("  • Use msa_read() to load MSA files\n");
    printf("  • Use msa_write() to save MSA files\n");
    printf("  • Use msa_to_st() to convert to raw ST format\n");
    printf("  • Use msa_get_track() for track access\n");
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
