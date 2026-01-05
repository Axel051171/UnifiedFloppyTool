/**
 * @file uft_wii_parser_v3.c
 * @brief GOD MODE WII Parser v3 - Nintendo Wii Disc
 * 
 * Supports WBFS and ISO formats
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define WII_MAGIC               0x5D1C9EA3
#define WBFS_MAGIC              "WBFS"

typedef struct {
    char game_code[7];
    char maker_code[3];
    uint8_t disc_id;
    uint8_t version;
    char game_name[65];
    bool is_wbfs;
    bool is_wii;
    size_t source_size;
    bool valid;
} wii_disc_t;

static uint32_t wii_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool wii_parse(const uint8_t* data, size_t size, wii_disc_t* wii) {
    if (!data || !wii || size < 0x100) return false;
    memset(wii, 0, sizeof(wii_disc_t));
    wii->source_size = size;
    
    /* Check for WBFS */
    if (memcmp(data, WBFS_MAGIC, 4) == 0) {
        wii->is_wbfs = true;
        wii->valid = true;
        return true;
    }
    
    /* Check Wii magic at 0x18 */
    uint32_t magic = wii_read_be32(data + 0x18);
    wii->is_wii = (magic == WII_MAGIC);
    
    if (wii->is_wii) {
        memcpy(wii->game_code, data, 6); wii->game_code[6] = '\0';
        memcpy(wii->maker_code, data + 4, 2); wii->maker_code[2] = '\0';
        wii->disc_id = data[6];
        wii->version = data[7];
        memcpy(wii->game_name, data + 0x20, 64); wii->game_name[64] = '\0';
    }
    
    wii->valid = wii->is_wii || wii->is_wbfs;
    return true;
}

#ifdef WII_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Wii Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* wii = calloc(1, 0x100);
    wii[0x18] = 0x5D; wii[0x19] = 0x1C; wii[0x1A] = 0x9E; wii[0x1B] = 0xA3;
    memcpy(wii, "RTEST", 5);
    wii_disc_t disc;
    assert(wii_parse(wii, 0x100, &disc) && disc.is_wii);
    free(wii);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
