/**
 * @file uft_ups_parser_v3.c
 * @brief GOD MODE UPS Parser v3 - Universal Patching System
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UPS_MAGIC               "UPS1"

typedef struct {
    char signature[5];
    uint64_t input_size;
    uint64_t output_size;
    uint32_t input_crc;
    uint32_t output_crc;
    uint32_t patch_crc;
    size_t source_size;
    bool valid;
} ups_file_t;

static bool ups_parse(const uint8_t* data, size_t size, ups_file_t* ups) {
    if (!data || !ups || size < 18) return false;
    memset(ups, 0, sizeof(ups_file_t));
    ups->source_size = size;
    
    memcpy(ups->signature, data, 4);
    ups->signature[4] = '\0';
    
    if (memcmp(ups->signature, UPS_MAGIC, 4) == 0) {
        /* CRCs at end of file */
        ups->input_crc = data[size-12] | (data[size-11]<<8) | ((uint32_t)data[size-10] << 16) | ((uint32_t)data[size-9] << 24);
        ups->output_crc = data[size-8] | (data[size-7]<<8) | ((uint32_t)data[size-6] << 16) | ((uint32_t)data[size-5] << 24);
        ups->patch_crc = data[size-4] | (data[size-3]<<8) | ((uint32_t)data[size-2] << 16) | ((uint32_t)data[size-1] << 24);
        ups->valid = true;
    }
    return true;
}

#ifdef UPS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== UPS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ups[32] = {'U', 'P', 'S', '1'};
    ups_file_t file;
    assert(ups_parse(ups, sizeof(ups), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
