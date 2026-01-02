/**
 * @file uft_atr_parser_v3.c
 * @brief GOD MODE ATR Parser v3 - Atari 8-bit Disk Format
 * 
 * ATR ist das Standard-Format für Atari 8-bit Computer:
 * - Single/Enhanced/Double Density
 * - 16-Byte Header
 * - Variable Sektoren (720, 1040, etc.)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#define ATR_SIGNATURE           0x0296
#define ATR_HEADER_SIZE         16

/* Sector sizes */
#define ATR_SECTOR_SD           128     /* Single density */
#define ATR_SECTOR_DD           256     /* Double density */

/* Standard disk sizes */
#define ATR_SD_SECTORS          720     /* 90K single density */
#define ATR_ED_SECTORS          1040    /* 130K enhanced density */
#define ATR_DD_SECTORS          720     /* 180K double density */
#define ATR_QD_SECTORS          1440    /* 360K quad density */

typedef enum {
    ATR_DIAG_OK = 0,
    ATR_DIAG_BAD_SIGNATURE,
    ATR_DIAG_BAD_SIZE,
    ATR_DIAG_TRUNCATED,
    ATR_DIAG_WRITE_PROTECTED,
    ATR_DIAG_BAD_SECTOR,
    ATR_DIAG_COUNT
} atr_diag_code_t;

typedef enum {
    ATR_DENSITY_SD = 0,     /* Single: 128 bytes/sector */
    ATR_DENSITY_DD = 1,     /* Double: 256 bytes/sector */
    ATR_DENSITY_ED = 2,     /* Enhanced: 128 bytes, 26 sectors/track */
    ATR_DENSITY_QD = 3      /* Quad: 256 bytes, more tracks */
} atr_density_t;

typedef struct { float overall; bool valid; atr_density_t density; } atr_score_t;
typedef struct { atr_diag_code_t code; uint16_t sector; char msg[128]; } atr_diagnosis_t;
typedef struct { atr_diagnosis_t* items; size_t count; size_t cap; float quality; } atr_diagnosis_list_t;

typedef struct {
    /* Header fields */
    uint16_t signature;
    uint16_t paragraphs;        /* Size in 16-byte paragraphs */
    uint16_t sector_size;
    uint8_t paragraphs_high;
    uint32_t crc;
    uint32_t unused;
    uint8_t flags;
    
    /* Derived values */
    uint32_t disk_size;
    uint16_t sector_count;
    atr_density_t density;
    uint8_t tracks;
    uint8_t sectors_per_track;
    bool write_protected;
    
    /* Boot info */
    uint8_t boot_sectors;
    uint16_t boot_address;
    uint16_t init_address;
    
    atr_score_t score;
    atr_diagnosis_list_t* diagnosis;
    size_t source_size;
    bool valid;
} atr_disk_t;

static atr_diagnosis_list_t* atr_diagnosis_create(void) {
    atr_diagnosis_list_t* l = calloc(1, sizeof(atr_diagnosis_list_t));
    if (l) { l->cap = 64; l->items = calloc(l->cap, sizeof(atr_diagnosis_t)); l->quality = 1.0f; }
    return l;
}
static void atr_diagnosis_free(atr_diagnosis_list_t* l) { if (l) { free(l->items); free(l); } }

static uint16_t atr_read_le16(const uint8_t* p) { return p[0] | (p[1] << 8); }
static uint32_t atr_read_le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24); }

static atr_density_t atr_detect_density(uint16_t sector_size, uint16_t sector_count) {
    if (sector_size == 128) {
        if (sector_count == 1040) return ATR_DENSITY_ED;
        return ATR_DENSITY_SD;
    }
    if (sector_size == 256) {
        if (sector_count > 720) return ATR_DENSITY_QD;
        return ATR_DENSITY_DD;
    }
    return ATR_DENSITY_SD;
}

