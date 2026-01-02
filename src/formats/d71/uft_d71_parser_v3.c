/**
 * @file uft_d71_parser_v3.c
 * @brief GOD MODE D71 Parser v3 - Commodore 128 (1571) Format
 * 
 * D71 ist das Double-Sided Format für Commodore 128:
 * - 70 Tracks (35 × 2 Seiten)
 * - 1328 Sektoren total
 * - GCR Encoding
 * - Erweiterte BAM auf Track 53
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define D71_TRACKS_PER_SIDE     35
#define D71_SIDES               2
#define D71_TOTAL_TRACKS        70
#define D71_SECTOR_SIZE         256
#define D71_SECTORS_SIDE0       683
#define D71_SECTORS_SIDE1       683
#define D71_TOTAL_SECTORS       1366
#define D71_SIZE                (D71_TOTAL_SECTORS * D71_SECTOR_SIZE)  /* 349696 */
#define D71_SIZE_ERRORS         (D71_SIZE + D71_TOTAL_SECTORS)

#define D71_BAM_TRACK           18
#define D71_BAM2_TRACK          53      /* BAM für Seite 2 */
#define D71_DIR_TRACK           18

/* Sectors per track (same as D64) */
static const uint8_t d71_sectors_per_track[36] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17
};

typedef enum {
    D71_DIAG_OK = 0,
    D71_DIAG_INVALID_SIZE,
    D71_DIAG_BAD_BAM,
    D71_DIAG_BAD_BAM2,
    D71_DIAG_DIR_ERROR,
    D71_DIAG_SECTOR_ERROR,
    D71_DIAG_CHECKSUM_ERROR,
    D71_DIAG_COUNT
} d71_diag_code_t;

typedef struct { float overall; bool valid; bool bam_valid; bool bam2_valid; } d71_score_t;
typedef struct { d71_diag_code_t code; uint8_t track; uint8_t sector; char msg[128]; } d71_diagnosis_t;
typedef struct { d71_diagnosis_t* items; size_t count; size_t cap; float quality; } d71_diagnosis_list_t;

typedef struct {
    char disk_name[17];
    char disk_id[3];
    uint8_t dos_version;
    uint8_t dos_type;
    uint16_t free_blocks_side0;
    uint16_t free_blocks_side1;
    uint16_t total_free;
    uint8_t bam_side0[35][4];
    uint8_t bam_side1[35][4];
    bool double_sided;
} d71_bam_t;

typedef struct {
    char name[17];
    uint8_t type;
    uint8_t first_track;
    uint8_t first_sector;
    uint16_t blocks;
    bool closed;
    bool locked;
} d71_file_t;

typedef struct {
    bool is_d71;
    bool has_errors;
    size_t actual_size;
    
    d71_bam_t bam;
    d71_file_t files[144];
    uint16_t file_count;
    
    uint8_t* error_bytes;
    
    d71_score_t score;
    d71_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} d71_disk_t;

