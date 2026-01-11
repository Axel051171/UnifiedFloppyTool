/**
 * @file uft_snk_parser_v3.c
 * @brief GOD MODE SNK Parser v3 - Neo Geo Pocket/Color
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NGP_LICENSE             "COPYRIGHT BY SNK"

typedef struct {
    char license[17];
    uint32_t startup_address;
    uint16_t catalog_number;
    uint8_t sub_catalog;
    uint8_t mode;            /* 0=NGP, 1=NGPC */
    char game_name[13];
    size_t source_size;
    bool valid;
} snk_rom_t;

static bool snk_parse(const uint8_t* data, size_t size, snk_rom_t* snk) {
    if (!data || !snk || size < 64) return false;
    memset(snk, 0, sizeof(snk_rom_t));
    snk->source_size = size;
    
    memcpy(snk->license, data, 16);
    snk->license[16] = '\0';
    
    if (memcmp(snk->license, NGP_LICENSE, 16) == 0) {
        snk->startup_address = data[16] | (data[17] << 8) | (data[18] << 16) | (data[19] << 24);
        snk->catalog_number = data[20] | (data[21] << 8);
        snk->sub_catalog = data[22];
        snk->mode = data[23];
        memcpy(snk->game_name, data + 36, 12);
        snk->game_name[12] = '\0';
        snk->valid = true;
    }
    return true;
}

#ifdef SNK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Neo Geo Pocket Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t snk[64] = {0};
    memcpy(snk, "COPYRIGHT BY SNK", 16);
    snk[23] = 1;  /* NGPC */
    snk_rom_t rom;
    assert(snk_parse(snk, sizeof(snk), &rom) && rom.mode == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
