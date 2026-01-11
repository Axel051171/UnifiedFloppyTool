/**
 * @file uft_dsk_ns_parser_v3.c
 * @brief GOD MODE DSK_NS Parser v3 - NorthStar Disk Format
 * 
 * NorthStar 5.25" Hard-Sector Format:
 * - 10 Hard-Sectors
 * - 35 Tracks
 * - 256/512 Bytes pro Sektor
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define NS_HARD_SECTORS         10
#define NS_TRACKS               35
#define NS_SIZE_SD              (35 * 10 * 256)   /* 89600 */
#define NS_SIZE_DD              (35 * 10 * 512)   /* 179200 */

typedef enum { NS_DIAG_OK = 0, NS_DIAG_INVALID_SIZE, NS_DIAG_COUNT } ns_diag_code_t;
typedef struct { float overall; bool valid; bool is_dd; } ns_score_t;
typedef struct { ns_diag_code_t code; char msg[128]; } ns_diagnosis_t;
typedef struct { ns_diagnosis_t* items; size_t count; size_t cap; float quality; } ns_diagnosis_list_t;

typedef struct {
    uint8_t tracks;
    uint8_t sectors_per_track;
    uint16_t sector_size;
    bool is_double_density;
    
    ns_score_t score;
    ns_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} ns_disk_t;

static ns_diagnosis_list_t* ns_diagnosis_create(void) {
    ns_diagnosis_list_t* l = calloc(1, sizeof(ns_diagnosis_list_t));
    if (l) { l->cap = 8; l->items = calloc(l->cap, sizeof(ns_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void ns_diagnosis_free(ns_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static bool ns_parse(const uint8_t* data, size_t size, ns_disk_t* disk) {
    if (!data || !disk || size < NS_SIZE_SD) return false;
    memset(disk, 0, sizeof(ns_disk_t));
    disk->diagnosis = ns_diagnosis_create();
    disk->source_size = size;
    
    disk->tracks = NS_TRACKS;
    disk->sectors_per_track = NS_HARD_SECTORS;
    disk->is_double_density = (size >= NS_SIZE_DD);
    disk->sector_size = disk->is_double_density ? 512 : 256;
    
    disk->score.is_dd = disk->is_double_density;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    return true;
}

static void ns_disk_free(ns_disk_t* disk) {
    if (disk && disk->diagnosis) ns_diagnosis_free(disk->diagnosis);
}

#ifdef DSK_NS_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== NorthStar Parser v3 Tests ===\n");
    printf("Testing... ");
    uint8_t* ns = calloc(1, NS_SIZE_SD);
    ns_disk_t disk;
    assert(ns_parse(ns, NS_SIZE_SD, &disk) && disk.valid);
    ns_disk_free(&disk); free(ns);
    printf("✓\n=== All tests passed! ===\n");
    return 0;
}
#endif
