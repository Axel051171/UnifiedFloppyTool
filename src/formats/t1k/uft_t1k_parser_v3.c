/**
 * @file uft_t1k_parser_v3.c
 * @brief GOD MODE T1K Parser v3 - Tandy 1000
 * 
 * PC compatible with enhanced graphics/sound
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define T1K_SIZE_360K           (40 * 2 * 9 * 512)
#define T1K_SIZE_720K           (80 * 2 * 9 * 512)

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint16_t sector_size;
    bool is_dos;
    size_t source_size;
    bool valid;
} t1k_disk_t;

static bool t1k_parse(const uint8_t* data, size_t size, t1k_disk_t* disk) {
    if (!data || !disk || size < T1K_SIZE_360K) return false;
    memset(disk, 0, sizeof(t1k_disk_t));
    disk->source_size = size;
    disk->sector_size = 512;
    disk->sectors = 9;
    
    if (size >= T1K_SIZE_720K) {
        disk->tracks = 80; disk->sides = 2;
    } else {
        disk->tracks = 40; disk->sides = 2;
    }
    
    /* Check for DOS boot sector */
    disk->is_dos = (data[0] == 0xEB || data[0] == 0xE9);
    disk->valid = true;
    return true;
}

#ifdef T1K_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Tandy 1000 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* t1k = calloc(1, T1K_SIZE_360K);
    t1k[0] = 0xEB;  /* DOS JMP */
    t1k_disk_t disk;
    assert(t1k_parse(t1k, T1K_SIZE_360K, &disk) && disk.is_dos);
    free(t1k);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
