/**
 * @file uft_dsk_vec_parser_v3.c
 * @brief GOD MODE DSK_VEC Parser v3 - Vectrex Game Storage Format
 * 
 * Vectrex ROM/Multicart Storage:
 * - Binary ROM dumps
 * - Typical 4K-64K
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VEC_MIN_SIZE            4096
#define VEC_MAX_SIZE            65536

typedef struct {
    uint32_t rom_size;
    bool has_header;
    size_t source_size;
    bool valid;
} vec_rom_t;

static bool vec_parse(const uint8_t* data, size_t size, vec_rom_t* rom) {
    if (!data || !rom || size < VEC_MIN_SIZE || size > VEC_MAX_SIZE) return false;
    memset(rom, 0, sizeof(vec_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    /* Check for "g GCE" header at common offsets */
    rom->has_header = (memcmp(data, "g GCE", 5) == 0);
    rom->valid = true;
    return true;
}

#ifdef DSK_VEC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Vectrex Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* vec = calloc(1, 8192);
    memcpy(vec, "g GCE", 5);
    vec_rom_t rom;
    assert(vec_parse(vec, 8192, &rom) && rom.has_header);
    free(vec);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
