/**
 * @file uft_avi_parser_v3.c
 * @brief GOD MODE AVI Parser v3 - Audio Video Interleave
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RIFF_MAGIC              "RIFF"
#define AVI_MAGIC               "AVI "

typedef struct {
    char riff_sig[5];
    char avi_sig[5];
    uint32_t file_size;
    uint32_t width;
    uint32_t height;
    uint32_t frame_count;
    uint32_t streams;
    size_t source_size;
    bool valid;
} avi_file_t;

static bool avi_parse(const uint8_t* data, size_t size, avi_file_t* avi) {
    if (!data || !avi || size < 12) return false;
    memset(avi, 0, sizeof(avi_file_t));
    avi->source_size = size;
    
    if (memcmp(data, RIFF_MAGIC, 4) == 0 && memcmp(data + 8, AVI_MAGIC, 4) == 0) {
        memcpy(avi->riff_sig, data, 4);
        avi->riff_sig[4] = '\0';
        memcpy(avi->avi_sig, data + 8, 4);
        avi->avi_sig[4] = '\0';
        avi->file_size = data[4] | (data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
        avi->valid = true;
    }
    return true;
}

#ifdef AVI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== AVI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t avi[32] = {'R', 'I', 'F', 'F', 100, 0, 0, 0, 'A', 'V', 'I', ' '};
    avi_file_t file;
    assert(avi_parse(avi, sizeof(avi), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
