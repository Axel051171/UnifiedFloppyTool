/**
 * @file uft_psx_parser_v3.c
 * @brief GOD MODE PSX Parser v3 - Sony PlayStation 1
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSX_SECTOR_SIZE         2352
#define PSX_EXE_MAGIC           "PS-X EXE"

typedef struct {
    char system_id[33];
    char volume_id[33];
    char exe_name[13];
    bool is_ings;  /* Is this an executable? */
    size_t source_size;
    bool valid;
} psx_disc_t;

static bool psx_parse(const uint8_t* data, size_t size, psx_disc_t* disc) {
    if (!data || !disc || size < 0x10000) return false;
    memset(disc, 0, sizeof(psx_disc_t));
    disc->source_size = size;
    
    /* Check for PS-X EXE header */
    disc->is_ings = (memcmp(data, PSX_EXE_MAGIC, 8) == 0);
    
    /* Or ISO9660 */
    const uint8_t* pvd = data + 16 * 2048;
    if (pvd[0] == 0x01 && memcmp(pvd + 1, "CD001", 5) == 0) {
        memcpy(disc->system_id, pvd + 8, 32); disc->system_id[32] = '\0';
        memcpy(disc->volume_id, pvd + 40, 32); disc->volume_id[32] = '\0';
        disc->valid = true;
    } else if (disc->is_ings) {
        disc->valid = true;
    }
    return true;
}

#ifdef PSX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PlayStation Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* psx = calloc(1, 0x20000);
    memcpy(psx, PSX_EXE_MAGIC, 8);
    psx_disc_t disc;
    assert(psx_parse(psx, 0x20000, &disc) && disc.is_ings);
    free(psx);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
