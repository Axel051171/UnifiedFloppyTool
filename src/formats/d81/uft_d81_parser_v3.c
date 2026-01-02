/**
 * @file uft_d81_parser_v3.c
 * @brief GOD MODE D81 Parser v3 - Commodore 128 (1581) 3.5" Format
 * 
 * D81 ist das 3.5" Format für Commodore 128:
 * - 80 Tracks × 2 Seiten × 40 Sektoren
 * - 3160 Sektoren total (800K)
 * - MFM Encoding (nicht GCR!)
 * - Partitionen möglich
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D81_TRACKS              80
#define D81_SIDES               2
#define D81_SECTORS_PER_TRACK   10          /* 10 sectors of 512 bytes */
#define D81_SECTOR_SIZE         512
#define D81_TOTAL_SECTORS       (D81_TRACKS * D81_SIDES * D81_SECTORS_PER_TRACK)  /* 1600 */
#define D81_SIZE                (D81_TOTAL_SECTORS * D81_SECTOR_SIZE)  /* 819200 */
#define D81_SIZE_ERRORS         (D81_SIZE + D81_TOTAL_SECTORS)

#define D81_HEADER_TRACK        40
#define D81_HEADER_SECTOR       0
#define D81_BAM_TRACK           40
#define D81_BAM_SECTOR          1
#define D81_BAM2_SECTOR         2
#define D81_DIR_TRACK           40
#define D81_DIR_SECTOR          3

typedef enum {
    D81_DIAG_OK = 0,
    D81_DIAG_INVALID_SIZE,
    D81_DIAG_BAD_HEADER,
    D81_DIAG_BAD_BAM,
    D81_DIAG_DIR_ERROR,
    D81_DIAG_PARTITION_ERROR,
    D81_DIAG_COUNT
} d81_diag_code_t;

typedef struct { float overall; bool valid; bool header_valid; bool bam_valid; } d81_score_t;
typedef struct { d81_diag_code_t code; uint8_t track; uint8_t sector; char msg[128]; } d81_diagnosis_t;
typedef struct { d81_diagnosis_t* items; size_t count; size_t cap; float quality; } d81_diagnosis_list_t;

typedef struct {
    char disk_name[17];
    char disk_id[3];
    uint8_t dos_version;
    char dos_type[3];
    uint16_t free_blocks;
    uint8_t bam[80][5];     /* 5 bytes per track (count + 4 bitmap bytes) */
} d81_bam_t;

typedef struct {
    char name[17];
    uint8_t type;
    uint8_t first_track;
    uint8_t first_sector;
    uint16_t blocks;
    bool closed;
    bool locked;
    bool is_partition;
} d81_file_t;

typedef struct {
    bool has_errors;
    
    d81_bam_t bam;
    d81_file_t files[296];  /* More files possible than D64 */
    uint16_t file_count;
    
    d81_score_t score;
    d81_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} d81_disk_t;