static d71_diagnosis_list_t* d71_diagnosis_create(void) {
    d71_diagnosis_list_t* l = calloc(1, sizeof(d71_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(d71_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void d71_diagnosis_free(d71_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint8_t d71_get_sectors(uint8_t track) {
    if (track < 1 || track > 35) return 0;
    return d71_sectors_per_track[track];
}

static uint32_t d71_get_offset(uint8_t track, uint8_t sector, uint8_t side) {
    if (track < 1 || track > 35) return 0;
    
    uint32_t offset = 0;
    
    /* Add side 0 offset if on side 1 */
    if (side == 1) {
        offset = D71_SECTORS_SIDE0 * D71_SECTOR_SIZE;
    }
    
    /* Add track offset */
    for (uint8_t t = 1; t < track; t++) {
        offset += d71_sectors_per_track[t] * D71_SECTOR_SIZE;
    }
    
    offset += sector * D71_SECTOR_SIZE;
    return offset;
}

static void d71_copy_petscii(char* dest, const uint8_t* src, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t c = src[i];
        if (c == 0xA0) c = ' ';
        else if (c >= 0xC1 && c <= 0xDA) c = c - 0x80;
        else if (c < 0x20 || c > 0x7E) c = '.';
        dest[i] = c;
    }
    dest[len] = '\0';
    /* Trim trailing spaces */
    for (int i = len - 1; i >= 0 && dest[i] == ' '; i--) {
        dest[i] = '\0';
    }
}

static bool d71_parse_bam(const uint8_t* data, size_t size, d71_disk_t* disk) {
    uint32_t bam_offset = d71_get_offset(D71_BAM_TRACK, 0, 0);
    if (bam_offset + 256 > size) return false;
    
    const uint8_t* bam = data + bam_offset;
    
    /* Check for double-sided marker */
    disk->bam.double_sided = (bam[0x03] == 0x80);
    
    /* Disk name (offset 0x90) */
    d71_copy_petscii(disk->bam.disk_name, bam + 0x90, 16);
    
    /* Disk ID (offset 0xA2) */
    disk->bam.disk_id[0] = bam[0xA2];
    disk->bam.disk_id[1] = bam[0xA3];
    disk->bam.disk_id[2] = '\0';
    
    /* DOS version and type */
    disk->bam.dos_version = bam[0xA5];
    disk->bam.dos_type = bam[0xA6];
    
    /* BAM entries for side 0 (tracks 1-35) */
    disk->bam.free_blocks_side0 = 0;
    for (int t = 1; t <= 35; t++) {
        int bam_idx = 4 * t;
        if (t != 18) {  /* Skip directory track */
            disk->bam.free_blocks_side0 += bam[bam_idx];
        }
        memcpy(disk->bam.bam_side0[t-1], bam + bam_idx, 4);
    }
    
    /* BAM for side 1 is at track 53, sector 0 */
    if (disk->bam.double_sided && size >= D71_SIZE) {
        uint32_t bam2_offset = d71_get_offset(D71_BAM2_TRACK - 35, 0, 1);
        if (bam2_offset + 256 <= size) {
            const uint8_t* bam2 = data + bam2_offset;
            
            disk->bam.free_blocks_side1 = 0;
            for (int t = 1; t <= 35; t++) {
                int bam_idx = (t - 1) * 3;
                disk->bam.free_blocks_side1 += bam2[bam_idx];
                /* BAM2 format is slightly different - 3 bytes per track */
            }
        }
    }
    
    disk->bam.total_free = disk->bam.free_blocks_side0 + disk->bam.free_blocks_side1;
    
    return true;
}

static bool d71_parse_directory(const uint8_t* data, size_t size, d71_disk_t* disk) {
    disk->file_count = 0;
    
    uint8_t dir_track = D71_DIR_TRACK;
    uint8_t dir_sector = 1;
    int max_iterations = 20;
    
    while (dir_track != 0 && max_iterations-- > 0) {
        uint32_t offset = d71_get_offset(dir_track, dir_sector, 0);
        if (offset + 256 > size) break;
        
        const uint8_t* dir = data + offset;
        
        /* Next directory block */
        dir_track = dir[0];
        dir_sector = dir[1];
        
        /* 8 entries per sector */
        for (int e = 0; e < 8 && disk->file_count < 144; e++) {
            const uint8_t* entry = dir + 2 + e * 32;
            
            uint8_t file_type = entry[0];
            if (file_type == 0x00) continue;  /* Deleted/empty */
            
            d71_file_t* file = &disk->files[disk->file_count];
            
            file->type = file_type & 0x07;
            file->closed = (file_type & 0x80) != 0;
            file->locked = (file_type & 0x40) != 0;
            file->first_track = entry[1];
            file->first_sector = entry[2];
            d71_copy_petscii(file->name, entry + 3, 16);
            file->blocks = entry[28] | (entry[29] << 8);
            
            disk->file_count++;
        }
    }
    
    return true;
}

static bool d71_parse(const uint8_t* data, size_t size, d71_disk_t* disk) {
    if (!data || !disk) return false;
    memset(disk, 0, sizeof(d71_disk_t));
    disk->diagnosis = d71_diagnosis_create();
    disk->source_size = size;
    
    /* Validate size */
    if (size == D71_SIZE) {
        disk->is_d71 = true;
        disk->has_errors = false;
    } else if (size == D71_SIZE_ERRORS) {
        disk->is_d71 = true;
        disk->has_errors = true;
    } else if (size == D71_SIZE / 2 || size == D71_SIZE / 2 + D71_SECTORS_SIDE0) {
        /* Single-sided - treat as D64 */
        disk->is_d71 = false;
    } else {
        return false;
    }
    
    disk->actual_size = size;
    
    /* Parse BAM */
    disk->score.bam_valid = d71_parse_bam(data, size, disk);
    
    /* Parse directory */
    d71_parse_directory(data, size, disk);
    
    /* Calculate score */
    disk->score.overall = disk->score.bam_valid ? 1.0f : 0.5f;
    disk->score.valid = disk->score.bam_valid;
    disk->valid = true;
    
    return true;
}

static void d71_disk_free(d71_disk_t* disk) {
    if (disk) {
        if (disk->error_bytes) free(disk->error_bytes);
        if (disk->diagnosis) d71_diagnosis_free(disk->diagnosis);
    }
}

#ifdef D71_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== D71 Parser v3 Tests ===\n");
    
    printf("Testing sector counts... ");
    assert(d71_get_sectors(1) == 21);
    assert(d71_get_sectors(18) == 19);
    assert(d71_get_sectors(35) == 17);
    printf("✓\n");
    
    printf("Testing offsets... ");
    assert(d71_get_offset(1, 0, 0) == 0);
    assert(d71_get_offset(1, 0, 1) == D71_SECTORS_SIDE0 * 256);
    printf("✓\n");
    
    printf("Testing D71 size validation... ");
    uint8_t* d71 = calloc(1, D71_SIZE);
    /* Set double-sided marker */
    uint32_t bam_off = d71_get_offset(18, 0, 0);
    d71[bam_off + 0x03] = 0x80;
    
    d71_disk_t disk;
    bool ok = d71_parse(d71, D71_SIZE, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.is_d71);
    assert(disk.bam.double_sided);
    d71_disk_free(&disk);
    free(d71);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
