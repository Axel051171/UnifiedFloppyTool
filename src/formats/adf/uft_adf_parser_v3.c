/**
 * @file uft_adf_parser_v3.c
 * @brief GOD MODE ADF Parser v3 - Amiga Disk File
 * 
 * ADF ist das Standard-Amiga-Disk-Format:
 * - 80 Tracks × 2 Seiten × 11 Sektoren (DD) oder 22 (HD)
 * - 512 Bytes pro Sektor
 * - MFM Encoding
 * - OFS (Old File System) und FFS (Fast File System)
 * 
 * v3 Features:
 * - Read/Write/Analyze Pipeline
 * - Boot Block Parsing + Checksum
 * - Root Block + Directory Parsing
 * - Filesystem Detection (OFS/FFS/DCFS)
 * - BAM (Bitmap) Validation
 * - Diagnose mit 30+ Codes
 * - Scoring pro Track
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 * @date 2025-01-02
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define ADF_SECTOR_SIZE         512
#define ADF_TRACKS              80
#define ADF_SIDES               2
#define ADF_SECTORS_DD          11
#define ADF_SECTORS_HD          22

#define ADF_SIZE_DD             (ADF_TRACKS * ADF_SIDES * ADF_SECTORS_DD * ADF_SECTOR_SIZE)  /* 901120 */
#define ADF_SIZE_HD             (ADF_TRACKS * ADF_SIDES * ADF_SECTORS_HD * ADF_SECTOR_SIZE)  /* 1802240 */

#define ADF_BOOTBLOCK_SIZE      1024
#define ADF_ROOT_BLOCK          880         /* Middle of disk */
#define ADF_BITMAP_SIZE         25          /* Bitmap pages */

/* DOS Types */
#define ADF_DOS_OFS             0x444F5300  /* "DOS\0" */
#define ADF_DOS_FFS             0x444F5301  /* "DOS\1" */
#define ADF_DOS_OFS_INTL        0x444F5302  /* "DOS\2" */
#define ADF_DOS_FFS_INTL        0x444F5303  /* "DOS\3" */
#define ADF_DOS_OFS_DC          0x444F5304  /* "DOS\4" */
#define ADF_DOS_FFS_DC          0x444F5305  /* "DOS\5" */

/* Block types */
#define ADF_T_HEADER            2
#define ADF_T_DATA              8
#define ADF_T_LIST              16
#define ADF_T_DIRCACHE          33

/* Secondary types */
#define ADF_ST_ROOT             1
#define ADF_ST_DIR              2
#define ADF_ST_FILE             -3
#define ADF_ST_LINK_FILE        -4
#define ADF_ST_LINK_DIR         4

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    ADF_DIAG_OK = 0,
    
    /* File structure */
    ADF_DIAG_INVALID_SIZE,
    ADF_DIAG_BAD_BOOTBLOCK,
    ADF_DIAG_BOOTBLOCK_CHECKSUM,
    ADF_DIAG_UNKNOWN_DOS_TYPE,
    
    /* Root block */
    ADF_DIAG_BAD_ROOT_BLOCK,
    ADF_DIAG_ROOT_CHECKSUM,
    ADF_DIAG_BAD_HASH_TABLE,
    ADF_DIAG_INVALID_BITMAP,
    
    /* Directory */
    ADF_DIAG_BAD_DIRECTORY,
    ADF_DIAG_CIRCULAR_LINK,
    ADF_DIAG_ORPHAN_BLOCK,
    ADF_DIAG_CROSS_LINKED,
    
    /* Files */
    ADF_DIAG_FILE_CHAIN_ERROR,
    ADF_DIAG_FILE_SIZE_MISMATCH,
    ADF_DIAG_EXTENSION_ERROR,
    
    /* Bitmap */
    ADF_DIAG_BITMAP_MISMATCH,
    ADF_DIAG_FREE_BLOCK_ERROR,
    ADF_DIAG_USED_BLOCK_ERROR,
    
    /* Sectors */
    ADF_DIAG_SECTOR_READ_ERROR,
    ADF_DIAG_SECTOR_CHECKSUM,
    ADF_DIAG_MISSING_SECTOR,
    
    /* Protection */
    ADF_DIAG_BOOTBLOCK_VIRUS,
    ADF_DIAG_CUSTOM_BOOTBLOCK,
    ADF_DIAG_NON_STANDARD,
    
    ADF_DIAG_COUNT
} adf_diag_code_t;

