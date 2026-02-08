/**
 * @file uft_nge_parser_v3.c
 * @brief GOD MODE NGE Parser v3 - Nokia N-Gage
 * 
 * Symbian S60 based game format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NGE_SIS_MAGIC           0x10000419

typedef struct {
    uint32_t uid1;
    uint32_t uid2;
    uint32_t uid3;
    bool is_sis;
    size_t source_size;
    bool valid;
} nge_file_t;

static uint32_t nge_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool nge_parse(const uint8_t* data, size_t size, nge_file_t* nge) {
    if (!data || !nge || size < 16) return false;
    memset(nge, 0, sizeof(nge_file_t));
    nge->source_size = size;
    
    nge->uid1 = nge_read_le32(data);
    nge->uid2 = nge_read_le32(data + 4);
    nge->uid3 = nge_read_le32(data + 8);
    
    nge->is_sis = (nge->uid1 == NGE_SIS_MAGIC);
    nge->valid = nge->is_sis;
    return true;
}

#ifdef NGE_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== N-Gage Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nge[16] = {0x19, 0x04, 0x00, 0x10, 0, 0, 0, 0};
    nge_file_t file;
    assert(nge_parse(nge, sizeof(nge), &file) && file.is_sis);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
