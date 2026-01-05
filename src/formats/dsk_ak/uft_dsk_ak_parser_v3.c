/**
 * @file uft_dsk_ak_parser_v3.c
 * @brief GOD MODE DSK_AK Parser v3 - Akai S-Series Sampler Disk Format
 * 
 * Akai S900/S950/S1000/S3000:
 * - Sampler Disk Format
 * - 800K DD / 1.6M HD
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define AK_SIZE_800K            819200
#define AK_SIZE_1600K           1638400

typedef struct {
    uint16_t blocks;
    bool is_hd;
    bool is_s1000;  /* S1000/S3000 vs S900/S950 */
    size_t source_size;
    bool valid;
} ak_disk_t;

static bool ak_parse(const uint8_t* data, size_t size, ak_disk_t* disk) {
    if (!data || !disk || size < AK_SIZE_800K) return false;
    memset(disk, 0, sizeof(ak_disk_t));
    disk->source_size = size;
    disk->is_hd = (size >= AK_SIZE_1600K);
    disk->blocks = size / 512;
    /* S1000+ uses different format markers */
    disk->is_s1000 = (data[0] == 0x0A && data[1] == 0x0A);
    disk->valid = true;
    return true;
}

#ifdef DSK_AK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Akai Sampler Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* ak = calloc(1, AK_SIZE_800K);
    ak_disk_t disk;
    assert(ak_parse(ak, AK_SIZE_800K, &disk) && disk.valid);
    free(ak);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
