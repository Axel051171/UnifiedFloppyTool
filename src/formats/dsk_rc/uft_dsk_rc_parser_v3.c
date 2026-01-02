/**
 * @file uft_dsk_rc_parser_v3.c
 * @brief GOD MODE DSK_RC Parser v3 - TRS-80 Color Computer (CoCo) Disk Format
 * 
 * Radio Shack Color Computer:
 * - RS-DOS / OS-9
 * - 35/40/80 Tracks
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RC_SIZE_160K            (35 * 18 * 256)
#define RC_SIZE_180K            (40 * 18 * 256)
#define RC_SIZE_360K            (40 * 2 * 18 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} rc_disk_t;

static bool rc_parse(const uint8_t* data, size_t size, rc_disk_t* disk) {
    if (!data || !disk || size < RC_SIZE_160K) return false;
    memset(disk, 0, sizeof(rc_disk_t));
    disk->source_size = size;
    disk->sectors = 18; disk->sector_size = 256;
    
    if (size >= RC_SIZE_360K) {
        disk->tracks = 40; disk->sides = 2;
    } else if (size >= RC_SIZE_180K) {
        disk->tracks = 40; disk->sides = 1;
    } else {
        disk->tracks = 35; disk->sides = 1;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_RC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TRS-80 CoCo Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* rc = calloc(1, RC_SIZE_160K);
    rc_disk_t disk;
    assert(rc_parse(rc, RC_SIZE_160K, &disk) && disk.tracks == 35);
    free(rc);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