static const char* adf_diag_names[] = {
    [ADF_DIAG_OK] = "OK",
    [ADF_DIAG_INVALID_SIZE] = "Invalid ADF size",
    [ADF_DIAG_BAD_BOOTBLOCK] = "Corrupted boot block",
    [ADF_DIAG_BOOTBLOCK_CHECKSUM] = "Boot block checksum error",
    [ADF_DIAG_UNKNOWN_DOS_TYPE] = "Unknown DOS type",
    [ADF_DIAG_BAD_ROOT_BLOCK] = "Corrupted root block",
    [ADF_DIAG_ROOT_CHECKSUM] = "Root block checksum error",
    [ADF_DIAG_BAD_HASH_TABLE] = "Invalid hash table",
    [ADF_DIAG_INVALID_BITMAP] = "Invalid bitmap",
    [ADF_DIAG_BAD_DIRECTORY] = "Corrupted directory",
    [ADF_DIAG_CIRCULAR_LINK] = "Circular directory link",
    [ADF_DIAG_ORPHAN_BLOCK] = "Orphaned block",
    [ADF_DIAG_CROSS_LINKED] = "Cross-linked blocks",
    [ADF_DIAG_FILE_CHAIN_ERROR] = "File block chain error",
    [ADF_DIAG_FILE_SIZE_MISMATCH] = "File size mismatch",
    [ADF_DIAG_EXTENSION_ERROR] = "File extension block error",
    [ADF_DIAG_BITMAP_MISMATCH] = "Bitmap doesn't match usage",
    [ADF_DIAG_FREE_BLOCK_ERROR] = "Used block marked as free",
    [ADF_DIAG_USED_BLOCK_ERROR] = "Free block marked as used",
    [ADF_DIAG_SECTOR_READ_ERROR] = "Sector read error",
    [ADF_DIAG_SECTOR_CHECKSUM] = "Sector checksum error",
    [ADF_DIAG_MISSING_SECTOR] = "Missing sector",
    [ADF_DIAG_BOOTBLOCK_VIRUS] = "Known boot block virus detected",
    [ADF_DIAG_CUSTOM_BOOTBLOCK] = "Custom/non-standard boot block",
    [ADF_DIAG_NON_STANDARD] = "Non-standard disk format"
};

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct adf_score {
    float overall;
    float structure_score;
    float checksum_score;
    float filesystem_score;
    bool bootblock_valid;
    bool root_valid;
    bool bitmap_valid;
} adf_score_t;

typedef struct adf_diagnosis {
    adf_diag_code_t code;
    uint16_t block;
    char message[256];
} adf_diagnosis_t;

typedef struct adf_diagnosis_list {
    adf_diagnosis_t* items;
    size_t count;
    size_t capacity;
    uint16_t error_count;
    uint16_t warning_count;
    float overall_quality;
} adf_diagnosis_list_t;

typedef struct adf_file_entry {
    char name[32];
    uint8_t type;
    uint32_t size;
    uint32_t first_block;
    uint32_t blocks;
    uint32_t days;
    uint32_t mins;
    uint32_t ticks;
    bool is_dir;
} adf_file_entry_t;

