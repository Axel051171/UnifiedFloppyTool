/**
 * @file uft_epub_parser_v3.c
 * @brief GOD MODE EPUB Parser v3 - Electronic Publication
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PK_MAGIC                0x04034B50

typedef struct {
    uint32_t pk_signature;
    char mimetype[32];
    bool is_epub;
    size_t source_size;
    bool valid;
} epub_file_t;

static uint32_t epub_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool epub_parse(const uint8_t* data, size_t size, epub_file_t* epub) {
    if (!data || !epub || size < 58) return false;
    memset(epub, 0, sizeof(epub_file_t));
    epub->source_size = size;
    
    epub->pk_signature = epub_read_le32(data);
    if (epub->pk_signature == PK_MAGIC) {
        /* Check for mimetype file as first entry */
        uint16_t name_len = data[26] | (data[27] << 8);
        if (name_len == 8 && memcmp(data + 30, "mimetype", 8) == 0) {
            /* Check mimetype content */
            uint32_t compressed_size = epub_read_le32(data + 18);
            if (compressed_size >= 20) {
                const char* mime = (const char*)(data + 38);
                if (strstr(mime, "application/epub+zip") != NULL) {
                    epub->is_epub = true;
                    memcpy(epub->mimetype, "application/epub+zip", 20);
                    epub->valid = true;
                }
            }
        }
    }
    return true;
}

#ifdef EPUB_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== EPUB Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t epub[128] = {0x50, 0x4B, 0x03, 0x04, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 20, 0, 0, 0, 0, 0, 0, 0, 8, 0};
    memcpy(epub + 30, "mimetype", 8);
    memcpy(epub + 38, "application/epub+zip", 20);
    epub_file_t file;
    assert(epub_parse(epub, sizeof(epub), &file) && file.is_epub);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
