/**
 * @file uft_d81_parser_v2.c
 * @brief GOD MODE D81 Parser v2 - Commodore 1581 3.5" Disk
 * 
 * D81 is the Commodore 1581 disk format (3.5" DD).
 * - 80 tracks, double-sided
 * - 40 sectors per track (10 per quarter track)
 * - 3200 sectors total (256 bytes each)
 * - MFM encoding (not GCR!)
 * - 819200 bytes (800KB)
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

#define D81_SECTOR_SIZE         256
#define D81_TRACKS              80
#define D81_SIDES               2
#define D81_SECTORS_PER_TRACK   40
#define D81_TOTAL_SECTORS       (D81_TRACKS * D81_SIDES * D81_SECTORS_PER_TRACK / 2)  /* 3200 */

#define D81_SIZE                (D81_TOTAL_SECTORS * D81_SECTOR_SIZE)  /* 819200 */
#define D81_SIZE_ERRORS         (D81_SIZE + D81_TOTAL_SECTORS)         /* 822400 */

/* Header/BAM track */
#define D81_HEADER_TRACK        40
#define D81_HEADER_SECTOR       0
#define D81_BAM_TRACK           40
#define D81_BAM_SECTOR          1
#define D81_BAM2_SECTOR         2

/* Directory */
#define D81_DIR_TRACK           40
#define D81_DIR_SECTOR          3
#define D81_DIR_ENTRIES_PER_SECTOR 8
#define D81_MAX_DIR_ENTRIES     296

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    D81_TYPE_DEL = 0,
    D81_TYPE_SEQ = 1,
    D81_TYPE_PRG = 2,
    D81_TYPE_USR = 3,
    D81_TYPE_REL = 4,
    D81_TYPE_CBM = 5,   /* Partition */
    D81_TYPE_UNKNOWN = 255
} d81_file_type_t;

typedef struct {
    uint8_t free_sectors;
    uint8_t bitmap[5];  /* 40 bits for 40 sectors */
} d81_bam_entry_t;

typedef struct {
    uint8_t file_type;
    uint8_t first_track;
    uint8_t first_sector;
    char filename[17];
    uint16_t blocks;
    d81_file_type_t type;
    bool locked;
    bool closed;
    bool deleted;
    /* REL file info */
    uint8_t side_track;
    uint8_t side_sector;
    uint8_t record_length;
} d81_dir_entry_t;

typedef struct {
    char disk_name[17];
    char disk_id[6];
    uint8_t dos_version;
    
    uint8_t num_tracks;
    uint16_t num_sectors;
    bool has_errors;
    
    d81_bam_entry_t bam[81];  /* Tracks 1-80 */
    uint16_t free_blocks;
    
    d81_dir_entry_t directory[D81_MAX_DIR_ENTRIES];
    uint16_t dir_entries;
    
    uint8_t* error_bytes;
    uint16_t total_errors;
    
    bool valid;
    char error[256];
} d81_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static size_t d81_sector_offset(uint8_t track, uint8_t sector) {
    if (track < 1 || track > 80 || sector >= 40) return 0;
    return ((track - 1) * 40 + sector) * D81_SECTOR_SIZE;
}

static const char* d81_file_type_name(d81_file_type_t type) {
    static const char* names[] = { "DEL", "SEQ", "PRG", "USR", "REL", "CBM" };
    if (type <= D81_TYPE_CBM) return names[type];
    return "???";
}

static char petscii_to_ascii(uint8_t c) {
    if (c >= 0x41 && c <= 0x5A) return c + 0x20;
    if (c >= 0xC1 && c <= 0xDA) return c - 0x80;
    if (c >= 0x20 && c <= 0x7E) return c;
    if (c == 0xA0) return ' ';
    return '.';
}

static void d81_copy_filename(char* dest, const uint8_t* src, size_t len) {
    size_t i;
    for (i = 0; i < len && src[i] != 0xA0 && src[i] != 0x00; i++) {
        dest[i] = petscii_to_ascii(src[i]);
    }
    dest[i] = '\0';
}

