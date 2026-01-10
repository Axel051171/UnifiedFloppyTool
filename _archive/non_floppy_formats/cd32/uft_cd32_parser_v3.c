/**
 * @file uft_cd32_parser_v3.c
 * @brief GOD MODE CD32 Parser v3 - Commodore Amiga CD32
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    char title[41];
    bool is_cd32;
    bool has_akiko;  /* CD32 custom chip */
    size_t source_size;
    bool valid;
} cd32_disc_t;

static bool cd32_parse(const uint8_t* data, size_t size, cd32_disc_t* cd32) {
    if (!data || !cd32 || size < 0x10000) return false;
    memset(cd32, 0, sizeof(cd32_disc_t));
    cd32->source_size = size;
    
    /* Check ISO PVD */
    const uint8_t* pvd = data + 16 * 2048;
    if (pvd[0] == 0x01 && memcmp(pvd + 1, "CD001", 5) == 0) {
        memcpy(cd32->title, pvd + 40, 32);
        cd32->title[32] = '\0';
        /* CD32 discs often have "AMIGA" in system ID */
        if (strstr((char*)(pvd + 8), "AMIGA") != NULL) {
            cd32->is_cd32 = true;
        }
        cd32->valid = true;
    } else {
        cd32->valid = true;
    }
    return true;
}

#ifdef CD32_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CD32 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* cd32 = calloc(1, 0x10000);
    uint8_t* pvd = cd32 + 16 * 2048;
    pvd[0] = 0x01; memcpy(pvd + 1, "CD001", 5);
    memcpy(pvd + 8, "AMIGA", 5);
    cd32_disc_t disc;
    assert(cd32_parse(cd32, 0x10000, &disc) && disc.is_cd32);
    free(cd32);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
