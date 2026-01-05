/**
 * @file uft_heif_parser_v3.c
 * @brief GOD MODE HEIF Parser v3 - High Efficiency Image Format (HEIC)
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
    bool is_heic;
    bool is_heif;
    bool is_avif;
    bool is_mif1;
    size_t source_size;
    bool valid;
} heif_file_t;

static uint32_t heif_read_be32(const uint8_t* p) {
    return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

static bool heif_parse(const uint8_t* data, size_t size, heif_file_t* heif) {
    if (!data || !heif || size < 12) return false;
    memset(heif, 0, sizeof(heif_file_t));
    heif->source_size = size;
    
    heif->ftyp_size = heif_read_be32(data);
    if (memcmp(data + 4, "ftyp", 4) == 0) {
        memcpy(heif->brand, data + 8, 4);
        heif->brand[4] = '\0';
        heif->minor_version = heif_read_be32(data + 12);
        
        if (memcmp(heif->brand, "heic", 4) == 0 || memcmp(heif->brand, "heix", 4) == 0) {
            heif->is_heic = true;
        } else if (memcmp(heif->brand, "mif1", 4) == 0) {
            heif->is_mif1 = true;
        } else if (memcmp(heif->brand, "avif", 4) == 0) {
            heif->is_avif = true;
        }
        heif->is_heif = true;
        heif->valid = true;
    }
    return true;
}

#ifdef HEIF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== HEIF Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t heif[32] = {0, 0, 0, 24, 'f', 't', 'y', 'p', 'h', 'e', 'i', 'c'};
    heif_file_t file;
    assert(heif_parse(heif, sizeof(heif), &file) && file.is_heic);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
