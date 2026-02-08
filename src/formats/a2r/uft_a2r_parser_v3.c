/**
 * @file uft_a2r_parser_v3.c
 * @brief GOD MODE A2R Parser v3 - Applesauce A2R Flux Format
 * 
 * A2R ist das Applesauce Flux-Capture Format:
 * - Raw Flux Timing Data
 * - Quarter-Track Support
 * - Copy Protection Analysis
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define A2R_SIGNATURE           "A2R2"
#define A2R_SIGNATURE_V3        "A2R3"
#define A2R_HEADER_SIZE         8

/* Chunk types */
#define A2R_CHUNK_INFO          "INFO"
#define A2R_CHUNK_STRM          "STRM"
#define A2R_CHUNK_META          "META"
#define A2R_CHUNK_RWCP          "RWCP"
#define A2R_CHUNK_SLVD          "SLVD"

typedef struct {
    char signature[5];
    uint8_t version;
    uint8_t disk_type;
    bool write_protected;
    bool synchronized;
    
    uint32_t stream_count;
    uint32_t total_flux_data;
    
    char creator[33];
    
    size_t source_size;
    bool valid;
} a2r_file_t;

static uint32_t a2r_read_le32(const uint8_t* p) {
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool a2r_parse(const uint8_t* data, size_t size, a2r_file_t* a2r) {
    if (!data || !a2r || size < A2R_HEADER_SIZE) return false;
    memset(a2r, 0, sizeof(a2r_file_t));
    a2r->source_size = size;
    
    /* Check signature */
    if (memcmp(data, A2R_SIGNATURE, 4) != 0 && 
        memcmp(data, A2R_SIGNATURE_V3, 4) != 0) {
        return false;
    }
    
    memcpy(a2r->signature, data, 4);
    a2r->signature[4] = '\0';
    a2r->version = (data[3] == '3') ? 3 : 2;
    
    /* Parse chunks */
    size_t pos = 8;
    while (pos + 8 <= size) {
        char chunk_id[5];
        memcpy(chunk_id, data + pos, 4);
        chunk_id[4] = '\0';
        uint32_t chunk_size = a2r_read_le32(data + pos + 4);
        
        if (memcmp(chunk_id, A2R_CHUNK_INFO, 4) == 0 && pos + 8 + chunk_size <= size) {
            a2r->disk_type = data[pos + 8 + 1];
            a2r->write_protected = data[pos + 8 + 2];
            a2r->synchronized = data[pos + 8 + 3];
        } else if (memcmp(chunk_id, A2R_CHUNK_STRM, 4) == 0) {
            a2r->stream_count++;
            a2r->total_flux_data += chunk_size;
        }
        
        pos += 8 + chunk_size;
    }
    
    a2r->valid = true;
    return true;
}

#ifdef A2R_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== A2R Flux Parser v3 Tests ===\n");
    printf("Testing A2R2... ");
    uint8_t a2r_data[128];
    memset(a2r_data, 0, sizeof(a2r_data));
    memcpy(a2r_data, "A2R2", 4);
    a2r_data[4] = 0xFF; /* FF FF 0A 0D */
    a2r_data[5] = 0xFF;
    a2r_data[6] = 0x0A;
    a2r_data[7] = 0x0D;
    
    a2r_file_t a2r;
    assert(a2r_parse(a2r_data, sizeof(a2r_data), &a2r) && a2r.version == 2);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
