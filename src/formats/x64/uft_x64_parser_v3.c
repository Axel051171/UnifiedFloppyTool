/**
 * @file uft_x64_parser_v3.c
 * @brief GOD MODE X64 Parser v3 - Commodore X64 Container
 * 
 * Extended D64 with 64-byte header
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define X64_MAGIC               "C64File"
#define X64_HEADER_SIZE         64

typedef struct {
    char signature[8];
    uint8_t version_major;
    uint8_t version_minor;
    uint8_t device_type;
    uint8_t max_tracks;
    uint8_t second_side;
    uint8_t error_info;
    size_t source_size;
    bool valid;
} x64_file_t;

static bool x64_parse(const uint8_t* data, size_t size, x64_file_t* x64) {
    if (!data || !x64 || size < X64_HEADER_SIZE) return false;
    memset(x64, 0, sizeof(x64_file_t));
    x64->source_size = size;
    
    if (memcmp(data, X64_MAGIC, 7) == 0) {
        memcpy(x64->signature, data, 7);
        x64->signature[7] = '\0';
        x64->version_major = data[7];
        x64->version_minor = data[8];
        x64->device_type = data[9];
        x64->max_tracks = data[10];
        x64->second_side = data[11];
        x64->error_info = data[12];
        x64->valid = true;
    }
    return true;
}

#ifdef X64_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Commodore X64 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t x64[128] = {'C', '6', '4', 'F', 'i', 'l', 'e', 0, 2, 1, 35};
    x64_file_t file;
    assert(x64_parse(x64, sizeof(x64), &file) && file.max_tracks == 35);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
