/**
 * @file uft_sat_parser_v3.c
 * @brief GOD MODE SAT Parser v3 - Sega Saturn
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SAT_HEADER_OFFSET       0x00
#define SAT_MAGIC               "SEGA SEGASATURN "

typedef struct {
    char hardware_id[17];
    char maker_id[17];
    char product_num[11];
    char version[7];
    char release_date[9];
    char device_info[17];
    char area_codes[11];
    char game_title[113];
    size_t source_size;
    bool valid;
} sat_disc_t;

static bool sat_parse(const uint8_t* data, size_t size, sat_disc_t* disc) {
    if (!data || !disc || size < 0x100) return false;
    memset(disc, 0, sizeof(sat_disc_t));
    disc->source_size = size;
    
    memcpy(disc->hardware_id, data, 16); disc->hardware_id[16] = '\0';
    memcpy(disc->maker_id, data + 0x10, 16); disc->maker_id[16] = '\0';
    memcpy(disc->product_num, data + 0x20, 10); disc->product_num[10] = '\0';
    memcpy(disc->version, data + 0x2A, 6); disc->version[6] = '\0';
    memcpy(disc->release_date, data + 0x30, 8); disc->release_date[8] = '\0';
    memcpy(disc->device_info, data + 0x38, 16); disc->device_info[16] = '\0';
    memcpy(disc->area_codes, data + 0x40, 10); disc->area_codes[10] = '\0';
    memcpy(disc->game_title, data + 0x60, 112); disc->game_title[112] = '\0';
    
    disc->valid = (memcmp(disc->hardware_id, SAT_MAGIC, 16) == 0);
    return true;
}

#ifdef SAT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sega Saturn Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t sat[256] = {0};
    memcpy(sat, "SEGA SEGASATURN ", 16);
    memcpy(sat + 0x60, "TEST GAME", 9);
    sat_disc_t disc;
    assert(sat_parse(sat, sizeof(sat), &disc) && disc.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
