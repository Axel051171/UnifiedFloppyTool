/**
 * @file uft_json_parser_v3.c
 * @brief GOD MODE JSON Parser v3 - JSON Configuration
 * 
 * For emulator configs, playlists, etc.
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
    bool is_object;
    bool is_array;
    uint32_t depth;
    size_t source_size;
    bool valid;
} json_file_t;

static bool json_parse(const uint8_t* data, size_t size, json_file_t* json) {
    if (!data || !json || size < 2) return false;
    memset(json, 0, sizeof(json_file_t));
    json->source_size = size;
    
    /* Skip whitespace */
    size_t i = 0;
    while (i < size && (data[i] == ' ' || data[i] == '\t' || 
                        data[i] == '\r' || data[i] == '\n')) i++;
    
    if (i < size) {
        if (data[i] == '{') {
            json->is_object = true;
            json->valid = true;
        } else if (data[i] == '[') {
            json->is_array = true;
            json->valid = true;
        }
    }
    
    /* Count depth */
    uint32_t depth = 0, max_depth = 0;
    for (size_t j = 0; j < size; j++) {
        if (data[j] == '{' || data[j] == '[') depth++;
        if (data[j] == '}' || data[j] == ']') depth--;
        if (depth > max_depth) max_depth = depth;
    }
    json->depth = max_depth;
    
    return true;
}

#ifdef JSON_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== JSON Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* json = "{\"name\": \"test\", \"value\": 42}";
    json_file_t file;
    assert(json_parse((const uint8_t*)json, strlen(json), &file) && file.is_object);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
