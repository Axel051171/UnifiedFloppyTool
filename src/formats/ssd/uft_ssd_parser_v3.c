/**
 * @file uft_ssd_parser_v3.c
 * @brief GOD MODE SSD Parser v3 - BBC Micro Single-Sided Disk
 * 
 * SSD/DSD sind die BBC Micro Disk-Formate:
 * - SSD: Single-sided (100K/200K)
 * - DSD: Double-sided (200K/400K)
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

#define SSD_SECTOR_SIZE         256
#define SSD_SECTORS_PER_TRACK   10
#define SSD_CATALOG_SIZE        512     /* Sectors 0 and 1 */

/* Standard sizes */
#define SSD_SIZE_40T            102400  /* 40 tracks × 10 sectors × 256 */
#define SSD_SIZE_80T            204800  /* 80 tracks × 10 sectors × 256 */
#define DSD_SIZE_40T            204800  /* 40 tracks × 2 sides */
#define DSD_SIZE_80T            409600  /* 80 tracks × 2 sides */

typedef enum {
    SSD_DIAG_OK = 0,
    SSD_DIAG_INVALID_SIZE,
    SSD_DIAG_BAD_CATALOG,
    SSD_DIAG_FILE_ERROR,
    SSD_DIAG_COUNT
} ssd_diag_code_t;

typedef struct { float overall; bool valid; uint8_t files; bool is_dsd; } ssd_score_t;
typedef struct { ssd_diag_code_t code; char msg[128]; } ssd_diagnosis_t;
typedef struct { ssd_diagnosis_t* items; size_t count; size_t cap; float quality; } ssd_diagnosis_list_t;

typedef struct {
    char name[8];
    char directory;
    bool locked;
    uint16_t load_address;
    uint16_t exec_address;
    uint16_t length;
    uint8_t start_sector;
    uint8_t start_track;
} ssd_file_t;

typedef struct {
    /* Catalog info */
    char title[13];
    uint8_t cycle;
    uint8_t file_count;
    uint8_t boot_option;
    uint16_t total_sectors;
    
    /* Geometry */
    uint8_t tracks;
    uint8_t sides;
    bool is_dsd;
    
    ssd_file_t files[31];   /* Max 31 files in DFS */
    uint8_t valid_files;
    
    ssd_score_t score;
    ssd_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} ssd_disk_t;

static ssd_diagnosis_list_t* ssd_diagnosis_create(void) {
    ssd_diagnosis_list_t* l = calloc(1, sizeof(ssd_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(ssd_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void ssd_diagnosis_free(ssd_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static void ssd_detect_geometry(size_t size, uint8_t* tracks, uint8_t* sides, bool* is_dsd) {
    *is_dsd = false;
    if (size == SSD_SIZE_40T) {
        *tracks = 40; *sides = 1;
    } else if (size == SSD_SIZE_80T) {
        *tracks = 80; *sides = 1;
    } else if (size == DSD_SIZE_40T) {
        *tracks = 40; *sides = 2; *is_dsd = true;
    } else if (size == DSD_SIZE_80T) {
        *tracks = 80; *sides = 2; *is_dsd = true;
    } else if (size <= SSD_SIZE_80T) {
        *tracks = 80; *sides = 1;
    } else {
        *tracks = 80; *sides = 2; *is_dsd = true;
    }
}

static bool ssd_parse(const uint8_t* data, size_t size, ssd_disk_t* disk) {
    if (!data || !disk || size < SSD_CATALOG_SIZE) return false;
    memset(disk, 0, sizeof(ssd_disk_t));
    disk->diagnosis = ssd_diagnosis_create();
    disk->source_size = size;
    
    /* Detect geometry */
    ssd_detect_geometry(size, &disk->tracks, &disk->sides, &disk->is_dsd);
    
    /* Sector 0: First part of catalog (filenames) */
    /* Sector 1: Second part of catalog (metadata) */
    const uint8_t* cat0 = data;
    const uint8_t* cat1 = data + 256;
    
    /* Disk title (first 8 chars in sector 0, next 4 in sector 1) */
    memcpy(disk->title, cat0, 8);
    memcpy(disk->title + 8, cat1, 4);
    disk->title[12] = '\0';
    
    /* Cycle number */
    disk->cycle = cat1[4];
    
    /* File count (bits 0-2 of byte 5, sector 1) * 8 */
    disk->file_count = (cat1[5] & 0x07) * 8;
    
    /* Boot option (bits 4-5 of byte 6, sector 1) */
    disk->boot_option = (cat1[6] >> 4) & 0x03;
    
    /* Total sectors (bits 0-1 of byte 6 + byte 7) */
    disk->total_sectors = ((cat1[6] & 0x03) << 8) | cat1[7];
    
    /* Parse file entries */
    disk->valid_files = 0;
    for (int i = 0; i < 31 && i * 8 < disk->file_count; i++) {
        const uint8_t* name_entry = cat0 + 8 + i * 8;
        const uint8_t* meta_entry = cat1 + 8 + i * 8;
        
        /* Check if entry is valid */
        if (name_entry[0] == 0x00) continue;
        
        ssd_file_t* file = &disk->files[disk->valid_files];
        
        /* Filename (7 chars) */
        for (int c = 0; c < 7; c++) {
            file->name[c] = name_entry[c] & 0x7F;
        }
        file->name[7] = '\0';
        
        /* Directory (bit 7 of last char encodes locked status) */
        file->directory = name_entry[7] & 0x7F;
        file->locked = (name_entry[7] & 0x80) != 0;
        
        /* Load/Exec addresses and length */
        file->load_address = meta_entry[0] | (meta_entry[1] << 8);
        file->exec_address = meta_entry[2] | (meta_entry[3] << 8);
        file->length = meta_entry[4] | (meta_entry[5] << 8);
        file->start_sector = meta_entry[7];
        file->start_track = (meta_entry[6] >> 2) & 0x03;
        
        disk->valid_files++;
    }
    
    disk->score.files = disk->valid_files;
    disk->score.is_dsd = disk->is_dsd;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void ssd_disk_free(ssd_disk_t* disk) {
    if (disk && disk->diagnosis) ssd_diagnosis_free(disk->diagnosis);
}

#ifdef SSD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== SSD Parser v3 Tests ===\n");
    
    printf("Testing geometry detection... ");
    uint8_t t, s; bool dsd;
    ssd_detect_geometry(SSD_SIZE_40T, &t, &s, &dsd);
    assert(t == 40 && s == 1 && !dsd);
    ssd_detect_geometry(DSD_SIZE_80T, &t, &s, &dsd);
    assert(t == 80 && s == 2 && dsd);
    printf("✓\n");
    
    printf("Testing SSD parsing... ");
    uint8_t* ssd = calloc(1, SSD_SIZE_40T);
    memcpy(ssd, "TEST    ", 8);
    memcpy(ssd + 256, "DISK", 4);
    ssd[256 + 5] = 0x01;  /* 1 file entry * 8 = 8 */
    
    ssd_disk_t disk;
    bool ok = ssd_parse(ssd, SSD_SIZE_40T, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 40);
    assert(!disk.is_dsd);
    ssd_disk_free(&disk);
    free(ssd);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
