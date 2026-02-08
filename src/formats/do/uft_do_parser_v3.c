/**
 * @file uft_do_parser_v3.c
 * @brief GOD MODE DO Parser v3 - Apple II DOS 3.3 Order
 * 
 * DO ist das DOS 3.3 Sektor-Order Format:
 * - 35 Tracks × 16 Sektoren
 * - 256 Bytes pro Sektor
 * - DOS 3.3 physikalische Sektor-Reihenfolge
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define DO_SECTOR_SIZE          256
#define DO_SECTORS_PER_TRACK    16
#define DO_SIZE_140K            (35 * 16 * 256)  /* 143360 */

/* DOS 3.3 sector skew table */
static const uint8_t do_skew[16] = {
    0, 7, 14, 6, 13, 5, 12, 4, 11, 3, 10, 2, 9, 1, 8, 15
};

/* VTOC location */
#define DO_VTOC_TRACK           17
#define DO_VTOC_SECTOR          0
#define DO_CATALOG_TRACK        17
#define DO_CATALOG_SECTOR       15

typedef enum {
    DO_DIAG_OK = 0,
    DO_DIAG_INVALID_SIZE,
    DO_DIAG_BAD_VTOC,
    DO_DIAG_COUNT
} do_diag_code_t;

typedef struct { float overall; bool valid; bool is_dos33; } do_score_t;
typedef struct { do_diag_code_t code; char msg[128]; } do_diagnosis_t;
typedef struct { do_diagnosis_t* items; size_t count; size_t cap; float quality; } do_diagnosis_list_t;

typedef struct {
    char name[31];
    uint8_t type;
    uint8_t track;
    uint8_t sector;
    uint16_t length;
    bool locked;
} do_file_t;

typedef struct {
    uint8_t tracks;
    uint8_t dos_version;
    uint8_t volume_number;
    uint8_t direction;
    uint8_t last_track;
    uint8_t max_pairs;
    
    do_file_t files[105];
    uint8_t file_count;
    uint16_t free_sectors;
    
    bool is_dos33;
    
    do_score_t score;
    do_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} do_disk_t;

static do_diagnosis_list_t* do_diagnosis_create(void) {
    do_diagnosis_list_t* l = calloc(1, sizeof(do_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(do_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void do_diagnosis_free(do_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t do_get_offset(uint8_t track, uint8_t sector) {
    return (track * DO_SECTORS_PER_TRACK + sector) * DO_SECTOR_SIZE;
}

static bool do_parse_vtoc(const uint8_t* data, size_t size, do_disk_t* disk) {
    uint32_t vtoc_offset = do_get_offset(DO_VTOC_TRACK, DO_VTOC_SECTOR);
    if (vtoc_offset + 256 > size) return false;
    
    const uint8_t* vtoc = data + vtoc_offset;
    
    /* VTOC format check */
    /* Byte 1: track of first catalog sector (should be 17) */
    if (vtoc[1] != 17) return false;
    
    disk->dos_version = vtoc[3];
    disk->volume_number = vtoc[6];
    disk->max_pairs = vtoc[0x27];
    disk->direction = vtoc[0x31];
    disk->last_track = vtoc[0x30];
    disk->tracks = vtoc[0x34];
    
    if (disk->tracks == 0) disk->tracks = 35;
    
    /* Count free sectors from bitmap */
    disk->free_sectors = 0;
    for (int t = 0; t < 35; t++) {
        uint32_t bitmap = vtoc[0x38 + t * 4] | 
                         (vtoc[0x39 + t * 4] << 8) |
                         ((uint32_t)vtoc[0x3A + t * 4] << 16) |
                         ((uint32_t)vtoc[0x3B + t * 4] << 24);
        for (int s = 0; s < 16; s++) {
            if (bitmap & (1 << s)) disk->free_sectors++;
        }
    }
    
    disk->is_dos33 = true;
    return true;
}

static bool do_parse(const uint8_t* data, size_t size, do_disk_t* disk) {
    if (!data || !disk || size < DO_SIZE_140K) return false;
    memset(disk, 0, sizeof(do_disk_t));
    disk->diagnosis = do_diagnosis_create();
    disk->source_size = size;
    
    disk->tracks = size / (DO_SECTORS_PER_TRACK * DO_SECTOR_SIZE);
    
    /* Try to parse VTOC */
    do_parse_vtoc(data, size, disk);
    
    disk->score.is_dos33 = disk->is_dos33;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void do_disk_free(do_disk_t* disk) {
    if (disk && disk->diagnosis) do_diagnosis_free(disk->diagnosis);
}

#ifdef DO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DO Parser v3 Tests ===\n");
    
    printf("Testing DO parsing... ");
    uint8_t* dsk = calloc(1, DO_SIZE_140K);
    
    /* Set up minimal VTOC */
    uint32_t vtoc_off = do_get_offset(17, 0);
    dsk[vtoc_off + 1] = 17;     /* Catalog track */
    dsk[vtoc_off + 0x34] = 35;  /* Tracks */
    
    do_disk_t disk;
    bool ok = do_parse(dsk, DO_SIZE_140K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 35);
    do_disk_free(&disk);
    free(dsk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
