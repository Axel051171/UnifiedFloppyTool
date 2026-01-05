/**
 * @file uft_ltm_parser_v3.c
 * @brief GOD MODE LTM Parser v3 - LSNES Movie
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define LTM_MAGIC               "lsmv"

typedef struct {
    char signature[5];
    bool is_zip_container;
    size_t source_size;
    bool valid;
} ltm_file_t;

static bool ltm_parse(const uint8_t* data, size_t size, ltm_file_t* ltm) {
    if (!data || !ltm || size < 4) return false;
    memset(ltm, 0, sizeof(ltm_file_t));
    ltm->source_size = size;
    
    /* LSNES movies are ZIP archives with lsmv marker */
    if (data[0] == 'P' && data[1] == 'K') {
        ltm->is_zip_container = true;
        ltm->valid = true;
    } else if (memcmp(data, LTM_MAGIC, 4) == 0) {
        memcpy(ltm->signature, data, 4);
        ltm->signature[4] = '\0';
        ltm->valid = true;
    }
    return true;
}

#ifdef LTM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== LTM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ltm[16] = {'P', 'K', 0x03, 0x04};
    ltm_file_t file;
    assert(ltm_parse(ltm, sizeof(ltm), &file) && file.is_zip_container);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
