/**
 * @file uft_gmv_parser_v3.c
 * @brief GOD MODE GMV Parser v3 - Gens Movie File
 * 
 * Genesis/Mega Drive emulator movie
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GMV_MAGIC               "Gens Movie"

typedef struct {
    char signature[16];
    uint32_t rerecord_count;
    uint8_t controller_flags;
    uint8_t flags;
    char rom_name[12];
    size_t source_size;
    bool valid;
} gmv_file_t;

static bool gmv_parse(const uint8_t* data, size_t size, gmv_file_t* gmv) {
    if (!data || !gmv || size < 64) return false;
    memset(gmv, 0, sizeof(gmv_file_t));
    gmv->source_size = size;
    
    memcpy(gmv->signature, data, 15);
    gmv->signature[15] = '\0';
    
    if (strstr(gmv->signature, "Gens Movie") != NULL) {
        gmv->rerecord_count = data[16] | (data[17]<<8) | (data[18]<<16) | (data[19]<<24);
        gmv->controller_flags = data[20];
        gmv->flags = data[21];
        gmv->valid = true;
    }
    return true;
}

#ifdef GMV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GMV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gmv[64] = {0};
    memcpy(gmv, "Gens Movie TEST", 15);
    gmv_file_t file;
    assert(gmv_parse(gmv, sizeof(gmv), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
