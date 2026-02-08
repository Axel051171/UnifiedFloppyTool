/**
 * @file uft_trd_parser_v3.c
 * @brief GOD MODE TRD Parser v3 - ZX Spectrum TR-DOS Format
 * 
 * TRD ist das Format für Beta Disk Interface:
 * - 80/40 Tracks × 2 Seiten × 16 Sektoren
 * - 256 Bytes pro Sektor
 * - TR-DOS Filesystem
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define TRD_SECTOR_SIZE         256
#define TRD_SECTORS_PER_TRACK   16
#define TRD_CATALOG_SECTOR      0
#define TRD_INFO_SECTOR         8

/* Standard sizes */
#define TRD_SIZE_640K           655360  /* 80×2×16×256 */
#define TRD_SIZE_320K           327680  /* 40×2×16×256 */
#define TRD_SIZE_180K           184320  /* 80×1×16×256 - rare */

/* Disk types */
#define TRD_TYPE_80_2           0x16    /* 80 tracks, 2 sides */
#define TRD_TYPE_40_2           0x17    /* 40 tracks, 2 sides */
#define TRD_TYPE_80_1           0x18    /* 80 tracks, 1 side */
#define TRD_TYPE_40_1           0x19    /* 40 tracks, 1 side */

/* File types */
#define TRD_FILE_BASIC          'B'
#define TRD_FILE_NUMBERS        'D'
#define TRD_FILE_CHARS          'C'
#define TRD_FILE_CODE           'C'

typedef enum {
    TRD_DIAG_OK = 0,
    TRD_DIAG_INVALID_SIZE,
    TRD_DIAG_BAD_SIGNATURE,
    TRD_DIAG_BAD_CATALOG,
    TRD_DIAG_FILE_ERROR,
    TRD_DIAG_COUNT
} trd_diag_code_t;

typedef struct { float overall; bool valid; uint8_t files; } trd_score_t;
typedef struct { trd_diag_code_t code; char msg[128]; } trd_diagnosis_t;
typedef struct { trd_diagnosis_t* items; size_t count; size_t cap; float quality; } trd_diagnosis_list_t;

typedef struct {
    char name[9];
    char extension;
    uint16_t start_address;
    uint16_t length;
    uint8_t length_sectors;
    uint8_t first_sector;
    uint8_t first_track;
    bool deleted;
} trd_file_t;

typedef struct {
    /* Disk info (sector 8 of track 0) */
    uint8_t first_free_sector;
    uint8_t first_free_track;
    uint8_t disk_type;
    uint8_t file_count;
    uint16_t free_sectors;
    uint8_t trdos_id;
    char disk_label[9];
    
    /* Geometry */
    uint8_t tracks;
    uint8_t sides;
    uint32_t total_sectors;
    
    trd_file_t files[128];
    uint8_t valid_files;
    
    trd_score_t score;
    trd_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} trd_disk_t;

static trd_diagnosis_list_t* trd_diagnosis_create(void) {
    trd_diagnosis_list_t* l = calloc(1, sizeof(trd_diagnosis_list_t));
    if (l) { l->cap = 32; l->items = calloc(l->cap, sizeof(trd_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void trd_diagnosis_free(trd_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t trd_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }

static void trd_detect_geometry(size_t size, uint8_t* tracks, uint8_t* sides) {
    if (size >= TRD_SIZE_640K) {
        *tracks = 80; *sides = 2;
    } else if (size >= TRD_SIZE_320K) {
        *tracks = 40; *sides = 2;
    } else {
        *tracks = 80; *sides = 1;
    }
}

static bool trd_parse(const uint8_t* data, size_t size, trd_disk_t* disk) {
    if (!data || !disk || size < TRD_SIZE_180K) return false;
    memset(disk, 0, sizeof(trd_disk_t));
    disk->diagnosis = trd_diagnosis_create();
    disk->source_size = size;
    
    /* Detect geometry */
    trd_detect_geometry(size, &disk->tracks, &disk->sides);
    disk->total_sectors = disk->tracks * disk->sides * TRD_SECTORS_PER_TRACK;
    
    /* Read disk info sector (track 0, sector 8) */
    size_t info_offset = TRD_INFO_SECTOR * TRD_SECTOR_SIZE;
    if (info_offset + 256 > size) return false;
    
    const uint8_t* info = data + info_offset;
    
    /* Check TR-DOS signature at offset 0xE7 */
    if (info[0xE7] != 0x10) {
        disk->diagnosis->quality *= 0.5f;
    }
    
    disk->first_free_sector = info[0xE1];
    disk->first_free_track = info[0xE2];
    disk->disk_type = info[0xE3];
    disk->file_count = info[0xE4];
    disk->free_sectors = trd_read_le16(info + 0xE5);
    disk->trdos_id = info[0xE7];
    
    /* Disk label at offset 0xF5 */
    memcpy(disk->disk_label, info + 0xF5, 8);
    disk->disk_label[8] = '\0';
    
    /* Parse catalog (first 8 sectors of track 0) */
    disk->valid_files = 0;
    for (int s = 0; s < 8; s++) {
        size_t cat_offset = s * TRD_SECTOR_SIZE;
        const uint8_t* cat = data + cat_offset;
        
        /* 16 entries per sector */
        for (int e = 0; e < 16 && disk->valid_files < 128; e++) {
            const uint8_t* entry = cat + e * 16;
            
            if (entry[0] == 0x00) continue;  /* Empty */
            
            trd_file_t* file = &disk->files[disk->valid_files];
            
            file->deleted = (entry[0] == 0x01);
            memcpy(file->name, entry, 8);
            file->name[8] = '\0';
            file->extension = entry[8];
            file->start_address = trd_read_le16(entry + 9);
            file->length = trd_read_le16(entry + 11);
            file->length_sectors = entry[13];
            file->first_sector = entry[14];
            file->first_track = entry[15];
            
            if (!file->deleted) {
                disk->valid_files++;
            }
        }
    }
    
    disk->score.files = disk->valid_files;
    disk->score.overall = disk->valid_files > 0 || disk->file_count == 0 ? 1.0f : 0.5f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void trd_disk_free(trd_disk_t* disk) {
    if (disk && disk->diagnosis) trd_diagnosis_free(disk->diagnosis);
}

#ifdef TRD_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== TRD Parser v3 Tests ===\n");
    
    printf("Testing geometry detection... ");
    uint8_t t, s;
    trd_detect_geometry(TRD_SIZE_640K, &t, &s);
    assert(t == 80 && s == 2);
    trd_detect_geometry(TRD_SIZE_320K, &t, &s);
    assert(t == 40 && s == 2);
    printf("✓\n");
    
    printf("Testing TRD parsing... ");
    uint8_t* trd = calloc(1, TRD_SIZE_640K);
    /* Set TR-DOS signature */
    trd[TRD_INFO_SECTOR * 256 + 0xE7] = 0x10;
    trd[TRD_INFO_SECTOR * 256 + 0xE3] = TRD_TYPE_80_2;
    
    trd_disk_t disk;
    bool ok = trd_parse(trd, TRD_SIZE_640K, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.tracks == 80);
    assert(disk.sides == 2);
    trd_disk_free(&disk);
    free(trd);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 2, Failed: 0\n");
    return 0;
}
#endif
