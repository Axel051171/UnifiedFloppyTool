/**
 * @file uft_adf_parser_v2.c
 * @brief GOD MODE ADF Parser v2 - Amiga Disk File Format
 * 
 * ADF is the standard Amiga floppy disk image format.
 * - 80 tracks, 2 sides
 * - 11 sectors per track (DD) or 22 (HD)
 * - 512 bytes per sector
 * - MFM encoding with Amiga-specific format
 * 
 * Filesystem support:
 * - OFS (Old File System) - AmigaDOS 1.x
 * - FFS (Fast File System) - AmigaDOS 2.x+
 * - International mode (INTL)
 * - Directory cache (DCFS)
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

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define ADF_SECTOR_SIZE         512
#define ADF_TRACKS              80
#define ADF_SIDES               2
#define ADF_SECTORS_DD          11      /* Double density */
#define ADF_SECTORS_HD          22      /* High density */

#define ADF_SIZE_DD             (ADF_TRACKS * ADF_SIDES * ADF_SECTORS_DD * ADF_SECTOR_SIZE)  /* 901120 */
#define ADF_SIZE_HD             (ADF_TRACKS * ADF_SIDES * ADF_SECTORS_HD * ADF_SECTOR_SIZE)  /* 1802240 */

/* Block types */
#define ADF_T_HEADER            2
#define ADF_T_DATA              8
#define ADF_T_LIST              16

/* Secondary types */
#define ADF_ST_ROOT             1
#define ADF_ST_USERDIR          2
#define ADF_ST_FILE             -3
#define ADF_ST_SOFTLINK         3
#define ADF_ST_LINKDIR          4

/* Filesystem types */
#define ADF_DOS0                0x444F5300  /* "DOS\0" - OFS */
#define ADF_DOS1                0x444F5301  /* "DOS\1" - FFS */
#define ADF_DOS2                0x444F5302  /* "DOS\2" - OFS INTL */
#define ADF_DOS3                0x444F5303  /* "DOS\3" - FFS INTL */
#define ADF_DOS4                0x444F5304  /* "DOS\4" - OFS DCFS */
#define ADF_DOS5                0x444F5305  /* "DOS\5" - FFS DCFS */

/* Block positions */
#define ADF_BOOTBLOCK           0
#define ADF_ROOTBLOCK_DD        880     /* Middle of disk for DD */
#define ADF_ROOTBLOCK_HD        1760    /* Middle of disk for HD */

#define ADF_MAX_NAME_LEN        30
#define ADF_MAX_COMMENT_LEN     79
#define ADF_HASH_SIZE           72

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Filesystem type
 */
typedef enum {
    ADF_FS_UNKNOWN = 0,
    ADF_FS_OFS,             /* Original File System */
    ADF_FS_FFS,             /* Fast File System */
    ADF_FS_OFS_INTL,        /* OFS International */
    ADF_FS_FFS_INTL,        /* FFS International */
    ADF_FS_OFS_DCFS,        /* OFS Dir Cache */
    ADF_FS_FFS_DCFS         /* FFS Dir Cache */
} adf_fs_type_t;

/**
 * @brief Disk type
 */
typedef enum {
    ADF_DISK_DD = 0,        /* Double density (880KB) */
    ADF_DISK_HD             /* High density (1.76MB) */
} adf_disk_type_t;

/**
 * @brief Boot block
 */
typedef struct {
    uint32_t dos_type;      /* "DOS\x" */
    uint32_t checksum;      /* Boot block checksum */
    uint32_t root_block;    /* Root block pointer */
    uint8_t bootcode[1012]; /* Boot code */
    bool bootable;          /* Has valid boot code */
    bool checksum_valid;    /* Checksum OK */
} adf_bootblock_t;

/**
 * @brief Date stamp
 */
typedef struct {
    uint32_t days;          /* Days since 1978-01-01 */
    uint32_t mins;          /* Minutes past midnight */
    uint32_t ticks;         /* Ticks (1/50 sec) past minute */
} adf_datestamp_t;

/**
 * @brief Root block
 */
typedef struct {
    uint32_t type;          /* T_HEADER (2) */
    uint32_t header_key;    /* Unused */
    uint32_t high_seq;      /* Unused */
    uint32_t hash_size;     /* Hash table size (72) */
    uint32_t first_data;    /* Unused */
    uint32_t checksum;      /* Block checksum */
    uint32_t hash_table[72];/* Hash table */
    uint32_t bitmap_flag;   /* -1 if bitmap valid */
    uint32_t bitmap_pages[25];  /* Bitmap block pointers */
    adf_datestamp_t last_modified;
    char disk_name[31];     /* Volume name (BCPL string) */
    adf_datestamp_t created;
    uint32_t next_hash;     /* Unused */
    uint32_t parent;        /* Unused */
    uint32_t extension;     /* FFS extension block */
    uint32_t sec_type;      /* ST_ROOT (1) */
    bool checksum_valid;
} adf_rootblock_t;

