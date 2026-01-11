/**
 * @file uft_ecm_parser_v3.c
 * @brief GOD MODE ECM Parser v3 - Error Code Modeler
 * 
 * CD image compression format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ECM_MAGIC               "ECM\x00"

typedef struct {
    char signature[5];
    uint32_t version;
    size_t source_size;
    bool valid;
} ecm_file_t;

static bool ecm_parse(const uint8_t* data, size_t size, ecm_file_t* ecm) {
    if (!data || !ecm || size < 4) return false;
    memset(ecm, 0, sizeof(ecm_file_t));
    ecm->source_size = size;
    
    /* Check ECM header */
    if (memcmp(data, ECM_MAGIC, 4) == 0) {
        memcpy(ecm->signature, data, 3);
        ecm->signature[3] = '\0';
        ecm->version = data[3];
        ecm->valid = true;
    }
    return true;
}

#ifdef ECM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ECM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ecm[16] = {'E', 'C', 'M', 0x00};
    ecm_file_t file;
    assert(ecm_parse(ecm, sizeof(ecm), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
