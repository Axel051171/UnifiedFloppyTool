/**
 * @file uft_mod_parser_v3.c
 * @brief GOD MODE MOD Parser v3 - Amiga Module
 * 
 * ProTracker/NoiseTracker module
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
    char title[21];
    char format[5];       /* M.K., 4CHN, 8CHN, etc. */
    uint8_t num_channels;
    uint8_t num_patterns;
    uint8_t num_samples;
    size_t source_size;
    bool valid;
} mod_file_t;

static bool mod_parse(const uint8_t* data, size_t size, mod_file_t* mod) {
    if (!data || !mod || size < 1084) return false;
    memset(mod, 0, sizeof(mod_file_t));
    mod->source_size = size;
    
    memcpy(mod->title, data, 20);
    mod->title[20] = '\0';
    
    /* Check for MOD signature at offset 1080 */
    memcpy(mod->format, data + 1080, 4);
    mod->format[4] = '\0';
    
    if (memcmp(mod->format, "M.K.", 4) == 0 || memcmp(mod->format, "M!K!", 4) == 0) {
        mod->num_channels = 4;
        mod->valid = true;
    } else if (memcmp(mod->format, "6CHN", 4) == 0) {
        mod->num_channels = 6;
        mod->valid = true;
    } else if (memcmp(mod->format, "8CHN", 4) == 0) {
        mod->num_channels = 8;
        mod->valid = true;
    }
    
    return true;
}

#ifdef MOD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MOD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* mod = calloc(1, 1084);
    memcpy(mod, "Test Module", 11);
    memcpy(mod + 1080, "M.K.", 4);
    mod_file_t file;
    assert(mod_parse(mod, 1084, &file) && file.num_channels == 4);
    free(mod);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
