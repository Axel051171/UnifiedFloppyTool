/**
 * @file uft_dsk_agt_parser_v3.c
 * @brief GOD MODE DSK_AGT Parser v3 - Soviet Agat Disk Format
 * 
 * Soviet Agat-7/Agat-9:
 * - Apple II clone
 * - Modified DOS 3.3
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define AGT_SIZE_140K           (35 * 16 * 256)
#define AGT_SIZE_840K           (80 * 2 * 21 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    bool is_140;  /* Apple II compat vs native */
    size_t source_size;
    bool valid;
} agt_disk_t;

static bool agt_parse(const uint8_t* data, size_t size, agt_disk_t* disk) {
    if (!data || !disk || size < AGT_SIZE_140K) return false;
    memset(disk, 0, sizeof(agt_disk_t));
    disk->source_size = size;
    disk->sector_size = 256;
    
    if (size == AGT_SIZE_140K) {
        disk->is_140 = true;
        disk->tracks = 35; disk->sides = 1; disk->sectors = 16;
    } else {
        disk->is_140 = false;
        disk->tracks = 80; disk->sides = 2; disk->sectors = 21;
    }
    disk->valid = true;
    return true;
}

#ifdef DSK_AGT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Soviet Agat Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* agt = calloc(1, AGT_SIZE_140K);
    agt_disk_t disk;
    assert(agt_parse(agt, AGT_SIZE_140K, &disk) && disk.is_140);
    free(agt);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
