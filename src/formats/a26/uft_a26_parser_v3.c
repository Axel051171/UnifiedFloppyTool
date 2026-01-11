/**
 * @file uft_a26_parser_v3.c
 * @brief GOD MODE A26 Parser v3 - Atari 2600 ROM
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* Common ROM sizes */
#define A26_SIZE_2K             2048
#define A26_SIZE_4K             4096
#define A26_SIZE_8K             8192
#define A26_SIZE_16K            16384
#define A26_SIZE_32K            32768

typedef struct {
    uint32_t rom_size;
    uint8_t bank_switching;  /* Detected mapper */
    size_t source_size;
    bool valid;
} a26_rom_t;

static bool a26_parse(const uint8_t* data, size_t size, a26_rom_t* rom) {
    if (!data || !rom || size < A26_SIZE_2K) return false;
    memset(rom, 0, sizeof(a26_rom_t));
    rom->source_size = size;
    rom->rom_size = size;
    
    /* Detect bank switching based on size */
    if (size <= A26_SIZE_4K) rom->bank_switching = 0;  /* None */
    else if (size == A26_SIZE_8K) rom->bank_switching = 1;  /* F8 */
    else if (size == A26_SIZE_16K) rom->bank_switching = 2;  /* F6 */
    else if (size == A26_SIZE_32K) rom->bank_switching = 3;  /* F4 */
    
    rom->valid = (size >= A26_SIZE_2K && size <= A26_SIZE_32K);
    return true;
}

#ifdef A26_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari 2600 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* a26 = calloc(1, A26_SIZE_4K);
    a26_rom_t rom;
    assert(a26_parse(a26, A26_SIZE_4K, &rom) && rom.valid);
    free(a26);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
