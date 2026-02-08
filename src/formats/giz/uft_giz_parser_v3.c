/**
 * @file uft_giz_parser_v3.c
 * @brief GOD MODE GIZ Parser v3 - Tiger Gizmondo
 * 
 * Windows CE-based handheld
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PE_MAGIC                "MZ"
#define PE_SIG_OFFSET           0x3C

typedef struct {
    bool is_pe;
    bool is_arm;
    uint32_t entry_point;
    size_t source_size;
    bool valid;
} giz_file_t;

static uint32_t giz_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool giz_parse(const uint8_t* data, size_t size, giz_file_t* giz) {
    if (!data || !giz || size < 64) return false;
    memset(giz, 0, sizeof(giz_file_t));
    giz->source_size = size;
    
    /* Check for PE executable */
    if (memcmp(data, PE_MAGIC, 2) == 0) {
        giz->is_pe = true;
        uint32_t pe_offset = giz_read_le32(data + PE_SIG_OFFSET);
        if (pe_offset + 6 < size) {
            uint16_t machine = data[pe_offset + 4] | (data[pe_offset + 5] << 8);
            giz->is_arm = (machine == 0x01C0 || machine == 0x01C2);  /* ARM */
        }
    }
    
    giz->valid = giz->is_pe;
    return true;
}

#ifdef GIZ_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Gizmondo Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t giz[256] = {'M', 'Z'};
    giz[0x3C] = 0x80;
    giz[0x80] = 'P'; giz[0x81] = 'E';
    giz[0x84] = 0xC0; giz[0x85] = 0x01;  /* ARM */
    giz_file_t file;
    assert(giz_parse(giz, sizeof(giz), &file) && file.is_pe);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
