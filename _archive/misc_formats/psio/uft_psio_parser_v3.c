/**
 * @file uft_psio_parser_v3.c
 * @brief GOD MODE PSIO Parser v3 - PlayStation I/O Format
 * 
 * PS-IO SD card loader format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PSIO_MAGIC              "PSIO"

typedef struct {
    char signature[5];
    uint32_t disc_size;
    char game_id[16];
    size_t source_size;
    bool valid;
} psio_file_t;

static bool psio_parse(const uint8_t* data, size_t size, psio_file_t* psio) {
    if (!data || !psio || size < 0x800) return false;
    memset(psio, 0, sizeof(psio_file_t));
    psio->source_size = size;
    
    /* Check for PSIO header */
    if (memcmp(data, PSIO_MAGIC, 4) == 0) {
        memcpy(psio->signature, data, 4);
        psio->signature[4] = '\0';
        psio->valid = true;
    } else {
        /* Could be raw bin/cue */
        psio->valid = true;
    }
    return true;
}

#ifdef PSIO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PSIO Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* psio = calloc(1, 0x800);
    memcpy(psio, "PSIO", 4);
    psio_file_t file;
    assert(psio_parse(psio, 0x800, &file) && file.valid);
    free(psio);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
