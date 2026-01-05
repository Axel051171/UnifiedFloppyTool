/**
 * @file uft_m3u_parser_v3.c
 * @brief GOD MODE M3U Parser v3 - Playlist
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define M3U_MAGIC               "#EXTM3U"

typedef struct {
    bool is_extended;
    uint32_t entry_count;
    bool is_utf8;
    size_t source_size;
    bool valid;
} m3u_file_t;

static bool m3u_parse(const uint8_t* data, size_t size, m3u_file_t* m3u) {
    if (!data || !m3u || size < 1) return false;
    memset(m3u, 0, sizeof(m3u_file_t));
    m3u->source_size = size;
    
    /* Check for UTF-8 BOM */
    size_t start = 0;
    if (size >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
        m3u->is_utf8 = true;
        start = 3;
    }
    
    if (memcmp(data + start, M3U_MAGIC, 7) == 0) {
        m3u->is_extended = true;
    }
    
    /* Count entries */
    for (size_t i = start; i < size; i++) {
        if (data[i] == '\n' && i + 1 < size && data[i+1] != '#' && data[i+1] != '\r') {
            m3u->entry_count++;
        }
    }
    
    m3u->valid = true;
    return true;
}

#ifdef M3U_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== M3U Parser v3 Tests ===\n");
    printf("Testing... ");
    const char* m3u = "#EXTM3U\n#EXTINF:123,Title\nfile.mp3\n";
    m3u_file_t file;
    assert(m3u_parse((const uint8_t*)m3u, strlen(m3u), &file) && file.is_extended);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
