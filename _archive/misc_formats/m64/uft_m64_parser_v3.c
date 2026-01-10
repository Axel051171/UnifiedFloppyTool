/**
 * @file uft_m64_parser_v3.c
 * @brief GOD MODE M64 Parser v3 - Mupen64 Movie
 * 
 * N64 emulator movie format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define M64_MAGIC               0x1A34364D  /* "M64\x1A" */

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t uid;
    uint32_t vi_count;
    uint32_t rerecord_count;
    uint8_t fps;
    uint8_t controllers;
    uint32_t input_samples;
    char rom_name[32];
    size_t source_size;
    bool valid;
} m64_file_t;

static uint32_t m64_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool m64_parse(const uint8_t* data, size_t size, m64_file_t* m64) {
    if (!data || !m64 || size < 0x400) return false;
    memset(m64, 0, sizeof(m64_file_t));
    m64->source_size = size;
    
    m64->signature = m64_read_le32(data);
    if (m64->signature == M64_MAGIC) {
        m64->version = m64_read_le32(data + 4);
        m64->uid = m64_read_le32(data + 8);
        m64->vi_count = m64_read_le32(data + 12);
        m64->rerecord_count = m64_read_le32(data + 16);
        m64->fps = data[20];
        m64->controllers = data[21];
        m64->input_samples = m64_read_le32(data + 24);
        memcpy(m64->rom_name, data + 0xC4, 32);
        m64->valid = true;
    }
    return true;
}

#ifdef M64_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== M64 Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* m64 = calloc(1, 0x400);
    m64[0] = 'M'; m64[1] = '6'; m64[2] = '4'; m64[3] = 0x1A;
    m64_file_t file;
    assert(m64_parse(m64, 0x400, &file) && file.valid);
    free(m64);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
