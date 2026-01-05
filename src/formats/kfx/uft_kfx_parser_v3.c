/**
 * @file uft_kfx_parser_v3.c
 * @brief GOD MODE KFX Parser v3 - Kryoflux Stream Format
 * 
 * Kryoflux Stream Files:
 * - Raw Flux Timing
 * - OOB (Out of Band) Blocks
 * - Index Synchronization
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

/* OOB Types */
#define KFX_OOB_INVALID         0x00
#define KFX_OOB_STREAM_INFO     0x01
#define KFX_OOB_INDEX           0x02
#define KFX_OOB_STREAM_END      0x03
#define KFX_OOB_KFINFO          0x04
#define KFX_OOB_EOF             0x0D

typedef struct {
    uint32_t stream_position;
    uint32_t sample_counter;
    uint32_t index_counter;
} kfx_index_t;

typedef struct {
    uint8_t track;
    uint8_t side;
    
    kfx_index_t indices[16];
    uint8_t index_count;
    
    uint32_t flux_count;
    uint32_t total_samples;
    
    char hw_name[64];
    char hw_version[32];
    
    size_t source_size;
    bool valid;
} kfx_stream_t;

static bool kfx_parse(const uint8_t* data, size_t size, kfx_stream_t* kfx) {
    if (!data || !kfx || size < 16) return false;
    memset(kfx, 0, sizeof(kfx_stream_t));
    kfx->source_size = size;
    
    size_t pos = 0;
    while (pos < size) {
        uint8_t byte = data[pos];
        
        if (byte <= 0x07) {
            /* Flux2 */
            if (pos + 1 < size) {
                kfx->flux_count++;
                kfx->total_samples += (byte << 8) | data[pos + 1];
                pos += 2;
            } else break;
        } else if (byte == 0x08) {
            /* NOP1 */
            pos++;
        } else if (byte == 0x09) {
            /* NOP2 */
            pos += 2;
        } else if (byte == 0x0A) {
            /* NOP3 */
            pos += 3;
        } else if (byte == 0x0B) {
            /* OVL16 */
            pos++;
        } else if (byte == 0x0C) {
            /* Flux3 */
            if (pos + 2 < size) {
                kfx->flux_count++;
                pos += 3;
            } else break;
        } else if (byte == 0x0D) {
            /* OOB */
            if (pos + 3 < size) {
                uint8_t oob_type = data[pos + 1];
                uint16_t oob_size = data[pos + 2] | (data[pos + 3] << 8);
                
                if (oob_type == KFX_OOB_INDEX && kfx->index_count < 16) {
                    kfx->index_count++;
                } else if (oob_type == KFX_OOB_EOF) {
                    break;
                }
                pos += 4 + oob_size;
            } else break;
        } else {
            /* Flux1 */
            kfx->flux_count++;
            kfx->total_samples += byte;
            pos++;
        }
    }
    
    kfx->valid = (kfx->flux_count > 0);
    return true;
}

#ifdef KFX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Kryoflux Stream Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t kfx_data[64];
    memset(kfx_data, 0x50, sizeof(kfx_data));  /* Flux1 values */
    kfx_data[60] = 0x0D;  /* OOB */
    kfx_data[61] = 0x0D;  /* EOF */
    kfx_data[62] = 0x00;
    kfx_data[63] = 0x00;
    
    kfx_stream_t kfx;
    assert(kfx_parse(kfx_data, sizeof(kfx_data), &kfx) && kfx.flux_count > 0);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
