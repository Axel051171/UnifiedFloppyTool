/*
 * example_bootblock_scanner.c
 * 
 * Demonstrates bootblock detection with the 2,988 signature database.
 * 
 * Usage:
 *   ./example_bootblock_scanner disk.adf
 *   ./example_bootblock_scanner *.adf
 * 
 * @version 1.0.0
 * @date 2024-12-25
 */

#include "bootblock_db.h"
#include "amigados_fs.h"
#include "format_adf.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*============================================================================*
 * BOOTBLOCK SCANNER
 *============================================================================*/

/**
 * @brief Scan single ADF file for bootblock
 */
static int scan_adf_file(const char *filename, bb_scan_stats_t *stats)
{
    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  Scanning: %s\n", filename);
    printf("═══════════════════════════════════════════════════════════\n");
    
    // Open ADF file
    FILE *f = fopen(filename, "rb");
    if (!f) {
        printf("  ❌ Cannot open file\n");
        return -1;
    }
    
    // Read first 1024 bytes (bootblock)
    uint8_t bootblock[BOOTBLOCK_SIZE];
    if (fread(bootblock, 1, BOOTBLOCK_SIZE, f) != BOOTBLOCK_SIZE) {
        printf("  ❌ Cannot read bootblock\n");
        fclose(f);
        return -1;
    }
    fclose(f);
    
    // Detect bootblock
    bb_detection_result_t result;
    if (bb_detect(bootblock, &result) != 0) {
        printf("  ❌ Detection failed\n");
        return -1;
    }
    
    // Update statistics
    if (stats) {
        bb_stats_add(stats, &result);
    }
    
    // Print results
    printf("\n");
    printf("  DOS Type:      %s\n", result.dos_type == 0 ? "OFS" : "FFS");
    printf("  Checksum:      0x%08X (%s)\n", 
           result.checksum,
           result.checksum_valid ? "✅ VALID" : "❌ INVALID");
    printf("  CRC32:         0x%08X\n", result.computed_crc);
    printf("\n");
    
    if (result.detected) {
        printf("  ✅ BOOTBLOCK DETECTED!\n");
        printf("\n");
        printf("  Name:          %s\n", result.signature.name);
        printf("  Category:      %s", bb_category_name(result.signature.category));
        
        // Special markers
        if (bb_is_virus(result.signature.category)) {
            printf(" ⚠️⚠️⚠️ VIRUS!");
        } else if (result.signature.category == BB_CAT_XCOPY) {
            printf(" 💾 (X-Copy synergy!)");
        } else if (result.signature.category == BB_CAT_DEMOSCENE ||
                   result.signature.category == BB_CAT_INTRO) {
            printf(" 🎨");
        }
        printf("\n");
        
        printf("  Bootable:      %s\n", result.signature.bootable ? "Yes" : "No");
        printf("  Has Data:      %s\n", result.signature.has_data ? "Yes" : "No");
        
        if (result.signature.kickstart[0]) {
            printf("  Kickstart:     %s\n", result.signature.kickstart);
        }
        
        printf("\n");
        printf("  Detection:     ");
        if (result.matched_by_pattern) printf("Pattern Match ");
        if (result.matched_by_crc) printf("CRC32 Match");
        printf("\n");
        
        if (result.signature.notes[0]) {
            printf("\n");
            printf("  Notes: %s\n", result.signature.notes);
        }
        
        if (result.signature.url[0]) {
            printf("  URL:   %s\n", result.signature.url);
        }
        
        // Special virus warning
        if (bb_is_virus(result.signature.category)) {
            printf("\n");
            printf("  ⚠️⚠️⚠️ WARNING: VIRUS DETECTED! ⚠️⚠️⚠️\n");
            printf("  This disk contains a known virus!\n");
            printf("  DO NOT boot this disk on real hardware!\n");
        }
        
    } else {
        printf("  ℹ️  Unknown bootblock (not in database)\n");
        printf("  This might be:\n");
        printf("    - Standard DOS bootblock\n");
        printf("    - Custom game bootblock\n");
        printf("    - Unknown protection\n");
    }
    
    printf("\n");
    return 0;
}

/*============================================================================*
 * MAIN
 *============================================================================*/

