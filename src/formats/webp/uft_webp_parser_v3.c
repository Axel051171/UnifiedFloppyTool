/**
 * @file uft_webp_parser_v3.c
 * @brief GOD MODE WEBP Parser v3 - Google WebP Image
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
#define WEBP_MAGIC              "WEBP"

typedef struct {
    char riff_sig[5];
    char webp_sig[5];
    uint32_t file_size;
    uint32_t width;
    uint32_t height;
    bool is_lossy;
    bool is_lossless;
    bool is_extended;
    bool has_alpha;
    bool is_animated;
    size_t source_size;
    bool valid;
} webp_file_t;

static uint32_t webp_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool webp_parse(const uint8_t* data, size_t size, webp_file_t* webp) {
    if (!data || !webp || size < 12) return false;
    memset(webp, 0, sizeof(webp_file_t));
    webp->source_size = size;
    
    if (memcmp(data, RIFF_MAGIC, 4) == 0 && memcmp(data + 8, WEBP_MAGIC, 4) == 0) {
        memcpy(webp->riff_sig, data, 4);
        webp->riff_sig[4] = '\0';
        memcpy(webp->webp_sig, data + 8, 4);
        webp->webp_sig[4] = '\0';
        webp->file_size = webp_read_le32(data + 4);
        
        /* Check chunk type */
        if (size >= 16) {
            if (memcmp(data + 12, "VP8 ", 4) == 0) webp->is_lossy = true;
            else if (memcmp(data + 12, "VP8L", 4) == 0) webp->is_lossless = true;
            else if (memcmp(data + 12, "VP8X", 4) == 0) webp->is_extended = true;
        }
        webp->valid = true;
    }
    return true;
}

#ifdef WEBP_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== WebP Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t webp[32] = {'R', 'I', 'F', 'F', 100, 0, 0, 0, 'W', 'E', 'B', 'P', 'V', 'P', '8', ' '};
    webp_file_t file;
    assert(webp_parse(webp, sizeof(webp), &file) && file.is_lossy);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
