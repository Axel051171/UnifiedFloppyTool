/**
 * @file uft_mgt_parser_v3.c
 * @brief GOD MODE MGT Parser v3 - ZX Spectrum +3/Sam Coupe Format
 * 
 * MGT ist das Format für:
 * - Sam Coupe
 * - ZX Spectrum +3 (MGT+D/Disciple)
 * - 80 Tracks × 2 Seiten × 10 Sektoren
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define MGT_SECTOR_SIZE         512
#define MGT_SECTORS_PER_TRACK   10
#define MGT_TRACKS              80
#define MGT_SIDES               2
#define MGT_SIZE                (MGT_TRACKS * MGT_SIDES * MGT_SECTORS_PER_TRACK * MGT_SECTOR_SIZE)  /* 819200 */

/* Directory entry structure */
#define MGT_DIR_ENTRY_SIZE      256
#define MGT_MAX_FILES           80

typedef enum {
    MGT_DIAG_OK = 0,
    MGT_DIAG_INVALID_SIZE,
    MGT_DIAG_BAD_DIRECTORY,
    MGT_DIAG_FILE_ERROR,
    MGT_DIAG_COUNT
} mgt_diag_code_t;

typedef struct { float overall; bool valid; uint8_t files; } mgt_score_t;
typedef struct { mgt_diag_code_t code; char msg[128]; } mgt_diagnosis_t;
typedef struct { mgt_diagnosis_t* items; size_t count; size_t cap; float quality; } mgt_diagnosis_list_t;

typedef struct {
    uint8_t type;
    char name[11];
    uint16_t sectors;
    uint8_t first_track;
    uint8_t first_sector;
    uint8_t sector_map[195];
    uint8_t flags;
    uint16_t start_address;
    uint16_t length;
    uint16_t exec_address;
} mgt_file_t;

typedef struct {
    mgt_file_t files[MGT_MAX_FILES];
    uint8_t file_count;
    uint16_t free_sectors;
    
    mgt_score_t score;
    mgt_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} mgt_disk_t;

static mgt_diagnosis_list_t* mgt_diagnosis_create(void) {
    mgt_diagnosis_list_t* l = calloc(1, sizeof(mgt_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(mgt_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void mgt_diagnosis_free(mgt_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t mgt_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static bool mgt_parse(const uint8_t* data, size_t size, mgt_disk_t* disk) {
    if (!data || !disk || size < MGT_SIZE / 2) return false;
    memset(disk, 0, sizeof(mgt_disk_t));
    disk->diagnosis = mgt_diagnosis_create();
    disk->source_size = size;
    
    /* Directory is in first 4 tracks (sectors 1-80) */
    /* Each directory entry is 256 bytes */
    disk->file_count = 0;
    
    for (int i = 0; i < MGT_MAX_FILES; i++) {
        size_t offset = i * MGT_DIR_ENTRY_SIZE;
        if (offset + MGT_DIR_ENTRY_SIZE > size) break;
        
        const uint8_t* entry = data + offset;
        
        /* Type 0 = free, type > 0 = used */
        if (entry[0] == 0) continue;
        
        mgt_file_t* file = &disk->files[disk->file_count];
        
        file->type = entry[0];
        memcpy(file->name, entry + 1, 10);
        file->name[10] = '\0';
        file->sectors = mgt_read_le16(entry + 11);
        file->first_track = entry[13];
        file->first_sector = entry[14];
        
        /* Sam Coupe specific fields */
        file->flags = entry[15];
        file->start_address = mgt_read_le16(entry + 232);
        file->length = mgt_read_le16(entry + 234) + (entry[236] << 16);
        file->exec_address = mgt_read_le16(entry + 237);
        
        disk->file_count++;
    }
    
    /* Calculate free sectors */
    disk->free_sectors = (MGT_TRACKS * MGT_SIDES * MGT_SECTORS_PER_TRACK) - 80;  /* Minus directory */
    for (int i = 0; i < disk->file_count; i++) {
        disk->free_sectors -= disk->files[i].sectors;
    }
    
    disk->score.files = disk->file_count;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void mgt_disk_free(mgt_disk_t* disk) {
    if (disk && disk->diagnosis) mgt_diagnosis_free(disk->diagnosis);
}

#ifdef MGT_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== MGT Parser v3 Tests ===\n");
    
    printf("Testing MGT constants... ");
    assert(MGT_SIZE == 819200);
    printf("✓\n");
    
    printf("Testing MGT parsing... ");
    uint8_t* mgt = calloc(1, MGT_SIZE);
    
    mgt_disk_t disk;
    bool ok = mgt_parse(mgt, MGT_SIZE, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.file_count == 0);  /* Empty disk */
    mgt_disk_free(&disk);
    free(mgt);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
