/**
 * @file uft_d71_parser_v2.c
 * @brief GOD MODE D71 Parser v2 - Commodore 128 Double-Sided Disk
 * 
 * D71 is the Commodore 1571 disk format (double-sided D64).
 * - 70 tracks (35 per side)
 * - 1366 sectors
 * - GCR encoding with variable sectors per track
 * - Two BAM sectors (one per side)
 * 
 * @author UFT Team / GOD MODE
 * @version 2.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define D71_SECTOR_SIZE         256
#define D71_TRACKS              70      /* 35 per side */
#define D71_TRACKS_PER_SIDE     35
#define D71_SECTORS             1366    /* 683 * 2 */

#define D71_SIZE                (D71_SECTORS * D71_SECTOR_SIZE)  /* 349696 */
#define D71_SIZE_ERRORS         (D71_SIZE + D71_SECTORS)         /* 351062 */

/* BAM locations */
#define D71_BAM_TRACK           18
#define D71_BAM_SECTOR          0
#define D71_BAM2_TRACK          53      /* Second BAM for side 2 */
#define D71_BAM2_SECTOR         0

/* Directory */
#define D71_DIR_TRACK           18
#define D71_DIR_SECTOR          1
#define D71_DIR_ENTRIES_PER_SECTOR 8
#define D71_MAX_DIR_ENTRIES     144

/* Sectors per track (same as D64) */
static const uint8_t d71_sectors_per_track[] = {
    0,  /* Track 0 doesn't exist */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 1-17 */
    19, 19, 19, 19, 19, 19, 19,  /* 18-24 */
    18, 18, 18, 18, 18, 18,      /* 25-30 */
    17, 17, 17, 17, 17,          /* 31-35 */
    /* Side 2 (tracks 36-70 = same pattern) */
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /* 36-52 */
    19, 19, 19, 19, 19, 19, 19,  /* 53-59 */
    18, 18, 18, 18, 18, 18,      /* 60-65 */
    17, 17, 17, 17, 17           /* 66-70 */
};

/* Track offsets (cumulative sectors) */
static const uint16_t d71_track_offset[] = {
    0,    /* Track 0 */
    /* Side 1 */
    0, 21, 42, 63, 84, 105, 126, 147, 168, 189,
    210, 231, 252, 273, 294, 315, 336,
    357, 376, 395, 414, 433, 452, 471,
    490, 508, 526, 544, 562, 580,
    598, 615, 632, 649, 666,
    /* Side 2 (starts at 683) */
    683, 704, 725, 746, 767, 788, 809, 830, 851, 872,
    893, 914, 935, 956, 977, 998, 1019,
    1040, 1059, 1078, 1097, 1116, 1135, 1154,
    1173, 1191, 1209, 1227, 1245, 1263,
    1281, 1298, 1315, 1332, 1349
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    D71_TYPE_DEL = 0,
    D71_TYPE_SEQ = 1,
    D71_TYPE_PRG = 2,
    D71_TYPE_USR = 3,
    D71_TYPE_REL = 4,
    D71_TYPE_UNKNOWN = 255
} d71_file_type_t;

typedef struct {
    uint8_t free_sectors;
    uint8_t bitmap[3];
} d71_bam_entry_t;

typedef struct {
    uint8_t file_type;
    uint8_t first_track;
    uint8_t first_sector;
    char filename[17];
    uint16_t blocks;
    d71_file_type_t type;
    bool locked;
    bool closed;
    bool deleted;
} d71_dir_entry_t;

typedef struct {
    char disk_name[17];
    char disk_id[6];
    uint8_t dos_type;
    
    uint8_t num_tracks;
    uint16_t num_sectors;
    bool has_errors;
    bool double_sided;
    
    d71_bam_entry_t bam[71];
    uint16_t free_blocks;
    
    d71_dir_entry_t directory[D71_MAX_DIR_ENTRIES];
    uint16_t dir_entries;
    
    uint8_t* error_bytes;
    uint16_t total_errors;
    
    bool valid;
    char error[256];
} d71_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint8_t d71_sectors_for_track(uint8_t track) {
    if (track < 1 || track > 70) return 0;
    return d71_sectors_per_track[track];
}

