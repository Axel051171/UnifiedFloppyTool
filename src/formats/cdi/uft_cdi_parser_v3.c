/**
 * @file uft_cdi_parser_v3.c
 * @brief GOD MODE CDI Parser v3 - Philips CD-i
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CDI_SECTOR_SIZE         2352
#define CDI_DATA_SIZE           2048

typedef struct {
    char system_id[33];
    char volume_id[33];
    uint32_t volume_size;
    size_t source_size;
    bool valid;
} cdi_disc_t;

static bool cdi_parse(const uint8_t* data, size_t size, cdi_disc_t* disc) {
    if (!data || !disc || size < 0x10000) return false;
    memset(disc, 0, sizeof(cdi_disc_t));
    disc->source_size = size;
    
    /* ISO9660 PVD at sector 16 */
    const uint8_t* pvd = data + 16 * CDI_DATA_SIZE;
    if (pvd[0] == 0x01 && memcmp(pvd + 1, "CD001", 5) == 0) {
        memcpy(disc->system_id, pvd + 8, 32); disc->system_id[32] = '\0';
        memcpy(disc->volume_id, pvd + 40, 32); disc->volume_id[32] = '\0';
        disc->valid = true;
    }
    return true;
}

#ifdef CDI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CD-i Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* cdi = calloc(1, 0x20000);
    uint8_t* pvd = cdi + 16 * CDI_DATA_SIZE;
    pvd[0] = 0x01;
    memcpy(pvd + 1, "CD001", 5);
    memcpy(pvd + 40, "TESTCDI", 7);
    cdi_disc_t disc;
    assert(cdi_parse(cdi, 0x20000, &disc) && disc.valid);
    free(cdi);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
