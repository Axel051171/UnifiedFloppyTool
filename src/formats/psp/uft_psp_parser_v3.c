/**
 * @file uft_psp_parser_v3.c
 * @brief GOD MODE PSP Parser v3 - PlayStation Portable
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSP_ISO_MAGIC           "PSP GAME"

typedef struct {
    char system_id[33];
    char volume_id[33];
    char game_id[10];
    bool is_iso;
    bool is_cso;  /* Compressed ISO */
    size_t source_size;
    bool valid;
} psp_disc_t;

static bool psp_parse(const uint8_t* data, size_t size, psp_disc_t* disc) {
    if (!data || !disc || size < 0x10000) return false;
    memset(disc, 0, sizeof(psp_disc_t));
    disc->source_size = size;
    
    /* Check for CSO header */
    if (memcmp(data, "CISO", 4) == 0) {
        disc->is_cso = true;
        disc->valid = true;
        return true;
    }
    
    /* ISO9660 PVD */
    const uint8_t* pvd = data + 16 * 2048;
    if (pvd[0] == 0x01 && memcmp(pvd + 1, "CD001", 5) == 0) {
        disc->is_iso = true;
        memcpy(disc->system_id, pvd + 8, 32); disc->system_id[32] = '\0';
        memcpy(disc->volume_id, pvd + 40, 32); disc->volume_id[32] = '\0';
        disc->valid = true;
    }
    return true;
}

#ifdef PSP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PSP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* psp = calloc(1, 0x20000);
    uint8_t* pvd = psp + 16 * 2048;
    pvd[0] = 0x01; memcpy(pvd + 1, "CD001", 5);
    psp_disc_t disc;
    assert(psp_parse(psp, 0x20000, &disc) && disc.is_iso);
    free(psp);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
