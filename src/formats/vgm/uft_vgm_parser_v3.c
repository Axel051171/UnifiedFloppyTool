/**
 * @file uft_vgm_parser_v3.c
 * @brief GOD MODE VGM Parser v3 - Video Game Music
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VGM_MAGIC               "Vgm "

typedef struct {
    char signature[5];
    uint32_t eof_offset;
    uint32_t version;
    uint32_t sn76489_clock;
    uint32_t ym2413_clock;
    uint32_t gd3_offset;
    uint32_t total_samples;
    uint32_t loop_offset;
    uint32_t loop_samples;
    uint32_t rate;
    size_t source_size;
    bool valid;
} vgm_file_t;

static uint32_t vgm_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool vgm_parse(const uint8_t* data, size_t size, vgm_file_t* vgm) {
    if (!data || !vgm || size < 64) return false;
    memset(vgm, 0, sizeof(vgm_file_t));
    vgm->source_size = size;
    
    memcpy(vgm->signature, data, 4);
    vgm->signature[4] = '\0';
    
    if (memcmp(vgm->signature, VGM_MAGIC, 4) == 0) {
        vgm->eof_offset = vgm_read_le32(data + 4);
        vgm->version = vgm_read_le32(data + 8);
        vgm->sn76489_clock = vgm_read_le32(data + 12);
        vgm->ym2413_clock = vgm_read_le32(data + 16);
        vgm->gd3_offset = vgm_read_le32(data + 20);
        vgm->total_samples = vgm_read_le32(data + 24);
        vgm->loop_offset = vgm_read_le32(data + 28);
        vgm->loop_samples = vgm_read_le32(data + 32);
        vgm->rate = vgm_read_le32(data + 36);
        vgm->valid = true;
    }
    return true;
}

#ifdef VGM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== VGM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t vgm[64] = {'V', 'g', 'm', ' '};
    vgm[8] = 0x50; vgm[9] = 0x01;  /* Version 1.50 */
    vgm_file_t file;
    assert(vgm_parse(vgm, sizeof(vgm), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
