/**
 * @file uft_exfat.h
 * @brief exFAT Filesystem Support
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * FEATURES:
 * - exFAT read/write support
 * - File allocation table management
 * - Directory entry parsing
 * - Long filename support (up to 255 chars)
 * - Cluster chain management
 * - Volume label handling
 * - Checksum validation
 * 
 * SPECIFICATIONS:
 * - Microsoft exFAT file system specification
 * - Cluster size: 4KB to 32MB
 * - Max file size: 16 EB (exabytes)
 * - Max volume size: 128 PB (petabytes)
 * 
 * USE CASES:
 * - SDXC cards (>32GB)
 * - Flash drives
 * - Cross-platform storage
 */

#ifndef UFT_EXFAT_H
#define UFT_EXFAT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_EXFAT_SIGNATURE         0xAA55
#define UFT_EXFAT_FS_NAME           "EXFAT   "  /* 8 bytes, space-padded */
#define UFT_EXFAT_BOOT_SECTOR_COUNT 12
#define UFT_EXFAT_MAX_FILENAME      255

/* Volume flags */
#define UFT_EXFAT_FLAG_ACTIVE_FAT   0x0001
#define UFT_EXFAT_FLAG_VOLUME_DIRTY 0x0002
#define UFT_EXFAT_FLAG_MEDIA_FAIL   0x0004
#define UFT_EXFAT_FLAG_CLEAR_ZERO   0x0008

/* Directory entry types */
#define UFT_EXFAT_ENTRY_EOD         0x00    /* End of directory */
#define UFT_EXFAT_ENTRY_BITMAP      0x81    /* Allocation bitmap */
#define UFT_EXFAT_ENTRY_UPCASE      0x82    /* Upcase table */
#define UFT_EXFAT_ENTRY_LABEL       0x83    /* Volume label */
#define UFT_EXFAT_ENTRY_FILE        0x85    /* File directory entry */
#define UFT_EXFAT_ENTRY_GUID        0xA0    /* Volume GUID */
#define UFT_EXFAT_ENTRY_STREAM      0xC0    /* Stream extension */
#define UFT_EXFAT_ENTRY_NAME        0xC1    /* File name extension */
#define UFT_EXFAT_ENTRY_VENDOR      0xE0    /* Vendor extension */
#define UFT_EXFAT_ENTRY_VENDOR_ALLOC 0xE1   /* Vendor allocation */

/* File attributes */
#define UFT_EXFAT_ATTR_READONLY     0x01
#define UFT_EXFAT_ATTR_HIDDEN       0x02
#define UFT_EXFAT_ATTR_SYSTEM       0x04
#define UFT_EXFAT_ATTR_DIRECTORY    0x10
#define UFT_EXFAT_ATTR_ARCHIVE      0x20

/* Special cluster values */
#define UFT_EXFAT_CLUSTER_FREE      0x00000000
#define UFT_EXFAT_CLUSTER_RESERVED  0x00000001
#define UFT_EXFAT_CLUSTER_BAD       0xFFFFFFF7
#define UFT_EXFAT_CLUSTER_END       0xFFFFFFFF
#define UFT_EXFAT_CLUSTER_MIN       2

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boot Sector (512 bytes)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  jump_boot[3];          /* Jump instruction (0xEB 0x76 0x90) */
    char     fs_name[8];            /* "EXFAT   " */
    uint8_t  must_be_zero[53];      /* Reserved (must be 0) */
    
    uint64_t partition_offset;      /* Sector offset of partition */
    uint64_t volume_length;         /* Size of volume in sectors */
    uint32_t fat_offset;            /* Sector offset of first FAT */
    uint32_t fat_length;            /* Length of each FAT in sectors */
    uint32_t cluster_heap_offset;   /* Sector offset of cluster heap */
    uint32_t cluster_count;         /* Number of clusters in cluster heap */
    uint32_t first_cluster_root;    /* First cluster of root directory */
    
    uint32_t volume_serial;         /* Volume serial number */
    uint16_t fs_revision;           /* File system revision (0x0100) */
    uint16_t volume_flags;          /* Volume flags */
    
    uint8_t  bytes_per_sector_shift; /* log2(bytes per sector), 9-12 */
    uint8_t  sectors_per_cluster_shift; /* log2(sectors per cluster), 0-25 */
    uint8_t  number_of_fats;        /* Number of FATs (1 or 2) */
    uint8_t  drive_select;          /* INT 13h drive number */
    uint8_t  percent_in_use;        /* Percentage of heap in use */
    
    uint8_t  reserved[7];
    uint8_t  boot_code[390];        /* Boot code */
    uint16_t boot_signature;        /* 0xAA55 */
} uft_exfat_boot_sector_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Extended Boot Sectors (sectors 1-8)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  extended_boot_code[510];
    uint16_t extended_boot_signature; /* 0xAA55 */
} uft_exfat_extended_boot_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════════
 * OEM Parameters (sector 9)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#pragma pack(push, 1)
