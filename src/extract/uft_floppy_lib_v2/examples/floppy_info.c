/**
 * @file floppy_info.c
 * @brief Display information about a floppy disk image
 * 
 * Usage: floppy_info <image_file>
 */

#include <stdio.h>
#include <stdlib.h>

#include "uft_floppy_types.h"
#include "uft_floppy_io.h"
#include "uft_floppy_geometry.h"
#include "uft_fat12.h"

static void print_geometry(const uft_geometry_t *geom) {
    printf("Geometry:\n");
    printf("  Cylinders:        %u\n", geom->cylinders);
    printf("  Heads:            %u\n", geom->heads);
    printf("  Sectors/Track:    %u\n", geom->sectors_per_track);
    printf("  Bytes/Sector:     %u\n", geom->bytes_per_sector);
    printf("  Total Sectors:    %u\n", geom->total_sectors);
    printf("  Total Bytes:      %u (%u KB)\n", 
           geom->total_bytes, geom->total_bytes / 1024);
    printf("  Type:             %s\n", 
           geom->description ? geom->description : "Unknown");
}

static void print_fat12_info(const uft_fat12_info_t *info) {
    printf("\nFAT12 Volume Information:\n");
    printf("  OEM Name:         %.8s\n", info->oem_name);
    printf("  Volume Label:     %s\n", 
           info->volume_label[0] ? info->volume_label : "(none)");
    printf("  Serial Number:    %08X\n", info->volume_serial);
    printf("  Media Type:       0x%02X\n", info->media_type);
    printf("\n");
    printf("  Bytes/Sector:     %u\n", info->bytes_per_sector);
    printf("  Sectors/Cluster:  %u\n", info->sectors_per_cluster);
    printf("  FAT Copies:       %u\n", info->fat_count);
    printf("  FAT Sectors:      %u\n", info->fat_sectors);
    printf("  Root Entries:     %u\n", info->root_entries);
    printf("\n");
    printf("  Total Sectors:    %u\n", info->total_sectors);
    printf("  Total Clusters:   %u\n", info->total_clusters);
    printf("  Free Clusters:    %u\n", info->free_clusters);
    printf("  Free Space:       %u KB\n", 
           (info->free_clusters * info->sectors_per_cluster * 
            info->bytes_per_sector) / 1024);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <image_file>\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    
    uft_error_t err = uft_disk_init();
    if (err != UFT_OK) {
        fprintf(stderr, "Error: Failed to initialize: %s\n", 
                uft_disk_error_string(err));
        return 1;
    }
    
    uft_disk_t *disk;
    err = uft_disk_open_image(filename, UFT_ACCESS_READ, &disk);
    if (err != UFT_OK) {
        fprintf(stderr, "Error: Cannot open '%s': %s\n", 
                filename, uft_disk_error_string(err));
        uft_disk_cleanup();
        return 1;
    }
    
    printf("=== Floppy Image Information ===\n");
    printf("File: %s\n\n", filename);
    
    uft_disk_info_t disk_info;
    err = uft_disk_get_info(disk, &disk_info);
    if (err == UFT_OK) {
        printf("Disk Information:\n");
        printf("  Size:             %llu bytes (%llu KB)\n",
               (unsigned long long)disk_info.total_size,
               (unsigned long long)disk_info.total_size / 1024);
        printf("  Sectors:          %u\n", disk_info.total_sectors);
        printf("  Sector Size:      %u\n", disk_info.sector_size);
        printf("\n");
        
        if (disk_info.geometry.cylinders > 0) {
            print_geometry(&disk_info.geometry);
        }
    }
    
    uft_fat12_t *vol;
    err = uft_fat12_mount(disk, &vol);
    if (err == UFT_OK) {
        uft_fat12_info_t fat_info;
        err = uft_fat12_get_info(vol, &fat_info);
        if (err == UFT_OK) {
            print_fat12_info(&fat_info);
        }
        uft_fat12_unmount(vol);
    } else {
        printf("\nNote: Not a valid FAT12 filesystem (%s)\n",
               uft_disk_error_string(err));
    }
    
    uft_disk_close(disk);
    uft_disk_cleanup();
    
    return 0;
}
