/**
 * @file uft_avif_parser_v3.c
 * @brief GOD MODE AVIF Parser v3 - AV1 Image File Format
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
    uint32_t ftyp_size;
    char brand[5];
    uint32_t minor_version;
    bool is_avif;
    bool is_avis;  /* AVIF sequence */
    size_t source_size;
    bool valid;
} avif_file_t;

static uint32_t avif_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool avif_parse(const uint8_t* data, size_t size, avif_file_t* avif) {
    if (!data || !avif || size < 16) return false;
    memset(avif, 0, sizeof(avif_file_t));
    avif->source_size = size;
    
    avif->ftyp_size = avif_read_be32(data);
    if (memcmp(data + 4, "ftyp", 4) == 0) {
        memcpy(avif->brand, data + 8, 4);
        avif->brand[4] = '\0';
        avif->minor_version = avif_read_be32(data + 12);
        
        if (memcmp(avif->brand, "avif", 4) == 0) {
            avif->is_avif = true;
            avif->valid = true;
        } else if (memcmp(avif->brand, "avis", 4) == 0) {
            avif->is_avis = true;
            avif->valid = true;
        }
    }
    return true;
}

#ifdef AVIF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== AVIF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t avif[32] = {0, 0, 0, 24, 'f', 't', 'y', 'p', 'a', 'v', 'i', 'f'};
    avif_file_t file;
    assert(avif_parse(avif, sizeof(avif), &file) && file.is_avif);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
