/**
 * @file uft_xm_parser_v3.c
 * @brief GOD MODE XM Parser v3 - FastTracker II Module
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XM_MAGIC                "Extended Module: "

typedef struct {
    char signature[18];
    char title[21];
    uint16_t version;
    uint32_t header_size;
    uint16_t song_length;
    uint16_t restart_position;
    uint16_t num_channels;
    uint16_t num_patterns;
    uint16_t num_instruments;
    uint16_t flags;
    uint16_t default_tempo;
    uint16_t default_bpm;
    size_t source_size;
    bool valid;
} xm_file_t;

static bool xm_parse(const uint8_t* data, size_t size, xm_file_t* xm) {
    if (!data || !xm || size < 80) return false;
    memset(xm, 0, sizeof(xm_file_t));
    xm->source_size = size;
    
    if (memcmp(data, XM_MAGIC, 17) == 0) {
        memcpy(xm->signature, data, 17);
        xm->signature[17] = '\0';
        memcpy(xm->title, data + 17, 20);
        xm->title[20] = '\0';
        xm->version = data[58] | (data[59] << 8);
        xm->header_size = data[60] | (data[61] << 8) | ((uint32_t)data[62] << 16) | ((uint32_t)data[63] << 24);
        xm->song_length = data[64] | (data[65] << 8);
        xm->num_channels = data[68] | (data[69] << 8);
        xm->num_patterns = data[70] | (data[71] << 8);
        xm->num_instruments = data[72] | (data[73] << 8);
        xm->valid = true;
    }
    return true;
}

#ifdef XM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== XM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t xm[80] = {0};
    memcpy(xm, "Extended Module: ", 17);
    xm[68] = 8;  /* 8 channels */
    xm_file_t file;
    assert(xm_parse(xm, sizeof(xm), &file) && file.num_channels == 8);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