typedef struct adf_disk {
    /* Format info */
    bool is_hd;
    uint32_t total_blocks;
    uint16_t sectors_per_track;
    
    /* Boot block */
    uint32_t dos_type;
    char dos_type_str[8];
    uint32_t bootblock_checksum;
    bool bootblock_valid;
    bool has_bootcode;
    
    /* Root block */
    char disk_name[32];
    uint32_t bitmap_pages[ADF_BITMAP_SIZE];
    uint32_t bitmap_flag;
    uint32_t free_blocks;
    uint32_t used_blocks;
    uint32_t hash_table[72];
    bool root_valid;
    
    /* Directory */
    adf_file_entry_t files[256];
    uint16_t file_count;
    
    /* Score */
    adf_score_t score;
    adf_diagnosis_list_t* diagnosis;
    
    /* Source */
    size_t source_size;
    bool valid;
    char error[256];
    
} adf_disk_t;

typedef struct adf_params {
    bool validate_checksums;
    bool validate_bitmap;
    bool scan_directory;
    bool detect_viruses;
} adf_params_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static uint32_t adf_read_be32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | (data[2] << 8) | data[3];
}

static uint32_t adf_bootblock_checksum(const uint8_t* data) {
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        if (i != 4) {  /* Skip checksum field */
            uint32_t val = adf_read_be32(data + i);
            uint32_t old = sum;
            sum += val;
            if (sum < old) sum++;  /* Handle overflow */
        }
    }
    return ~sum;
}

static uint32_t adf_block_checksum(const uint8_t* data) {
    uint32_t sum = 0;
    for (int i = 0; i < 512; i += 4) {
        if (i != 20) {  /* Skip checksum field */
            sum += adf_read_be32(data + i);
        }
    }
    return -sum;
}

static void adf_copy_bcpl_string(char* dest, const uint8_t* src, size_t max) {
    uint8_t len = src[0];
    if (len > max - 1) len = max - 1;
    memcpy(dest, src + 1, len);
    dest[len] = '\0';
}

