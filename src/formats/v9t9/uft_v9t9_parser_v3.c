/**
 * @file uft_v9t9_parser_v3.c
 * @brief GOD MODE V9T9 Parser v3 - TI-99/4A Disk Format
 * 
 * V9T9/DSK ist das TI-99/4A Disk-Format:
 * - 256 Bytes pro Sektor
 * - 40 Tracks × 1/2 Seiten
 * - TI-DOS Filesystem
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define V9T9_SECTOR_SIZE        256
#define V9T9_SECTORS_SS         (40 * 9)    /* 360 sectors, 90K */
#define V9T9_SECTORS_DS         (40 * 9 * 2) /* 720 sectors, 180K */
#define V9T9_SIZE_SS            (V9T9_SECTORS_SS * V9T9_SECTOR_SIZE)
#define V9T9_SIZE_DS            (V9T9_SECTORS_DS * V9T9_SECTOR_SIZE)

/* Volume Information Block (VIB) at sector 0 */
#define V9T9_VIB_NAME           0x00    /* 10 bytes */
#define V9T9_VIB_TOTAL          0x0A    /* 2 bytes BE */
#define V9T9_VIB_SECTORS        0x0C    /* 1 byte */
#define V9T9_VIB_DSK            0x0D    /* "DSK" */
#define V9T9_VIB_PROTECTED      0x10    /* Protection flag */
#define V9T9_VIB_TRACKS         0x11    /* Tracks per side */
#define V9T9_VIB_SIDES          0x12    /* Sides */
#define V9T9_VIB_DENSITY        0x13    /* Density */

typedef enum {
    V9T9_DIAG_OK = 0,
    V9T9_DIAG_INVALID_SIZE,
    V9T9_DIAG_BAD_VIB,
    V9T9_DIAG_COUNT
} v9t9_diag_code_t;

typedef struct { float overall; bool valid; uint8_t sides; } v9t9_score_t;
typedef struct { v9t9_diag_code_t code; char msg[128]; } v9t9_diagnosis_t;
typedef struct { v9t9_diagnosis_t* items; size_t count; size_t cap; float quality; } v9t9_diagnosis_list_t;

typedef struct {
    char name[11];
    uint8_t type;
    uint16_t sectors;
    uint8_t eof_offset;
    uint8_t rec_len;
    uint16_t level3_rec;
} v9t9_file_t;

typedef struct {
    /* VIB info */
    char volume_name[11];
    uint16_t total_sectors;
    uint8_t sectors_per_track;
    char dsk_marker[4];
    uint8_t protected;
    uint8_t tracks_per_side;
    uint8_t sides;
    uint8_t density;
    
    /* Derived */
    uint32_t disk_size;
    
    v9t9_file_t files[127];
    uint8_t file_count;
    
    v9t9_score_t score;
    v9t9_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} v9t9_disk_t;

static v9t9_diagnosis_list_t* v9t9_diagnosis_create(void) {
    v9t9_diagnosis_list_t* l = calloc(1, sizeof(v9t9_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(v9t9_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void v9t9_diagnosis_free(v9t9_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t v9t9_read_be16(const uint8_t* p) { return (p[0] << 8) | p[1]; }

static bool v9t9_parse(const uint8_t* data, size_t size, v9t9_disk_t* disk) {
    if (!data || !disk || size < V9T9_SIZE_SS) return false;
    memset(disk, 0, sizeof(v9t9_disk_t));
    disk->diagnosis = v9t9_diagnosis_create();
    disk->source_size = size;
    
    /* Parse VIB (sector 0) */
    memcpy(disk->volume_name, data + V9T9_VIB_NAME, 10);
    disk->volume_name[10] = '\0';
    
    disk->total_sectors = v9t9_read_be16(data + V9T9_VIB_TOTAL);
    disk->sectors_per_track = data[V9T9_VIB_SECTORS];
    
    memcpy(disk->dsk_marker, data + V9T9_VIB_DSK, 3);
    disk->dsk_marker[3] = '\0';
    
    disk->protected = data[V9T9_VIB_PROTECTED];
    disk->tracks_per_side = data[V9T9_VIB_TRACKS];
    disk->sides = data[V9T9_VIB_SIDES];
    disk->density = data[V9T9_VIB_DENSITY];
    
    /* Validate DSK marker */
    if (memcmp(disk->dsk_marker, "DSK", 3) != 0) {
        disk->diagnosis->quality *= 0.5f;
    }
    
    /* Defaults if not set */
    if (disk->sectors_per_track == 0) disk->sectors_per_track = 9;
    if (disk->tracks_per_side == 0) disk->tracks_per_side = 40;
    if (disk->sides == 0) disk->sides = (size >= V9T9_SIZE_DS) ? 2 : 1;
    
    disk->disk_size = size;
    
    disk->score.sides = disk->sides;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void v9t9_disk_free(v9t9_disk_t* disk) {
    if (disk && disk->diagnosis) v9t9_diagnosis_free(disk->diagnosis);
}

#ifdef V9T9_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== V9T9 Parser v3 Tests ===\n");
    
    printf("Testing V9T9 parsing... ");
    uint8_t* v9t9 = calloc(1, V9T9_SIZE_DS);
    
    /* Set up VIB */
    memcpy(v9t9, "TESTDISK  ", 10);
    v9t9[V9T9_VIB_TOTAL] = 0x02;
    v9t9[V9T9_VIB_TOTAL + 1] = 0xD0;  /* 720 sectors BE */
    v9t9[V9T9_VIB_SECTORS] = 9;
    memcpy(v9t9 + V9T9_VIB_DSK, "DSK", 3);
    v9t9[V9T9_VIB_TRACKS] = 40;
    v9t9[V9T9_VIB_SIDES] = 2;
    
    v9t9_disk_t disk;
    bool ok = v9t9_parse(v9t9, V9T9_SIZE_DS, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.sides == 2);
    assert(disk.tracks_per_side == 40);
    v9t9_disk_free(&disk);
    free(v9t9);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
