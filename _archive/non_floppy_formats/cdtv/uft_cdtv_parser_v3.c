/**
 * @file uft_cdtv_parser_v3.c
 * @brief GOD MODE CDTV Parser v3 - Commodore CDTV
 * 
 * Amiga-based CD console
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CDTV_TM_OFFSET          0x00
#define CDTV_MAGIC              "CDTV"

typedef struct {
    char signature[5];
    char title[41];
    bool is_cdtv;
    bool is_iso;
    size_t source_size;
    bool valid;
} cdtv_disc_t;

static bool cdtv_parse(const uint8_t* data, size_t size, cdtv_disc_t* cdtv) {
    if (!data || !cdtv || size < 0x10000) return false;
    memset(cdtv, 0, sizeof(cdtv_disc_t));
    cdtv->source_size = size;
    
    /* Check for CDTV trademark file or ISO */
    const uint8_t* pvd = data + 16 * 2048;
    if (pvd[0] == 0x01 && memcmp(pvd + 1, "CD001", 5) == 0) {
        cdtv->is_iso = true;
        memcpy(cdtv->title, pvd + 40, 32);
        cdtv->title[32] = '\0';
        cdtv->valid = true;
    }
    
    /* Check for CDTV specific markers */
    if (memcmp(data, "CDTV", 4) == 0) {
        cdtv->is_cdtv = true;
        memcpy(cdtv->signature, data, 4);
        cdtv->signature[4] = '\0';
        cdtv->valid = true;
    }
    
    if (!cdtv->valid) cdtv->valid = true;  /* Accept as raw */
    return true;
}

#ifdef CDTV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CDTV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* cdtv = calloc(1, 0x10000);
    memcpy(cdtv, "CDTV", 4);
    cdtv_disc_t disc;
    assert(cdtv_parse(cdtv, 0x10000, &disc) && disc.is_cdtv);
    free(cdtv);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
