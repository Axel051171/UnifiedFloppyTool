// SPDX-License-Identifier: MIT
/*
 * example_amiga_adf.c - AmigaDOS/ADF Example
 * 
 * Demonstrates how to:
 * 1. Read an ADF file
 * 2. Access AmigaDOS filesystem
 * 3. List files and directories
 * 4. Extract file information
 */

#include <stdio.h>
#include <stdlib.h>
#include "../../libflux_format/include/flux_format/adf.h"
#include "../../libflux_core/include/filesystems/amigados_filesystem.h"
#include "../../libflux_core/include/flux_core.h"

// Callback for directory listing
void print_file_info(void *user_data, const amigados_file_info_t *info)
{
    const char *type = info->is_directory ? "DIR " : "FILE";
    
    printf("  [%s] %-30s %10u bytes\n", 
           type, 
           info->filename, 
           info->size);
}

int main(int argc, char *argv[])
{
    printf("═══════════════════════════════════════════════════════\n");
    printf("UnifiedFloppyTool v2.6.1 - Amiga/ADF Example\n");
    printf("═══════════════════════════════════════════════════════\n\n");
    
    if (argc < 2) {
        printf("Usage: %s <adf_file>\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s workbench.adf\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    
    // Step 1: Probe ADF file
    printf("Probing '%s'...\n", filename);
    if (adf_probe(filename) <= 0) {
        fprintf(stderr, "Error: File is not a valid ADF image\n");
        return 1;
    }
    printf("✅ Valid ADF file detected\n\n");
    
    // Step 2: Read ADF into UFM
    printf("Reading ADF...\n");
    ufm_disk_t disk;
    ufm_disk_init(&disk);
    
    if (adf_read(filename, &disk) != 0) {
        fprintf(stderr, "Error: Failed to read ADF file\n");
        ufm_disk_free(&disk);
        return 1;
    }
    
    printf("✅ ADF loaded: %d cylinders, %d heads\n", 
           disk.cyls, disk.heads);
    printf("\n");
    
    // Step 3: Open AmigaDOS filesystem
    printf("Opening AmigaDOS filesystem...\n");
    amigados_fs_t *fs = amigados_open(&disk);
    if (!fs) {
        fprintf(stderr, "Error: Failed to open AmigaDOS filesystem\n");
        ufm_disk_free(&disk);
        return 1;
    }
    
    // Detect filesystem type
    amigados_type_t fs_type = amigados_detect_type(&disk);
    const char *fs_name;
    switch(fs_type) {
        case AMIGADOS_OFS:      fs_name = "OFS (Old File System)"; break;
        case AMIGADOS_FFS:      fs_name = "FFS (Fast File System)"; break;
        case AMIGADOS_OFS_INTL: fs_name = "OFS International"; break;
        case AMIGADOS_FFS_INTL: fs_name = "FFS International"; break;
        default:                fs_name = "Unknown"; break;
    }
    
    printf("✅ Filesystem: %s\n\n", fs_name);
    
    // Step 4: List root directory
    printf("Root directory contents:\n");
    printf("─────────────────────────────────────────────────────────\n");
    amigados_list_directory(fs, "/", print_file_info, NULL);
    printf("─────────────────────────────────────────────────────────\n");
    printf("\n");
    
    // Step 5: Read a specific file (example)
    printf("Reading file: DEVS/system-configuration\n");
    amigados_file_info_t file_info;
    if (amigados_read_file(fs, "DEVS/system-configuration", &file_info) == 0) {
        printf("✅ File found:\n");
        printf("   Name: %s\n", file_info.filename);
        printf("   Size: %u bytes\n", file_info.size);
        printf("   Type: %s\n", file_info.is_directory ? "Directory" : "File");
        printf("   Protection: 0x%08X\n", file_info.protection);
    } else {
        printf("⚠️  File not found (this is OK if disk doesn't have it)\n");
    }
    printf("\n");
    
    // Cleanup
    amigados_close(fs);
    ufm_disk_free(&disk);
    
    printf("═══════════════════════════════════════════════════════\n");
    printf("Done! AmigaDOS filesystem successfully accessed.\n");
    printf("═══════════════════════════════════════════════════════\n");
    
    return 0;
}