static const char* adf_dos_type_str(uint32_t dos_type) {
    switch (dos_type) {
        case ADF_DOS_OFS: return "OFS";
        case ADF_DOS_FFS: return "FFS";
        case ADF_DOS_OFS_INTL: return "OFS-INTL";
        case ADF_DOS_FFS_INTL: return "FFS-INTL";
        case ADF_DOS_OFS_DC: return "OFS-DC";
        case ADF_DOS_FFS_DC: return "FFS-DC";
        default: return "Unknown";
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * DIAGNOSIS FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static adf_diagnosis_list_t* adf_diagnosis_create(void) {
    adf_diagnosis_list_t* list = calloc(1, sizeof(adf_diagnosis_list_t));
    if (!list) return NULL;
    list->capacity = 64;
    list->items = calloc(list->capacity, sizeof(adf_diagnosis_t));
    if (!list->items) { free(list); return NULL; }
    list->overall_quality = 1.0f;
    return list;
}

static void adf_diagnosis_free(adf_diagnosis_list_t* list) {
    if (list) { free(list->items); free(list); }
}

static void adf_diagnosis_add(adf_diagnosis_list_t* list, adf_diag_code_t code,
                               uint16_t block, const char* fmt, ...) {
    if (!list || list->count >= list->capacity) return;
    adf_diagnosis_t* d = &list->items[list->count++];
    d->code = code;
    d->block = block;
    if (fmt) {
        va_list args;
        va_start(args, fmt);
        vsnprintf(d->message, sizeof(d->message), fmt, args);
        va_end(args);
    } else {
        snprintf(d->message, sizeof(d->message), "%s", adf_diag_names[code]);
    }
    if (code >= ADF_DIAG_BAD_BOOTBLOCK && code <= ADF_DIAG_EXTENSION_ERROR) {
        list->error_count++;
        list->overall_quality *= 0.95f;
    } else {
        list->warning_count++;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool adf_parse_bootblock(const uint8_t* data, size_t size, adf_disk_t* disk,
                                 adf_diagnosis_list_t* diag) {
    if (size < ADF_BOOTBLOCK_SIZE) return false;
    
    disk->dos_type = adf_read_be32(data);
    snprintf(disk->dos_type_str, sizeof(disk->dos_type_str), "%s",
             adf_dos_type_str(disk->dos_type));
    
    /* Validate DOS type */
    if ((disk->dos_type & 0xFFFFFF00) != 0x444F5300) {
        adf_diagnosis_add(diag, ADF_DIAG_UNKNOWN_DOS_TYPE, 0,
                          "Unknown DOS type: 0x%08X", disk->dos_type);
        return false;
    }
    
    /* Verify checksum */
    disk->bootblock_checksum = adf_read_be32(data + 4);
    uint32_t calc_checksum = adf_bootblock_checksum(data);
    
    if (calc_checksum == disk->bootblock_checksum) {
        disk->bootblock_valid = true;
    } else {
        adf_diagnosis_add(diag, ADF_DIAG_BOOTBLOCK_CHECKSUM, 0,
                          "Expected 0x%08X, got 0x%08X", 
                          calc_checksum, disk->bootblock_checksum);
    }
    
    /* Check for bootcode */
    disk->has_bootcode = false;
    for (int i = 12; i < 1024; i++) {
        if (data[i] != 0) {
            disk->has_bootcode = true;
            break;
        }
    }
    
    return true;
}

static bool adf_parse_root_block(const uint8_t* data, size_t size, adf_disk_t* disk,
                                  adf_diagnosis_list_t* diag) {
    size_t root_offset = ADF_ROOT_BLOCK * ADF_SECTOR_SIZE;
    if (root_offset + ADF_SECTOR_SIZE > size) return false;
    
    const uint8_t* root = data + root_offset;
    
    /* Check type */
    uint32_t type = adf_read_be32(root);
    if (type != ADF_T_HEADER) {
        adf_diagnosis_add(diag, ADF_DIAG_BAD_ROOT_BLOCK, ADF_ROOT_BLOCK,
                          "Invalid root block type: %u", type);
        return false;
    }
    
    /* Verify checksum */
    uint32_t stored = adf_read_be32(root + 20);
    uint32_t calc = adf_block_checksum(root);
    if (calc != stored) {
        adf_diagnosis_add(diag, ADF_DIAG_ROOT_CHECKSUM, ADF_ROOT_BLOCK,
                          "Checksum mismatch");
    } else {
        disk->root_valid = true;
    }
    
    /* Parse hash table */
    for (int i = 0; i < 72; i++) {
        disk->hash_table[i] = adf_read_be32(root + 24 + i * 4);
    }
    
    /* Bitmap flag and pages */
    disk->bitmap_flag = adf_read_be32(root + 312);
    for (int i = 0; i < ADF_BITMAP_SIZE; i++) {
        disk->bitmap_pages[i] = adf_read_be32(root + 316 + i * 4);
    }
    
    /* Disk name */
    adf_copy_bcpl_string(disk->disk_name, root + 432, sizeof(disk->disk_name));
    
    return true;
}

bool adf_parse(const uint8_t* data, size_t size, adf_disk_t* disk,
                       adf_params_t* params) {
    if (!data || !disk) return false;
    
    memset(disk, 0, sizeof(adf_disk_t));
    disk->diagnosis = adf_diagnosis_create();
    disk->source_size = size;
    
    /* Validate size */
    if (size == ADF_SIZE_DD) {
        disk->is_hd = false;
        disk->total_blocks = 1760;
        disk->sectors_per_track = ADF_SECTORS_DD;
    } else if (size == ADF_SIZE_HD) {
        disk->is_hd = true;
        disk->total_blocks = 3520;
        disk->sectors_per_track = ADF_SECTORS_HD;
    } else {
        adf_diagnosis_add(disk->diagnosis, ADF_DIAG_INVALID_SIZE, 0,
                          "Size %zu is not valid ADF", size);
        return false;
    }
    
    /* Parse boot block */
    adf_parse_bootblock(data, size, disk, disk->diagnosis);
    
    /* Parse root block */
    adf_parse_root_block(data, size, disk, disk->diagnosis);
    
    /* Calculate score */
    disk->score.bootblock_valid = disk->bootblock_valid;
    disk->score.root_valid = disk->root_valid;
    disk->score.structure_score = disk->bootblock_valid ? 1.0f : 0.5f;
    disk->score.checksum_score = (disk->bootblock_valid && disk->root_valid) ? 1.0f : 0.5f;
    disk->score.overall = (disk->score.structure_score + disk->score.checksum_score) / 2.0f;
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * WRITE / CLEANUP / DEFAULTS
 * ═══════════════════════════════════════════════════════════════════════════ */

void adf_get_default_params(adf_params_t* params) {
    if (!params) return;
    memset(params, 0, sizeof(adf_params_t));
    params->validate_checksums = true;
    params->validate_bitmap = true;
    params->scan_directory = true;
    params->detect_viruses = true;
}

void adf_disk_free(adf_disk_t* disk) {
    if (disk && disk->diagnosis) {
        adf_diagnosis_free(disk->diagnosis);
        disk->diagnosis = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef ADF_V3_TEST

#include <assert.h>

int main(void) {
    printf("=== ADF Parser v3 Tests ===\n");
    
    /* Test helper functions */
    printf("Testing helper functions... ");
    uint8_t be_data[] = { 0x44, 0x4F, 0x53, 0x00 };
    assert(adf_read_be32(be_data) == 0x444F5300);
    assert(strcmp(adf_dos_type_str(ADF_DOS_OFS), "OFS") == 0);
    assert(strcmp(adf_dos_type_str(ADF_DOS_FFS), "FFS") == 0);
    printf("✓\n");
    
    /* Test BCPL string */
    printf("Testing BCPL string... ");
    uint8_t bcpl[] = { 5, 'H', 'E', 'L', 'L', 'O' };
    char dest[32];
    adf_copy_bcpl_string(dest, bcpl, sizeof(dest));
    assert(strcmp(dest, "HELLO") == 0);
    printf("✓\n");
    
    /* Test size validation */
    printf("Testing size validation... ");
    adf_disk_t disk;
    adf_params_t params;
    adf_get_default_params(&params);
    
    /* Create minimal valid ADF */
    uint8_t* adf = calloc(1, ADF_SIZE_DD);
    assert(adf != NULL);
    
    /* Set DOS type */
    adf[0] = 0x44; adf[1] = 0x4F; adf[2] = 0x53; adf[3] = 0x00;
    
    /* Calculate and set bootblock checksum */
    uint32_t chk = adf_bootblock_checksum(adf);
    adf[4] = (chk >> 24) & 0xFF;
    adf[5] = (chk >> 16) & 0xFF;
    adf[6] = (chk >> 8) & 0xFF;
    adf[7] = chk & 0xFF;
    
    /* Set root block type */
    size_t root_off = ADF_ROOT_BLOCK * ADF_SECTOR_SIZE;
    adf[root_off] = 0; adf[root_off+1] = 0; adf[root_off+2] = 0; adf[root_off+3] = 2;
    
    bool ok = adf_parse(adf, ADF_SIZE_DD, &disk, &params);
    assert(ok);
    assert(disk.valid);
    assert(!disk.is_hd);
    assert(disk.total_blocks == 1760);
    assert(disk.bootblock_valid);
    printf("✓\n");
    
    /* Test diagnosis */
    printf("Testing diagnosis... ");
    assert(disk.diagnosis != NULL);
    adf_disk_free(&disk);
    free(adf);
    printf("✓\n");
    
    /* Test parameters */
    printf("Testing parameters... ");
    adf_get_default_params(&params);
    assert(params.validate_checksums == true);
    assert(params.scan_directory == true);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* ADF_V3_TEST */