static const char* atr_density_name(atr_density_t d) {
    switch (d) {
        case ATR_DENSITY_SD: return "Single Density";
        case ATR_DENSITY_DD: return "Double Density";
        case ATR_DENSITY_ED: return "Enhanced Density";
        case ATR_DENSITY_QD: return "Quad Density";
        default: return "Unknown";
    }
}

static uint32_t atr_get_sector_offset(const atr_disk_t* disk, uint16_t sector) {
    if (sector < 1 || sector > disk->sector_count) return 0;
    
    /* First 3 sectors are always 128 bytes in DD mode */
    if (disk->sector_size == 256 && sector <= 3) {
        return ATR_HEADER_SIZE + (sector - 1) * 128;
    }
    
    if (disk->sector_size == 256) {
        /* After first 3 sectors */
        return ATR_HEADER_SIZE + 3 * 128 + (sector - 4) * 256;
    }
    
    return ATR_HEADER_SIZE + (sector - 1) * disk->sector_size;
}

static bool atr_parse_boot(const uint8_t* data, size_t size, atr_disk_t* disk) {
    uint32_t offset = atr_get_sector_offset(disk, 1);
    if (offset + 128 > size) return false;
    
    const uint8_t* boot = data + offset;
    
    /* Boot header: flags, count, load_lo, load_hi, init_lo, init_hi */
    disk->boot_sectors = boot[1];
    disk->boot_address = atr_read_le16(boot + 2);
    disk->init_address = atr_read_le16(boot + 4);
    
    return true;
}

static bool atr_parse(const uint8_t* data, size_t size, atr_disk_t* disk) {
    if (!data || !disk || size < ATR_HEADER_SIZE) return false;
    memset(disk, 0, sizeof(atr_disk_t));
    disk->diagnosis = atr_diagnosis_create();
    disk->source_size = size;
    
    /* Parse header */
    disk->signature = atr_read_le16(data);
    if (disk->signature != ATR_SIGNATURE) {
        return false;
    }
    
    disk->paragraphs = atr_read_le16(data + 2);
    disk->sector_size = atr_read_le16(data + 4);
    disk->paragraphs_high = data[6];
    disk->crc = atr_read_le32(data + 8);
    disk->unused = atr_read_le32(data + 12);
    disk->flags = data[15];
    
    /* Calculate disk size */
    disk->disk_size = ((uint32_t)disk->paragraphs_high << 16 | disk->paragraphs) * 16;
    
    /* Calculate sector count */
    if (disk->sector_size == 256) {
        /* First 3 sectors are 128 bytes */
        disk->sector_count = 3 + (disk->disk_size - 3 * 128) / 256;
    } else {
        disk->sector_count = disk->disk_size / disk->sector_size;
    }
    
    /* Detect density */
    disk->density = atr_detect_density(disk->sector_size, disk->sector_count);
    
    /* Calculate geometry */
    switch (disk->density) {
        case ATR_DENSITY_SD:
            disk->tracks = 40;
            disk->sectors_per_track = 18;
            break;
        case ATR_DENSITY_ED:
            disk->tracks = 40;
            disk->sectors_per_track = 26;
            break;
        case ATR_DENSITY_DD:
            disk->tracks = 40;
            disk->sectors_per_track = 18;
            break;
        case ATR_DENSITY_QD:
            disk->tracks = 80;
            disk->sectors_per_track = 18;
            break;
    }
    
    disk->write_protected = (disk->flags & 0x01) != 0;
    
    /* Parse boot sector */
    atr_parse_boot(data, size, disk);
    
    /* Validate size */
    size_t expected = ATR_HEADER_SIZE + disk->disk_size;
    if (size < expected) {
        disk->diagnosis->quality *= 0.8f;
    }
    
    disk->score.density = disk->density;
    disk->score.overall = 1.0f;
    disk->score.valid = true;
    disk->valid = true;
    
    return true;
}