typedef struct {
    uint8_t  parameters[480];
    uint8_t  reserved[32];
} uft_exfat_oem_params_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Boot Checksum (sector 11)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Sector 11 contains repeated 32-bit checksums */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Entries (32 bytes each)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Generic directory entry */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* Entry type and flags */
    uint8_t  custom[19];            /* Type-specific data */
    uint32_t first_cluster;         /* First cluster */
    uint64_t data_length;           /* Data length */
} uft_exfat_dir_entry_t;
#pragma pack(pop)

/* File directory entry (type 0x85) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* 0x85 */
    uint8_t  secondary_count;       /* Number of secondary entries */
    uint16_t set_checksum;          /* Checksum of entry set */
    uint16_t file_attributes;       /* File attributes */
    uint16_t reserved1;
    
    uint32_t create_timestamp;      /* Create time (DOS format) */
    uint32_t modify_timestamp;      /* Last modified time */
    uint32_t access_timestamp;      /* Last access time */
    
    uint8_t  create_10ms;           /* Create time 10ms increment */
    uint8_t  modify_10ms;           /* Modify time 10ms increment */
    uint8_t  create_utc_offset;     /* Create time UTC offset */
    uint8_t  modify_utc_offset;     /* Modify time UTC offset */
    uint8_t  access_utc_offset;     /* Access time UTC offset */
    
    uint8_t  reserved2[7];
} uft_exfat_file_entry_t;
#pragma pack(pop)

/* Stream extension entry (type 0xC0) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* 0xC0 */
    uint8_t  general_secondary_flags;
    uint8_t  reserved1;
    uint8_t  name_length;           /* Length of filename in chars */
    uint16_t name_hash;             /* Filename hash */
    uint16_t reserved2;
    
    uint64_t valid_data_length;     /* Valid data length */
    uint32_t reserved3;
    uint32_t first_cluster;         /* First cluster of data */
    uint64_t data_length;           /* Allocated data length */
} uft_exfat_stream_entry_t;
#pragma pack(pop)

/* File name extension entry (type 0xC1) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* 0xC1 */
    uint8_t  general_secondary_flags;
    uint16_t file_name[15];         /* Up to 15 UTF-16 characters */
} uft_exfat_name_entry_t;
#pragma pack(pop)

/* Volume label entry (type 0x83) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* 0x83 */
    uint8_t  character_count;       /* Label length (0-11) */
    uint16_t volume_label[11];      /* UTF-16 label */
    uint8_t  reserved[8];
} uft_exfat_label_entry_t;
#pragma pack(pop)

/* Allocation bitmap entry (type 0x81) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* 0x81 */
    uint8_t  bitmap_flags;          /* 0 = first, 1 = second bitmap */
    uint8_t  reserved[18];
    uint32_t first_cluster;         /* First cluster of bitmap */
    uint64_t data_length;           /* Size of bitmap in bytes */
} uft_exfat_bitmap_entry_t;
#pragma pack(pop)

