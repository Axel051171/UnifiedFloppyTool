// SPDX-License-Identifier: MIT
/*
 * example_flux_to_usb.c - Complete Flux → USB Workflow
 * 
 * Demonstrates the complete workflow:
 *   1. Read flux from hardware (Applesauce, KryoFlux, etc.)
 *   2. Convert to disk image (.img, .adf, etc.)
 *   3. Write directly to USB floppy drive or USB stick
 * 
 * @version 2.8.2
 * @date 2024-12-26
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unified_api.h"
#include "usb_writer.h"
#include "apple_protection.h"

/*============================================================================*
 * EXAMPLE 1: APPLESAUCE → IMAGE → USB
 *============================================================================*/

static void example_applesauce_to_usb(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 1: Applesauce → Image → USB Floppy              ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Step 1: Read from Applesauce */
    printf("Step 1: Reading from Applesauce hardware...\n");
    
    unified_handle_t *h = unified_open_hardware("applesauce", "/dev/ttyACM0");
    if (!h) {
        fprintf(stderr, "Error: Cannot open Applesauce\n");
        return;
    }
    
    printf("  ✓ Applesauce connected\n");
    
    /* Step 2: Read disk */
    printf("\nStep 2: Reading disk...\n");
    
    if (unified_read_disk(h, "disk.ufm") != 0) {
        fprintf(stderr, "Error: Cannot read disk\n");
        unified_close(h);
        return;
    }
    
    printf("  ✓ Disk read to disk.ufm\n");
    
    /* Step 3: Detect protection (Apple II specific) */
    printf("\nStep 3: Analyzing protection...\n");
    
    FILE *f = fopen("disk.ufm", "rb");
    if (f) {
        uint8_t buf[1024];
        size_t n = fread(buf, 1, sizeof(buf), f);
        fclose(f);
        
        const apple_protection_pattern_t *prot =
            apple_protection_detect_signature(buf, n);
        
        if (prot) {
            printf("  ⚠️  Protection detected:\n");
            apple_protection_print_info(prot);
        } else {
            printf("  ✓ No known protection detected\n");
        }
    }
    
    /* Step 4: Convert to image */
    printf("\nStep 4: Converting to .img...\n");
    
    if (unified_convert(h, "disk.ufm", "disk.img", "img") != 0) {
        fprintf(stderr, "Error: Cannot convert to IMG\n");
        unified_close(h);
        return;
    }
    
    printf("  ✓ Converted to disk.img\n");
    
    unified_close(h);
    
    /* Step 5: Write to USB */
    printf("\nStep 5: Writing to USB floppy...\n");
    
    usb_writer_options_t opts = {
        .verify = 1,
        .progress = 1,
        .confirm = 1,
        .sync = 1
    };
    
    if (usb_writer_write_image("/dev/sdb", "disk.img", &opts) == 0) {
        printf("\n✓ Disk successfully written to USB!\n");
    } else {
        fprintf(stderr, "Error: USB write failed\n");
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 2: KRYOFLUX → ADF → USB
 *============================================================================*/

static void example_kryoflux_to_usb(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 2: KryoFlux → ADF → USB (Amiga)                 ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Step 1: Read from KryoFlux */
    printf("Step 1: Reading from KryoFlux...\n");
    
    unified_handle_t *h = unified_open_hardware("kryoflux", NULL);
    if (!h) {
        fprintf(stderr, "Error: Cannot open KryoFlux\n");
        return;
    }
    
    printf("  ✓ KryoFlux connected\n");
    
    /* Step 2: Read Amiga disk */
    printf("\nStep 2: Reading Amiga disk...\n");
    
    unified_set_format(h, "amigados");
    
    if (unified_read_disk(h, "amiga.ufm") != 0) {
        fprintf(stderr, "Error: Cannot read disk\n");
        unified_close(h);
        return;
    }
    
    printf("  ✓ Amiga disk read\n");
    
    /* Step 3: Convert to ADF */
    printf("\nStep 3: Converting to ADF...\n");
    
    if (unified_convert(h, "amiga.ufm", "amiga.adf", "adf") != 0) {
        fprintf(stderr, "Error: Cannot convert to ADF\n");
        unified_close(h);
        return;
    }
    
    printf("  ✓ Converted to amiga.adf (880 KB)\n");
    
    unified_close(h);
    
    /* Step 4: Write to USB */
    printf("\nStep 4: Writing to USB...\n");
    
    usb_writer_options_t opts = {
        .verify = 1,
        .progress = 1,
        .confirm = 0,  /* Skip confirmation for automation */
        .sync = 1
    };
    
    if (usb_writer_write_image("/dev/sdb", "amiga.adf", &opts) == 0) {
        printf("\n✓ Amiga disk written to USB!\n");
    }
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 3: SCP FILE → IMG → USB
 *============================================================================*/

static void example_scp_to_usb(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 3: SCP File → IMG → USB                         ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Step 1: Read SCP file */
    printf("Step 1: Reading SCP file...\n");
    
    unified_handle_t *h = unified_open_file("archive.scp");
    if (!h) {
        fprintf(stderr, "Error: Cannot open SCP file\n");
        return;
    }
    
    printf("  ✓ SCP file loaded\n");
    
    /* Step 2: Convert to IMG */
    printf("\nStep 2: Converting SCP → IMG...\n");
    
    if (unified_convert(h, "archive.scp", "disk.img", "img") != 0) {
        fprintf(stderr, "Error: Cannot convert\n");
        unified_close(h);
        return;
    }
    
    printf("  ✓ Converted to disk.img\n");
    
    unified_close(h);
    
    /* Step 3: Write to USB */
    printf("\nStep 3: Writing to USB...\n");
    
    usb_writer_write_image("/dev/sdb", "disk.img", NULL);
    
    printf("\n");
}

/*============================================================================*
 * EXAMPLE 4: USB DEVICE INFO
 *============================================================================*/

static void example_usb_info(void)
{
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  EXAMPLE 4: USB Device Information                       ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    const char *devices[] = {"/dev/sda", "/dev/sdb", "/dev/sdc", NULL};
    
    printf("Scanning USB devices...\n\n");
    
    for (int i = 0; devices[i]; i++) {
        usb_device_info_t info;
        
        if (usb_writer_get_info(devices[i], &info) == 0) {
            printf("Device: %s\n", info.device_path);
            printf("  Size:          %lld bytes (%.2f MB)\n",
                   (long long)info.size_bytes,
                   info.size_bytes / (1024.0 * 1024.0));
            printf("  Sectors:       %lld\n", (long long)info.size_sectors);
            printf("  Removable:     %s\n", info.is_removable ? "Yes" : "No");
            printf("  Write-protect: %s\n",
                   info.is_write_protected ? "Yes" : "No");
            
            /* Guess type */
            if (info.is_removable) {
                if (info.size_bytes == 1474560) {
                    printf("  Type:          3.5\" HD Floppy (1.44 MB)\n");
                } else if (info.size_bytes == 737280) {
                    printf("  Type:          3.5\" DD Floppy (720 KB)\n");
                } else if (info.size_bytes == 1228800) {
                    printf("  Type:          5.25\" HD Floppy (1.2 MB)\n");
                } else if (info.size_bytes == 368640) {
                    printf("  Type:          5.25\" DD Floppy (360 KB)\n");
                } else if (info.size_bytes == 901120) {
                    printf("  Type:          Amiga DD Floppy (880 KB)\n");
                } else {
                    printf("  Type:          USB Storage\n");
                }
            }
            
            printf("\n");
        }
    }
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("\n");
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  FLUX → USB COMPLETE WORKFLOW EXAMPLES                    ║\n");
    printf("║  v2.8.2 - Format Master Edition                           ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    if (argc > 1) {
        /* Run specific example */
        int example = atoi(argv[1]);
        
        switch (example) {
            case 1:
                example_applesauce_to_usb();
                break;
            case 2:
                example_kryoflux_to_usb();
                break;
            case 3:
                example_scp_to_usb();
                break;
            case 4:
                example_usb_info();
                break;
            default:
                printf("Usage: %s [1-4]\n", argv[0]);
                printf("  1 - Applesauce → IMG → USB\n");
                printf("  2 - KryoFlux → ADF → USB (Amiga)\n");
                printf("  3 - SCP File → IMG → USB\n");
                printf("  4 - USB Device Info\n");
                return 1;
        }
    } else {
        /* Run all examples */
        example_usb_info();
        example_applesauce_to_usb();
        example_kryoflux_to_usb();
        example_scp_to_usb();
    }
    
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║  Examples completed! ✓                                    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    printf("TYPICAL WORKFLOW:\n");
    printf("  1. Insert original disk in hardware device\n");
    printf("  2. Run: uft read --device applesauce --output disk.ufm\n");
    printf("  3. Run: uft convert disk.ufm disk.img\n");
    printf("  4. Insert blank disk in USB floppy\n");
    printf("  5. Run: uft write disk.img /dev/sdb\n");
    printf("\n");
    printf("OR in one command:\n");
    printf("  uft copy --from applesauce --to /dev/sdb\n");
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
