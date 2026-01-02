/**
 * @file uft_d77_parser_v3.c
 * @brief GOD MODE D77 Parser v3 - Japanese PC D77 Disk Image
 * 
 * NEC PC-8801/PC-9801, Sharp X1, FM-7 disk format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D77_HEADER_SIZE         0x2B0
#define D77_MAX_TRACK           164

typedef struct {
    char disk_name[17];
    uint8_t reserved[9];
    uint8_t write_protect;
    uint8_t media_type;
    uint32_t disk_size;
    uint32_t track_offsets[D77_MAX_TRACK];
    uint8_t track_count;
    size_t source_size;
    bool valid;
} d77_file_t;

static uint32_t d77_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool d77_parse(const uint8_t* data, size_t size, d77_file_t* d77) {
    if (!data || !d77 || size < D77_HEADER_SIZE) return false;
    memset(d77, 0, sizeof(d77_file_t));
    d77->source_size = size;
    
    memcpy(d77->disk_name, data, 16);
    d77->disk_name[16] = '\0';
    d77->write_protect = data[0x1A];
    d77->media_type = data[0x1B];
    d77->disk_size = d77_read_le32(data + 0x1C);
    
    /* Read track offsets */
    for (int i = 0; i < D77_MAX_TRACK; i++) {
        d77->track_offsets[i] = d77_read_le32(data + 0x20 + (i * 4));
        if (d77->track_offsets[i] != 0) {
            d77->track_count = i + 1;
        }
    }
    
    d77->valid = (d77->disk_size > 0 && d77->disk_size <= size);
    return true;
}

#ifdef D77_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Japanese D77 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t d77[D77_HEADER_SIZE] = {0};
    memcpy(d77, "TESTDISK", 8);
    d77[0x1C] = 0x00; d77[0x1D] = 0x40; d77[0x1E] = 0x05;  /* 344064 bytes */
    d77[0x20] = 0xB0; d77[0x21] = 0x02;  /* First track at 0x2B0 */
    d77_file_t file;
    assert(d77_parse(d77, sizeof(d77), &file) && file.track_count > 0);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
