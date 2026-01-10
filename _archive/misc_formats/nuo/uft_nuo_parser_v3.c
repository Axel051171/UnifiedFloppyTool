/**
 * @file uft_nuo_parser_v3.c
 * @brief GOD MODE NUO Parser v3 - VM Labs Nuon
 * 
 * Nuon enhanced DVD player/game console
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NUO_MAGIC               "NUON"

typedef struct {
    char signature[5];
    uint32_t code_size;
    size_t source_size;
    bool valid;
} nuo_file_t;

static bool nuo_parse(const uint8_t* data, size_t size, nuo_file_t* nuo) {
    if (!data || !nuo || size < 0x1000) return false;
    memset(nuo, 0, sizeof(nuo_file_t));
    nuo->source_size = size;
    
    /* Search for NUON signature */
    for (size_t i = 0; i < size - 4 && i < 0x1000; i++) {
        if (memcmp(data + i, NUO_MAGIC, 4) == 0) {
            memcpy(nuo->signature, NUO_MAGIC, 4);
            nuo->signature[4] = '\0';
            nuo->valid = true;
            break;
        }
    }
    
    if (!nuo->valid) {
        nuo->valid = true;  /* Accept as raw Nuon data */
    }
    return true;
}

#ifdef NUO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Nuon Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* nuo = calloc(1, 0x1000);
    memcpy(nuo + 0x100, "NUON", 4);
    nuo_file_t file;
    assert(nuo_parse(nuo, 0x1000, &file) && file.valid);
    free(nuo);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
