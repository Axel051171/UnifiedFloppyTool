/**
 * @file uft_fm2_parser_v3.c
 * @brief GOD MODE FM2 Parser v3 - FCEUX Movie File
 * 
 * NES emulator movie format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include "uft/core/uft_safe_parse.h"
#include <stdlib.h>
#include "uft/core/uft_safe_parse.h"
#include <string.h>
#include "uft/core/uft_safe_parse.h"
#include <stdint.h>
#include "uft/core/uft_safe_parse.h"
#include <stdbool.h>
#include "uft/core/uft_safe_parse.h"

typedef struct {
    uint32_t version;
    char rom_filename[256];
    uint32_t rerecord_count;
    uint32_t frame_count;
    bool is_pal;
    size_t source_size;
    bool valid;
} fm2_file_t;

static bool fm2_parse(const uint8_t* data, size_t size, fm2_file_t* fm2) {
    if (!data || !fm2 || size < 10) return false;
    memset(fm2, 0, sizeof(fm2_file_t));
    fm2->source_size = size;
    
    /* FM2 is a text-based format */
    if (strstr((const char*)data, "version") != NULL) {
        fm2->valid = true;
        /* Parse version */
        const char* ver = strstr((const char*)data, "version ");
        if (ver) { int32_t t; if(uft_parse_int32(ver+8,&t,10)) fm2->version=t; }
    }
    return true;
}

#ifdef FM2_V3_TEST
#include <assert.h>
#include "uft/core/uft_safe_parse.h"
int main(void) {
    printf("=== FM2 Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* fm2 = "version 3\nromFilename test.nes\n";
    fm2_file_t file;
    assert(fm2_parse((const uint8_t*)fm2, strlen(fm2), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
