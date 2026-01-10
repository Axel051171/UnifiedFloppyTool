/**
 * @file uft_vbm_parser_v3.c
 * @brief GOD MODE VBM Parser v3 - VisualBoyAdvance Movie
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VBM_MAGIC               0x1A4D4256  /* "VBM\x1A" */

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t uid;
    uint32_t frame_count;
    uint32_t rerecord_count;
    uint8_t start_flags;
    uint8_t controller_flags;
    uint8_t system_flags;
    size_t source_size;
    bool valid;
} vbm_file_t;

static uint32_t vbm_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool vbm_parse(const uint8_t* data, size_t size, vbm_file_t* vbm) {
    if (!data || !vbm || size < 64) return false;
    memset(vbm, 0, sizeof(vbm_file_t));
    vbm->source_size = size;
    
    vbm->signature = vbm_read_le32(data);
    if (vbm->signature == VBM_MAGIC) {
        vbm->version = vbm_read_le32(data + 4);
        vbm->uid = vbm_read_le32(data + 8);
        vbm->frame_count = vbm_read_le32(data + 12);
        vbm->rerecord_count = vbm_read_le32(data + 16);
        vbm->start_flags = data[20];
        vbm->controller_flags = data[21];
        vbm->system_flags = data[22];
        vbm->valid = true;
    }
    return true;
}

#ifdef VBM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== VBM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t vbm[64] = {'V', 'B', 'M', 0x1A};
    vbm_file_t file;
    assert(vbm_parse(vbm, sizeof(vbm), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
