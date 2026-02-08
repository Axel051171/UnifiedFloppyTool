/**
 * @file uft_xbe_parser_v3.c
 * @brief GOD MODE XBE Parser v3 - Microsoft Xbox Executable
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XBE_MAGIC               "XBEH"
#define XBE_HEADER_SIZE         0x1000

typedef struct {
    char magic[5];
    uint32_t base_address;
    uint32_t headers_size;
    uint32_t image_size;
    uint32_t entry_point;
    uint32_t tls_address;
    char title_name[41];
    uint32_t cert_timestamp;
    uint32_t title_id;
    size_t source_size;
    bool valid;
} xbe_file_t;

static uint32_t xbe_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool xbe_parse(const uint8_t* data, size_t size, xbe_file_t* xbe) {
    if (!data || !xbe || size < XBE_HEADER_SIZE) return false;
    memset(xbe, 0, sizeof(xbe_file_t));
    xbe->source_size = size;
    
    memcpy(xbe->magic, data, 4); xbe->magic[4] = '\0';
    xbe->base_address = xbe_read_le32(data + 0x104);
    xbe->headers_size = xbe_read_le32(data + 0x108);
    xbe->image_size = xbe_read_le32(data + 0x10C);
    xbe->entry_point = xbe_read_le32(data + 0x128);
    
    xbe->valid = (memcmp(xbe->magic, XBE_MAGIC, 4) == 0);
    return true;
}

#ifdef XBE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Xbox XBE Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* xbe = calloc(1, XBE_HEADER_SIZE);
    memcpy(xbe, "XBEH", 4);
    xbe_file_t file;
    assert(xbe_parse(xbe, XBE_HEADER_SIZE, &file) && file.valid);
    free(xbe);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
