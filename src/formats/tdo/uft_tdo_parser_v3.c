/**
 * @file uft_3do_parser_v3.c
 * @brief GOD MODE 3DO Parser v3 - 3DO Interactive Multiplayer
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TDO_SIGNATURE           0x0100
#define TDO_BLOCK_SIZE          2048

typedef struct {
    uint32_t volume_flags;
    char volume_label[33];
    uint32_t root_dir_id;
    uint32_t root_dir_blocks;
    uint32_t root_dir_size;
    size_t source_size;
    bool valid;
} tdo_disc_t;

static uint32_t tdo_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool tdo_parse(const uint8_t* data, size_t size, tdo_disc_t* disc) {
    if (!data || !disc || size < TDO_BLOCK_SIZE) return false;
    memset(disc, 0, sizeof(tdo_disc_t));
    disc->source_size = size;
    
    /* Volume header at block 0 */
    disc->volume_flags = tdo_read_be32(data + 0x00);
    memcpy(disc->volume_label, data + 0x28, 32);
    disc->volume_label[32] = '\0';
    disc->root_dir_id = tdo_read_be32(data + 0x64);
    
    disc->valid = (disc->volume_flags != 0);
    return true;
}

#ifdef TDO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== 3DO Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* tdo = calloc(1, TDO_BLOCK_SIZE);
    tdo[0] = 0x01;  /* Volume flags */
    memcpy(tdo + 0x28, "TEST3DO", 7);
    tdo_disc_t disc;
    assert(tdo_parse(tdo, TDO_BLOCK_SIZE, &disc) && disc.valid);
    free(tdo);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