/* Upcase table entry (type 0x82) */
#pragma pack(push, 1)
typedef struct {
    uint8_t  entry_type;            /* 0x82 */
    uint8_t  reserved1[3];
    uint32_t table_checksum;        /* Checksum of upcase table */
    uint8_t  reserved2[12];
    uint32_t first_cluster;         /* First cluster of table */
    uint64_t data_length;           /* Size of table */
} uft_exfat_upcase_entry_t;
#pragma pack(pop)

/* ═══════════════════════════════════════════════════════════════════════════════
 * Timestamp Conversion
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uint16_t year;      /* 1980 + year field */
    uint8_t  month;     /* 1-12 */
    uint8_t  day;       /* 1-31 */
    uint8_t  hour;      /* 0-23 */
    uint8_t  minute;    /* 0-59 */
    uint8_t  second;    /* 0-59 */
    uint16_t ms;        /* Milliseconds 0-999 */
    int8_t   utc_offset;/* UTC offset in 15-min increments */
} uft_exfat_timestamp_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * File Information
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    char     name[UFT_EXFAT_MAX_FILENAME + 1];  /* UTF-8 filename */
    uint16_t attributes;
    
    uint64_t size;              /* File size in bytes */
    uint64_t allocated_size;    /* Allocated size */
    uint32_t first_cluster;     /* Starting cluster */
    
    uft_exfat_timestamp_t created;
    uft_exfat_timestamp_t modified;
    uft_exfat_timestamp_t accessed;
    
    bool     is_directory;
    bool     is_contiguous;     /* Data is contiguous (no FAT chain) */
} uft_exfat_file_info_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Volume Structure
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    /* Boot sector info */
    uft_exfat_boot_sector_t boot;
    
    /* Calculated values */
    uint32_t bytes_per_sector;
    uint32_t bytes_per_cluster;
    uint32_t sectors_per_cluster;
    uint64_t total_size;
    uint64_t free_space;
    
    /* FAT */
    uint32_t *fat;              /* File Allocation Table */
    uint32_t fat_entries;       /* Number of FAT entries */
    
    /* Allocation bitmap */
    uint8_t  *bitmap;
    size_t   bitmap_size;
    
    /* Upcase table */
    uint16_t *upcase;
    size_t   upcase_entries;
    
    /* Volume label */
    char     label[24];         /* UTF-8 volume label */
    
    /* Raw data access */
    uint8_t  *data;
    size_t   data_size;
    
    /* State */
    bool     modified;
    bool     mounted;
    char     *filename;
} uft_exfat_volume_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Directory Iterator
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_exfat_volume_t *volume;
    uint32_t cluster;           /* Current cluster */
    uint32_t entry_index;       /* Entry index within cluster */
    bool     at_end;
} uft_exfat_dir_iter_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Volume Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect if data is exFAT
 * @return Confidence 0-100
 */
int uft_exfat_detect(const uint8_t *data, size_t size);

/**
 * @brief Mount exFAT volume
 */
int uft_exfat_mount(const char *filename, uft_exfat_volume_t *volume);

/**
 * @brief Mount from memory
 */
int uft_exfat_mount_mem(const uint8_t *data, size_t size,
                        uft_exfat_volume_t *volume);

/**
 * @brief Format new exFAT volume
 */
int uft_exfat_format(uft_exfat_volume_t *volume,
                     uint64_t size,
                     uint32_t cluster_size,
                     const char *label);

/**
 * @brief Unmount volume
 */
void uft_exfat_unmount(uft_exfat_volume_t *volume);

/**
 * @brief Sync changes to disk
 */
int uft_exfat_sync(uft_exfat_volume_t *volume);

/**
 * @brief Get volume information
 */
void uft_exfat_volume_info(const uft_exfat_volume_t *volume);

/**
 * @brief Set volume label
 */
int uft_exfat_set_label(uft_exfat_volume_t *volume, const char *label);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Directory Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open directory for iteration
 * @param path Directory path (use "/" for root)
 */
int uft_exfat_dir_open(uft_exfat_volume_t *volume, const char *path,
                       uft_exfat_dir_iter_t *iter);

/**
 * @brief Get next directory entry
 * @return 0 on success, -1 at end
 */
