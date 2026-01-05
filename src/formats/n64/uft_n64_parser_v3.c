/**
 * @file uft_n64_parser_v3.c
 * @brief GOD MODE N64 Parser v3 - Nintendo 64 ROM
 * 
 * N64 ROM Formats:
 * - .z64 (Big-endian)
 * - .v64 (Byte-swapped)
 * - .n64 (Little-endian)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define N64_HEADER_SIZE         0x40

/* Magic bytes for different byte orders */
#define N64_Z64_MAGIC           0x80371240  /* Big-endian */
#define N64_V64_MAGIC           0x37804012  /* Byte-swapped */
#define N64_N64_MAGIC           0x40123780  /* Little-endian */

typedef enum {
    N64_FORMAT_Z64 = 0,
    N64_FORMAT_V64 = 1,
    N64_FORMAT_N64 = 2,
    N64_FORMAT_UNKNOWN = -1
} n64_format_t;

typedef struct {
    n64_format_t format;
    char title[21];
    char game_code[5];
    uint8_t version;
    uint32_t crc1;
    uint32_t crc2;
    size_t source_size;
    bool valid;
} n64_rom_t;

static uint32_t n64_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool n64_parse(const uint8_t* data, size_t size, n64_rom_t* rom) {
    if (!data || !rom || size < N64_HEADER_SIZE) return false;
    memset(rom, 0, sizeof(n64_rom_t));
    rom->source_size = size;
    
    uint32_t magic = n64_read_be32(data);
    
    if (magic == N64_Z64_MAGIC) rom->format = N64_FORMAT_Z64;
    else if (magic == N64_V64_MAGIC) rom->format = N64_FORMAT_V64;
    else if (magic == N64_N64_MAGIC) rom->format = N64_FORMAT_N64;
    else { rom->format = N64_FORMAT_UNKNOWN; return false; }
    
    /* For Z64 format, read directly */
    if (rom->format == N64_FORMAT_Z64) {
        memcpy(rom->title, data + 0x20, 20); rom->title[20] = '\0';
        memcpy(rom->game_code, data + 0x3B, 4); rom->game_code[4] = '\0';
        rom->version = data[0x3F];
        rom->crc1 = n64_read_be32(data + 0x10);
        rom->crc2 = n64_read_be32(data + 0x14);
    }
    
    rom->valid = true;
    return true;
}

#ifdef N64_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== N64 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t n64[0x40] = {0x80, 0x37, 0x12, 0x40};  /* Z64 magic */
    memcpy(n64 + 0x20, "TESTGAME            ", 20);
    n64_rom_t rom;
    assert(n64_parse(n64, sizeof(n64), &rom) && rom.format == N64_FORMAT_Z64);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
