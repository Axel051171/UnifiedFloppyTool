/**
 * @file fat12_ls.c
 * @brief List files on a FAT12 floppy disk or image
 * 
 * Usage: fat12_ls <image_file | drive_number>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "uft_floppy.h"

static void print_usage(const char *prog) {
    printf("Usage: %s <image_file | drive_number>\n", prog);
}

static void list_directory(uft_fat12_t *vol) {
    uft_fat12_dir_t *dir = NULL;
    uft_error_t err;
    
    err = uft_fat12_opendir_root(vol, &dir);
    if (err != UFT_OK) {
        fprintf(stderr, "Error opening root directory\n");
        return;
    }
    
    printf("\n%-12s  %-5s  %10s  %s\n", "Name", "Attr", "Size", "Modified");
    printf("%-12s  %-5s  %10s  %s\n", "----", "----", "----", "--------");
    
    uft_fat12_entry_t entry;
    int count = 0;
    
    while ((err = uft_fat12_readdir(dir, &entry)) == UFT_OK) {
        if (entry.name[0] == '.') continue;
        
        char attrs[6];
        attrs[0] = entry.is_directory ? 'D' : '-';
        attrs[1] = entry.is_readonly  ? 'R' : '-';
        attrs[2] = entry.is_hidden    ? 'H' : '-';
        attrs[3] = entry.is_system    ? 'S' : '-';
        attrs[4] = '\0';
        
        char size_buf[16];
        if (entry.is_directory) {
            strncpy(size_buf, "<DIR>", sizeof(size_buf)-1); size_buf[sizeof(size_buf)-1] = '\0';
        } else {
            snprintf(size_buf, sizeof(size_buf), "%u", entry.size);
        }
        
        char date_buf[20];
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u %02u:%02u",
                 entry.modified.year, entry.modified.month, entry.modified.day,
                 entry.modified.hour, entry.modified.minute);
        
        printf("%-12s  %-5s  %10s  %s\n", entry.name, attrs, size_buf, date_buf);
        count++;
    }
    
    printf("\n%d file(s)\n", count);
    uft_fat12_closedir(dir);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    uft_disk_init();
    
    uft_disk_t *disk = NULL;
    uft_error_t err;
    
    if (isdigit((unsigned char)argv[1][0]) && strlen(argv[1]) <= 2) {
        err = uft_disk_open_drive(atoi(argv[1]), UFT_ACCESS_READ, &disk);
    } else {
        err = uft_disk_open_image(argv[1], UFT_ACCESS_READ, &disk);
    }
    
    if (err != UFT_OK) {
        fprintf(stderr, "Error: %s\n", uft_disk_error_string(err));
        uft_disk_cleanup();
        return 1;
    }
    
    uft_fat12_t *vol = NULL;
    err = uft_fat12_mount(disk, &vol);
    
    if (err == UFT_OK) {
        list_directory(vol);
        uft_fat12_unmount(vol);
    } else {
        fprintf(stderr, "Error mounting FAT12\n");
    }
    
    uft_disk_close(disk);
    uft_disk_cleanup();
    return 0;
}
