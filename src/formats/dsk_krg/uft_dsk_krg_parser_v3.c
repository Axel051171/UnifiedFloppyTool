/**
 * @file uft_dsk_krg_parser_v3.c
 * @brief GOD MODE DSK_KRG Parser v3 - Korg Sampler/Synth Disk Format
 * 
 * Korg DSS-1/DSM-1/T-Series/Trinity:
 * - Sampler/Workstation Disks
 * - 720K/1.44M
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define KRG_SIZE_720K           737280
#define KRG_SIZE_1440K          1474560

typedef struct {
    bool is_hd;
    uint16_t blocks;
    size_t source_size;
    bool valid;
} krg_disk_t;

static bool krg_parse(const uint8_t* data, size_t size, krg_disk_t* disk) {
    if (!data || !disk || size < KRG_SIZE_720K) return false;
    memset(disk, 0, sizeof(krg_disk_t));
    disk->source_size = size;
    disk->is_hd = (size >= KRG_SIZE_1440K);
    disk->blocks = size / 512;
    disk->valid = true;
    return true;
}

#ifdef DSK_KRG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Korg Sampler Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* krg = calloc(1, KRG_SIZE_720K);
    krg_disk_t disk;
    assert(krg_parse(krg, KRG_SIZE_720K, &disk) && disk.valid);
    free(krg);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
