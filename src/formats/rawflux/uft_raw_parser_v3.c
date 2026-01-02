/**
 * @file uft_raw_parser_v3.c
 * @brief GOD MODE RAW Parser v3 - Raw Flux Stream
 * 
 * Generic raw flux timing data (KryoFlux compatible)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RAW_OOB_MARKER          0x0D
#define RAW_FLUX_2              0x0C
#define RAW_FLUX_3              0x00
#define RAW_INDEX               0x02
#define RAW_STREAM_END          0x0D
#define RAW_INFO                0x00
#define RAW_EOF                 0x0D

typedef struct {
    uint32_t flux_count;
    uint32_t index_count;
    uint32_t overflow_count;
    uint8_t track;
    uint8_t side;
    double sample_clock_mhz;
    double index_clock_mhz;
    bool has_oob;
    size_t source_size;
    bool valid;
} raw_flux_t;

static bool raw_parse(const uint8_t* data, size_t size, raw_flux_t* raw) {
    if (!data || !raw || size < 10) return false;
    memset(raw, 0, sizeof(raw_flux_t));
    raw->source_size = size;
    raw->sample_clock_mhz = 24.027428;  /* Default KryoFlux clock */
    raw->index_clock_mhz = 73.728;
    
    /* Parse flux stream */
    size_t i = 0;
    while (i < size) {
        uint8_t b = data[i++];
        
        if (b <= 0x07) {
            /* Flux timing values 0x00-0x07 are 2-byte */
            if (i < size) {
                raw->flux_count++;
                i++;
            }
        } else if (b >= 0x0E) {
            /* Flux timing 0x0E-0xFF are 1-byte */
            raw->flux_count++;
        } else if (b == RAW_FLUX_2) {
            /* 2-byte overflow */
            raw->overflow_count++;
            i += 2;
        } else if (b == RAW_OOB_MARKER) {
            /* Out-of-band marker */
            raw->has_oob = true;
            if (i + 3 <= size) {
                uint8_t oob_type = data[i];
                uint16_t oob_len = data[i+1] | (data[i+2] << 8);
                i += 3 + oob_len;
                if (oob_type == RAW_INDEX) {
                    raw->index_count++;
                }
            } else {
                break;
            }
        }
    }
    
    raw->valid = (raw->flux_count > 0);
    return true;
}

#ifdef RAW_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Raw Flux Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t raw[32] = {0x50, 0x60, 0x70, 0x80, 0x0D, 0x00, 0, 0, 0x0D, 0x0D, 0, 0};
    raw_flux_t file;
    assert(raw_parse(raw, sizeof(raw), &file) && file.flux_count > 0);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
