/**
 * @file uft_rx_parser_v3.c
 * @brief GOD MODE RX Parser v3 - DEC RX01/RX02 Disk Format
 * 
 * DEC 8" Floppy Formate:
 * - RX01: 256K Single Density
 * - RX02: 512K Double Density
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define RX01_SECTOR_SIZE        128
#define RX02_SECTOR_SIZE        256
#define RX_TRACKS               77
#define RX_SECTORS              26
#define RX01_SIZE               (77 * 26 * 128)  /* 256256 */
#define RX02_SIZE               (77 * 26 * 256)  /* 512512 */

typedef enum {
    RX_DIAG_OK = 0,
    RX_DIAG_INVALID_SIZE,
    RX_DIAG_COUNT
} rx_diag_code_t;

typedef enum { RX_TYPE_RX01 = 1, RX_TYPE_RX02 = 2 } rx_type_t;

typedef struct { float overall; bool valid; rx_type_t type; } rx_score_t;
typedef struct { rx_diag_code_t code; char msg[128]; } rx_diagnosis_t;
typedef struct { rx_diagnosis_t* items; size_t count; size_t cap; float quality; } rx_diagnosis_list_t;

typedef struct {
    rx_type_t type;
    uint8_t tracks;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    uint32_t total_sectors;
    
    rx_score_t score;
    rx_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} rx_disk_t;

static rx_diagnosis_list_t* rx_diagnosis_create(void) {
    rx_diagnosis_list_t* l = calloc(1, sizeof(rx_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(rx_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void rx_diagnosis_free(rx_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static bool rx_parse(const uint8_t* data, size_t size, rx_disk_t* disk) {
    if (!data || !disk || size < RX01_SIZE) return false;
    memset(disk, 0, sizeof(rx_disk_t));
    disk->diagnosis = rx_diagnosis_create();
    disk->source_size = size;
    
    if (size >= RX02_SIZE) {
        disk->type = RX_TYPE_RX02;
        disk->sector_size = RX02_SECTOR_SIZE;
    } else {
        disk->type = RX_TYPE_RX01;
        disk->sector_size = RX01_SECTOR_SIZE;
    }
    
    disk->tracks = RX_TRACKS;
    disk->sectors_per_track = RX_SECTORS;
    disk->total_sectors = disk->tracks * disk->sectors_per_track;
    
    disk->score.type = disk->type;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void rx_disk_free(rx_disk_t* disk) {
    if (disk && disk->diagnosis) rx_diagnosis_free(disk->diagnosis);
}

#ifdef RX_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== DEC RX Parser v3 Tests ===\n");
    printf("Testing RX01... ");
    uint8_t* rx = calloc(1, RX01_SIZE);
    rx_disk_t disk;
    bool ok = rx_parse(rx, RX01_SIZE, &disk);
    assert(ok && disk.type == RX_TYPE_RX01);
    rx_disk_free(&disk);
    free(rx);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
