/**
 * @file uft_ini_parser_v3.c
 * @brief GOD MODE INI Parser v3 - INI Configuration
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
    uint32_t section_count;
    uint32_t key_count;
    char first_section[64];
    size_t source_size;
    bool valid;
} ini_file_t;

static bool ini_parse(const uint8_t* data, size_t size, ini_file_t* ini) {
    if (!data || !ini || size < 1) return false;
    memset(ini, 0, sizeof(ini_file_t));
    ini->source_size = size;
    
    bool found_first = false;
    for (size_t i = 0; i < size; i++) {
        if (data[i] == '[') {
            ini->section_count++;
            if (!found_first) {
                size_t j = i + 1;
                while (j < size && data[j] != ']' && j - i < 63) j++;
                memcpy(ini->first_section, data + i + 1, j - i - 1);
                found_first = true;
            }
        }
        if (data[i] == '=' && i > 0 && data[i-1] != '\n') {
            ini->key_count++;
        }
    }
    
    ini->valid = (ini->section_count > 0 || ini->key_count > 0);
    return true;
}

#ifdef INI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== INI Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* ini = "[Section1]\nkey1=value1\n[Section2]\nkey2=value2\n";
    ini_file_t file;
    assert(ini_parse((const uint8_t*)ini, strlen(ini), &file) && file.section_count == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
