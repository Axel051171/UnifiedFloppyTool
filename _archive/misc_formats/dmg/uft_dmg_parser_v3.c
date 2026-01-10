/**
 * @file uft_dmg_parser_v3.c
 * @brief GOD MODE DMG Parser v3 - Apple Disk Image
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DMG_MAGIC               "koly"

typedef struct {
    char signature[5];
    uint32_t version;
    uint32_t header_size;
    uint32_t flags;
    uint64_t running_data_fork_offset;
    uint64_t data_fork_offset;
    uint64_t data_fork_length;
    size_t source_size;
    bool valid;
} dmg_file_t;

static bool dmg_parse(const uint8_t* data, size_t size, dmg_file_t* dmg) {
    if (!data || !dmg || size < 512) return false;
    memset(dmg, 0, sizeof(dmg_file_t));
    dmg->source_size = size;
    
    /* DMG trailer at end of file */
    const uint8_t* trailer = data + size - 512;
    if (memcmp(trailer, DMG_MAGIC, 4) == 0) {
        memcpy(dmg->signature, trailer, 4);
        dmg->signature[4] = '\0';
        dmg->valid = true;
    }
    return true;
}

#ifdef DMG_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Apple DMG Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* dmg = calloc(1, 1024);
    memcpy(dmg + 512, "koly", 4);
    dmg_file_t file;
    assert(dmg_parse(dmg, 1024, &file) && file.valid);
    free(dmg);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
