/**
 * @file uft_dsk_p3_parser_v3.c
 * @brief GOD MODE DSK_P3 Parser v3 - ZX Spectrum +3 Disk Format
 * 
 * +3DOS ist das Spectrum +3 Disk-Format:
 * - CP/M-kompatibel
 * - 40/80 Tracks × 1/2 Seiten
 * - 512 Bytes/Sektor
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define P3_SIGNATURE            "EXTENDED"
#define P3_SECTOR_SIZE          512
#define P3_SIZE_180K            (40 * 1 * 9 * 512)
#define P3_SIZE_720K            (80 * 2 * 9 * 512)

/* +3DOS header */
#define P3_HEADER_SIGNATURE     "+3DOS"
#define P3_HEADER_SIZE          128

typedef enum {
    P3_DIAG_OK = 0,
    P3_DIAG_INVALID_SIZE,
    P3_DIAG_BAD_HEADER,
    P3_DIAG_COUNT
} p3_diag_code_t;

typedef struct { float overall; bool valid; uint8_t format; } p3_score_t;
typedef struct { p3_diag_code_t code; char msg[128]; } p3_diagnosis_t;
typedef struct { p3_diagnosis_t* items; size_t count; size_t cap; float quality; } p3_diagnosis_list_t;

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint8_t format_type;        /* 0=SSSD, 1=DSDD, etc. */
    
    /* +3DOS specific */
    bool has_plus3_header;
    uint8_t plus3_type;
    uint32_t file_length;
    
    p3_score_t score;
    p3_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} p3_disk_t;

static p3_diagnosis_list_t* p3_diagnosis_create(void) {
    p3_diagnosis_list_t* l = calloc(1, sizeof(p3_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(p3_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void p3_diagnosis_free(p3_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static bool p3_parse(const uint8_t* data, size_t size, p3_disk_t* disk) {
    if (!data || !disk || size < P3_SIZE_180K) return false;
    memset(disk, 0, sizeof(p3_disk_t));
    disk->diagnosis = p3_diagnosis_create();
    disk->source_size = size;
    
    /* Detect geometry from size */
    if (size == P3_SIZE_180K) {
        disk->tracks = 40;
        disk->sides = 1;
        disk->sectors_per_track = 9;
        disk->format_type = 0;  /* SSSD */
    } else if (size == P3_SIZE_720K) {
        disk->tracks = 80;
        disk->sides = 2;
        disk->sectors_per_track = 9;
        disk->format_type = 1;  /* DSDD */
    } else {
        /* Try to guess */
        disk->tracks = 80;
        disk->sides = 2;
        disk->sectors_per_track = 9;
    }
    
    disk->sector_size = P3_SECTOR_SIZE;
    
    /* Check for +3DOS file header in first sector */
    if (size >= P3_HEADER_SIZE && memcmp(data, "+3DOS\x1A", 6) == 0) {
        disk->has_plus3_header = true;
        disk->plus3_type = data[15];
        disk->file_length = data[11] | (data[12] << 8) | ((uint32_t)data[13] << 16) | ((uint32_t)data[14] << 24);
    }
    
    disk->score.format = disk->format_type;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void p3_disk_free(p3_disk_t* disk) {
    if (disk && disk->diagnosis) p3_diagnosis_free(disk->diagnosis);
}

#ifdef DSK_P3_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== +3DOS Parser v3 Tests ===\n");
    
    printf("Testing +3DOS parsing... ");
    uint8_t* p3 = calloc(1, P3_SIZE_720K);
    
    p3_disk_t disk;
    bool ok = p3_parse(p3, P3_SIZE_720K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 80);
    assert(disk.sides == 2);
    p3_disk_free(&disk);
    free(p3);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
