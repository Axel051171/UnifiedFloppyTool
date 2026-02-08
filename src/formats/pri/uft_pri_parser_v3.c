/**
 * @file uft_pri_parser_v3.c
 * @brief GOD MODE PRI Parser v3 - MESS Preservation Raw Image
 * 
 * MAME/MESS raw flux preservation format
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PRI_MAGIC               "PRI "
#define PRI_CHUNK_TEXT          "TEXT"
#define PRI_CHUNK_TRAK          "TRAK"
#define PRI_CHUNK_DATA          "DATA"
#define PRI_CHUNK_WEAK          "WEAK"

typedef struct {
    char signature[5];
    uint32_t version;
    uint32_t track_count;
    bool has_weak_bits;
    bool has_text;
    size_t source_size;
    bool valid;
} pri_file_t;

static uint32_t pri_read_be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | (p[2] << 8) | p[3];
}

static bool pri_parse(const uint8_t* data, size_t size, pri_file_t* pri) {
    if (!data || !pri || size < 12) return false;
    memset(pri, 0, sizeof(pri_file_t));
    pri->source_size = size;
    
    if (memcmp(data, PRI_MAGIC, 4) == 0) {
        memcpy(pri->signature, data, 4);
        pri->signature[4] = '\0';
        pri->version = pri_read_be32(data + 4);
        
        /* Scan chunks */
        size_t offset = 12;
        while (offset + 8 < size) {
            uint32_t chunk_size = pri_read_be32(data + offset);
            const char* chunk_id = (const char*)(data + offset + 4);
            
            if (memcmp(chunk_id, PRI_CHUNK_TRAK, 4) == 0) {
                pri->track_count++;
            } else if (memcmp(chunk_id, PRI_CHUNK_WEAK, 4) == 0) {
                pri->has_weak_bits = true;
            } else if (memcmp(chunk_id, PRI_CHUNK_TEXT, 4) == 0) {
                pri->has_text = true;
            }
            
            offset += chunk_size + 8;
        }
        
        pri->valid = true;
    }
    return true;
}

#ifdef PRI_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MESS PRI Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t pri[32] = {'P', 'R', 'I', ' ', 0, 0, 0, 2};
    pri_file_t file;
    assert(pri_parse(pri, sizeof(pri), &file) && file.valid);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
