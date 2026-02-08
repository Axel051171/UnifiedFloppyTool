/**
 * @file uft_pdi_parser_v3.c
 * @brief GOD MODE PDI Parser v3 - Disk Utility Plus Format
 * 
 * Various preservation disk images
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PDI_MAGIC               "PDI\x00"

typedef struct {
    char signature[4];
    uint16_t version;
    uint8_t cylinders;
    uint8_t heads;
    uint8_t sectors;
    uint16_t bytes_per_sector;
    uint32_t data_offset;
    size_t source_size;
    bool valid;
} pdi_file_t;

static bool pdi_parse(const uint8_t* data, size_t size, pdi_file_t* pdi) {
    if (!data || !pdi || size < 16) return false;
    memset(pdi, 0, sizeof(pdi_file_t));
    pdi->source_size = size;
    
    if (memcmp(data, PDI_MAGIC, 4) == 0) {
        memcpy(pdi->signature, data, 4);
        pdi->version = data[4] | (data[5] << 8);
        pdi->cylinders = data[6];
        pdi->heads = data[7];
        pdi->sectors = data[8];
        pdi->bytes_per_sector = data[9] | (data[10] << 8);
        pdi->valid = true;
    }
    return true;
}

#ifdef PDI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PDI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pdi[32] = {'P', 'D', 'I', 0, 1, 0, 80, 2, 18, 0, 2};
    pdi_file_t file;
    assert(pdi_parse(pdi, sizeof(pdi), &file) && file.cylinders == 80);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
