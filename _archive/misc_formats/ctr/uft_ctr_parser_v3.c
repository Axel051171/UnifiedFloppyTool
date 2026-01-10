/**
 * @file uft_ctr_parser_v3.c
 * @brief GOD MODE CTR Parser v3 - CAPS CTRaw Format
 * 
 * SPS/CAPS raw flux dump format (Kryoflux style)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define CTR_MAGIC               "CTRAW"

typedef struct {
    char signature[6];
    uint16_t version;
    uint8_t track;
    uint8_t side;
    uint32_t data_size;
    uint32_t index_count;
    double sample_clock;
    size_t source_size;
    bool valid;
} ctr_file_t;

static uint32_t ctr_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool ctr_parse(const uint8_t* data, size_t size, ctr_file_t* ctr) {
    if (!data || !ctr || size < 16) return false;
    memset(ctr, 0, sizeof(ctr_file_t));
    ctr->source_size = size;
    
    if (memcmp(data, CTR_MAGIC, 5) == 0) {
        memcpy(ctr->signature, data, 5);
        ctr->signature[5] = '\0';
        ctr->version = data[5] | (data[6] << 8);
        ctr->track = data[7];
        ctr->side = data[8];
        ctr->data_size = ctr_read_le32(data + 9);
        ctr->valid = true;
    }
    return true;
}

#ifdef CTR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== CAPS CTRaw Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t ctr[32] = {'C', 'T', 'R', 'A', 'W', 1, 0, 5, 0};
    ctr_file_t file;
    assert(ctr_parse(ctr, sizeof(ctr), &file) && file.track == 5);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
