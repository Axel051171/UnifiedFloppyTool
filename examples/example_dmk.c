// SPDX-License-Identifier: MIT
/*
 * example_dmk.c - DMK Format Examples
 * 
 * Demonstrates DMK format usage in UFT v2.8.5
 * 
 * @version 2.8.5
 * @date 2024-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dmk.h"

/*============================================================================*
 * EXAMPLE 1: Create DMK Image
 *============================================================================*/

static void example_create_dmk(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Create DMK Image                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    dmk_image_t image;
    
    /* Create TRS-80 Model I format (40 tracks, SS, SD) */
    if (!dmk_init(&image, 40, 1, false)) {
        printf("❌ Initialization failed\n");
        return;
    }
    
    printf("✅ Created TRS-80 Model I DMK image\n");
    printf("   Tracks: %d\n", image.num_tracks);
    printf("   Sides:  %d\n", image.num_sides);
    printf("   Density: Single\n");
    printf("   Track length: %d bytes\n", image.header.track_length);
    printf("\n");
    
    /* Format track 0 with 10 sectors of 256 bytes */
    if (dmk_format_track(&image, 0, 0, 10, 256, 0xE5)) {
        printf("✅ Formatted track 0 with 10 sectors (256 bytes each)\n");
    }
    
    /* Write some data to sector 1 */
    uint8_t sector_data[256];
    memset(sector_data, 0, sizeof(sector_data));
    sprintf((char*)sector_data, "TRS-80 DMK Test - Track 0, Sector 1");
    
    if (dmk_write_sector(&image, 0, 0, 1, sector_data, 256)) {
        printf("✅ Wrote data to track 0, sector 1\n");
    }
    
    /* Write to file */
    const char *filename = "trs80_test.dmk";
    if (dmk_write(filename, &image)) {
        printf("✅ Saved to '%s'\n", filename);
    } else {
        printf("❌ Failed to save\n");
    }
    
    dmk_free(&image);
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: Read and Analyze DMK
 *============================================================================*/

static void example_read_dmk(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: Read and Analyze DMK                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    dmk_image_t image;
    const char *filename = "trs80_test.dmk";
    
    printf("Reading: %s\n", filename);
    
    if (dmk_read(filename, &image)) {
        printf("✅ Read successful!\n\n");
        
        /* Display info */
        printf("Image Information:\n");
        printf("  Write protected: %s\n",
               image.header.write_protect == DMK_WRITE_PROTECTED ? "Yes" : "No");
        printf("  Tracks:          %d\n", image.num_tracks);
        printf("  Sides:           %d\n", image.num_sides);
        printf("  Track length:    %d bytes\n", image.header.track_length);
        printf("  Double sided:    %s\n",
               (image.header.flags & DMK_FLAG_DOUBLE_SIDED) ? "Yes" : "No");
        printf("  Double density:  %s\n",
               (image.header.flags & DMK_FLAG_DOUBLE_DENSITY) ? "Yes" : "No");
        printf("\n");
        
        /* Validate */
        char errors[1024];
        if (dmk_validate(&image, errors, sizeof(errors))) {
            printf("✅ Validation: PASSED\n");
        } else {
            printf("❌ Validation: FAILED\n");
            printf("Errors:\n%s\n", errors);
        }
        
        /* Read sector */
        uint8_t *data;
        size_t size;
        if (dmk_get_sector(&image, 0, 0, 1, &data, &size)) {
            printf("\nTrack 0, Sector 1 contents:\n");
            printf("  Size: %zu bytes\n", size);
            printf("  Data: %s\n", data);
        }
        
        dmk_free(&image);
    } else {
        printf("❌ Read failed\n");
        printf("Note: Make sure '%s' exists\n", filename);
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: CP/M Disk Format
 *============================================================================*/

static void example_cpm_disk(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: Create CP/M Disk                             ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    dmk_image_t image;
    
    /* Create CP/M format (40 tracks, SS, DD, 512-byte sectors) */
    if (!dmk_init(&image, 40, 1, true)) {
        printf("❌ Initialization failed\n");
        return;
    }
    
    printf("✅ Created CP/M DMK image\n");
    printf("   Format: 40 tracks, Single sided, Double density\n");
    printf("   Sector size: 512 bytes\n");
    printf("\n");
    
    /* Format all tracks */
    for (uint8_t track = 0; track < 40; track++) {
        dmk_format_track(&image, track, 0, 10, 512, 0xE5);
    }
    
    printf("✅ Formatted 40 tracks\n");
    
    /* Create CP/M directory entry on track 0 */
    uint8_t dir_entry[512];
    memset(dir_entry, 0xE5, sizeof(dir_entry));
    
    /* First directory entry (simplified) */
    dir_entry[0] = 0x00;  /* User number */
    memcpy(dir_entry + 1, "TESTFILE", 8);  /* Filename */
    memcpy(dir_entry + 9, "TXT", 3);       /* Extension */
    
    if (dmk_write_sector(&image, 0, 0, 1, dir_entry, 512)) {
        printf("✅ Created CP/M directory entry\n");
    }
    
    /* Save */
    const char *filename = "cpm_disk.dmk";
    if (dmk_write(filename, &image)) {
        printf("✅ Saved to '%s'\n", filename);
    }
    
    dmk_free(&image);
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: TRS-80 Model III/4 Format
 *============================================================================*/

static void example_trs80_model3(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: TRS-80 Model III/4 Format                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    dmk_image_t image;
    
    /* Model III/4: 40 tracks, DS, DD */
    if (!dmk_init(&image, 40, 2, true)) {
        printf("❌ Initialization failed\n");
        return;
    }
    
    printf("✅ Created TRS-80 Model III/4 DMK image\n");
    printf("   Tracks: %d\n", image.num_tracks);
    printf("   Sides:  %d (Double sided)\n", image.num_sides);
    printf("   Density: Double\n");
    printf("\n");
    
    /* Format both sides of track 0 */
    dmk_format_track(&image, 0, 0, 18, 256, 0xE5);
    dmk_format_track(&image, 0, 1, 18, 256, 0xE5);
    
    printf("✅ Formatted track 0 (both sides)\n");
    printf("   Side 0: 18 sectors × 256 bytes\n");
    printf("   Side 1: 18 sectors × 256 bytes\n");
    
    /* Save */
    const char *filename = "trs80_model3.dmk";
    if (dmk_write(filename, &image)) {
        printf("✅ Saved to '%s'\n", filename);
    }
    
    dmk_free(&image);
    printf("\n");
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  DMK FORMAT EXAMPLES                                      ║\n");
    printf("║  UFT v2.8.5 - DMK Edition                                ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    if (argc > 1) {
        int example = atoi(argv[1]);
        
        switch (example) {
            case 1:
                example_create_dmk();
                break;
            case 2:
                example_read_dmk();
                break;
            case 3:
                example_cpm_disk();
                break;
            case 4:
                example_trs80_model3();
                break;
            default:
                printf("\nUsage: %s [1-4]\n", argv[0]);
                printf("  1 - Create DMK image (TRS-80 Model I)\n");
                printf("  2 - Read and analyze DMK\n");
                printf("  3 - Create CP/M disk\n");
                printf("  4 - Create TRS-80 Model III/4 disk\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_create_dmk();
        example_read_dmk();
        example_cpm_disk();
        example_trs80_model3();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Examples completed! ✓                                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("DMK FORMAT INFORMATION:\n");
    printf("  • DMK (Disk Master Kopyright) by David Keil\n");
    printf("  • Standard format for TRS-80 emulators\n");
    printf("  • Supports CP/M, TRSDOS, LDOS, NewDOS\n");
    printf("  • Variable sector sizes (128/256/512/1024)\n");
    printf("  • Single/Double density and sided\n");
    printf("\n");
    printf("SUPPORTED SYSTEMS:\n");
    printf("  • TRS-80 Model I/III/4\n");
    printf("  • CP/M systems\n");
    printf("  • Various Z80-based computers\n");
    printf("\n");
    printf("INTEGRATION NOTES:\n");
    printf("  • Use dmk_read() to load DMK files\n");
    printf("  • Use dmk_write() to save DMK files\n");
    printf("  • Use dmk_init() to create new images\n");
    printf("  • Use dmk_format_track() to format tracks\n");
    printf("  • Use dmk_get/write_sector() for sector I/O\n");
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
