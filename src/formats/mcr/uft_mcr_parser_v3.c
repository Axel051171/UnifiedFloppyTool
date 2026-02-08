/**
 * @file uft_mcr_parser_v3.c
 * @brief GOD MODE MCR Parser v3 - PlayStation Memory Card
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MCR_MAGIC               "MC"
#define MCR_SIZE                131072
#define MCR_BLOCK_SIZE          8192
#define MCR_BLOCKS              16

typedef struct {
    char signature[3];
    uint8_t used_blocks;
    uint8_t free_blocks;
    size_t source_size;
    bool valid;
} mcr_file_t;

static bool mcr_parse(const uint8_t* data, size_t size, mcr_file_t* mcr) {
    if (!data || !mcr || size < MCR_SIZE) return false;
    memset(mcr, 0, sizeof(mcr_file_t));
    mcr->source_size = size;
    
    memcpy(mcr->signature, data, 2);
    mcr->signature[2] = '\0';
    
    if (memcmp(mcr->signature, MCR_MAGIC, 2) == 0) {
        /* Count used/free blocks */
        for (int i = 1; i < MCR_BLOCKS; i++) {
            const uint8_t* block_header = data + (i * MCR_BLOCK_SIZE);
            if (block_header[0] == 0x51) {
                mcr->used_blocks++;
            } else if (block_header[0] == 0xA0) {
                mcr->free_blocks++;
            }
        }
        mcr->valid = true;
    }
    return true;
}

#ifdef MCR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MCR Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* mcr = calloc(1, MCR_SIZE);
    mcr[0] = 'M'; mcr[1] = 'C';
    mcr_file_t file;
    assert(mcr_parse(mcr, MCR_SIZE, &file) && file.valid);
    free(mcr);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