static void atr_disk_free(atr_disk_t* disk) {
    if (disk && disk->diagnosis) atr_diagnosis_free(disk->diagnosis);
}

/* ============================================================================
 * EXTENDED FEATURES - Atari DOS Support
 * ============================================================================ */

/* Atari DOS 2.0/2.5 structures */
#define ATR_VTOC_SECTOR         360     /* Volume Table of Contents */
#define ATR_DIR_START           361     /* Directory starts here */
#define ATR_DIR_SECTORS         8       /* 8 sectors for directory */

typedef struct {
    uint8_t flags;          /* Bit 7=in use, Bit 6=deleted, Bit 5=locked */
    uint16_t sector_count;
    uint16_t first_sector;
    char name[8];
    char ext[3];
} atr_dir_entry_t;

typedef struct {
    uint8_t dos_code;
    uint16_t total_sectors;
    uint16_t free_sectors;
    uint8_t bitmap[90];     /* Allocation bitmap */
} atr_vtoc_t;

/* Read directory entry from raw data */
static bool atr_read_dir_entry(const uint8_t* data, atr_dir_entry_t* entry) {
    if (!data || !entry) return false;
    
    entry->flags = data[0];
    entry->sector_count = data[1] | (data[2] << 8);
    entry->first_sector = data[3] | (data[4] << 8);
    
    memcpy(entry->name, data + 5, 8);
    entry->name[8] = '\0';
    memcpy(entry->ext, data + 13, 3);
    entry->ext[3] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 7; i >= 0 && entry->name[i] == ' '; i--) entry->name[i] = '\0';
    for (int i = 2; i >= 0 && entry->ext[i] == ' '; i--) entry->ext[i] = '\0';
    
    return (entry->flags & 0x40) == 0;  /* Return false if deleted */
}

/* Parse VTOC */
static bool atr_parse_vtoc(const uint8_t* data, size_t size, const atr_disk_t* disk,
                           atr_vtoc_t* vtoc) {
    if (!data || !vtoc || !disk) return false;
    
    uint32_t vtoc_offset = atr_get_sector_offset(disk, ATR_VTOC_SECTOR);
    if (vtoc_offset + 128 > size) return false;
    
    const uint8_t* v = data + vtoc_offset;
    
    vtoc->dos_code = v[0];
    vtoc->total_sectors = v[1] | (v[2] << 8);
    vtoc->free_sectors = v[3] | (v[4] << 8);
    memcpy(vtoc->bitmap, v + 10, 90);
    
    return true;
}

/* Count files in directory */
static int atr_count_files(const uint8_t* data, size_t size, const atr_disk_t* disk) {
    if (!data || !disk) return 0;
    
    int count = 0;
    
    for (int dir_sec = 0; dir_sec < ATR_DIR_SECTORS; dir_sec++) {
        uint32_t offset = atr_get_sector_offset(disk, ATR_DIR_START + dir_sec);
        if (offset + 128 > size) break;
        
        const uint8_t* sec = data + offset;
        
        /* 8 entries per sector (16 bytes each) */
        for (int e = 0; e < 8; e++) {
            const uint8_t* entry_data = sec + e * 16;
            
            if (entry_data[0] == 0x00) continue;  /* Empty */
            if (entry_data[0] & 0x80) count++;    /* In use */
        }
    }
    
    return count;
}

