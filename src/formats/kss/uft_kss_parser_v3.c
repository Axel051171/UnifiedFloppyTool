/**
 * @file uft_kss_parser_v3.c
 * @brief GOD MODE KSS Parser v3 - MSX Sound Format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define KSS_MAGIC               "KSCC"
#define KSSX_MAGIC              "KSSX"

typedef struct {
    char signature[5];
    uint16_t load_address;
    uint16_t data_size;
    uint16_t init_address;
    uint16_t play_address;
    uint8_t start_bank;
    uint8_t extra_bank;
    uint8_t extra_header;
    uint8_t device_flags;
    bool is_extended;
    size_t source_size;
    bool valid;
} kss_file_t;

static bool kss_parse(const uint8_t* data, size_t size, kss_file_t* kss) {
    if (!data || !kss || size < 16) return false;
    memset(kss, 0, sizeof(kss_file_t));
    kss->source_size = size;
    
    if (memcmp(data, KSS_MAGIC, 4) == 0 || memcmp(data, KSSX_MAGIC, 4) == 0) {
        memcpy(kss->signature, data, 4);
        kss->signature[4] = '\0';
        kss->is_extended = (memcmp(data, KSSX_MAGIC, 4) == 0);
        kss->load_address = data[4] | (data[5] << 8);
        kss->data_size = data[6] | (data[7] << 8);
        kss->init_address = data[8] | (data[9] << 8);
        kss->play_address = data[10] | (data[11] << 8);
        kss->start_bank = data[12];
        kss->extra_bank = data[13];
        kss->extra_header = data[14];
        kss->device_flags = data[15];
        kss->valid = true;
    }
    return true;
}

#ifdef KSS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== KSS Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t kss[32] = {'K', 'S', 'C', 'C', 0x00, 0x40};
    kss_file_t file;
    assert(kss_parse(kss, sizeof(kss), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
