/**
 * @file uft_cso_parser_v3.c
 * @brief GOD MODE CSO Parser v3 - Compressed ISO
 * 
 * CISO format for PSP/PS2
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CSO_MAGIC               "CISO"
#define CSO_HEADER_SIZE         24

typedef struct {
    char signature[5];
    uint32_t header_size;
    uint64_t total_bytes;
    uint32_t block_size;
    uint8_t version;
    uint8_t align;
    size_t source_size;
    bool valid;
} cso_file_t;

static uint32_t cso_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool cso_parse(const uint8_t* data, size_t size, cso_file_t* cso) {
    if (!data || !cso || size < CSO_HEADER_SIZE) return false;
    memset(cso, 0, sizeof(cso_file_t));
    cso->source_size = size;
    
    /* Check CSO magic */
    if (memcmp(data, CSO_MAGIC, 4) == 0) {
        memcpy(cso->signature, data, 4);
        cso->signature[4] = '\0';
        cso->header_size = cso_read_le32(data + 4);
        cso->total_bytes = cso_read_le32(data + 8);  /* Low 32 bits */
        cso->block_size = cso_read_le32(data + 16);
        cso->version = data[20];
        cso->align = data[21];
        cso->valid = true;
    }
    return true;
}

#ifdef CSO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CSO Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t cso[32] = {'C', 'I', 'S', 'O'};
    cso[16] = 0x00; cso[17] = 0x08;  /* Block size 2048 */
    cso_file_t file;
    assert(cso_parse(cso, sizeof(cso), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
