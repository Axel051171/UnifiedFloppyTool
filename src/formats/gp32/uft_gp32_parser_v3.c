/**
 * @file uft_gp32_parser_v3.c
 * @brief GOD MODE GP32 Parser v3 - GamePark GP32
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define GP32_MAGIC              "GP32"
#define GP32_HEADER_SIZE        256

typedef struct {
    char signature[5];
    char game_name[33];
    uint32_t file_size;
    uint32_t icon_offset;
    size_t source_size;
    bool valid;
} gp32_file_t;

static bool gp32_parse(const uint8_t* data, size_t size, gp32_file_t* gp32) {
    if (!data || !gp32 || size < GP32_HEADER_SIZE) return false;
    memset(gp32, 0, sizeof(gp32_file_t));
    gp32->source_size = size;
    
    memcpy(gp32->signature, data, 4); gp32->signature[4] = '\0';
    memcpy(gp32->game_name, data + 0x10, 32); gp32->game_name[32] = '\0';
    
    gp32->valid = (memcmp(gp32->signature, GP32_MAGIC, 4) == 0);
    return true;
}

#ifdef GP32_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== GP32 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t gp32[GP32_HEADER_SIZE] = {0};
    memcpy(gp32, "GP32", 4);
    memcpy(gp32 + 0x10, "Test Game", 9);
    gp32_file_t file;
    assert(gp32_parse(gp32, sizeof(gp32), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
