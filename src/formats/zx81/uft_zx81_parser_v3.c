/**
 * @file uft_zx81_parser_v3.c
 * @brief GOD MODE ZX81 Parser v3 - Sinclair ZX81/TS1000
 * 
 * P file format
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
    uint16_t d_file;     /* Display file address */
    uint16_t vars;       /* Variables address */
    uint16_t e_line;     /* End of program */
    uint32_t prog_size;
    size_t source_size;
    bool valid;
} zx81_file_t;

static bool zx81_parse(const uint8_t* data, size_t size, zx81_file_t* zx81) {
    if (!data || !zx81 || size < 116) return false;
    memset(zx81, 0, sizeof(zx81_file_t));
    zx81->source_size = size;
    
    /* P file: first 116 bytes are system variables */
    zx81->d_file = data[12] | (data[13] << 8);
    zx81->vars = data[8] | (data[9] << 8);
    zx81->e_line = data[20] | (data[21] << 8);
    zx81->prog_size = size - 116;
    
    /* Validate addresses */
    zx81->valid = (zx81->d_file >= 0x4000 && zx81->d_file < 0x8000);
    return true;
}

#ifdef ZX81_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ZX81 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t zx81[256] = {0};
    zx81[12] = 0x00; zx81[13] = 0x40;  /* D_FILE = 0x4000 */
    zx81_file_t file;
    assert(zx81_parse(zx81, sizeof(zx81), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
