/**
 * @file uft_msa_parser_v3.c
 * @brief GOD MODE MSA Parser v3 - Atari ST Magic Shadow Archiver
 * 
 * MSA ist ein komprimiertes Atari ST Format:
 * - RLE Kompression pro Track
 * - 16-Byte Header
 * - Sektor-weise Daten
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MSA_SIGNATURE           0x0E0F
#define MSA_HEADER_SIZE         10
#define MSA_SECTOR_SIZE         512

typedef enum {
    MSA_DIAG_OK = 0,
    MSA_DIAG_BAD_SIGNATURE,
    MSA_DIAG_BAD_GEOMETRY,
    MSA_DIAG_TRUNCATED,
    MSA_DIAG_RLE_ERROR,
    MSA_DIAG_COUNT
} msa_diag_code_t;

typedef struct { float overall; bool valid; bool compressed; } msa_score_t;
typedef struct { msa_diag_code_t code; uint16_t track; char msg[128]; } msa_diagnosis_t;
typedef struct { msa_diagnosis_t* items; size_t count; size_t cap; float quality; } msa_diagnosis_list_t;

typedef struct {
    uint16_t signature;
    uint16_t sectors_per_track;
    uint16_t sides;
    uint16_t start_track;
    uint16_t end_track;
    
    /* Derived */
    uint16_t track_count;
    uint32_t uncompressed_size;
    bool is_compressed;
    
    msa_score_t score;
    msa_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} msa_disk_t;

static msa_diagnosis_list_t* msa_diagnosis_create(void) {
    msa_diagnosis_list_t* l = calloc(1, sizeof(msa_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(msa_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void msa_diagnosis_free(msa_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t msa_read_be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

static bool msa_parse(const uint8_t* data, size_t size, msa_disk_t* disk) {
    if (!data || !disk || size < MSA_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(msa_disk_t));
    disk->diagnosis = msa_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    disk->signature = msa_read_be16(data);
    if (disk->signature != MSA_SIGNATURE) {
        return false;
    }
    
    /* Parse header */
    disk->sectors_per_track = msa_read_be16(data + 2);
    disk->sides = msa_read_be16(data + 4) + 1;
    disk->start_track = msa_read_be16(data + 6);
    disk->end_track = msa_read_be16(data + 8);
    
    /* Validate geometry */
    if (disk->sectors_per_track < 1 || disk->sectors_per_track > 22 ||
        disk->sides < 1 || disk->sides > 2 ||
        disk->end_track < disk->start_track ||
        disk->end_track > 85) {
        return false;
    }
    
    disk->track_count = disk->end_track - disk->start_track + 1;
    
    /* Calculate uncompressed size */
    uint32_t track_size = disk->sectors_per_track * MSA_SECTOR_SIZE;
    disk->uncompressed_size = disk->track_count * disk->sides * track_size;
    
    /* Check if compressed by scanning track data */
    size_t pos = MSA_HEADER_SIZE;
    for (uint16_t t = 0; t < disk->track_count * disk->sides && pos + 2 <= size; t++) {
        uint16_t data_length = msa_read_be16(data + pos);
        pos += 2;
        
        if (data_length != track_size) {
            disk->is_compressed = true;
        }
        
        pos += data_length;
    }
    
    disk->score.compressed = disk->is_compressed;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void msa_disk_free(msa_disk_t* disk) {
    if (disk && disk->diagnosis) msa_diagnosis_free(disk->diagnosis);
}

#ifdef MSA_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MSA Parser v3 Tests ===\n");
    
    printf("Testing MSA parsing... ");
    uint8_t msa[32];
    memset(msa, 0, sizeof(msa));
    msa[0] = 0x0E; msa[1] = 0x0F;  /* Signature */
    msa[2] = 0; msa[3] = 9;        /* 9 sectors/track */
    msa[4] = 0; msa[5] = 1;        /* 2 sides */
    msa[6] = 0; msa[7] = 0;        /* Start track 0 */
    msa[8] = 0; msa[9] = 79;       /* End track 79 */
    
    msa_disk_t disk;
    bool ok = msa_parse(msa, sizeof(msa), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.sectors_per_track == 9);
    assert(disk.sides == 2);
    assert(disk.track_count == 80);
    msa_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
