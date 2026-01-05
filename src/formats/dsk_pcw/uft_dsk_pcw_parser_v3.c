/**
 * @file uft_dsk_pcw_parser_v3.c
 * @brief GOD MODE DSK_PCW Parser v3 - Amstrad PCW Disk Format
 * 
 * Amstrad PCW Disk Format:
 * - CF2: 180K Single-Sided
 * - CF2DD: 720K Double-Sided
 * - CP/M Plus kompatibel
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PCW_CF2_SIZE            (40 * 9 * 512)    /* 180K */
#define PCW_CF2DD_SIZE          (80 * 2 * 9 * 512) /* 720K */

typedef enum { PCW_CF2 = 1, PCW_CF2DD = 2 } pcw_type_t;

typedef struct {
    pcw_type_t type;
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} pcw_disk_t;

static bool pcw_parse(const uint8_t* data, size_t size, pcw_disk_t* disk) {
    if (!data || !disk || size < PCW_CF2_SIZE) return false;
    memset(disk, 0, sizeof(pcw_disk_t));
    disk->source_size = size;
    
    if (size >= PCW_CF2DD_SIZE) {
        disk->type = PCW_CF2DD;
        disk->tracks = 80; disk->sides = 2;
    } else {
        disk->type = PCW_CF2;
        disk->tracks = 40; disk->sides = 1;
    }
    disk->sectors = 9;
    disk->sector_size = 512;
    disk->valid = true;
    return true;
}

#ifdef DSK_PCW_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Amstrad PCW Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* pcw = calloc(1, PCW_CF2DD_SIZE);
    pcw_disk_t disk;
    assert(pcw_parse(pcw, PCW_CF2DD_SIZE, &disk) && disk.type == PCW_CF2DD);
    free(pcw);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
