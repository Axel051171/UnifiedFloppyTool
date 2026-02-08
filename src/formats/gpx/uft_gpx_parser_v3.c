/**
 * @file uft_gpx_parser_v3.c
 * @brief GOD MODE GPX Parser v3 - GamePark GP2X/Caanoo
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* GP2X uses ARM Linux executables */
#define ELF_MAGIC               "\x7F""ELF"

typedef struct {
    bool is_elf;
    bool is_arm;
    uint32_t entry_point;
    size_t source_size;
    bool valid;
} gpx_file_t;

static uint32_t gpx_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool gpx_parse(const uint8_t* data, size_t size, gpx_file_t* gpx) {
    if (!data || !gpx || size < 52) return false;
    memset(gpx, 0, sizeof(gpx_file_t));
    gpx->source_size = size;
    
    gpx->is_elf = (memcmp(data, ELF_MAGIC, 4) == 0);
    if (gpx->is_elf) {
        gpx->is_arm = (data[18] == 0x28);  /* ARM machine */
        gpx->entry_point = gpx_read_le32(data + 24);
    }
    
    gpx->valid = gpx->is_elf && gpx->is_arm;
    return true;
}

#ifdef GPX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GP2X Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gpx[52] = {0x7F, 'E', 'L', 'F', 1, 1, 1, 0};
    gpx[18] = 0x28;  /* ARM */
    gpx_file_t file;
    assert(gpx_parse(gpx, sizeof(gpx), &file) && file.is_arm);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