/**
 * @brief Directory entry
 */
typedef struct {
    char name[31];
    uint32_t header_block;
    int32_t type;           /* Positive = dir, negative = file */
    uint32_t size;          /* File size in bytes */
    uint32_t first_block;   /* First data block */
    uint32_t protection;    /* Protection bits */
    char comment[80];
    adf_datestamp_t modified;
    bool is_dir;
} adf_entry_t;

/**
 * @brief ADF disk structure
 */
typedef struct {
    adf_disk_type_t disk_type;
    adf_fs_type_t fs_type;
    adf_bootblock_t bootblock;
    adf_rootblock_t rootblock;
    
    /* Disk geometry */
    uint8_t tracks;
    uint8_t sides;
    uint8_t sectors;
    uint32_t total_blocks;
    uint32_t root_block;
    
    /* Bitmap info */
    uint32_t free_blocks;
    uint32_t used_blocks;
    
    /* Directory entries */
    adf_entry_t* entries;
    uint16_t entry_count;
    uint16_t entry_capacity;
    
    /* Status */
    bool valid;
    char error[256];
} adf_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * HELPER FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read big-endian 32-bit
 */
static uint32_t read_be32(const uint8_t* data) {
    return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
           ((uint32_t)data[2] << 8) | data[3];
}

/**
 * @brief Check valid ADF size
 */
static bool adf_is_valid_size(size_t size, adf_disk_type_t* type) {
    if (size == ADF_SIZE_DD) {
        *type = ADF_DISK_DD;
        return true;
    }
    if (size == ADF_SIZE_HD) {
        *type = ADF_DISK_HD;
        return true;
    }
    return false;
}

/**
 * @brief Get filesystem type name
 */
static const char* adf_fs_type_name(adf_fs_type_t type) {
    switch (type) {
        case ADF_FS_OFS: return "OFS";
        case ADF_FS_FFS: return "FFS";
        case ADF_FS_OFS_INTL: return "OFS-INTL";
        case ADF_FS_FFS_INTL: return "FFS-INTL";
        case ADF_FS_OFS_DCFS: return "OFS-DCFS";
        case ADF_FS_FFS_DCFS: return "FFS-DCFS";
        default: return "Unknown";
    }
}

/**
 * @brief Get disk type name
 */
static const char* adf_disk_type_name(adf_disk_type_t type) {
    return (type == ADF_DISK_HD) ? "HD (1.76MB)" : "DD (880KB)";
}

/**
 * @brief Calculate block checksum
 */
static uint32_t adf_block_checksum(const uint8_t* block, size_t offset) {
    uint32_t sum = 0;
    for (size_t i = 0; i < ADF_SECTOR_SIZE; i += 4) {
        if (i != offset) {  /* Skip checksum position */
            sum += read_be32(block + i);
        }
    }
    return (uint32_t)(-(int32_t)sum);
}

/**
 * @brief Get block offset
 */
static size_t adf_block_offset(uint32_t block) {
    return block * ADF_SECTOR_SIZE;
}

/**
 * @brief Read BCPL string (first byte is length)
 */
static void adf_read_bcpl_string(const uint8_t* src, char* dest, size_t max_len) {
    uint8_t len = src[0];
    if (len > max_len - 1) len = max_len - 1;
    memcpy(dest, src + 1, len);
    dest[len] = '\0';
}

/**
 * @brief Decode filesystem type from DOS type
 */
