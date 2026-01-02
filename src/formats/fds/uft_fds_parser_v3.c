/**
 * @file uft_fds_parser_v3.c
 * @brief GOD MODE FDS Parser v3 - Famicom Disk System
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FDS_HEADER_SIZE         16
#define FDS_SIDE_SIZE           65500
#define FDS_MAGIC               "FDS\x1A"
#define FDS_NINTENDO            "*NINTENDO-HVC*"

typedef struct {
    bool has_fwnes_header;
    uint8_t side_count;
    char game_name[4];
    uint8_t game_version;
    uint8_t side_number;
    uint8_t disk_number;
    uint8_t manufacturer;
    size_t source_size;
    bool valid;
} fds_disk_t;

static bool fds_parse(const uint8_t* data, size_t size, fds_disk_t* disk) {
    if (!data || !disk || size < FDS_SIDE_SIZE) return false;
    memset(disk, 0, sizeof(fds_disk_t));
    disk->source_size = size;
    
    /* Check for FDS header (fwNES format) */
    disk->has_fwnes_header = (memcmp(data, FDS_MAGIC, 4) == 0);
    size_t offset = disk->has_fwnes_header ? FDS_HEADER_SIZE : 0;
    
    if (disk->has_fwnes_header) {
        disk->side_count = data[4];
    } else {
        disk->side_count = (size + FDS_SIDE_SIZE - 1) / FDS_SIDE_SIZE;
    }
    
    /* Check Nintendo signature */
    const uint8_t* disk_info = data + offset + 1;
    disk->valid = (memcmp(disk_info, FDS_NINTENDO, 14) == 0);
    
    if (disk->valid) {
        memcpy(disk->game_name, disk_info + 16, 3); disk->game_name[3] = '\0';
        disk->game_version = disk_info[20];
        disk->side_number = disk_info[21];
        disk->disk_number = disk_info[22];
        disk->manufacturer = disk_info[15];
    }
    return true;
}

#ifdef FDS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FDS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* fds = calloc(1, FDS_SIDE_SIZE + 16);
    memcpy(fds, "FDS\x1A", 4);
    fds[4] = 1;  /* 1 side */
    memcpy(fds + 17, "*NINTENDO-HVC*", 14);
    fds_disk_t disk;
    assert(fds_parse(fds, FDS_SIDE_SIZE + 16, &disk) && disk.has_fwnes_header);
    free(fds);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
