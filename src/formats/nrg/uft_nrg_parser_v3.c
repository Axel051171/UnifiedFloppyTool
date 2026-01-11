/**
 * @file uft_nrg_parser_v3.c
 * @brief GOD MODE NRG Parser v3 - Nero Burning ROM Image
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NRG_MAGIC_V1            "NERO"
#define NRG_MAGIC_V2            "NER5"

typedef struct {
    uint8_t version;
    uint64_t footer_offset;
    size_t source_size;
    bool valid;
} nrg_file_t;

static bool nrg_parse(const uint8_t* data, size_t size, nrg_file_t* nrg) {
    if (!data || !nrg || size < 12) return false;
    memset(nrg, 0, sizeof(nrg_file_t));
    nrg->source_size = size;
    
    /* Footer at end of file */
    const uint8_t* footer = data + size - 12;
    
    if (memcmp(footer + 8, NRG_MAGIC_V2, 4) == 0) {
        nrg->version = 2;
        nrg->footer_offset = ((uint64_t)footer[0] << 56) | ((uint64_t)footer[1] << 48) |
                             ((uint64_t)footer[2] << 40) | ((uint64_t)footer[3] << 32) |
                             ((uint64_t)footer[4] << 24) | ((uint64_t)footer[5] << 16) |
                             ((uint64_t)footer[6] << 8) | footer[7];
        nrg->valid = true;
    } else if (size >= 8 && memcmp(data + size - 8, NRG_MAGIC_V1, 4) == 0) {
        nrg->version = 1;
        nrg->valid = true;
    }
    return true;
}

#ifdef NRG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nero NRG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t nrg[256] = {0};
    memcpy(nrg + 256 - 4, "NER5", 4);
    nrg_file_t file;
    assert(nrg_parse(nrg, sizeof(nrg), &file) && file.version == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