int uft_exfat_dir_next(uft_exfat_dir_iter_t *iter,
                       uft_exfat_file_info_t *info);

/**
 * @brief Close directory iterator
 */
void uft_exfat_dir_close(uft_exfat_dir_iter_t *iter);

/**
 * @brief Create directory
 */
int uft_exfat_mkdir(uft_exfat_volume_t *volume, const char *path);

/**
 * @brief Remove directory (must be empty)
 */
int uft_exfat_rmdir(uft_exfat_volume_t *volume, const char *path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - File Operations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get file information
 */
int uft_exfat_stat(uft_exfat_volume_t *volume, const char *path,
                   uft_exfat_file_info_t *info);

/**
 * @brief Read file contents
 */
int uft_exfat_read_file(uft_exfat_volume_t *volume, const char *path,
                        uint8_t *buffer, size_t size, uint64_t offset);

/**
 * @brief Write file contents
 */
int uft_exfat_write_file(uft_exfat_volume_t *volume, const char *path,
                         const uint8_t *data, size_t size, uint64_t offset);

/**
 * @brief Create new file
 */
int uft_exfat_create_file(uft_exfat_volume_t *volume, const char *path);

/**
 * @brief Delete file
 */
int uft_exfat_delete_file(uft_exfat_volume_t *volume, const char *path);

/**
 * @brief Rename/move file
 */
int uft_exfat_rename(uft_exfat_volume_t *volume,
                     const char *old_path, const char *new_path);

/**
 * @brief Truncate file to size
 */
int uft_exfat_truncate(uft_exfat_volume_t *volume, const char *path,
                       uint64_t new_size);

/**
 * @brief Extract file to host filesystem
 */
int uft_exfat_extract(uft_exfat_volume_t *volume, const char *src_path,
                      const char *dest_path);

/**
 * @brief Add file from host filesystem
 */
int uft_exfat_add(uft_exfat_volume_t *volume, const char *src_path,
                  const char *dest_path);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Cluster Management
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read cluster data
 */
int uft_exfat_read_cluster(const uft_exfat_volume_t *volume,
                           uint32_t cluster, uint8_t *buffer);

/**
 * @brief Write cluster data
 */
int uft_exfat_write_cluster(uft_exfat_volume_t *volume,
                            uint32_t cluster, const uint8_t *data);

/**
 * @brief Get next cluster in chain
 */
uint32_t uft_exfat_next_cluster(const uft_exfat_volume_t *volume,
                                uint32_t cluster);

/**
 * @brief Allocate clusters
 */
int uft_exfat_alloc_clusters(uft_exfat_volume_t *volume,
                             uint32_t count, uint32_t *first);

/**
 * @brief Free cluster chain
 */
int uft_exfat_free_chain(uft_exfat_volume_t *volume, uint32_t first);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Utilities
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Validate filesystem structure
 */
int uft_exfat_validate(const uft_exfat_volume_t *volume);

/**
 * @brief Check and repair filesystem
 */
int uft_exfat_fsck(uft_exfat_volume_t *volume, bool repair);

/**
 * @brief Calculate boot sector checksum
 */
uint32_t uft_exfat_boot_checksum(const uint8_t *sectors);

/**
 * @brief Calculate entry set checksum
 */
uint16_t uft_exfat_entry_checksum(const uft_exfat_dir_entry_t *entries,
                                  int count);

/**
 * @brief Convert timestamp to struct
 */
void uft_exfat_decode_timestamp(uint32_t timestamp, uint8_t ms_10,
                                int8_t utc_offset,
                                uft_exfat_timestamp_t *ts);

/**
 * @brief Convert struct to timestamp
 */
uint32_t uft_exfat_encode_timestamp(const uft_exfat_timestamp_t *ts,
                                    uint8_t *ms_10, int8_t *utc_offset);

/**
 * @brief Calculate filename hash
 */
uint16_t uft_exfat_filename_hash(const uint16_t *name, int length,
                                 const uint16_t *upcase);

#ifdef __cplusplus
}
#endif

#endif /* UFT_EXFAT_H */