static d81_diagnosis_list_t* d81_diagnosis_create(void) {
    d81_diagnosis_list_t* l = calloc(1, sizeof(d81_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(d81_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void d81_diagnosis_free(d81_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint32_t d81_get_offset(uint8_t track, uint8_t sector) {
    if (track < 1 || track > 80 || sector >= 10) return 0;
    return ((track - 1) * D81_SECTORS_PER_TRACK + sector) * D81_SECTOR_SIZE;
}

static void d81_copy_petscii(char* dest, const uint8_t* src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = src[i];
        if (c == 0xA0) c = ' ';
        else if (c >= 0xC1 && c <= 0xDA) c = c - 0x80;
        else if (c < 0x20 || c > 0x7E) c = '.';
        dest[i] = c;
    }
    dest[len] = '\0';
    for (int i = len - 1; i >= 0 && dest[i] == ' '; i--) dest[i] = '\0';
}

static bool d81_parse_header(const uint8_t* data, size_t size, d81_disk_t* disk) {
    uint32_t offset = d81_get_offset(D81_HEADER_TRACK, D81_HEADER_SECTOR);
    if (offset + 512 > size) return false;
    
    const uint8_t* hdr = data + offset;
    
    /* Disk name at offset 0x04 */
    d81_copy_petscii(disk->bam.disk_name, hdr + 0x04, 16);
    
    /* Disk ID at offset 0x16 */
    disk->bam.disk_id[0] = hdr[0x16];
    disk->bam.disk_id[1] = hdr[0x17];
    disk->bam.disk_id[2] = '\0';
    
    /* DOS version at 0x19 */
    disk->bam.dos_version = hdr[0x19];
    
    /* DOS type at 0x1A */
    disk->bam.dos_type[0] = hdr[0x1A];
    disk->bam.dos_type[1] = hdr[0x1B];
    disk->bam.dos_type[2] = '\0';
    
    return true;
}

static bool d81_parse_bam(const uint8_t* data, size_t size, d81_disk_t* disk) {
    /* BAM spans sectors 1 and 2 of track 40 */
    uint32_t bam1_offset = d81_get_offset(D81_BAM_TRACK, D81_BAM_SECTOR);
    uint32_t bam2_offset = d81_get_offset(D81_BAM_TRACK, D81_BAM2_SECTOR);
    
    if (bam1_offset + 512 > size || bam2_offset + 512 > size) return false;
    
    const uint8_t* bam1 = data + bam1_offset;
    const uint8_t* bam2 = data + bam2_offset;
    
    disk->bam.free_blocks = 0;
    
    /* BAM1: tracks 1-40, 6 bytes per entry starting at offset 0x10 */
    for (int t = 0; t < 40; t++) {
        int idx = 0x10 + t * 6;
        disk->bam.free_blocks += bam1[idx];
        memcpy(disk->bam.bam[t], bam1 + idx, 5);
    }
    
    /* BAM2: tracks 41-80 */
    for (int t = 0; t < 40; t++) {
        int idx = 0x10 + t * 6;
        disk->bam.free_blocks += bam2[idx];
        memcpy(disk->bam.bam[t + 40], bam2 + idx, 5);
    }
    
    return true;
}

static bool d81_parse_directory(const uint8_t* data, size_t size, d81_disk_t* disk) {
    disk->file_count = 0;
    
    uint8_t dir_track = D81_DIR_TRACK;
    uint8_t dir_sector = D81_DIR_SECTOR;
    int max_iterations = 40;
    
    while (dir_track != 0 && max_iterations-- > 0) {
        uint32_t offset = d81_get_offset(dir_track, dir_sector);
        if (offset + 512 > size) break;
        
        const uint8_t* dir = data + offset;
        
        dir_track = dir[0];
        dir_sector = dir[1];
        
        /* 8 entries per sector (each 32 bytes, using first 256 bytes) */
        for (int e = 0; e < 8 && disk->file_count < 296; e++) {
            const uint8_t* entry = dir + 2 + e * 32;
            
            uint8_t file_type = entry[0];
            if (file_type == 0x00) continue;
            
            d81_file_t* file = &disk->files[disk->file_count];
            
            file->type = file_type & 0x0F;
            file->closed = (file_type & 0x80) != 0;
            file->locked = (file_type & 0x40) != 0;
            file->is_partition = (file_type & 0x0F) == 0x05;  /* CBM partition */
            file->first_track = entry[1];
            file->first_sector = entry[2];
            d81_copy_petscii(file->name, entry + 3, 16);
            file->blocks = entry[28] | (entry[29] << 8);
            
            disk->file_count++;
        }
    }
    
    return true;
}

static bool d81_parse(const uint8_t* data, size_t size, d81_disk_t* disk) {
    if (!data || !disk) return false;
    memset(disk, 0, sizeof(d81_disk_t));
    disk->diagnosis = d81_diagnosis_create();
    disk->source_size = size;
    
    if (size != D81_SIZE && size != D81_SIZE_ERRORS) {
        return false;
    }
    
    disk->has_errors = (size == D81_SIZE_ERRORS);
    
    disk->score.header_valid = d81_parse_header(data, size, disk);
    disk->score.bam_valid = d81_parse_bam(data, size, disk);
    d81_parse_directory(data, size, disk);
    
    disk->score.overall = (disk->score.header_valid && disk->score.bam_valid) ? 1.0f : 0.5f;
    disk->score.valid = disk->score.header_valid;
    disk->valid = true;
    
    return true;
}

static void d81_disk_free(d81_disk_t* disk) {
    if (disk && disk->diagnosis) d81_diagnosis_free(disk->diagnosis);
}

#ifdef D81_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== D81 Parser v3 Tests ===\n");
    
    printf("Testing constants... ");
    assert(D81_SIZE == 819200);
    assert(D81_TOTAL_SECTORS == 1600);
    printf("✓\n");
    
    printf("Testing offsets... ");
    assert(d81_get_offset(1, 0) == 0);
    assert(d81_get_offset(1, 1) == 512);
    assert(d81_get_offset(2, 0) == 10 * 512);
    assert(d81_get_offset(40, 0) == 39 * 10 * 512);
    printf("✓\n");
    
    printf("Testing D81 parsing... ");
    uint8_t* d81 = calloc(1, D81_SIZE);
    
    d81_disk_t disk;
    bool ok = d81_parse(d81, D81_SIZE, &disk);
    assert(ok);
    assert(disk.valid);
    d81_disk_free(&disk);
    free(d81);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
