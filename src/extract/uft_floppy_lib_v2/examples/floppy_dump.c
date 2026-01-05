/**
 * @file floppy_dump.c
 * @brief Dump raw sectors from a floppy image
 * 
 * Usage: floppy_dump <image_file> [start_sector] [count]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "uft_floppy_types.h"
#include "uft_floppy_io.h"
#include "uft_floppy_geometry.h"

static void hex_dump(const uint8_t *data, size_t size, uint64_t offset) {
    for (size_t i = 0; i < size; i += 16) {
        /* Offset */
        printf("%08llX: ", (unsigned long long)(offset + i));
        
        /* Hex bytes */
        for (size_t j = 0; j < 16; j++) {
            if (i + j < size) {
                printf("%02X ", data[i + j]);
            } else {
                printf("   ");
            }
            if (j == 7) printf(" ");
        }
        
        printf(" |");
        
        /* ASCII */
        for (size_t j = 0; j < 16 && i + j < size; j++) {
            uint8_t c = data[i + j];
            printf("%c", isprint(c) ? c : '.');
        }
        
        printf("|\n");
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 4) {
        fprintf(stderr, "Usage: %s <image_file> [start_sector] [count]\n", argv[0]);
        fprintf(stderr, "\n");
        fprintf(stderr, "  start_sector  First sector to dump (default: 0)\n");
        fprintf(stderr, "  count         Number of sectors (default: 1)\n");
        return 1;
    }
    
    const char *filename = argv[1];
    uint64_t start_sector = (argc >= 3) ? strtoull(argv[2], NULL, 0) : 0;
    uint32_t count = (argc >= 4) ? (uint32_t)strtoul(argv[3], NULL, 0) : 1;
    
    if (count == 0) count = 1;
    if (count > 256) {
        fprintf(stderr, "Warning: Limiting to 256 sectors\n");
        count = 256;
    }
    
    uft_error_t err = uft_disk_init();
    if (err != UFT_OK) {
        fprintf(stderr, "Error: %s\n", uft_disk_error_string(err));
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
    
    uint16_t sector_size = uft_disk_get_sector_size(disk);
    uint8_t *buffer = (uint8_t*)malloc((size_t)count * sector_size);
    if (!buffer) {
        fprintf(stderr, "Error: Out of memory\n");
        uft_disk_close(disk);
        uft_disk_cleanup();
        return 1;
    }
    
    printf("=== Sector Dump ===\n");
    printf("File:    %s\n", filename);
    printf("Sectors: %llu to %llu (%u sectors)\n",
           (unsigned long long)start_sector,
           (unsigned long long)(start_sector + count - 1),
           count);
    printf("Sector size: %u bytes\n\n", sector_size);
    
    err = uft_disk_read_sectors(disk, buffer, start_sector, count);
    if (err != UFT_OK) {
        fprintf(stderr, "Error: Read failed: %s\n", uft_disk_error_string(err));
        free(buffer);
        uft_disk_close(disk);
        uft_disk_cleanup();
        return 1;
    }
    
    /* Dump each sector */
    for (uint32_t i = 0; i < count; i++) {
        uint64_t sector = start_sector + i;
        
        /* Convert to CHS if possible */
        uft_disk_info_t info;
        if (uft_disk_get_info(disk, &info) == UFT_OK && 
            info.geometry.cylinders > 0) {
            uft_chs_t chs;
            if (uft_lba_to_chs(&info.geometry, (uint32_t)sector, &chs) == UFT_OK) {
                printf("--- Sector %llu (C:%u H:%u S:%u) ---\n",
                       (unsigned long long)sector,
                       chs.cylinder, chs.head, chs.sector);
            } else {
                printf("--- Sector %llu ---\n", (unsigned long long)sector);
            }
        } else {
            printf("--- Sector %llu ---\n", (unsigned long long)sector);
        }
        
        hex_dump(buffer + (i * sector_size), sector_size, sector * sector_size);
        printf("\n");
    }
    
    free(buffer);
    uft_disk_close(disk);
    uft_disk_cleanup();
    
    return 0;
}
