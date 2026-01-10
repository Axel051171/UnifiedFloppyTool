/**
 * @file uft_scd_parser_v3.c
 * @brief GOD MODE SCD Parser v3 - Sega CD/Mega CD
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define SCD_MAGIC               "SEGADISCSYSTEM  "

typedef struct {
    char system_id[17];
    char volume_id[12];
    uint16_t volume_version;
    char copyright[17];
    char title_domestic[49];
    char title_overseas[49];
    size_t source_size;
    bool valid;
} scd_disc_t;

static bool scd_parse(const uint8_t* data, size_t size, scd_disc_t* disc) {
    if (!data || !disc || size < 0x200) return false;
    memset(disc, 0, sizeof(scd_disc_t));
    disc->source_size = size;
    
    memcpy(disc->system_id, data, 16); disc->system_id[16] = '\0';
    memcpy(disc->volume_id, data + 0x10, 11); disc->volume_id[11] = '\0';
    disc->volume_version = (data[0x1C] << 8) | data[0x1D];
    memcpy(disc->copyright, data + 0x20, 16); disc->copyright[16] = '\0';
    memcpy(disc->title_domestic, data + 0x30, 48); disc->title_domestic[48] = '\0';
    memcpy(disc->title_overseas, data + 0x60, 48); disc->title_overseas[48] = '\0';
    
    disc->valid = (memcmp(disc->system_id, SCD_MAGIC, 16) == 0);
    return true;
}

#ifdef SCD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Sega CD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t scd[0x200] = {0};
    memcpy(scd, "SEGADISCSYSTEM  ", 16);
    memcpy(scd + 0x30, "TEST GAME", 9);
    scd_disc_t disc;
    assert(scd_parse(scd, sizeof(scd), &disc) && disc.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
