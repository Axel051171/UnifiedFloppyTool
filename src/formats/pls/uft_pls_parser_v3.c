/**
 * @file uft_pls_parser_v3.c
 * @brief GOD MODE PLS Parser v3 - PLS Playlist
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

#define PLS_MAGIC               "[playlist]"

typedef struct {
    bool is_valid_header;
    uint32_t num_entries;
    uint32_t version;
    size_t source_size;
    bool valid;
} pls_file_t;

static bool pls_parse(const uint8_t* data, size_t size, pls_file_t* pls) {
    if (!data || !pls || size < 10) return false;
    memset(pls, 0, sizeof(pls_file_t));
    pls->source_size = size;
    
    const char* text = (const char*)data;
    
    /* Case-insensitive check for [playlist] */
    if (strncasecmp(text, PLS_MAGIC, 10) == 0) {
        pls->is_valid_header = true;
        
        /* Find NumberOfEntries */
        const char* num = strstr(text, "NumberOfEntries=");
        if (num) {
            { int32_t t; if(uft_parse_int32(num+16,&t,10)) pls->num_entries=t; }
        }
        
        /* Find Version */
        const char* ver = strstr(text, "Version=");
        if (ver) {
            { int32_t t; if(uft_parse_int32(ver+8,&t,10)) pls->version=t; }
        }
        
        pls->valid = true;
    }
    return true;
}

#ifdef PLS_V3_TEST
#include <assert.h>
#include "uft/core/uft_safe_parse.h"
int main(void) {
    printf("=== PLS Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* pls = "[playlist]\nNumberOfEntries=3\nVersion=2\n";
    pls_file_t file;
    assert(pls_parse((const uint8_t*)pls, strlen(pls), &file) && file.num_entries == 3);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
