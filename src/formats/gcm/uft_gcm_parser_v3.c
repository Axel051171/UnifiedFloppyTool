/**
 * @file uft_gcm_parser_v3.c
 * @brief GOD MODE GCM Parser v3 - Nintendo GameCube Disc
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GCM_HEADER_SIZE         0x2440
#define GCM_MAGIC               0xC2339F3D

typedef struct {
    char game_code[7];
    char maker_code[3];
    uint8_t disc_id;
    uint8_t version;
    char game_name[65];
    uint32_t dol_offset;
    uint32_t fst_offset;
    uint32_t fst_size;
    size_t source_size;
    bool valid;
} gcm_disc_t;

static uint32_t gcm_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool gcm_parse(const uint8_t* data, size_t size, gcm_disc_t* gcm) {
    if (!data || !gcm || size < GCM_HEADER_SIZE) return false;
    memset(gcm, 0, sizeof(gcm_disc_t));
    gcm->source_size = size;
    
    memcpy(gcm->game_code, data, 6); gcm->game_code[6] = '\0';
    memcpy(gcm->maker_code, data + 4, 2); gcm->maker_code[2] = '\0';
    gcm->disc_id = data[6];
    gcm->version = data[7];
    memcpy(gcm->game_name, data + 0x20, 64); gcm->game_name[64] = '\0';
    
    gcm->dol_offset = gcm_read_be32(data + 0x420);
    gcm->fst_offset = gcm_read_be32(data + 0x424);
    gcm->fst_size = gcm_read_be32(data + 0x428);
    
    /* Check magic at 0x1C */
    uint32_t magic = gcm_read_be32(data + 0x1C);
    gcm->valid = (magic == GCM_MAGIC);
    return true;
}

#ifdef GCM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GameCube GCM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* gcm = calloc(1, GCM_HEADER_SIZE);
    memcpy(gcm, "GTEST", 5);
    gcm[0x1C] = 0xC2; gcm[0x1D] = 0x33; gcm[0x1E] = 0x9F; gcm[0x1F] = 0x3D;
    memcpy(gcm + 0x20, "Test Game", 9);
    gcm_disc_t disc;
    assert(gcm_parse(gcm, GCM_HEADER_SIZE, &disc) && disc.valid);
    free(gcm);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
