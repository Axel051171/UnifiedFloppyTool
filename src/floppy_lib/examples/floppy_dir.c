/**
 * @file floppy_dir.c
 * @brief List directory contents of a FAT12 floppy image
 * 
 * Usage: floppy_dir <image_file> [path]
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft_floppy_types.h"
#include "uft_floppy_io.h"
#include "uft_fat12.h"

static const char* attr_string(uint8_t attr) {
    static char buf[7];
    buf[0] = (attr & UFT_ATTR_READ_ONLY) ? 'R' : '-';
    buf[1] = (attr & UFT_ATTR_HIDDEN)    ? 'H' : '-';
    buf[2] = (attr & UFT_ATTR_SYSTEM)    ? 'S' : '-';
    buf[3] = (attr & UFT_ATTR_VOLUME_ID) ? 'V' : '-';
    buf[4] = (attr & UFT_ATTR_DIRECTORY) ? 'D' : '-';
    buf[5] = (attr & UFT_ATTR_ARCHIVE)   ? 'A' : '-';
    buf[6] = '\0';
    return buf;
}

int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <image_file> [path]\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    const char *path = (argc == 3) ? argv[2] : "/";
    
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
    
    uft_fat12_t *vol;
    err = uft_fat12_mount(disk, &vol);
    if (err != UFT_OK) {
        fprintf(stderr, "Error: Cannot mount FAT12: %s\n", 
                uft_disk_error_string(err));
        uft_disk_close(disk);
        uft_disk_cleanup();
        return 1;
    }
    
    /* Get volume info for label */
    uft_fat12_info_t info;
    uft_fat12_get_info(vol, &info);
    
    printf(" Volume in drive is %s\n", 
           info.volume_label[0] ? info.volume_label : "(no label)");
    printf(" Volume Serial Number is %04X-%04X\n",
           (info.volume_serial >> 16) & 0xFFFF,
           info.volume_serial & 0xFFFF);
    printf("\n Directory of %s\n\n", path);
    
    /* Open root directory */
    uft_fat12_dir_t *dir;
    err = uft_fat12_opendir_root(vol, &dir);
    if (err != UFT_OK) {
        fprintf(stderr, "Error: Cannot open directory: %s\n",
                uft_disk_error_string(err));
        uft_fat12_unmount(vol);
        uft_disk_close(disk);
        uft_disk_cleanup();
        return 1;
    }
    
    /* List entries */
    uft_fat12_entry_t entry;
    int file_count = 0;
    int dir_count = 0;
    uint64_t total_size = 0;
    
    while (uft_fat12_readdir(dir, &entry) == UFT_OK) {
        /* Skip . and .. */
        if (strcmp(entry.name, ".") == 0 || strcmp(entry.name, "..") == 0) {
            continue;
        }
        
        printf("%04u-%02u-%02u  %02u:%02u  %s  ",
               entry.modified.year, entry.modified.month, entry.modified.day,
               entry.modified.hour, entry.modified.minute,
               attr_string(entry.attributes));
        
        if (entry.is_directory) {
            printf("    <DIR>     ");
            dir_count++;
        } else {
            printf("%10u  ", entry.size);
            total_size += entry.size;
            file_count++;
        }
        
        printf("%s\n", entry.name);
    }
    
    uft_fat12_closedir(dir);
    
    /* Summary */
    printf("\n");
    printf("     %d File(s)  %llu bytes\n", file_count, (unsigned long long)total_size);
    printf("     %d Dir(s)   %u bytes free\n", dir_count,
           info.free_clusters * info.sectors_per_cluster * info.bytes_per_sector);
    
    uft_fat12_unmount(vol);
    uft_disk_close(disk);
    uft_disk_cleanup();
    
    return 0;
}
