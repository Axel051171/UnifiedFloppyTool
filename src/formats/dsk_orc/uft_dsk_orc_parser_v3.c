/**
 * @file uft_dsk_orc_parser_v3.c
 * @brief GOD MODE DSK_ORC Parser v3 - Oric Microdisc Format
 * 
 * Oric DSK ist das Microdisc-Format:
 * - MFM Header mit Geometrie
 * - Variable Sektorgrößen
 * - 40/80 Tracks
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ORC_SIGNATURE           "MFM_DISK"
#define ORC_SIGNATURE_LEN       8
#define ORC_HEADER_SIZE         256

typedef enum {
    ORC_DIAG_OK = 0,
    ORC_DIAG_BAD_SIGNATURE,
    ORC_DIAG_BAD_GEOMETRY,
    ORC_DIAG_TRUNCATED,
    ORC_DIAG_COUNT
} orc_diag_code_t;

typedef struct { float overall; bool valid; } orc_score_t;
typedef struct { orc_diag_code_t code; char msg[128]; } orc_diagnosis_t;
typedef struct { orc_diagnosis_t* items; size_t count; size_t cap; float quality; } orc_diagnosis_list_t;

typedef struct {
    char signature[9];
    uint32_t sides;
    uint32_t tracks;
    uint32_t sectors;
    uint32_t geometry;
    
    uint32_t data_size;
    
    orc_score_t score;
    orc_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} orc_disk_t;

static orc_diagnosis_list_t* orc_diagnosis_create(void) {
    orc_diagnosis_list_t* l = calloc(1, sizeof(orc_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(orc_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void orc_diagnosis_free(orc_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t orc_read_le32(const uint8_t* p) { 
    return p[0] | (p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); 
}

static bool orc_parse(const uint8_t* data, size_t size, orc_disk_t* disk) {
    if (!data || !disk || size < ORC_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(orc_disk_t));
    disk->diagnosis = orc_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (memcmp(data, ORC_SIGNATURE, ORC_SIGNATURE_LEN) != 0) {
        return false;
    }
    memcpy(disk->signature, data, 8);
    disk->signature[8] = '\0';
    
    /* Parse header */
    disk->sides = orc_read_le32(data + 8);
    disk->tracks = orc_read_le32(data + 12);
    disk->geometry = orc_read_le32(data + 16);
    
    /* Sectors from geometry */
    disk->sectors = (disk->geometry >> 8) & 0xFF;
    if (disk->sectors == 0) disk->sectors = 17;
    
    disk->data_size = size - ORC_HEADER_SIZE;
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void orc_disk_free(orc_disk_t* disk) {
    if (disk && disk->diagnosis) orc_diagnosis_free(disk->diagnosis);
}

#ifdef DSK_ORC_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Oric DSK Parser v3 Tests ===\n");
    
    printf("Testing Oric DSK parsing... ");
    uint8_t orc[512];
    memset(orc, 0, sizeof(orc));
    memcpy(orc, "MFM_DISK", 8);
    orc[8] = 2;   /* Sides */
    orc[12] = 80; /* Tracks */
    orc[17] = 17; /* Sectors */
    
    orc_disk_t disk;
    bool ok = orc_parse(orc, sizeof(orc), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.sides == 2);
    assert(disk.tracks == 80);
    orc_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
