/**
 * @file uft_pc99_parser_v3.c
 * @brief GOD MODE PC99 Parser v3 - TI-99/4A PC99 Disk Format
 * 
 * PC99 emulator disk format (extended V9T9)
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
    char disk_name[11];
    uint16_t total_sectors;
    uint8_t sectors_per_track;
    uint8_t tracks;
    uint8_t sides;
    uint8_t density;
    bool is_sssd;
    bool is_dssd;
    bool is_dsdd;
    size_t source_size;
    bool valid;
} pc99_file_t;

static bool pc99_parse(const uint8_t* data, size_t size, pc99_file_t* pc99) {
    if (!data || !pc99 || size < 256) return false;
    memset(pc99, 0, sizeof(pc99_file_t));
    pc99->source_size = size;
    
    memcpy(pc99->disk_name, data, 10);
    pc99->disk_name[10] = '\0';
    
    pc99->total_sectors = (data[0x0A] << 8) | data[0x0B];
    pc99->sectors_per_track = data[0x0C];
    pc99->tracks = data[0x11];
    pc99->sides = data[0x12];
    pc99->density = data[0x13];
    
    /* Detect format */
    if (size == 92160) {
        pc99->is_sssd = true;
    } else if (size == 184320) {
        pc99->is_dssd = true;
    } else if (size == 368640) {
        pc99->is_dsdd = true;
    }
    
    if (pc99->sectors_per_track >= 9 && pc99->sectors_per_track <= 18) {
        pc99->valid = true;
    }
    
    return true;
}

#ifdef PC99_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TI-99 PC99 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pc99[512] = {0};
    memcpy(pc99, "TESTDISK  ", 10);
    pc99[0x0C] = 9;
    pc99_file_t file;
    assert(pc99_parse(pc99, sizeof(pc99), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
