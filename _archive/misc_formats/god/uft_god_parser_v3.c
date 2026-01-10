/**
 * @file uft_god_parser_v3.c
 * @brief GOD MODE GOD Parser v3 - Xbox 360 Games on Demand
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GOD_MAGIC               "LIVE"
#define CON_MAGIC               "CON "
#define PIRS_MAGIC              "PIRS"

typedef struct {
    char signature[5];
    uint32_t content_type;
    uint32_t metadata_version;
    uint64_t content_size;
    char title_id[9];
    char display_name[128];
    bool is_live;
    bool is_con;
    bool is_pirs;
    size_t source_size;
    bool valid;
} god_file_t;

static bool god_parse(const uint8_t* data, size_t size, god_file_t* god) {
    if (!data || !god || size < 512) return false;
    memset(god, 0, sizeof(god_file_t));
    god->source_size = size;
    
    memcpy(god->signature, data, 4);
    god->signature[4] = '\0';
    
    if (memcmp(data, GOD_MAGIC, 4) == 0) {
        god->is_live = true;
        god->valid = true;
    } else if (memcmp(data, CON_MAGIC, 4) == 0) {
        god->is_con = true;
        god->valid = true;
    } else if (memcmp(data, PIRS_MAGIC, 4) == 0) {
        god->is_pirs = true;
        god->valid = true;
    }
    
    return true;
}

#ifdef GOD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Xbox 360 GOD Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t god[512] = {'L', 'I', 'V', 'E'};
    god_file_t file;
    assert(god_parse(god, sizeof(god), &file) && file.is_live);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
