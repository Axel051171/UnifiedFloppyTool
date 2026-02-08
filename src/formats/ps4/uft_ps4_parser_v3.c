/**
 * @file uft_ps4_parser_v3.c
 * @brief GOD MODE PS4 Parser v3 - PlayStation 4 Package
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PS4_PKG_MAGIC           0x7F434E54  /* "\x7FCNT" */

typedef struct {
    uint32_t magic;
    uint32_t pkg_type;
    uint32_t pkg_revision;
    uint32_t pkg_size_hi;
    uint32_t pkg_size_lo;
    uint32_t entry_count;
    uint16_t entry_count2;
    uint16_t table_offset;
    char content_id[48];
    size_t source_size;
    bool valid;
} ps4_pkg_t;

static uint32_t ps4_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool ps4_parse(const uint8_t* data, size_t size, ps4_pkg_t* ps4) {
    if (!data || !ps4 || size < 128) return false;
    memset(ps4, 0, sizeof(ps4_pkg_t));
    ps4->source_size = size;
    
    ps4->magic = ps4_read_le32(data);
    /* PS4 PKG magic in little endian */
    if (data[0] == 0x7F && data[1] == 'C' && data[2] == 'N' && data[3] == 'T') {
        ps4->pkg_type = ps4_read_le32(data + 4);
        ps4->pkg_revision = ps4_read_le32(data + 8);
        ps4->entry_count = ps4_read_le32(data + 16);
        memcpy(ps4->content_id, data + 64, 36);
        ps4->content_id[36] = '\0';
        ps4->valid = true;
    }
    return true;
}

#ifdef PS4_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PS4 PKG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ps4[128] = {0x7F, 'C', 'N', 'T', 0, 0, 0, 0, 1, 0, 0, 0};
    ps4_pkg_t file;
    assert(ps4_parse(ps4, sizeof(ps4), &file) && file.pkg_revision == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