static bool d81_is_valid_size(size_t size, bool* has_errors) {
    *has_errors = false;
    if (size == D81_SIZE) return true;
    if (size == D81_SIZE_ERRORS) {
        *has_errors = true;
        return true;
    }
    return false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool d81_parse_header(const uint8_t* data, size_t size, d81_disk_t* disk) {
    size_t header_offset = d81_sector_offset(D81_HEADER_TRACK, D81_HEADER_SECTOR);
    if (header_offset + D81_SECTOR_SIZE > size) return false;
    
    const uint8_t* header = data + header_offset;
    
    /* Disk name at offset 0x04 */
    d81_copy_filename(disk->disk_name, header + 0x04, 16);
    
    /* Disk ID at offset 0x16 */
    disk->disk_id[0] = petscii_to_ascii(header[0x16]);
    disk->disk_id[1] = petscii_to_ascii(header[0x17]);
    disk->disk_id[2] = ' ';
    disk->disk_id[3] = petscii_to_ascii(header[0x19]);  /* DOS type */
    disk->disk_id[4] = petscii_to_ascii(header[0x1A]);
    disk->disk_id[5] = '\0';
    
    disk->dos_version = header[0x19];
    
    return true;
}

static bool d81_parse_bam(const uint8_t* data, size_t size, d81_disk_t* disk) {
    /* BAM 1: Tracks 1-40 */
    size_t bam_offset = d81_sector_offset(D81_BAM_TRACK, D81_BAM_SECTOR);
    if (bam_offset + D81_SECTOR_SIZE > size) return false;
    
    const uint8_t* bam = data + bam_offset;
    
    disk->free_blocks = 0;
    
    /* Each track entry: 6 bytes (free count + 5 bitmap bytes) */
    for (int track = 1; track <= 40; track++) {
        size_t entry_offset = 0x10 + (track - 1) * 6;
        disk->bam[track].free_sectors = bam[entry_offset];
        memcpy(disk->bam[track].bitmap, bam + entry_offset + 1, 5);
        
        if (track != D81_BAM_TRACK) {
            disk->free_blocks += disk->bam[track].free_sectors;
        }
    }
    
    /* BAM 2: Tracks 41-80 */
    size_t bam2_offset = d81_sector_offset(D81_BAM_TRACK, D81_BAM2_SECTOR);
    if (bam2_offset + D81_SECTOR_SIZE > size) return false;
    
    const uint8_t* bam2 = data + bam2_offset;
    
    for (int track = 41; track <= 80; track++) {
        size_t entry_offset = 0x10 + (track - 41) * 6;
        disk->bam[track].free_sectors = bam2[entry_offset];
        memcpy(disk->bam[track].bitmap, bam2 + entry_offset + 1, 5);
        disk->free_blocks += disk->bam[track].free_sectors;
    }
    
    return true;
}

static bool d81_parse_dir_entry(const uint8_t* entry, d81_dir_entry_t* dir) {
    memset(dir, 0, sizeof(d81_dir_entry_t));
    
    dir->file_type = entry[2];
    dir->first_track = entry[3];
    dir->first_sector = entry[4];
    
    uint8_t type_code = dir->file_type & 0x0F;
    if (type_code <= D81_TYPE_CBM) {
        dir->type = type_code;
    } else {
        dir->type = D81_TYPE_UNKNOWN;
    }
    
    dir->locked = (dir->file_type & 0x40) != 0;
    dir->closed = (dir->file_type & 0x80) != 0;
    dir->deleted = (dir->file_type == 0);
    
    d81_copy_filename(dir->filename, entry + 5, 16);
    
    dir->side_track = entry[21];
    dir->side_sector = entry[22];
    dir->record_length = entry[23];
    
    dir->blocks = entry[30] | (entry[31] << 8);
    
    return !dir->deleted && dir->first_track > 0;
}

static bool d81_parse_directory(const uint8_t* data, size_t size, d81_disk_t* disk) {
    uint8_t track = D81_DIR_TRACK;
    uint8_t sector = D81_DIR_SECTOR;
    disk->dir_entries = 0;
    
    int max_sectors = 40;
    
    while (track != 0 && max_sectors-- > 0) {
        size_t offset = d81_sector_offset(track, sector);
        if (offset + D81_SECTOR_SIZE > size) break;
        
        const uint8_t* sec = data + offset;
        
        for (int i = 0; i < D81_DIR_ENTRIES_PER_SECTOR; i++) {
            if (disk->dir_entries >= D81_MAX_DIR_ENTRIES) break;
            
            const uint8_t* entry = sec + 2 + i * 32;
            if (entry[2] == 0) continue;
            
            d81_dir_entry_t* dir = &disk->directory[disk->dir_entries];
            if (d81_parse_dir_entry(sec + i * 32, dir)) {
                disk->dir_entries++;
            }
        }
        
        track = sec[0];
        sector = sec[1];
    }
    
    return true;
}

static bool d81_parse(const uint8_t* data, size_t size, d81_disk_t* disk) {
    memset(disk, 0, sizeof(d81_disk_t));
    
    if (!d81_is_valid_size(size, &disk->has_errors)) {
        snprintf(disk->error, sizeof(disk->error),
                 "Invalid D81 size: %zu bytes (expected %d)", size, D81_SIZE);
        return false;
    }
    
    disk->num_tracks = D81_TRACKS;
    disk->num_sectors = D81_TOTAL_SECTORS;
    
    if (!d81_parse_header(data, size, disk)) return false;
    if (!d81_parse_bam(data, size, disk)) return false;
    if (!d81_parse_directory(data, size, disk)) return false;
    
    disk->valid = true;
    return true;
}

static uint8_t* d81_create_blank(const char* disk_name, const char* disk_id, size_t* out_size) {
    uint8_t* data = calloc(1, D81_SIZE);
    if (!data) {
        *out_size = 0;
        return NULL;
    }
    
    /* Initialize header sector (40/0) */
    size_t header_offset = d81_sector_offset(D81_HEADER_TRACK, D81_HEADER_SECTOR);
    uint8_t* header = data + header_offset;
    
    header[0] = D81_BAM_TRACK;
    header[1] = D81_BAM_SECTOR;
    header[2] = 0x44;  /* 'D' */
    header[3] = 0x00;
    
    /* Disk name */
    memset(header + 0x04, 0xA0, 16);
    if (disk_name) {
        size_t len = strlen(disk_name);
        if (len > 16) len = 16;
        for (size_t i = 0; i < len; i++) {
            header[0x04 + i] = toupper((unsigned char)disk_name[i]);
        }
    }
    
    header[0x14] = 0xA0;
    header[0x15] = 0xA0;
    
    /* Disk ID */
    if (disk_id && strlen(disk_id) >= 2) {
        header[0x16] = toupper((unsigned char)disk_id[0]);
        header[0x17] = toupper((unsigned char)disk_id[1]);
    } else {
        header[0x16] = '0';
        header[0x17] = '0';
    }
    
    header[0x18] = 0xA0;
    header[0x19] = '3';  /* DOS version */
    header[0x1A] = 'D';
    
    /* Initialize BAM sectors */
    size_t bam_offset = d81_sector_offset(D81_BAM_TRACK, D81_BAM_SECTOR);
    uint8_t* bam = data + bam_offset;
    
    bam[0] = D81_BAM_TRACK;
    bam[1] = D81_BAM2_SECTOR;
    bam[2] = 0x44;
    bam[3] = 0xBB;  /* ID */
    
    /* All sectors free except track 40 */
    for (int track = 1; track <= 40; track++) {
        size_t entry_offset = 0x10 + (track - 1) * 6;
        if (track == D81_BAM_TRACK) {
            bam[entry_offset] = 36;  /* 4 sectors used */
            bam[entry_offset + 1] = 0xF0;
            bam[entry_offset + 2] = 0xFF;
            bam[entry_offset + 3] = 0xFF;
            bam[entry_offset + 4] = 0xFF;
            bam[entry_offset + 5] = 0xFF;
        } else {
            bam[entry_offset] = 40;
            memset(bam + entry_offset + 1, 0xFF, 5);
        }
    }
    
    /* BAM 2 */
    size_t bam2_offset = d81_sector_offset(D81_BAM_TRACK, D81_BAM2_SECTOR);
    uint8_t* bam2 = data + bam2_offset;
    
    bam2[0] = 0;
    bam2[1] = 0xFF;
    bam2[2] = 0x44;
    bam2[3] = 0xBB;
    
    for (int track = 41; track <= 80; track++) {
        size_t entry_offset = 0x10 + (track - 41) * 6;
        bam2[entry_offset] = 40;
        memset(bam2 + entry_offset + 1, 0xFF, 5);
    }
    
    /* Directory sector */
    size_t dir_offset = d81_sector_offset(D81_DIR_TRACK, D81_DIR_SECTOR);
    data[dir_offset] = 0;
    data[dir_offset + 1] = 0xFF;
    
    *out_size = D81_SIZE;
    return data;
}

static void d81_free(d81_disk_t* disk) {
    if (disk && disk->error_bytes) {
        free(disk->error_bytes);
        disk->error_bytes = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef D81_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== D81 Parser v2 Tests ===\n");
    
    printf("Testing valid_sizes... ");
    bool has_errors;
    assert(d81_is_valid_size(D81_SIZE, &has_errors) && !has_errors);
    assert(d81_is_valid_size(D81_SIZE_ERRORS, &has_errors) && has_errors);
    assert(!d81_is_valid_size(12345, &has_errors));
    printf("✓\n");
    
    printf("Testing sector_offset... ");
    assert(d81_sector_offset(1, 0) == 0);
    assert(d81_sector_offset(1, 1) == 256);
    assert(d81_sector_offset(2, 0) == 40 * 256);
    assert(d81_sector_offset(40, 0) == 39 * 40 * 256);
    printf("✓\n");
    
    printf("Testing file_type_names... ");
    assert(strcmp(d81_file_type_name(D81_TYPE_PRG), "PRG") == 0);
    assert(strcmp(d81_file_type_name(D81_TYPE_CBM), "CBM") == 0);
    printf("✓\n");
    
    printf("Testing blank_creation... ");
    size_t size = 0;
    uint8_t* data = d81_create_blank("TEST DISK", "TD", &size);
    assert(data != NULL);
    assert(size == D81_SIZE);
    
    d81_disk_t disk;
    assert(d81_parse(data, size, &disk));
    assert(disk.valid);
    assert(disk.num_tracks == 80);
    assert(disk.free_blocks > 3100);
    
    d81_free(&disk);
    free(data);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    return 0;
}

#endif /* D81_PARSER_TEST */
