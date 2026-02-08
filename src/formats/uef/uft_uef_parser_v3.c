/**
 * @file uft_uef_parser_v3.c
 * @brief GOD MODE UEF Parser v3 - Acorn Tape Format
 * 
 * UEF ist das universelle Tape-Format für:
 * - BBC Micro
 * - Acorn Electron
 * - Chunk-basiert mit Kompression
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define UEF_SIGNATURE           "UEF File!"
#define UEF_SIGNATURE_LEN       10
#define UEF_HEADER_SIZE         12

/* Chunk IDs */
#define UEF_ORIGIN              0x0000
#define UEF_INSTRUCTIONS        0x0001
#define UEF_CREDITS             0x0002
#define UEF_TARGET_MACHINE      0x0005
#define UEF_CARRIER_TONE        0x0110
#define UEF_CARRIER_TONE_DUMMY  0x0111
#define UEF_DATA_BLOCK          0x0100
#define UEF_DEFINED_DATA        0x0102
#define UEF_GAP                 0x0112
#define UEF_BAUD_RATE           0x0113
#define UEF_PHASE_CHANGE        0x0115
#define UEF_FLOATING_POINT_GAP  0x0116

typedef enum {
    UEF_DIAG_OK = 0,
    UEF_DIAG_BAD_SIGNATURE,
    UEF_DIAG_BAD_CHUNK,
    UEF_DIAG_TRUNCATED,
    UEF_DIAG_COMPRESSED,
    UEF_DIAG_COUNT
} uef_diag_code_t;

typedef struct { float overall; bool valid; uint16_t chunks; } uef_score_t;
typedef struct { uef_diag_code_t code; char msg[128]; } uef_diagnosis_t;
typedef struct { uef_diagnosis_t* items; size_t count; size_t cap; float quality; } uef_diagnosis_list_t;

typedef struct {
    uint16_t id;
    uint32_t length;
    uint32_t offset;
} uef_chunk_t;

typedef struct {
    uint8_t version_minor;
    uint8_t version_major;
    
    uef_chunk_t* chunks;
    uint16_t chunk_count;
    uint16_t chunk_capacity;
    
    /* Statistics */
    uint16_t data_blocks;
    uint16_t carrier_tones;
    uint32_t total_data;
    
    uef_score_t score;
    uef_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} uef_file_t;

static uef_diagnosis_list_t* uef_diagnosis_create(void) {
    uef_diagnosis_list_t* l = calloc(1, sizeof(uef_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(uef_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void uef_diagnosis_free(uef_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t uef_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t uef_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }

static bool uef_parse(const uint8_t* data, size_t size, uef_file_t* uef) {
    if (!data || !uef || size < UEF_HEADER_SIZE) return false;
    memset(uef, 0, sizeof(uef_file_t));
    uef->diagnosis = uef_diagnosis_create();
    uef->source_size = size;
    
    /* Check signature */
    if (memcmp(data, UEF_SIGNATURE, UEF_SIGNATURE_LEN) != 0) {
        /* Could be gzip compressed */
        if (data[0] == 0x1F && data[1] == 0x8B) {
            /* gzip header - would need decompression */
            return false;
        }
        return false;
    }
    
    uef->version_minor = data[10];
    uef->version_major = data[11];
    
    /* Allocate chunks */
    uef->chunk_capacity = 256;
    uef->chunks = calloc(uef->chunk_capacity, sizeof(uef_chunk_t));
    if (!uef->chunks) return false;
    
    /* Parse chunks */
    size_t pos = UEF_HEADER_SIZE;
    while (pos + 6 <= size && uef->chunk_count < uef->chunk_capacity) {
        uef_chunk_t* chunk = &uef->chunks[uef->chunk_count];
        
        chunk->id = uef_read_le16(data + pos);
        chunk->length = uef_read_le32(data + pos + 2);
        chunk->offset = pos + 6;
        
        /* Count statistics */
        if (chunk->id == UEF_DATA_BLOCK || chunk->id == UEF_DEFINED_DATA) {
            uef->data_blocks++;
            uef->total_data += chunk->length;
        } else if (chunk->id == UEF_CARRIER_TONE || chunk->id == UEF_CARRIER_TONE_DUMMY) {
            uef->carrier_tones++;
        }
        
        pos += 6 + chunk->length;
        uef->chunk_count++;
    }
    
    uef->score.chunks = uef->chunk_count;
    uef->score.overall = uef->chunk_count > 0 ? 1.0f : 0.0f;
    uef->score.valid = uef->chunk_count > 0;
    uef->valid = true;
    
    return true;
}

static void uef_file_free(uef_file_t* uef) {
    if (uef) {
        if (uef->chunks) free(uef->chunks);
        if (uef->diagnosis) uef_diagnosis_free(uef->diagnosis);
    }
}

#ifdef UEF_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== UEF Parser v3 Tests ===\n");
    
    printf("Testing UEF parsing... ");
    uint8_t uef_data[64];
    memset(uef_data, 0, sizeof(uef_data));
    memcpy(uef_data, "UEF File!", 10);
    uef_data[10] = 10;  /* Minor version */
    uef_data[11] = 0;   /* Major version */
    /* Add a data chunk */
    uef_data[12] = 0x00; uef_data[13] = 0x01;  /* Chunk ID 0x0100 */
    uef_data[14] = 4;    /* Length */
    
    uef_file_t uef;
    bool ok = uef_parse(uef_data, sizeof(uef_data), &uef);
    assert(ok);
    assert(uef.valid);
    assert(uef.chunk_count >= 1);
    uef_file_free(&uef);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
