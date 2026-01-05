/**
 * @file uft_pbp_parser_v3.c
 * @brief GOD MODE PBP Parser v3 - PSP Packaged Executable
 * 
 * EBOOT.PBP format for PSP
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PBP_MAGIC               "\x00PBP"
#define PBP_HEADER_SIZE         0x28

typedef struct {
    char signature[5];
    uint32_t version;
    uint32_t param_offset;
    uint32_t icon0_offset;
    uint32_t icon1_offset;
    uint32_t pic0_offset;
    uint32_t pic1_offset;
    uint32_t snd0_offset;
    uint32_t data_psp_offset;
    uint32_t data_psar_offset;
    size_t source_size;
    bool valid;
} pbp_file_t;

static uint32_t pbp_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool pbp_parse(const uint8_t* data, size_t size, pbp_file_t* pbp) {
    if (!data || !pbp || size < PBP_HEADER_SIZE) return false;
    memset(pbp, 0, sizeof(pbp_file_t));
    pbp->source_size = size;
    
    /* Check PBP magic */
    if (memcmp(data, PBP_MAGIC, 4) == 0) {
        memcpy(pbp->signature, data + 1, 3);
        pbp->signature[3] = '\0';
        pbp->version = pbp_read_le32(data + 4);
        pbp->param_offset = pbp_read_le32(data + 8);
        pbp->icon0_offset = pbp_read_le32(data + 12);
        pbp->icon1_offset = pbp_read_le32(data + 16);
        pbp->pic0_offset = pbp_read_le32(data + 20);
        pbp->pic1_offset = pbp_read_le32(data + 24);
        pbp->snd0_offset = pbp_read_le32(data + 28);
        pbp->data_psp_offset = pbp_read_le32(data + 32);
        pbp->data_psar_offset = pbp_read_le32(data + 36);
        pbp->valid = true;
    }
    return true;
}

#ifdef PBP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PBP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pbp[64] = {0x00, 'P', 'B', 'P'};
    pbp_file_t file;
    assert(pbp_parse(pbp, sizeof(pbp), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