/* Extract file from ATR */
static bool atr_extract_file(const uint8_t* data, size_t size, const atr_disk_t* disk,
                             uint16_t first_sector, uint16_t sector_count,
                             uint8_t* out_data, size_t* out_size, size_t max_size) {
    if (!data || !disk || !out_data || !out_size) return false;
    
    size_t pos = 0;
    uint16_t current = first_sector;
    int max_iter = sector_count + 10;
    
    while (current != 0 && max_iter-- > 0) {
        uint32_t offset = atr_get_sector_offset(disk, current);
        if (offset + disk->sector_size > size) return false;
        
        const uint8_t* sec = data + offset;
        
        /* DOS 2.x: Byte 0-1 = file number + next sector high bits
         * Bytes 2-end = data
         * Last 3 bytes: link to next sector
         */
        uint8_t file_bits = sec[0];
        uint8_t next_hi = sec[1] & 0x03;
        uint8_t bytes_used = sec[disk->sector_size - 1];
        uint16_t next_sector = sec[disk->sector_size - 2] | (next_hi << 8);
        
        size_t data_bytes = (bytes_used > 0) ? bytes_used : (disk->sector_size - 3);
        if (data_bytes > disk->sector_size - 3) data_bytes = disk->sector_size - 3;
        
        if (pos + data_bytes > max_size) {
            data_bytes = max_size - pos;
        }
        
        memcpy(out_data + pos, sec + 3, data_bytes);
        pos += data_bytes;
        
        /* Check for end of file */
        if ((file_bits & 0x03) != (file_bits & 0x03)) break;  /* Sanity */
        if (next_sector == 0 || bytes_used < (disk->sector_size - 3)) break;
        
        current = next_sector;
    }
    
    *out_size = pos;
    return true;
}

/* Detect DOS version */
static const char* atr_detect_dos(const uint8_t* data, size_t size, const atr_disk_t* disk) {
    if (!data || !disk || disk->sector_count < 720) return "Unknown";
    
    /* Check VTOC sector for DOS signature */
    uint32_t vtoc_offset = atr_get_sector_offset(disk, ATR_VTOC_SECTOR);
    if (vtoc_offset + 128 > size) return "Unknown";
    
    uint8_t dos_code = data[vtoc_offset];
    
    switch (dos_code) {
        case 0x02: return "DOS 2.0S";
        case 0x22: return "DOS 2.5";
        case 0x41: return "DOS 3.0";
        case 0x42: return "MyDOS";
        case 0x46: return "SpartaDOS";
        default:
            if (dos_code >= 0x01 && dos_code <= 0x03) return "DOS 2.x";
            return "Unknown";
    }
}

#ifdef ATR_V3_TEST
#include <assert.h>
int main(void) {
    printf("=== ATR Parser v3 Tests ===\n");
    
    printf("Testing density detection... ");
    assert(atr_detect_density(128, 720) == ATR_DENSITY_SD);
    assert(atr_detect_density(128, 1040) == ATR_DENSITY_ED);
    assert(atr_detect_density(256, 720) == ATR_DENSITY_DD);
    assert(atr_detect_density(256, 1440) == ATR_DENSITY_QD);
    printf("✓\n");
    
    printf("Testing density names... ");
    assert(strcmp(atr_density_name(ATR_DENSITY_SD), "Single Density") == 0);
    assert(strcmp(atr_density_name(ATR_DENSITY_DD), "Double Density") == 0);
    printf("✓\n");
    
    printf("Testing ATR parsing... ");
    /* Create minimal ATR: 90K single density */
    size_t atr_size = ATR_HEADER_SIZE + 720 * 128;
    uint8_t* atr = calloc(1, atr_size);
    
    /* Header */
    atr[0] = 0x96; atr[1] = 0x02;  /* Signature */
    uint32_t paragraphs = (720 * 128) / 16;
    atr[2] = paragraphs & 0xFF;
    atr[3] = (paragraphs >> 8) & 0xFF;
    atr[4] = 128; atr[5] = 0;      /* Sector size */
    
    atr_disk_t disk;
    bool ok = atr_parse(atr, atr_size, &disk);
    assert(ok);
    assert(disk.valid);
    assert(disk.sector_size == 128);
    assert(disk.sector_count == 720);
    assert(disk.density == ATR_DENSITY_SD);
    atr_disk_free(&disk);
    free(atr);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 3, Failed: 0\n");
    return 0;
}
#endif
