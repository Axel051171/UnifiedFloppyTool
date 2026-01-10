/**
 * @file uft_dtm_parser_v3.c
 * @brief GOD MODE DTM Parser v3 - Dolphin TAS Movie
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DTM_MAGIC               "DTM\x1A"

typedef struct {
    char signature[5];
    char game_id[7];
    bool is_wii;
    uint8_t controllers;
    bool from_save_state;
    uint64_t vi_count;
    uint64_t input_count;
    uint64_t rerecords;
    size_t source_size;
    bool valid;
} dtm_file_t;

static bool dtm_parse(const uint8_t* data, size_t size, dtm_file_t* dtm) {
    if (!data || !dtm || size < 256) return false;
    memset(dtm, 0, sizeof(dtm_file_t));
    dtm->source_size = size;
    
    if (memcmp(data, DTM_MAGIC, 4) == 0) {
        memcpy(dtm->signature, data, 4);
        dtm->signature[4] = '\0';
        memcpy(dtm->game_id, data + 4, 6);
        dtm->game_id[6] = '\0';
        dtm->is_wii = data[10];
        dtm->controllers = data[11];
        dtm->from_save_state = data[12];
        dtm->valid = true;
    }
    return true;
}

#ifdef DTM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DTM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* dtm = calloc(1, 256);
    memcpy(dtm, "DTM\x1A", 4);
    memcpy(dtm + 4, "GZLE01", 6);
    dtm_file_t file;
    assert(dtm_parse(dtm, 256, &file) && file.valid);
    free(dtm);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
