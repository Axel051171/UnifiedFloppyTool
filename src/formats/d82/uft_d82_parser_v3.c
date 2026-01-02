/**
 * @file uft_d82_parser_v3.c
 * @brief GOD MODE D82 Parser v3 - Commodore 8250 Double-Sided
 * 
 * D82 ist das Format für Commodore 8250 Drives:
 * - 77 Tracks × 2 Seiten
 * - Variable Sektoren pro Track (23-29)
 * - GCR Encoding
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D82_TRACKS              77
#define D82_SIDES               2
#define D82_SECTOR_SIZE         256
#define D82_SECTORS_PER_SIDE    2083
#define D82_TOTAL_SECTORS       (D82_SECTORS_PER_SIDE * D82_SIDES)  /* 4166 */
#define D82_SIZE                (D82_TOTAL_SECTORS * D82_SECTOR_SIZE)  /* 1066496 */

#define D82_BAM_TRACK           38
#define D82_DIR_TRACK           39

/* Same sector layout as D80 */
static const uint8_t d82_sectors_per_track[78] = {
    0,
    29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
    29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
    25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
    23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23
};

typedef enum {
    D82_DIAG_OK = 0,
    D82_DIAG_INVALID_SIZE,
    D82_DIAG_BAD_BAM,
    D82_DIAG_DIR_ERROR,
    D82_DIAG_COUNT
} d82_diag_code_t;

typedef struct { float overall; bool valid; } d82_score_t;
typedef struct { d82_diag_code_t code; uint8_t track; char msg[128]; } d82_diagnosis_t;
typedef struct { d82_diagnosis_t* items; size_t count; size_t cap; } d82_diagnosis_list_t;

typedef struct {
    char disk_name[17];
    char disk_id[3];
    uint16_t free_blocks_side0;
    uint16_t free_blocks_side1;
    uint16_t total_free;
} d82_bam_t;

typedef struct {
    d82_bam_t bam;
    
    d82_score_t score;
    d82_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} d82_disk_t;

static d82_diagnosis_list_t* d82_diagnosis_create(void) {
    d82_diagnosis_list_t* l = calloc(1, sizeof(d82_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(d82_diagnosis_t)); }
    return l;
}
static void d82_diagnosis_free(d82_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t d82_get_offset(uint8_t track, uint8_t sector, uint8_t side) {
    if (track < 1 || track > 77) return 0;
    
    uint32_t offset = 0;
    
    if (side == 1) {
        offset = D82_SECTORS_PER_SIDE * D82_SECTOR_SIZE;
    }
    
    for (uint8_t t = 1; t < track; t++) {
        offset += d82_sectors_per_track[t] * D82_SECTOR_SIZE;
    }
    offset += sector * D82_SECTOR_SIZE;
    return offset;
}

static bool d82_parse(const uint8_t* data, size_t size, d82_disk_t* disk) {
    if (!data || !disk) return false;
    memset(disk, 0, sizeof(d82_disk_t));
    disk->diagnosis = d82_diagnosis_create();
    disk->source_size = size;
    
    if (size != D82_SIZE) {
        return false;
    }
    
    /* Parse BAM at track 38, sector 0 */
    uint32_t bam_offset = d82_get_offset(D82_BAM_TRACK, 0, 0);
    if (bam_offset + 256 <= size) {
        const uint8_t* bam = data + bam_offset;
        
        for (int i = 0; i < 16; i++) {
            uint8_t c = bam[0x06 + i];
            disk->bam.disk_name[i] = (c == 0xA0) ? ' ' : c;
        }
        disk->bam.disk_name[16] = '\0';
        
        disk->bam.disk_id[0] = bam[0x18];
        disk->bam.disk_id[1] = bam[0x19];
        disk->bam.disk_id[2] = '\0';
    }
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    return true;
}

static void d82_disk_free(d82_disk_t* disk) {
    if (disk && disk->diagnosis) d82_diagnosis_free(disk->diagnosis);
}

#ifdef D82_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== D82 Parser v3 Tests ===\n");
    
    printf("Testing size... ");
    assert(D82_SIZE == 1066496);
    assert(D82_TOTAL_SECTORS == 4166);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