static size_t d71_sector_offset(uint8_t track, uint8_t sector) {
    if (track < 1 || track > 70) return 0;
    if (sector >= d71_sectors_per_track[track]) return 0;
    return (d71_track_offset[track] + sector) * D71_SECTOR_SIZE;
}

static const char* d71_file_type_name(d71_file_type_t type) {
    static const char* names[] = { "DEL", "SEQ", "PRG", "USR", "REL" };
    if (type <= D71_TYPE_REL) return names[type];
    return "???";
}

static char petscii_to_ascii(uint8_t c) {
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;
    if (c >= 0x20 && c <= 0x7E) return c;
    if (c == 0xA0) return ' ';
    return '.';
}

static void d71_copy_filename(char* dest, const uint8_t* src, size_t len) {
    size_t i;
    for (i = 0; i < len && src[i] != 0xA0 && src[i] != 0x00; i++) {
        dest[i] = petscii_to_ascii(src[i]);
    }
    dest[i] = '\0';
}

static bool d71_is_valid_size(size_t size, bool* has_errors) {
    *has_errors = false;
    if (size == D71_SIZE) return true;
    if (size == D71_SIZE_ERRORS) {
        *has_errors = true;
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool d71_parse_bam(const uint8_t* data, size_t size, d71_disk_t* disk) {
    /* Side 1 BAM at track 18, sector 0 */
    size_t bam_offset = d71_sector_offset(D71_BAM_TRACK, D71_BAM_SECTOR);
    if (bam_offset + D71_SECTOR_SIZE > size) return false;
    
    const uint8_t* bam = data + bam_offset;
    
    disk->dos_type = bam[2];
    disk->double_sided = (bam[3] & 0x80) != 0;
    
    /* BAM entries for tracks 1-35 */
    disk->free_blocks = 0;
    for (int track = 1; track <= 35; track++) {
        size_t entry_offset = 4 + (track - 1) * 4;
        disk->bam[track].free_sectors = bam[entry_offset];
        disk->bam[track].bitmap[0] = bam[entry_offset + 1];
        disk->bam[track].bitmap[1] = bam[entry_offset + 2];
        disk->bam[track].bitmap[2] = bam[entry_offset + 3];
        
        if (track != D71_BAM_TRACK) {
            disk->free_blocks += disk->bam[track].free_sectors;
        }
    }
    
    /* Disk name and ID */
    d71_copy_filename(disk->disk_name, bam + 0x90, 16);
    disk->disk_id[0] = petscii_to_ascii(bam[0xA2]);
    disk->disk_id[1] = petscii_to_ascii(bam[0xA3]);
    disk->disk_id[2] = ' ';
    disk->disk_id[3] = petscii_to_ascii(bam[0xA5]);
    disk->disk_id[4] = petscii_to_ascii(bam[0xA6]);
    disk->disk_id[5] = '\0';
    
    /* Side 2 BAM at track 53, sector 0 */
    if (disk->double_sided) {
        size_t bam2_offset = d71_sector_offset(D71_BAM2_TRACK, D71_BAM2_SECTOR);
        if (bam2_offset + D71_SECTOR_SIZE <= size) {
            const uint8_t* bam2 = data + bam2_offset;
            
            /* Free sector counts for tracks 36-70 at BAM offset 0x00-0x22 */
            for (int track = 36; track <= 70; track++) {
                int idx = track - 36;
                disk->bam[track].free_sectors = bam2[idx];
                
                /* Bitmap in main BAM at 0xDD + 3 bytes per track */
                size_t bitmap_offset = 0xDD + (track - 36) * 3;
                if (bitmap_offset + 3 <= D71_SECTOR_SIZE) {
                    disk->bam[track].bitmap[0] = bam[bitmap_offset];
                    disk->bam[track].bitmap[1] = bam[bitmap_offset + 1];
                    disk->bam[track].bitmap[2] = bam[bitmap_offset + 2];
                }
                
                if (track != D71_BAM2_TRACK) {
                    disk->free_blocks += disk->bam[track].free_sectors;
                }
            }
        }
    }
    
    return true;
}

static bool d71_parse_dir_entry(const uint8_t* entry, d71_dir_entry_t* dir) {
    memset(dir, 0, sizeof(d71_dir_entry_t));
    
    dir->file_type = entry[2];
    dir->first_track = entry[3];
    dir->first_sector = entry[4];
    
    uint8_t type_code = dir->file_type & 0x07;
    dir->type = (type_code <= D71_TYPE_REL) ? type_code : D71_TYPE_UNKNOWN;
    dir->locked = (dir->file_type & 0x40) != 0;
    dir->closed = (dir->file_type & 0x80) != 0;
    dir->deleted = (dir->file_type == 0);
    
    d71_copy_filename(dir->filename, entry + 5, 16);
    dir->blocks = entry[30] | (entry[31] << 8);
    
    return !dir->deleted && dir->first_track > 0;
}

static bool d71_parse_directory(const uint8_t* data, size_t size, d71_disk_t* disk) {
    uint8_t track = D71_DIR_TRACK;
    uint8_t sector = D71_DIR_SECTOR;
    disk->dir_entries = 0;
    
    int max_sectors = 38;  /* Both sides */
    
    while (track != 0 && max_sectors-- > 0) {
        size_t offset = d71_sector_offset(track, sector);
        if (offset + D71_SECTOR_SIZE > size) break;
        
        const uint8_t* sec = data + offset;
        
        for (int i = 0; i < D71_DIR_ENTRIES_PER_SECTOR; i++) {
            if (disk->dir_entries >= D71_MAX_DIR_ENTRIES) break;
            
            const uint8_t* entry = sec + 2 + i * 32;
            if (entry[2] == 0) continue;
            
            d71_dir_entry_t* dir = &disk->directory[disk->dir_entries];
            if (d71_parse_dir_entry(sec + i * 32, dir)) {
                disk->dir_entries++;
            }
        }
        
        track = sec[0];
        sector = sec[1];
    }
    
    return true;
}

static bool d71_parse(const uint8_t* data, size_t size, d71_disk_t* disk) {
    memset(disk, 0, sizeof(d71_disk_t));
    
    if (!d71_is_valid_size(size, &disk->has_errors)) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid D71 size: %zu bytes (expected %d)", size, D71_SIZE);
        return false;
    }
    
    disk->num_tracks = D71_TRACKS;
    disk->num_sectors = D71_SECTORS;
    
    if (!d71_parse_bam(data, size, disk)) return false;
    if (!d71_parse_directory(data, size, disk)) return false;
    
    if (disk->has_errors) {
        size_t error_offset = D71_SIZE;
        disk->error_bytes = malloc(D71_SECTORS);
        if (disk->error_bytes) {
            memcpy(disk->error_bytes, data + error_offset, D71_SECTORS);
            for (uint16_t i = 0; i < D71_SECTORS; i++) {
                if (disk->error_bytes[i] != 0x01 && disk->error_bytes[i] != 0) {
                    disk->total_errors++;
                }
            }
        }
    }
    
    disk->valid = true;
    return true;
}

static void d71_free(d71_disk_t* disk) {
    if (disk && disk->error_bytes) {
        free(disk->error_bytes);
        disk->error_bytes = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef D71_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== D71 Parser v2 Tests ===\n");
    
    printf("Testing valid_sizes... ");
    bool has_errors;
    assert(d71_is_valid_size(D71_SIZE, &has_errors) && !has_errors);
    assert(d71_is_valid_size(D71_SIZE_ERRORS, &has_errors) && has_errors);
    assert(!d71_is_valid_size(12345, &has_errors));
    printf("✓\n");
    
    printf("Testing sectors_per_track... ");
    assert(d71_sectors_for_track(1) == 21);
    assert(d71_sectors_for_track(36) == 21);
    assert(d71_sectors_for_track(53) == 19);
    printf("✓\n");
    
    printf("Testing sector_offset... ");
    assert(d71_sector_offset(1, 0) == 0);
    assert(d71_sector_offset(36, 0) == 683 * 256);
    printf("✓\n");
    
    printf("Testing file_type_names... ");
    assert(strcmp(d71_file_type_name(D71_TYPE_PRG), "PRG") == 0);
    assert(strcmp(d71_file_type_name(D71_TYPE_SEQ), "SEQ") == 0);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* D71_PARSER_TEST */
