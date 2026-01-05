/**
 * @file uft_po_parser_v3.c
 * @brief GOD MODE PO Parser v3 - Apple II ProDOS Order
 * 
 * PO ist das ProDOS Sektor-Order Format:
 * - 35/40/80 Tracks × 16 Sektoren
 * - 256 Bytes pro Sektor
 * - ProDOS Block-Order (unterschiedlich von DOS 3.3)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define PO_SECTOR_SIZE          256
#define PO_SECTORS_PER_TRACK    16
#define PO_BLOCK_SIZE           512

/* Standard sizes */
#define PO_SIZE_140K            (35 * 16 * 256)     /* 143360 - 5.25" */
#define PO_SIZE_160K            (40 * 16 * 256)     /* 163840 - 5.25" */
#define PO_SIZE_800K            (1600 * 512)        /* 819200 - 3.5" */

/* ProDOS sector interleave table */
static const uint8_t po_interleave[16] = {
    0, 8, 1, 9, 2, 10, 3, 11, 4, 12, 5, 13, 6, 14, 7, 15
};

typedef enum {
    PO_DIAG_OK = 0,
    PO_DIAG_INVALID_SIZE,
    PO_DIAG_BAD_VOLUME,
    PO_DIAG_COUNT
} po_diag_code_t;

typedef struct { float overall; bool valid; uint8_t tracks; } po_score_t;
typedef struct { po_diag_code_t code; char msg[128]; } po_diagnosis_t;
typedef struct { po_diagnosis_t* items; size_t count; size_t cap; float quality; } po_diagnosis_list_t;

typedef struct {
    uint8_t tracks;
    uint16_t blocks;
    bool is_prodos;
    
    /* ProDOS Volume Directory */
    uint8_t storage_type;
    char volume_name[16];
    uint16_t total_blocks;
    uint16_t bitmap_pointer;
    
    po_score_t score;
    po_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} po_disk_t;

static po_diagnosis_list_t* po_diagnosis_create(void) {
    po_diagnosis_list_t* l = calloc(1, sizeof(po_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(po_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void po_diagnosis_free(po_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t po_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool po_parse_volume(const uint8_t* data, size_t size, po_disk_t* disk) {
    /* ProDOS volume directory is at block 2 */
    if (size < 3 * PO_BLOCK_SIZE) return false;
    
    const uint8_t* vol = data + 2 * PO_BLOCK_SIZE;
    
    /* Check storage type (high nibble should be 0xF for volume header) */
    disk->storage_type = vol[4] >> 4;
    if (disk->storage_type != 0x0F) {
        return false;
    }
    
    /* Volume name length */
    uint8_t name_len = vol[4] & 0x0F;
    if (name_len > 15) name_len = 15;
    
    memcpy(disk->volume_name, vol + 5, name_len);
    disk->volume_name[name_len] = '\0';
    
    /* Total blocks at offset 0x29 */
    disk->total_blocks = po_read_le16(vol + 0x29);
    
    /* Bitmap pointer at offset 0x27 */
    disk->bitmap_pointer = po_read_le16(vol + 0x27);
    
    disk->is_prodos = true;
    return true;
}

static bool po_parse(const uint8_t* data, size_t size, po_disk_t* disk) {
    if (!data || !disk || size < PO_SIZE_140K) return false;
    memset(disk, 0, sizeof(po_disk_t));
    disk->diagnosis = po_diagnosis_create();
    disk->source_size = size;
    
    /* Detect geometry */
    if (size == PO_SIZE_140K) {
        disk->tracks = 35;
        disk->blocks = 280;
    } else if (size == PO_SIZE_160K) {
        disk->tracks = 40;
        disk->blocks = 320;
    } else if (size == PO_SIZE_800K) {
        disk->tracks = 0;  /* Block device */
        disk->blocks = 1600;
    } else {
        /* Try to calculate */
        disk->blocks = size / PO_BLOCK_SIZE;
        disk->tracks = (size / PO_SECTOR_SIZE) / PO_SECTORS_PER_TRACK;
    }
    
    /* Try to parse ProDOS volume */
    po_parse_volume(data, size, disk);
    
    disk->score.tracks = disk->tracks;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void po_disk_free(po_disk_t* disk) {
    if (disk && disk->diagnosis) po_diagnosis_free(disk->diagnosis);
}

#ifdef PO_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== PO Parser v3 Tests ===\n");
    
    printf("Testing PO parsing... ");
    uint8_t* po = calloc(1, PO_SIZE_140K);
    
    po_disk_t disk;
    bool ok = po_parse(po, PO_SIZE_140K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 35);
    assert(disk.blocks == 280);
    po_disk_free(&disk);
    free(po);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
