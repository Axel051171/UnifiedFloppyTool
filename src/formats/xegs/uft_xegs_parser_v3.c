/**
 * @file uft_xegs_parser_v3.c
 * @brief GOD MODE XEGS Parser v3 - Atari XEGS
 * 
 * Atari XE Game System (8-bit compatible)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define XEGS_MIN_SIZE           8192
#define XEGS_MAX_SIZE           131072

typedef struct {
    uint32_t rom_size;
    bool has_cart_header;
    uint8_t cart_type;
    size_t source_size;
    bool valid;
} xegs_rom_t;

static bool xegs_parse(const uint8_t* data, size_t size, xegs_rom_t* xegs) {
    if (!data || !xegs || size < XEGS_MIN_SIZE) return false;
    memset(xegs, 0, sizeof(xegs_rom_t));
    xegs->source_size = size;
    xegs->rom_size = size;
    
    /* Check for CART header */
    if (memcmp(data, "CART", 4) == 0) {
        xegs->has_cart_header = true;
        xegs->cart_type = data[7];
    }
    
    xegs->valid = (size >= XEGS_MIN_SIZE && size <= XEGS_MAX_SIZE);
    return true;
}

#ifdef XEGS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Atari XEGS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* xegs = calloc(1, XEGS_MIN_SIZE);
    memcpy(xegs, "CART", 4);
    xegs_rom_t rom;
    assert(xegs_parse(xegs, XEGS_MIN_SIZE, &rom) && rom.has_cart_header);
    free(xegs);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
