/**
 * @file uft_dsk_hp_parser_v3.c
 * @brief GOD MODE DSK_HP Parser v3 - HP 9114/9121/LIF Disk Format
 * 
 * HP LIF (Logical Interchange Format):
 * - HP 9114, HP 9121
 * - LIF0, LIF1 Filesystem
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define HP_LIF_SIGNATURE        0x8000
#define HP_SIZE_264K            (77 * 2 * 256)
#define HP_SIZE_630K            (77 * 2 * 16 * 256)

typedef struct {
    uint16_t magic;
    char volume_label[7];
    uint32_t directory_start;
    uint16_t lif_version;
    uint16_t tracks;
    uint16_t surfaces;
    uint16_t blocks_per_track;
    size_t source_size;
    bool valid;
} hp_disk_t;

static uint16_t hp_read_be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }
static uint32_t hp_read_be32(const uint8_t* p) { return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]; }

static bool hp_parse(const uint8_t* data, size_t size, hp_disk_t* disk) {
    if (!data || !disk || size < 256) return false;
    memset(disk, 0, sizeof(hp_disk_t));
    disk->source_size = size;
    
    disk->magic = hp_read_be16(data);
    memcpy(disk->volume_label, data + 2, 6);
    disk->volume_label[6] = '\0';
    disk->directory_start = hp_read_be32(data + 8);
    disk->lif_version = hp_read_be16(data + 14);
    disk->tracks = hp_read_be16(data + 24);
    disk->surfaces = hp_read_be16(data + 26);
    disk->blocks_per_track = hp_read_be16(data + 28);
    
    disk->valid = (disk->magic == HP_LIF_SIGNATURE);
    return disk->valid || (size > 0);  /* Accept even without LIF header */
}

#ifdef DSK_HP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== HP LIF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t hp[512] = {0};
    hp[0] = 0x80; hp[1] = 0x00;  /* LIF signature */
    memcpy(hp + 2, "HPTEST", 6);
    hp_disk_t disk;
    assert(hp_parse(hp, sizeof(hp), &disk) && disk.magic == 0x8000);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
