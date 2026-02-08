/**
 * @file uft_mbd_parser_v3.c
 * @brief GOD MODE MBD Parser v3 - Microbee Disk Format
 * 
 * MBD ist das Microbee Disk-Format:
 * - 40/80 Tracks
 * - CP/M kompatibel
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

#define MBD_SECTOR_SIZE         512
#define MBD_SIZE_200K           (40 * 1 * 10 * 512)
#define MBD_SIZE_400K           (40 * 2 * 10 * 512)
#define MBD_SIZE_800K           (80 * 2 * 10 * 512)

typedef enum {
    MBD_DIAG_OK = 0,
    MBD_DIAG_INVALID_SIZE,
    MBD_DIAG_COUNT
} mbd_diag_code_t;

typedef struct { float overall; bool valid; } mbd_score_t;
typedef struct { mbd_diag_code_t code; char msg[128]; } mbd_diagnosis_t;
typedef struct { mbd_diagnosis_t* items; size_t count; size_t cap; float quality; } mbd_diagnosis_list_t;

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    
    mbd_score_t score;
    mbd_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} mbd_disk_t;

static mbd_diagnosis_list_t* mbd_diagnosis_create(void) {
    mbd_diagnosis_list_t* l = calloc(1, sizeof(mbd_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(mbd_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void mbd_diagnosis_free(mbd_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static bool mbd_parse(const uint8_t* data, size_t size, mbd_disk_t* disk) {
    if (!data || !disk || size < MBD_SIZE_200K) return false;
    memset(disk, 0, sizeof(mbd_disk_t));
    disk->diagnosis = mbd_diagnosis_create();
    disk->source_size = size;
    
    /* Detect geometry */
    if (size >= MBD_SIZE_800K) {
        disk->tracks = 80;
        disk->sides = 2;
    } else if (size >= MBD_SIZE_400K) {
        disk->tracks = 40;
        disk->sides = 2;
    } else {
        disk->tracks = 40;
        disk->sides = 1;
    }
    disk->sectors_per_track = 10;
    disk->sector_size = MBD_SECTOR_SIZE;
    
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void mbd_disk_free(mbd_disk_t* disk) {
    if (disk && disk->diagnosis) mbd_diagnosis_free(disk->diagnosis);
}

#ifdef MBD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== Microbee MBD Parser v3 Tests ===\n");
    
    printf("Testing MBD parsing... ");
    uint8_t* mbd = calloc(1, MBD_SIZE_800K);
    
    mbd_disk_t disk;
    bool ok = mbd_parse(mbd, MBD_SIZE_800K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 80);
    mbd_disk_free(&disk);
    free(mbd);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
