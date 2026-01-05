/**
 * @file uft_pls_parser_v3.c
 * @brief GOD MODE PLS Parser v3 - PLS Playlist
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

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
            pls->num_entries = atoi(num + 16);
        }
        
        /* Find Version */
        const char* ver = strstr(text, "Version=");
        if (ver) {
            pls->version = atoi(ver + 8);
        }
        
        pls->valid = true;
    }
    return true;
}

#ifdef PLS_V3_TEST
#include <assert.h>
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
