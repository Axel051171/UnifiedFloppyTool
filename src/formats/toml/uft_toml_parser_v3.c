/**
 * @file uft_toml_parser_v3.c
 * @brief GOD MODE TOML Parser v3 - Tom's Obvious Minimal Language
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
    uint32_t table_count;
    uint32_t array_table_count;
    uint32_t key_count;
    char first_table[64];
    size_t source_size;
    bool valid;
} toml_file_t;

static bool toml_parse(const uint8_t* data, size_t size, toml_file_t* toml) {
    if (!data || !toml || size < 1) return false;
    memset(toml, 0, sizeof(toml_file_t));
    toml->source_size = size;
    
    bool found_first = false;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '[') {
            if (i + 1 < size && data[i+1] == '[') {
                toml->array_table_count++;
                i++;
            } else {
                toml->table_count++;
                if (!found_first) {
                    size_t j = i + 1;
                    while (j < size && data[j] != ']' && j - i < 63) j++;
                    memcpy(toml->first_table, data + i + 1, j - i - 1);
                    found_first = true;
                }
            }
        }
        if (data[i] == '=' && (i == 0 || (data[i-1] != '!' && data[i-1] != '<' && data[i-1] != '>'))) {
            toml->key_count++;
        }
    }
    
    toml->valid = (toml->key_count > 0);
    return true;
}

#ifdef TOML_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TOML Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* toml = "[package]\nname = \"test\"\nversion = \"1.0\"\n";
    toml_file_t file;
    assert(toml_parse((const uint8_t*)toml, strlen(toml), &file) && file.table_count == 1);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
