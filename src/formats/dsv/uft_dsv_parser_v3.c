/**
 * @file uft_dsv_parser_v3.c
 * @brief GOD MODE DSV Parser v3 - DeSmuME Save File
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DSV_FOOTER              "|-DESMUME SAVE-|"
#define DSV_FOOTER_SIZE         122

typedef struct {
    uint32_t save_size;
    bool has_footer;
    size_t source_size;
    bool valid;
} dsv_file_t;

static bool dsv_parse(const uint8_t* data, size_t size, dsv_file_t* dsv) {
    if (!data || !dsv || size < 16) return false;
    memset(dsv, 0, sizeof(dsv_file_t));
    dsv->source_size = size;
    
    /* Check for DeSmuME footer at end */
    if (size > DSV_FOOTER_SIZE) {
        const uint8_t* footer = data + size - DSV_FOOTER_SIZE;
        if (memcmp(footer, DSV_FOOTER, 16) == 0) {
            dsv->has_footer = true;
            dsv->save_size = size - DSV_FOOTER_SIZE;
        }
    }
    
    if (!dsv->has_footer) {
        dsv->save_size = size;
    }
    
    dsv->valid = true;
    return true;
}

#ifdef DSV_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DSV Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dsv[256] = {0};
    dsv_file_t file;
    assert(dsv_parse(dsv, sizeof(dsv), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
