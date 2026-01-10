/**
 * @file uft_dsk_smc_parser_v3.c
 * @brief GOD MODE DSK_SMC Parser v3 - Sony SMC-70/SMC-777 Disk Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SMC_SIZE_280K           (70 * 16 * 256)
#define SMC_SIZE_560K           (70 * 2 * 16 * 256)

typedef struct {
    uint8_t tracks, sides, sectors;
    uint16_t sector_size;
    size_t source_size;
    bool valid;
} smc_disk_t;

static bool smc_parse(const uint8_t* data, size_t size, smc_disk_t* disk) {
    if (!data || !disk || size < SMC_SIZE_280K) return false;
    memset(disk, 0, sizeof(smc_disk_t));
    disk->source_size = size;
    disk->tracks = 70; disk->sectors = 16; disk->sector_size = 256;
    disk->sides = (size >= SMC_SIZE_560K) ? 2 : 1;
    disk->valid = true;
    return true;
}

#ifdef DSK_SMC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sony SMC Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* smc = calloc(1, SMC_SIZE_280K);
    smc_disk_t disk;
    assert(smc_parse(smc, SMC_SIZE_280K, &disk) && disk.valid);
    free(smc);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
