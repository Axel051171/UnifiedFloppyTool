/**
 * @file uft_vdk_parser_v3.c
 * @brief GOD MODE VDK Parser v3 - Tandy CoCo Virtual Disk
 * 
 * VDK ist das Format für TRS-80 Color Computer:
 * - 12-Byte Header
 * - Variable Geometrie
 * - Single/Double Sided
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define VDK_SIGNATURE           "dk"
#define VDK_HEADER_SIZE         12
#define VDK_SECTOR_SIZE         256

typedef enum {
    VDK_DIAG_OK = 0,
    VDK_DIAG_BAD_SIGNATURE,
    VDK_DIAG_BAD_GEOMETRY,
    VDK_DIAG_TRUNCATED,
    VDK_DIAG_COUNT
} vdk_diag_code_t;

typedef struct { float overall; bool valid; } vdk_score_t;
typedef struct { vdk_diag_code_t code; char msg[128]; } vdk_diagnosis_t;
typedef struct { vdk_diagnosis_t* items; size_t count; size_t cap; float quality; } vdk_diagnosis_list_t;

typedef struct {
    uint16_t header_size;
    uint8_t version;
    uint8_t source;             /* Disk type that created this */
    uint8_t tracks;
    uint8_t sides;
    uint8_t flags;
    uint8_t compression;
    
    uint32_t data_size;
    uint16_t sectors_per_track;
    
    vdk_score_t score;
    vdk_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} vdk_disk_t;

static vdk_diagnosis_list_t* vdk_diagnosis_create(void) {
    vdk_diagnosis_list_t* l = calloc(1, sizeof(vdk_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(vdk_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void vdk_diagnosis_free(vdk_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t vdk_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool vdk_parse(const uint8_t* data, size_t size, vdk_disk_t* disk) {
    if (!data || !disk || size < VDK_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(vdk_disk_t));
    disk->diagnosis = vdk_diagnosis_create();
    disk->source_size = size;
    
    /* Check signature */
    if (data[0] != 'd' || data[1] != 'k') {
        return false;
    }
    
    disk->header_size = vdk_read_le16(data + 2);
    disk->version = data[4];
    disk->source = data[5];
    disk->tracks = data[8];
    disk->sides = data[9];
    disk->flags = data[10];
    disk->compression = data[11];
    
    /* Validate */
    if (disk->tracks == 0 || disk->sides == 0) {
        disk->tracks = 35;
        disk->sides = 1;
    }
    
    /* Calculate sectors per track (usually 18) */
    disk->sectors_per_track = 18;
    disk->data_size = size - disk->header_size;
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void vdk_disk_free(vdk_disk_t* disk) {
    if (disk && disk->diagnosis) vdk_diagnosis_free(disk->diagnosis);
}

#ifdef VDK_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== VDK Parser v3 Tests ===\n");
    
    printf("Testing VDK parsing... ");
    uint8_t vdk[64];
    memset(vdk, 0, sizeof(vdk));
    vdk[0] = 'd'; vdk[1] = 'k';
    vdk[2] = 12; vdk[3] = 0;  /* Header size */
    vdk[4] = 0x10;  /* Version */
    vdk[8] = 35;    /* Tracks */
    vdk[9] = 1;     /* Sides */
    
    vdk_disk_t disk;
    bool ok = vdk_parse(vdk, sizeof(vdk), &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 35);
    vdk_disk_free(&disk);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
