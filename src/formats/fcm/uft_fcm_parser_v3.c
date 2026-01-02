/**
 * @file uft_fcm_parser_v3.c
 * @brief GOD MODE FCM Parser v3 - FCEU Movie (old format)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define FCM_MAGIC               0x4D4D4346  /* "FCMM" little-endian */

typedef struct {
    uint32_t signature;
    uint32_t version;
    uint32_t frame_count;
    uint32_t rerecord_count;
    size_t source_size;
    bool valid;
} fcm_file_t;

static uint32_t fcm_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24);
}

static bool fcm_parse(const uint8_t* data, size_t size, fcm_file_t* fcm) {
    if (!data || !fcm || size < 32) return false;
    memset(fcm, 0, sizeof(fcm_file_t));
    fcm->source_size = size;
    
    fcm->signature = fcm_read_le32(data);
    if (fcm->signature == FCM_MAGIC) {
        fcm->version = fcm_read_le32(data + 4);
        fcm->frame_count = fcm_read_le32(data + 8);
        fcm->rerecord_count = fcm_read_le32(data + 12);
        fcm->valid = true;
    }
    return true;
}

#ifdef FCM_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== FCM Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t fcm[32] = {0x46, 0x43, 0x4D, 0x4D};  /* FCMM in LE */
    fcm_file_t file;
    assert(fcm_parse(fcm, sizeof(fcm), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