int main(int argc, char *argv[])
{
    printf("═══════════════════════════════════════════════════════════\n");
    printf("  BOOTBLOCK SCANNER v1.0.0\n");
    printf("  AmigaBootBlockReader Database (2,988 signatures)\n");
    printf("═══════════════════════════════════════════════════════════\n");
    
    // Initialize database
    printf("\n");
    printf("Loading bootblock database...\n");
    
    if (bb_db_init(NULL) != 0) {
        fprintf(stderr, "ERROR: Cannot load brainfile.xml\n");
        fprintf(stderr, "Make sure brainfile.xml is in the current directory.\n");
        return 1;
    }
    
    // Get database stats
    int total, viruses, xcopy;
    bb_db_get_stats(&total, &viruses, &xcopy);
    printf("✅ Database loaded!\n");
    printf("   Total signatures: %d\n", total);
    printf("   Virus signatures: %d\n", viruses);
    printf("   X-Copy signatures: %d\n", xcopy);
    
    if (argc < 2) {
        printf("\n");
        printf("Usage: %s <disk.adf> [disk2.adf ...]\n", argv[0]);
        printf("\n");
        printf("Example:\n");
        printf("  %s myamigadisk.adf\n", argv[0]);
        printf("  %s *.adf\n", argv[0]);
        printf("\n");
        bb_db_free();
        return 0;
    }
    
    // Scan all files
    bb_scan_stats_t stats;
    bb_stats_init(&stats);
    
    for (int i = 1; i < argc; i++) {
        scan_adf_file(argv[i], &stats);
    }
    
    // Print statistics
    if (argc > 2) {
        bb_stats_print(&stats);
    }
    
    // Cleanup
    bb_db_free();
    
    printf("\n");
    printf("✅ Scan complete!\n");
    printf("\n");
    
    return 0;
}

/*============================================================================*
 * EXAMPLE OUTPUT
 *============================================================================*/

/*

═══════════════════════════════════════════════════════════
  BOOTBLOCK SCANNER v1.0.0
  AmigaBootBlockReader Database (2,988 signatures)
═══════════════════════════════════════════════════════════

Loading bootblock database...
bb_db_init: Loaded 2988 signatures (422 viruses, 126 X-Copy)
✅ Database loaded!
   Total signatures: 2988
   Virus signatures: 422
   X-Copy signatures: 126

═══════════════════════════════════════════════════════════
  Scanning: lemmings.adf
═══════════════════════════════════════════════════════════

  DOS Type:      OFS
  Checksum:      0x370482A1 (✅ VALID)
  CRC32:         0xA5B3C7D2

  ✅ BOOTBLOCK DETECTED!

  Name:          Psygnosis Loader
  Category:      Loader
  Bootable:      Yes
  Has Data:      No
  Kickstart:     KS1.2+

  Detection:     Pattern Match 

═══════════════════════════════════════════════════════════
  Scanning: virus_test.adf
═══════════════════════════════════════════════════════════

  DOS Type:      OFS
  Checksum:      0x12345678 (❌ INVALID)
  CRC32:         0x492A98FC

  ✅ BOOTBLOCK DETECTED!

  Name:          16-Bit Crew Virus
  Category:      VIRUS ⚠️⚠️⚠️ VIRUS!
  Bootable:      True
  Has Data:      False
  Kickstart:     KS1.3

  Detection:     Pattern Match CRC32 Match

  Notes: This is a known boot sector virus. Remove immediately!
  URL:   http://amiga.nvg.org/amiga/VirusEncyclopedia/ae000016.php

  ⚠️⚠️⚠️ WARNING: VIRUS DETECTED! ⚠️⚠️⚠️
  This disk contains a known virus!
  DO NOT boot this disk on real hardware!

═══════════════════════════════════════════════════════════
  BOOTBLOCK SCAN STATISTICS
═══════════════════════════════════════════════════════════
  Total disks scanned:  2
  Detected bootblocks:  2
  Viruses found:        1 ⚠️
  X-Copy bootblocks:    0
  Demoscene intros:     0
  Unknown bootblocks:   0
═══════════════════════════════════════════════════════════

✅ Scan complete!

*/
