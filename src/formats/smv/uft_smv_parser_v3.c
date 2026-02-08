/**
 * @file uft_smv_parser_v3.c
 * @brief GOD MODE SMV Parser v3 - Snes9x Movie
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SMV_MAGIC               0x1A564D53  /* "SMV\x1A" */

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t uid;
    uint32_t rerecord_count;
    uint32_t frame_count;
    uint8_t controller_flags;
    uint8_t movie_flags;
    size_t source_size;
    bool valid;
} smv_file_t;

static uint32_t smv_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool smv_parse(const uint8_t* data, size_t size, smv_file_t* smv) {
    if (!data || !smv || size < 32) return false;
    memset(smv, 0, sizeof(smv_file_t));
    smv->source_size = size;
    
    smv->signature = smv_read_le32(data);
    if (smv->signature == SMV_MAGIC) {
        smv->version = smv_read_le32(data + 4);
        smv->uid = smv_read_le32(data + 8);
        smv->rerecord_count = smv_read_le32(data + 12);
        smv->frame_count = smv_read_le32(data + 16);
        smv->controller_flags = data[20];
        smv->movie_flags = data[21];
        smv->valid = true;
    }
    return true;
}

#ifdef SMV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SMV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t smv[64] = {'S', 'M', 'V', 0x1A};
    smv_file_t file;
    assert(smv_parse(smv, sizeof(smv), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
