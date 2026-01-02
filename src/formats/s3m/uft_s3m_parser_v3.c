/**
 * @file uft_s3m_parser_v3.c
 * @brief GOD MODE S3M Parser v3 - ScreamTracker 3 Module
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define S3M_MAGIC               "SCRM"

typedef struct {
    char title[29];
    char signature[5];
    uint8_t type;
    uint16_t order_count;
    uint16_t instrument_count;
    uint16_t pattern_count;
    uint16_t flags;
    uint16_t tracker_version;
    uint8_t global_volume;
    uint8_t initial_speed;
    uint8_t initial_tempo;
    size_t source_size;
    bool valid;
} s3m_file_t;

static bool s3m_parse(const uint8_t* data, size_t size, s3m_file_t* s3m) {
    if (!data || !s3m || size < 96) return false;
    memset(s3m, 0, sizeof(s3m_file_t));
    s3m->source_size = size;
    
    /* Signature at offset 44 */
    if (memcmp(data + 44, S3M_MAGIC, 4) == 0) {
        memcpy(s3m->title, data, 28);
        s3m->title[28] = '\0';
        memcpy(s3m->signature, data + 44, 4);
        s3m->signature[4] = '\0';
        s3m->type = data[29];
        s3m->order_count = data[32] | (data[33] << 8);
        s3m->instrument_count = data[34] | (data[35] << 8);
        s3m->pattern_count = data[36] | (data[37] << 8);
        s3m->global_volume = data[48];
        s3m->initial_speed = data[49];
        s3m->initial_tempo = data[50];
        s3m->valid = true;
    }
    return true;
}

#ifdef S3M_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== S3M Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t s3m[96] = {0};
    memcpy(s3m, "Test Song", 9);
    memcpy(s3m + 44, "SCRM", 4);
    s3m_file_t file;
    assert(s3m_parse(s3m, sizeof(s3m), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
