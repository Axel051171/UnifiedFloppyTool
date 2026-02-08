/**
 * @file uft_zod_parser_v3.c
 * @brief GOD MODE ZOD Parser v3 - Tapwave Zodiac
 * 
 * Palm OS-based gaming handheld
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PRC_MAGIC               "appl"
#define PDB_TYPE                0x6170706C

typedef struct {
    char name[33];
    uint16_t attributes;
    uint16_t version;
    uint32_t type;
    uint32_t creator;
    bool is_prc;
    size_t source_size;
    bool valid;
} zod_file_t;

static bool zod_parse(const uint8_t* data, size_t size, zod_file_t* zod) {
    if (!data || !zod || size < 78) return false;
    memset(zod, 0, sizeof(zod_file_t));
    zod->source_size = size;
    
    /* Palm database header */
    memcpy(zod->name, data, 32); zod->name[32] = '\0';
    zod->attributes = (data[32] << 8) | data[33];
    zod->version = (data[34] << 8) | data[35];
    zod->type = ((uint32_t)data[60] << 24) | ((uint32_t)data[61] << 16) | (data[62] << 8) | data[63];
    zod->creator = ((uint32_t)data[64] << 24) | ((uint32_t)data[65] << 16) | (data[66] << 8) | data[67];
    
    zod->is_prc = (zod->type == PDB_TYPE);
    zod->valid = true;
    return true;
}

#ifdef ZOD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Tapwave Zodiac Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t zod[78] = {0};
    memcpy(zod, "TestApp", 7);
    zod_file_t file;
    assert(zod_parse(zod, sizeof(zod), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
