/**
 * @file uft_ps3_parser_v3.c
 * @brief GOD MODE PS3 Parser v3 - PlayStation 3 Package
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PS3_PKG_MAGIC           0x7F504B47  /* "\x7FPKG" */

typedef struct {
    uint32_t magic;
    uint16_t revision;
    uint16_t type;
    uint32_t metadata_offset;
    uint32_t metadata_count;
    uint32_t header_size;
    uint64_t data_size;
    char content_id[48];
    size_t source_size;
    bool valid;
} ps3_pkg_t;

static uint32_t ps3_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static uint16_t ps3_read_be16(const uint8_t* p) {
    return (p[0] << 8) | p[1];
}

static bool ps3_parse(const uint8_t* data, size_t size, ps3_pkg_t* ps3) {
    if (!data || !ps3 || size < 128) return false;
    memset(ps3, 0, sizeof(ps3_pkg_t));
    ps3->source_size = size;
    
    ps3->magic = ps3_read_be32(data);
    if (ps3->magic == PS3_PKG_MAGIC) {
        ps3->revision = ps3_read_be16(data + 4);
        ps3->type = ps3_read_be16(data + 6);
        ps3->metadata_offset = ps3_read_be32(data + 8);
        ps3->metadata_count = ps3_read_be32(data + 12);
        ps3->header_size = ps3_read_be32(data + 16);
        memcpy(ps3->content_id, data + 48, 36);
        ps3->content_id[36] = '\0';
        ps3->valid = true;
    }
    return true;
}

#ifdef PS3_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PS3 PKG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ps3[128] = {0x7F, 'P', 'K', 'G', 0, 1, 0, 1};
    ps3_pkg_t file;
    assert(ps3_parse(ps3, sizeof(ps3), &file) && file.revision == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
