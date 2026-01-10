/**
 * @file uft_udi_parser_v3.c
 * @brief GOD MODE UDI Parser v3 - Ultra Disk Image
 * 
 * ZX Spectrum .udi format with precise timing
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UDI_MAGIC               "UDI!"
#define UDI_FOOTER              "UDI!"

typedef struct {
    char signature[5];
    uint32_t file_size;
    uint8_t version;
    uint8_t cylinders;
    uint8_t sides;
    uint8_t unused;
    uint32_t extended_header_size;
    size_t source_size;
    bool valid;
} udi_file_t;

static uint32_t udi_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool udi_parse(const uint8_t* data, size_t size, udi_file_t* udi) {
    if (!data || !udi || size < 16) return false;
    memset(udi, 0, sizeof(udi_file_t));
    udi->source_size = size;
    
    if (memcmp(data, UDI_MAGIC, 4) == 0) {
        memcpy(udi->signature, data, 4);
        udi->signature[4] = '\0';
        udi->file_size = udi_read_le32(data + 4);
        udi->version = data[8];
        udi->cylinders = data[9];
        udi->sides = data[10];
        udi->extended_header_size = udi_read_le32(data + 12);
        udi->valid = true;
    }
    return true;
}

#ifdef UDI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== UDI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t udi[32] = {'U', 'D', 'I', '!', 0, 0, 0, 0, 0, 80, 2};
    udi_file_t file;
    assert(udi_parse(udi, sizeof(udi), &file) && file.cylinders == 80);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
