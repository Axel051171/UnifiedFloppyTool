/**
 * @file example_d64_bam_prg.c
 * @brief Example: D64 BAM and PRG Manipulation
 * @version 1.0.0
 * 
 * Demonstrates the use of the D64 BAM and PRG API functions:
 * - Reading disk information
 * - Modifying load addresses
 * - Removing write protection
 * - Pattern searching
 * 
 * Usage: example_d64_bam_prg <d64_file>
 * 
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uft/cbm/uft_d64_layout.h>
#include <uft/cbm/uft_d64_bam.h>
#include <uft/cbm/uft_d64_prg.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static void print_disk_info(const uft_d64_bam_info_t* info)
{
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║ Disk Information                                              ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║ Disk Name:     %-16s                              ║\n", info->disk_name);
    printf("║ Disk ID:       %-5s                                         ║\n", info->disk_id);
    printf("║ DOS Type:      %-3s                                           ║\n", info->dos_type);
    printf("║ DOS Version:   $%02X (%c)                                       ║\n", 
           info->dos_version, info->dos_version);
    printf("║ Free Blocks:   %-4u                                          ║\n", info->free_blocks);
    printf("║ Dir Track:     %-2u                                            ║\n", info->dir_track);
    printf("║ Dir Sector:    %-2u                                            ║\n", info->dir_sector);
    printf("║ Write Protect: %-3s                                           ║\n", 
           info->is_write_protected ? "Yes" : "No");
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
}

static void print_file_info(const uft_d64_prg_info_t* info)
{
    const char* type_str = "???";
    switch (info->file_type & 0x0F) {
        case 0: type_str = "DEL"; break;
        case 1: type_str = "SEQ"; break;
        case 2: type_str = "PRG"; break;
        case 3: type_str = "USR"; break;
        case 4: type_str = "REL"; break;
    }
    
    printf("║ %-16s  %3s  %c%c  T%02u/S%02u  $%04X  %4u blks ║\n",
           info->filename,
           type_str,
           info->is_closed ? ' ' : '*',
           info->is_locked ? '<' : ' ',
           info->start_track,
           info->start_sector,
           info->load_address,
           info->size_blocks);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════ */

int main(int argc, char* argv[])
{
    printf("═══════════════════════════════════════════════════════════════\n");
    printf(" UFT D64 BAM & PRG API Example\n");
    printf(" \"Bei uns geht kein Bit verloren\"\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    if (argc < 2) {
        printf("\nUsage: %s <d64_file> [prg_name] [new_load_addr]\n", argv[0]);
        printf("\nExamples:\n");
        printf("  %s game.d64                    # Show disk info\n", argv[0]);
        printf("  %s game.d64 INTRO              # Show file info\n", argv[0]);
        printf("  %s game.d64 INTRO 0xC000       # Change load address\n", argv[0]);
        return 1;
    }
    
    const char* filename = argv[1];
    const char* prg_name = (argc > 2) ? argv[2] : NULL;
    const char* new_addr_str = (argc > 3) ? argv[3] : NULL;
    
    /* Load D64 image */
    printf("\nLoading: %s\n", filename);
    
    uft_d64_image_t img;
    int rc = uft_d64_load(filename, &img);
    if (rc != 0) {
        fprintf(stderr, "Error: Could not load D64 file (error %d)\n", rc);
        return 1;
    }
    
    /* Read and display BAM info */
    uft_d64_bam_info_t bam_info;
    rc = uft_d64_bam_read_info(&img, &bam_info);
    if (rc != 0) {
        fprintf(stderr, "Error: Could not read BAM info\n");
        uft_d64_free(&img);
        return 1;
    }
    
    print_disk_info(&bam_info);
    
    /* Check for write protection */
    if (bam_info.is_write_protected) {
        printf("\n⚠️  Disk appears to be write-protected (DOS version != 0x41)\n");
        printf("    Use uft_d64_bam_unwrite_protect() to remove protection.\n");
    }
    
    /* If PRG name specified, show file info */
    if (prg_name) {
        printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
        printf("║ File: %-56s║\n", prg_name);
        printf("╠═══════════════════════════════════════════════════════════════╣\n");
        
        uft_d64_prg_info_t prg_info;
        rc = uft_d64_prg_get_info(&img, prg_name, &prg_info);
        if (rc != 0) {
            printf("║ ERROR: File not found                                         ║\n");
        } else {
            print_file_info(&prg_info);
            
            if (prg_info.is_basic) {
                printf("║ Detected as: BASIC program                                    ║\n");
            } else if ((prg_info.file_type & 0x0F) == UFT_D64_FTYPE_PRG) {
                printf("║ Detected as: Machine language                                 ║\n");
            }
            
            /* If new load address specified, modify it */
            if (new_addr_str) {
                uint16_t new_addr = (uint16_t)strtol(new_addr_str, NULL, 0);
                printf("╠═══════════════════════════════════════════════════════════════╣\n");
                printf("║ Changing load address: $%04X -> $%04X                         ║\n",
                       prg_info.load_address, new_addr);
                
                rc = uft_d64_prg_set_load_address(&img, prg_name, new_addr);
                if (rc == 0) {
                    printf("║ ✓ Load address changed successfully                          ║\n");
                    
                    /* Save modified image */
                    char out_filename[256];
                    snprintf(out_filename, sizeof(out_filename), "%s.modified.d64", filename);
                    rc = uft_d64_save(out_filename, &img);
                    if (rc == 0) {
                        printf("║ ✓ Saved to: %-49s║\n", out_filename);
                    } else {
                        printf("║ ✗ Failed to save modified image                              ║\n");
                    }
                } else {
                    printf("║ ✗ Failed to change load address                              ║\n");
                }
            }
        }
        printf("╚═══════════════════════════════════════════════════════════════╝\n");
    }
    
    /* Show BAM statistics */
    printf("\n╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║ BAM Statistics by Track                                       ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    printf("║ Track │ Free │ Used │ Sectors                                 ║\n");
    printf("╠═══════════════════════════════════════════════════════════════╣\n");
    
    for (uint8_t t = 1; t <= img.layout->max_tracks; t++) {
        uint8_t spt = uft_d64_sectors_per_track(img.layout, t);
        uint8_t free = uft_d64_bam_get_track_free(&img, t);
        uint8_t used = spt - free;
        
        printf("║  %2u   │  %2u  │  %2u  │   %2u                                   ║\n",
               t, free, used, spt);
    }
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    
    /* Cleanup */
    uft_d64_free(&img);
    
    printf("\nDone.\n");
    return 0;
}
