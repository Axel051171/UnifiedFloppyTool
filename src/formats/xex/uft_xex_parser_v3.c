/**
 * @file uft_xex_parser_v3.c
 * @brief GOD MODE XEX Parser v3 - Xbox 360 Executable
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XEX_MAGIC               "XEX2"
#define XEX_HEADER_SIZE         0x1000

typedef struct {
    char magic[5];
    uint32_t module_flags;
    uint32_t header_size;
    uint32_t image_size;
    uint32_t load_address;
    size_t source_size;
    bool valid;
} xex_file_t;

static uint32_t xex_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool xex_parse(const uint8_t* data, size_t size, xex_file_t* xex) {
    if (!data || !xex || size < XEX_HEADER_SIZE) return false;
    memset(xex, 0, sizeof(xex_file_t));
    xex->source_size = size;
    
    memcpy(xex->magic, data, 4); xex->magic[4] = '\0';
    xex->module_flags = xex_read_be32(data + 0x04);
    xex->header_size = xex_read_be32(data + 0x08);
    xex->image_size = xex_read_be32(data + 0x10);
    
    xex->valid = (memcmp(xex->magic, XEX_MAGIC, 4) == 0);
    return true;
}

#ifdef XEX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Xbox 360 XEX Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* xex = calloc(1, XEX_HEADER_SIZE);
    memcpy(xex, "XEX2", 4);
    xex_file_t file;
    assert(xex_parse(xex, XEX_HEADER_SIZE, &file) && file.valid);
    free(xex);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
