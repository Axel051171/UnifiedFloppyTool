/**
 * @file uft_ps2_parser_v3.c
 * @brief GOD MODE PS2 Parser v3 - Sony PlayStation 2
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PS2_MAGIC               "PLAYSTATION"

typedef struct {
    char system_id[33];
    char volume_id[33];
    bool is_dvd;
    size_t source_size;
    bool valid;
} ps2_disc_t;

static bool ps2_parse(const uint8_t* data, size_t size, ps2_disc_t* disc) {
    if (!data || !disc || size < 0x10000) return false;
    memset(disc, 0, sizeof(ps2_disc_t));
    disc->source_size = size;
    
    const uint8_t* pvd = data + 16 * 2048;
    if (pvd[0] == 0x01 && memcmp(pvd + 1, "CD001", 5) == 0) {
        memcpy(disc->system_id, pvd + 8, 32); disc->system_id[32] = '\0';
        memcpy(disc->volume_id, pvd + 40, 32); disc->volume_id[32] = '\0';
        disc->is_dvd = (size > 700 * 1024 * 1024);
        disc->valid = (strstr(disc->system_id, "PLAYSTATION") != NULL);
    }
    return true;
}

#ifdef PS2_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PlayStation 2 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* ps2 = calloc(1, 0x20000);
    uint8_t* pvd = ps2 + 16 * 2048;
    pvd[0] = 0x01; memcpy(pvd + 1, "CD001", 5);
    memcpy(pvd + 8, "PLAYSTATION", 11);
    ps2_disc_t disc;
    assert(ps2_parse(ps2, 0x20000, &disc) && disc.valid);
    free(ps2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
