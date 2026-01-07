// SPDX-License-Identifier: MIT
/*
 * example_woz2.c - WOZ2 Format Examples
 * 
 * Demonstrates WOZ2 format usage in UFT v2.8.4
 * 
 * @version 2.8.4
 * @date 2024-12-26
 */

#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"

#include "woz2.h"
#include "uft/core/uft_safe_parse.h"

/*============================================================================*
 * EXAMPLE 1: Convert DSK to WOZ2
 *============================================================================*/

static void example_dsk_to_woz2(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Convert DSK to WOZ2                          ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    const char *dsk_file = "disk.dsk";
    const char *woz2_file = "disk.woz";
    
    printf("Converting: %s → %s\n", dsk_file, woz2_file);
    
    if (woz2_from_dsk(dsk_file, woz2_file, WOZ2_DISK_TYPE_5_25)) {
        printf("✅ Conversion successful!\n");
        
        /* Verify the output */
        woz2_image_t image;
        if (woz2_read(woz2_file, &image)) {
            printf("\nResult:\n");
            printf("  Disk type:    %s\n", 
                   image.info.disk_type == 1 ? "5.25\"" : "3.5\"");
            printf("  Tracks:       %d\n", image.num_tracks);
            printf("  Creator:      %s\n", image.info.creator);
            printf("  Largest track: %d blocks\n", image.info.largest_track);
            
            woz2_free(&image);
        }
    } else {
        printf("❌ Conversion failed\n");
        printf("Note: Make sure 'disk.dsk' exists (143,360 bytes)\n");
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: Read and Validate WOZ2
 *============================================================================*/

static void example_read_and_validate(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: Read and Validate WOZ2                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    const char *woz2_file = "disk.woz";
    
    printf("Reading: %s\n", woz2_file);
    
    woz2_image_t image;
    if (woz2_read(woz2_file, &image)) {
        printf("✅ Read successful!\n\n");
        
        /* Display info */
        printf("Image Information:\n");
        printf("  Version:         %d\n", image.info.version);
        printf("  Disk type:       %d (%s)\n", 
               image.info.disk_type,
               image.info.disk_type == 1 ? "5.25\"" : "3.5\"");
        printf("  Write protected: %s\n",
               image.info.write_protected ? "Yes" : "No");
        printf("  Synchronized:    %s\n",
               image.info.synchronized ? "Yes" : "No");
        printf("  Cleaned:         %s\n",
               image.info.cleaned ? "Yes" : "No");
        printf("  Creator:         %s\n", image.info.creator);
        printf("  Disk sides:      %d\n", image.info.disk_sides);
        printf("  Tracks:          %d\n", image.num_tracks);
        printf("  Track data size: %zu bytes\n", image.track_data_size);
        printf("\n");
        
        /* Validate */
        char errors[1024];
        if (woz2_validate(&image, errors, sizeof(errors))) {
            printf("✅ Validation: PASSED\n");
        } else {
            printf("❌ Validation: FAILED\n");
            printf("Errors:\n%s\n", errors);
        }
        
        /* Show track map */
        printf("\nTrack Map (first 10 tracks):\n");
        for (int t = 0; t < 10; t++) {
            for (int q = 0; q < 4; q++) {
                int idx = t * 4 + q;
                uint8_t trk_idx = image.tmap.map[idx];
                
                if (trk_idx == 0xFF) {
                    printf("  Track %d.%d: Empty\n", t, q * 25);
                } else {
                    printf("  Track %d.%d: TRK[%d] - %u bits\n",
                           t, q * 25, trk_idx,
                           image.tracks[trk_idx].bit_count);
                }
            }
        }
        
        woz2_free(&image);
    } else {
        printf("❌ Read failed\n");
        printf("Note: Make sure '%s' exists and is valid WOZ2\n", woz2_file);
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: Create WOZ2 from Scratch
 *============================================================================*/

static void example_create_woz2(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: Create WOZ2 from Scratch                     ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    woz2_image_t image;
    
    /* Initialize */
    if (!woz2_init(&image, WOZ2_DISK_TYPE_5_25)) {
        printf("❌ Initialization failed\n");
        return;
    }
    
    printf("✅ Initialized empty 5.25\" WOZ2 image\n\n");
    
    /* Add a simple track (just sync bytes for demo) */
    uint8_t track_data[1024];
    memset(track_data, 0xFF, sizeof(track_data));  /* All sync bytes */
    
    for (uint8_t t = 0; t < 35; t++) {
        if (!woz2_add_track(&image, t, 0, track_data, sizeof(track_data) * 8)) {
            printf("❌ Failed to add track %d\n", t);
            woz2_free(&image);
            return;
        }
    }
    
    printf("✅ Added 35 tracks\n");
    
    /* Set metadata */
    strncpy(image.info.creator, "UFT v2.8.4 Example", 31);
    image.info.write_protected = WOZ2_WRITE_PROTECTED_NO;
    image.info.synchronized = WOZ2_SYNCHRONIZED_YES;
    image.info.cleaned = WOZ2_CLEANED_YES;
    
    /* Write to file */
    const char *output = "example.woz";
    if (woz2_write(output, &image)) {
        printf("✅ Wrote to '%s'\n", output);
        
        printf("\nImage statistics:\n");
        printf("  Tracks:          %d\n", image.num_tracks);
        printf("  Largest track:   %d blocks\n", image.info.largest_track);
        printf("  Total data size: %zu bytes\n", image.track_data_size);
    } else {
        printf("❌ Write failed\n");
    }
    
    woz2_free(&image);
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: WOZ1 to WOZ2 Upgrade
 *============================================================================*/

static void example_woz1_to_woz2(void)
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: Upgrade WOZ1 to WOZ2                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    const char *woz1_file = "old_disk.woz";
    const char *woz2_file = "new_disk.woz";
    
    printf("Upgrading: %s → %s\n", woz1_file, woz2_file);
    
    if (woz2_from_woz1(woz1_file, woz2_file)) {
        printf("✅ Upgrade successful!\n");
        printf("\nBenefits of WOZ2:\n");
        printf("  • Enhanced metadata support\n");
        printf("  • Improved timing accuracy\n");
        printf("  • Better protection preservation\n");
        printf("  • Variable block sizes (more efficient)\n");
    } else {
        printf("❌ Upgrade failed\n");
        printf("Note: Make sure '%s' exists and is valid WOZ1\n", woz1_file);
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
    printf("║  WOZ2 FORMAT EXAMPLES                                     ║\n");
    printf("║  UFT v2.8.4 - WOZ2 Edition                               ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    
    if (argc > 1) {
        int32_t example = 0;
        if (!uft_parse_int32(argv[1], &example, 10)) {
            fprintf(stderr, "Invalid argument: %s\n", argv[1]);
            return 1;
        }
        
        switch (example) {
            case 1:
                example_dsk_to_woz2();
                break;
            case 2:
                example_read_and_validate();
                break;
            case 3:
                example_create_woz2();
                break;
            case 4:
                example_woz1_to_woz2();
                break;
            default:
                printf("\nUsage: %s [1-4]\n", argv[0]);
                printf("  1 - Convert DSK to WOZ2\n");
                printf("  2 - Read and validate WOZ2\n");
                printf("  3 - Create WOZ2 from scratch\n");
                printf("  4 - Upgrade WOZ1 to WOZ2\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_create_woz2();
        example_dsk_to_woz2();
        example_read_and_validate();
        example_woz1_to_woz2();
    }
    
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Examples completed! ✓                                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("WOZ2 FORMAT INFORMATION:\n");
    printf("  • WOZ 2.0 is the enhanced Apple II disk image format\n");
    printf("  • Supports flux-level preservation\n");
    printf("  • Better copy protection handling\n");
    printf("  • Improved metadata and timing\n");
    printf("  • Industry standard since 2018\n");
    printf("\n");
    printf("INTEGRATION NOTES:\n");
    printf("  • Use woz2_read() to load WOZ2 files\n");
    printf("  • Use woz2_write() to save WOZ2 files\n");
    printf("  • Use woz2_from_dsk() to convert DSK → WOZ2\n");
    printf("  • Use woz2_from_woz1() to upgrade WOZ1 → WOZ2\n");
    printf("  • Use woz2_validate() to check image integrity\n");
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
