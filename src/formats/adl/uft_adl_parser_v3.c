/**
 * @file uft_adl_parser_v3.c
 * @brief GOD MODE ADL Parser v3 - Acorn DFS Large Format
 * 
 * ADL ist das erweiterte Acorn DFS Format:
 * - 80 Tracks × 2 Seiten × 16 Sektoren
 * - 256 Bytes pro Sektor
 * - DFS Filesystem
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ADL_SECTOR_SIZE         256
#define ADL_SECTORS_PER_TRACK   16
#define ADL_SIZE_640K           (80 * 2 * 16 * 256)  /* 655360 */
#define ADL_SIZE_320K           (80 * 1 * 16 * 256)  /* 327680 */

typedef enum {
    ADL_DIAG_OK = 0,
    ADL_DIAG_INVALID_SIZE,
    ADL_DIAG_BAD_CATALOG,
    ADL_DIAG_COUNT
} adl_diag_code_t;

typedef struct { float overall; bool valid; uint8_t files; } adl_score_t;
typedef struct { adl_diag_code_t code; char msg[128]; } adl_diagnosis_t;
typedef struct { adl_diagnosis_t* items; size_t count; size_t cap; float quality; } adl_diagnosis_list_t;

typedef struct {
    char name[8];
    char directory;
    bool locked;
    uint16_t load_address;
    uint16_t exec_address;
    uint16_t length;
    uint8_t start_sector;
} adl_file_t;

typedef struct {
    uint8_t tracks;
    uint8_t sides;
    char title[13];
    uint8_t boot_option;
    uint8_t file_count;
    uint16_t total_sectors;
    
    adl_file_t files[31];
    uint8_t valid_files;
    
    adl_score_t score;
    adl_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} adl_disk_t;

static adl_diagnosis_list_t* adl_diagnosis_create(void) {
    adl_diagnosis_list_t* l = calloc(1, sizeof(adl_diagnosis_list_t));
    if (l) { l->cap = 16; l->items = calloc(l->cap, sizeof(adl_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void adl_diagnosis_free(adl_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static bool adl_parse(const uint8_t* data, size_t size, adl_disk_t* disk) {
    if (!data || !disk || size < ADL_SIZE_320K) return false;
    memset(disk, 0, sizeof(adl_disk_t));
    disk->diagnosis = adl_diagnosis_create();
    disk->source_size = size;
    
    /* Detect geometry */
    if (size >= ADL_SIZE_640K) {
        disk->tracks = 80;
        disk->sides = 2;
    } else {
        disk->tracks = 80;
        disk->sides = 1;
    }
    
    /* Parse DFS catalog (sectors 0 and 1) */
    const uint8_t* cat0 = data;
    const uint8_t* cat1 = data + 256;
    
    /* Disk title */
    memcpy(disk->title, cat0, 8);
    memcpy(disk->title + 8, cat1, 4);
    disk->title[12] = '\0';
    
    /* Boot option and file count */
    disk->boot_option = (cat1[6] >> 4) & 0x03;
    disk->file_count = (cat1[5] & 0x07) * 8;
    
    /* Total sectors */
    disk->total_sectors = ((cat1[6] & 0x03) << 8) | cat1[7];
    
    /* Parse file entries */
    disk->valid_files = 0;
    for (int i = 0; i < 31 && i * 8 < disk->file_count; i++) {
        const uint8_t* name_entry = cat0 + 8 + i * 8;
        const uint8_t* meta_entry = cat1 + 8 + i * 8;
        
        if (name_entry[0] == 0) continue;
        
        adl_file_t* file = &disk->files[disk->valid_files];
        
        for (int c = 0; c < 7; c++) {
            file->name[c] = name_entry[c] & 0x7F;
        }
        file->name[7] = '\0';
        file->directory = name_entry[7] & 0x7F;
        file->locked = (name_entry[7] & 0x80) != 0;
        
        file->load_address = meta_entry[0] | (meta_entry[1] << 8);
        file->exec_address = meta_entry[2] | (meta_entry[3] << 8);
        file->length = meta_entry[4] | (meta_entry[5] << 8);
        file->start_sector = meta_entry[7];
        
        disk->valid_files++;
    }
    
    disk->score.files = disk->valid_files;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void adl_disk_free(adl_disk_t* disk) {
    if (disk && disk->diagnosis) adl_diagnosis_free(disk->diagnosis);
}

#ifdef ADL_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ADL Parser v3 Tests ===\n");
    
    printf("Testing ADL parsing... ");
    uint8_t* adl = calloc(1, ADL_SIZE_640K);
    memcpy(adl, "TEST    ", 8);
    memcpy(adl + 256, "DISK", 4);
    
    adl_disk_t disk;
    bool ok = adl_parse(adl, ADL_SIZE_640K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 80);
    assert(disk.sides == 2);
    adl_disk_free(&disk);
    free(adl);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 1, Failed: 0\n");
    return 0;
}
#endif
