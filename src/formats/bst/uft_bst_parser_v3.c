/**
 * @file uft_bst_parser_v3.c
 * @brief GOD MODE BST Parser v3 - BSNES/higan Save State
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define BST_MAGIC               "BST"

typedef struct {
    char signature[4];
    uint32_t version;
    size_t source_size;
    bool valid;
} bst_file_t;

static bool bst_parse(const uint8_t* data, size_t size, bst_file_t* bst) {
    if (!data || !bst || size < 8) return false;
    memset(bst, 0, sizeof(bst_file_t));
    bst->source_size = size;
    
    if (memcmp(data, BST_MAGIC, 3) == 0) {
        memcpy(bst->signature, data, 3);
        bst->signature[3] = '\0';
        bst->version = data[4] | (data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        bst->valid = true;
    }
    return true;
}

#ifdef BST_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== BST Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t bst[16] = {'B', 'S', 'T', 0, 1, 0, 0, 0};
    bst_file_t file;
    assert(bst_parse(bst, sizeof(bst), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