static adf_fs_type_t adf_decode_fs_type(uint32_t dos_type) {
    switch (dos_type) {
        case ADF_DOS0: return ADF_FS_OFS;
        case ADF_DOS1: return ADF_FS_FFS;
        case ADF_DOS2: return ADF_FS_OFS_INTL;
        case ADF_DOS3: return ADF_FS_FFS_INTL;
        case ADF_DOS4: return ADF_FS_OFS_DCFS;
        case ADF_DOS5: return ADF_FS_FFS_DCFS;
        default: return ADF_FS_UNKNOWN;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * PARSING FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse boot block
 */
static bool adf_parse_bootblock(const uint8_t* data, adf_disk_t* disk) {
    disk->bootblock.dos_type = read_be32(data);
    disk->bootblock.checksum = read_be32(data + 4);
    disk->bootblock.root_block = read_be32(data + 8);
    
    /* Check DOS magic */
    if ((disk->bootblock.dos_type & 0xFFFFFF00) != 0x444F5300) {
        return false;  /* Not "DOS\x" */
    }
    
    disk->fs_type = adf_decode_fs_type(disk->bootblock.dos_type);
    
    /* Check if bootable (has boot code) */
    disk->bootblock.bootable = false;
    for (int i = 12; i < 1024; i++) {
        if (data[i] != 0) {
            disk->bootblock.bootable = true;
            break;
        }
    }
    
    /* Verify checksum (sum of all longs should be 0) */
    uint32_t sum = 0;
    for (int i = 0; i < 1024; i += 4) {
        sum += read_be32(data + i);
    }
    disk->bootblock.checksum_valid = (sum == 0);
    
    return true;
}

/**
 * @brief Parse root block
 */
static bool adf_parse_rootblock(const uint8_t* data, size_t size, adf_disk_t* disk) {
    size_t offset = adf_block_offset(disk->root_block);
    if (offset + ADF_SECTOR_SIZE > size) return false;
    
    const uint8_t* block = data + offset;
    
    disk->rootblock.type = read_be32(block);
    if (disk->rootblock.type != ADF_T_HEADER) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid root block type: %u", disk->rootblock.type);
        return false;
    }
    
    disk->rootblock.checksum = read_be32(block + 20);
    
    /* Hash table */
    for (int i = 0; i < 72; i++) {
        disk->rootblock.hash_table[i] = read_be32(block + 24 + i * 4);
    }
    
    disk->rootblock.bitmap_flag = read_be32(block + 312);
    
    /* Bitmap block pointers */
    for (int i = 0; i < 25; i++) {
        disk->rootblock.bitmap_pages[i] = read_be32(block + 316 + i * 4);
    }
    
    /* Timestamps */
    disk->rootblock.last_modified.days = read_be32(block + 420);
    disk->rootblock.last_modified.mins = read_be32(block + 424);
    disk->rootblock.last_modified.ticks = read_be32(block + 428);
    
    /* Disk name */
    adf_read_bcpl_string(block + 432, disk->rootblock.disk_name, 31);
    
    /* Created timestamp */
    disk->rootblock.created.days = read_be32(block + 472);
    disk->rootblock.created.mins = read_be32(block + 476);
    disk->rootblock.created.ticks = read_be32(block + 480);
    
    disk->rootblock.sec_type = read_be32(block + 508);
    
    /* Verify checksum */
    uint32_t expected = adf_block_checksum(block, 20);
    disk->rootblock.checksum_valid = (disk->rootblock.checksum == expected);
    
    return true;
}

/**
 * @brief Count free blocks from bitmap
 */
static void adf_count_free_blocks(const uint8_t* data, size_t size, adf_disk_t* disk) {
    disk->free_blocks = 0;
    disk->used_blocks = 0;
    
    /* Iterate bitmap pages */
    for (int i = 0; i < 25 && disk->rootblock.bitmap_pages[i] != 0; i++) {
        uint32_t bm_block = disk->rootblock.bitmap_pages[i];
        size_t offset = adf_block_offset(bm_block);
        if (offset + ADF_SECTOR_SIZE > size) continue;
        
        const uint8_t* bitmap = data + offset;
        
        /* Each bit represents one block (1 = free, 0 = used) */
        for (int j = 4; j < ADF_SECTOR_SIZE; j++) {
            for (int bit = 0; bit < 8; bit++) {
                if (bitmap[j] & (1 << bit)) {
                    disk->free_blocks++;
                } else {
                    disk->used_blocks++;
                }
            }
        }
    }
}

/**
 * @brief Parse ADF disk
 */
static bool adf_parse(const uint8_t* data, size_t size, adf_disk_t* disk) {
    memset(disk, 0, sizeof(adf_disk_t));
    
    if (!adf_is_valid_size(size, &disk->disk_type)) {
        snprintf(disk->error, sizeof(disk->error), 
                 "Invalid ADF size: %zu bytes", size);
        return false;
    }
    
    /* Set geometry */
    disk->tracks = ADF_TRACKS;
    disk->sides = ADF_SIDES;
    disk->sectors = (disk->disk_type == ADF_DISK_HD) ? ADF_SECTORS_HD : ADF_SECTORS_DD;
    disk->total_blocks = size / ADF_SECTOR_SIZE;
    disk->root_block = (disk->disk_type == ADF_DISK_HD) ? ADF_ROOTBLOCK_HD : ADF_ROOTBLOCK_DD;
    
    /* Parse boot block */
    if (!adf_parse_bootblock(data, disk)) {
        snprintf(disk->error, sizeof(disk->error), "Invalid boot block");
        return false;
    }
    
    /* Parse root block */
    if (!adf_parse_rootblock(data, size, disk)) {
        return false;
    }
    
    /* Count free blocks */
    adf_count_free_blocks(data, size, disk);
    
    disk->valid = true;
    return true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * CREATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create blank ADF
 */
static uint8_t* adf_create_blank(const char* name, adf_disk_type_t type, 
                                  adf_fs_type_t fs, size_t* out_size) {
    size_t size = (type == ADF_DISK_HD) ? ADF_SIZE_HD : ADF_SIZE_DD;
    uint8_t* data = calloc(1, size);
    if (!data) { *out_size = 0; return NULL; }
    
    /* Boot block */
    uint32_t dos_type;
    switch (fs) {
        case ADF_FS_FFS: dos_type = ADF_DOS1; break;
        case ADF_FS_OFS_INTL: dos_type = ADF_DOS2; break;
        case ADF_FS_FFS_INTL: dos_type = ADF_DOS3; break;
        default: dos_type = ADF_DOS0; break;
    }
    
    data[0] = (dos_type >> 24) & 0xFF;
    data[1] = (dos_type >> 16) & 0xFF;
    data[2] = (dos_type >> 8) & 0xFF;
    data[3] = dos_type & 0xFF;
    
    /* Root block pointer */
    uint32_t root = (type == ADF_DISK_HD) ? ADF_ROOTBLOCK_HD : ADF_ROOTBLOCK_DD;
    data[8] = (root >> 24) & 0xFF;
    data[9] = (root >> 16) & 0xFF;
    data[10] = (root >> 8) & 0xFF;
    data[11] = root & 0xFF;
    
    /* Initialize root block */
    size_t root_offset = root * ADF_SECTOR_SIZE;
    uint8_t* rb = data + root_offset;
    
    /* Type = T_HEADER */
    rb[3] = ADF_T_HEADER;
    
    /* Hash table size */
    rb[15] = 72;
    
    /* Bitmap flag = -1 (valid) */
    rb[312] = rb[313] = rb[314] = rb[315] = 0xFF;
    
    /* Disk name */
    size_t name_len = name ? strlen(name) : 0;
    if (name_len > 30) name_len = 30;
    rb[432] = name_len;
    if (name) memcpy(rb + 433, name, name_len);
    
    /* Secondary type = ST_ROOT */
    rb[508] = 0;
    rb[509] = 0;
    rb[510] = 0;
    rb[511] = ADF_ST_ROOT;
    
    /* Calculate and set checksum */
    uint32_t checksum = adf_block_checksum(rb, 20);
    rb[20] = (checksum >> 24) & 0xFF;
    rb[21] = (checksum >> 16) & 0xFF;
    rb[22] = (checksum >> 8) & 0xFF;
    rb[23] = checksum & 0xFF;
    
    *out_size = size;
    return data;
}

/**
 * @brief Free ADF disk structure
 */
static void adf_free(adf_disk_t* disk) {
    if (disk && disk->entries) {
        free(disk->entries);
        disk->entries = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * TEST SUITE
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef ADF_PARSER_TEST

#include <assert.h>

int main(void) {
    printf("=== ADF Parser v2 Tests ===\n");
    
    printf("Testing valid sizes... ");
    adf_disk_type_t type;
    assert(adf_is_valid_size(ADF_SIZE_DD, &type) && type == ADF_DISK_DD);
    assert(adf_is_valid_size(ADF_SIZE_HD, &type) && type == ADF_DISK_HD);
    assert(!adf_is_valid_size(12345, &type));
    printf("✓\n");
    
    printf("Testing filesystem names... ");
    assert(strcmp(adf_fs_type_name(ADF_FS_OFS), "OFS") == 0);
    assert(strcmp(adf_fs_type_name(ADF_FS_FFS), "FFS") == 0);
    assert(strcmp(adf_fs_type_name(ADF_FS_FFS_INTL), "FFS-INTL") == 0);
    printf("✓\n");
    
    printf("Testing disk type names... ");
    assert(strcmp(adf_disk_type_name(ADF_DISK_DD), "DD (880KB)") == 0);
    assert(strcmp(adf_disk_type_name(ADF_DISK_HD), "HD (1.76MB)") == 0);
    printf("✓\n");
    
    printf("Testing blank creation... ");
    size_t size;
    uint8_t* data = adf_create_blank("WORKBENCH", ADF_DISK_DD, ADF_FS_FFS, &size);
    assert(data != NULL);
    assert(size == ADF_SIZE_DD);
    
    adf_disk_t disk;
    assert(adf_parse(data, size, &disk));
    assert(disk.valid);
    assert(disk.fs_type == ADF_FS_FFS);
    assert(strcmp(disk.rootblock.disk_name, "WORKBENCH") == 0);
    adf_free(&disk);
    free(data);
    printf("✓\n");
    
    printf("Testing constants... ");
    assert(ADF_SIZE_DD == 901120);
    assert(ADF_SIZE_HD == 1802240);
    assert(ADF_ROOTBLOCK_DD == 880);
    printf("✓\n");
    
    printf("\n=== All tests passed! ===\n");
    printf("Passed: 5, Failed: 0\n");
    return 0;
}

#endif /* ADF_PARSER_TEST */
