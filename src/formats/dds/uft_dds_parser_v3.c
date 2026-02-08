/**
 * @file uft_dds_parser_v3.c
 * @brief GOD MODE DDS Parser v3 - DirectDraw Surface
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DDS_MAGIC               0x20534444  /* "DDS " */

typedef struct {
    uint32_t magic;
    uint32_t header_size;
    uint32_t flags;
    uint32_t height;
    uint32_t width;
    uint32_t pitch_or_linear_size;
    uint32_t depth;
    uint32_t mipmap_count;
    uint32_t pixel_format_fourcc;
    bool is_dx10;
    size_t source_size;
    bool valid;
} dds_file_t;

static uint32_t dds_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool dds_parse(const uint8_t* data, size_t size, dds_file_t* dds) {
    if (!data || !dds || size < 128) return false;
    memset(dds, 0, sizeof(dds_file_t));
    dds->source_size = size;
    
    dds->magic = dds_read_le32(data);
    if (dds->magic == DDS_MAGIC) {
        dds->header_size = dds_read_le32(data + 4);
        dds->flags = dds_read_le32(data + 8);
        dds->height = dds_read_le32(data + 12);
        dds->width = dds_read_le32(data + 16);
        dds->pitch_or_linear_size = dds_read_le32(data + 20);
        dds->depth = dds_read_le32(data + 24);
        dds->mipmap_count = dds_read_le32(data + 28);
        dds->pixel_format_fourcc = dds_read_le32(data + 84);
        
        dds->is_dx10 = (dds->pixel_format_fourcc == 0x30315844);  /* "DX10" */
        dds->valid = true;
    }
    return true;
}

#ifdef DDS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DDS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t dds[128] = {'D', 'D', 'S', ' ', 124, 0, 0, 0};
    dds[12] = 0; dds[13] = 1; /* height 256 */
    dds[16] = 0; dds[17] = 2; /* width 512 */
    dds_file_t file;
    assert(dds_parse(dds, sizeof(dds), &file) && file.width == 512);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
