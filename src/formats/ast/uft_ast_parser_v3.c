/**
 * @file uft_ast_parser_v3.c
 * @brief GOD MODE AST Parser v3 - Amstrad CPC Extended DSK
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define AST_MAGIC_STD           "MV - CPCEMU Disk-File"
#define AST_MAGIC_EXT           "EXTENDED CPC DSK File"
#define AST_HEADER_SIZE         256

typedef struct {
    char signature[35];
    char creator[15];
    uint8_t tracks;
    uint8_t sides;
    uint16_t track_size;
    bool is_extended;
    size_t source_size;
    bool valid;
} ast_disk_t;

static bool ast_parse(const uint8_t* data, size_t size, ast_disk_t* disk) {
    if (!data || !disk || size < AST_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(ast_disk_t));
    disk->source_size = size;
    
    memcpy(disk->signature, data, 34); disk->signature[34] = '\0';
    memcpy(disk->creator, data + 0x22, 14); disk->creator[14] = '\0';
    disk->tracks = data[0x30];
    disk->sides = data[0x31];
    disk->track_size = data[0x32] | (data[0x33] << 8);
    
    disk->is_extended = (memcmp(data, AST_MAGIC_EXT, 21) == 0);
    disk->valid = (memcmp(data, AST_MAGIC_STD, 21) == 0 || disk->is_extended);
    return true;
}

#ifdef AST_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Amstrad Extended DSK Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ast[256] = {0};
    memcpy(ast, "MV - CPCEMU Disk-File", 21);
    ast[0x30] = 40; ast[0x31] = 1;
    ast_disk_t disk;
    assert(ast_parse(ast, sizeof(ast), &disk) && disk.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
