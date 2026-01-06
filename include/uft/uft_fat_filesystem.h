/**
 * @file uft_fat_filesystem.h
 * @brief FAT12/FAT16/FAT32 Filesystem Structures and Recovery
 * @version 3.3.0
 *
 * Extracted from:
 * - FAT12Extractor-master (msdosdir.c, msdosextr.c)
 * - DataRecovery-1-master (fatmbr.h, ntfs.h)
 *
 * Provides:
 * - Complete FAT12/16/32 Boot Sector structures
 * - FAT Entry decoding (12/16/32-bit)
 * - Directory entry parsing
 * - Cluster chain following
 * - Recovery algorithms for damaged FAT
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_FAT_FILESYSTEM_H
#define UFT_FAT_FILESYSTEM_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * PRAGMA PACK - Plattformunabhängig
 *============================================================================*/

#ifndef UFT_PACKED_BEGIN
#if defined(_MSC_VER)
    #define UFT_PACKED_BEGIN __pragma(pack(push, 1))
    #define UFT_PACKED_END   __pragma(pack(pop))
    #define UFT_PACKED
#elif defined(__GNUC__) || defined(__clang__)
    #define UFT_PACKED_BEGIN
    #define UFT_PACKED_END
    #define UFT_PACKED 
#else
    #define UFT_PACKED_BEGIN
    #define UFT_PACKED_END
    #define UFT_PACKED
#endif

/*============================================================================
 * FAT12/16 BOOT SECTOR (BPB - BIOS Parameter Block)
 *============================================================================*/

UFT_PACKED_BEGIN
typedef struct uft_fat_bpb {
    uint8_t  jump[3];               /* 0x00: Jump instruction (EB XX 90) */
    uint8_t  oem_name[8];           /* 0x03: OEM Name (e.g., "MSDOS5.0") */
    uint16_t bytes_per_sector;      /* 0x0B: Bytes per sector (512, 1024, 2048, 4096) */
    uint8_t  sectors_per_cluster;   /* 0x0D: Sectors per cluster (1,2,4,8,16,32,64,128) */
    uint16_t reserved_sectors;      /* 0x0E: Reserved sectors (FAT12/16: 1, FAT32: 32) */
    uint8_t  num_fats;              /* 0x10: Number of FATs (usually 2) */
    uint16_t root_entries;          /* 0x11: Root directory entries (FAT12/16: 224/512) */
    uint16_t total_sectors_16;      /* 0x13: Total sectors (16-bit, 0 if >65535) */
    uint8_t  media_type;            /* 0x15: Media descriptor (F0=removable, F8=fixed) */
    uint16_t fat_size_16;           /* 0x16: Sectors per FAT (FAT12/16) */
    uint16_t sectors_per_track;     /* 0x18: Sectors per track */
    uint16_t num_heads;             /* 0x1A: Number of heads */
    uint32_t hidden_sectors;        /* 0x1C: Hidden sectors */
    uint32_t total_sectors_32;      /* 0x20: Total sectors (32-bit) */
    
    /* FAT12/16 Extended Boot Record */
    uint8_t  drive_number;          /* 0x24: Drive number (0x00=floppy, 0x80=HDD) */
    uint8_t  reserved1;             /* 0x25: Reserved (NT flags) */
    uint8_t  boot_signature;        /* 0x26: Extended boot signature (0x29) */
    uint32_t volume_serial;         /* 0x27: Volume serial number */
    uint8_t  volume_label[11];      /* 0x2B: Volume label */
    uint8_t  fs_type[8];            /* 0x36: File system type ("FAT12   ", "FAT16   ") */
    uint8_t  boot_code[448];        /* 0x3E: Boot code */
    uint16_t signature;             /* 0x1FE: Boot sector signature (0xAA55) */
} UFT_PACKED uft_fat_bpb_t;
UFT_PACKED_END

/*============================================================================
 * FAT32 EXTENDED BPB
 *============================================================================*/

UFT_PACKED_BEGIN
typedef struct uft_fat32_bpb {
    /* Standard BPB (first 36 bytes identical to FAT12/16) */
    uint8_t  jump[3];
    uint8_t  oem_name[8];
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;          /* Must be 0 for FAT32 */
    uint16_t total_sectors_16;      /* Must be 0 for FAT32 */
    uint8_t  media_type;
    uint16_t fat_size_16;           /* Must be 0 for FAT32 */
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    
    /* FAT32 Extended BPB */
    uint32_t fat_size_32;           /* 0x24: Sectors per FAT (FAT32) */
    uint16_t ext_flags;             /* 0x28: Extended flags */
    uint16_t fs_version;            /* 0x2A: File system version (0x0000) */
    uint32_t root_cluster;          /* 0x2C: Root directory cluster (usually 2) */
    uint16_t fs_info_sector;        /* 0x30: FSInfo sector (usually 1) */
    uint16_t backup_boot_sector;    /* 0x32: Backup boot sector (usually 6) */
    uint8_t  reserved[12];          /* 0x34: Reserved */
    
    /* Extended Boot Record (same as FAT12/16 at offset 0x40) */
    uint8_t  drive_number;          /* 0x40: Drive number */
    uint8_t  reserved1;             /* 0x41: Reserved */
    uint8_t  boot_signature;        /* 0x42: Extended boot signature (0x29) */
    uint32_t volume_serial;         /* 0x43: Volume serial number */
    uint8_t  volume_label[11];      /* 0x47: Volume label */
    uint8_t  fs_type[8];            /* 0x52: File system type ("FAT32   ") */
    uint8_t  boot_code[420];        /* 0x5A: Boot code */
    uint16_t signature;             /* 0x1FE: Boot sector signature (0xAA55) */
} UFT_PACKED uft_fat32_bpb_t;
UFT_PACKED_END

/*============================================================================
 * FAT32 FSINFO SECTOR
 *============================================================================*/

UFT_PACKED_BEGIN
typedef struct uft_fat32_fsinfo {
    uint32_t lead_sig;              /* 0x00: Lead signature (0x41615252) */
    uint8_t  reserved1[480];        /* 0x04: Reserved */
    uint32_t struct_sig;            /* 0x1E4: Structure signature (0x61417272) */
    uint32_t free_count;            /* 0x1E8: Free cluster count (0xFFFFFFFF=unknown) */
    uint32_t next_free;             /* 0x1EC: Next free cluster hint */
    uint8_t  reserved2[12];         /* 0x1F0: Reserved */
    uint32_t trail_sig;             /* 0x1FC: Trail signature (0xAA550000) */
} UFT_PACKED uft_fat32_fsinfo_t;
UFT_PACKED_END

/* FSInfo signatures */
#define UFT_FSINFO_LEAD_SIG     0x41615252
#define UFT_FSINFO_STRUCT_SIG   0x61417272
#define UFT_FSINFO_TRAIL_SIG    0xAA550000

/*============================================================================
 * DIRECTORY ENTRY (8.3 Format)
 *============================================================================*/

UFT_PACKED_BEGIN
typedef struct uft_fat_dirent {
    uint8_t  name[8];               /* 0x00: Filename (space padded) */
    uint8_t  ext[3];                /* 0x08: Extension (space padded) */
    uint8_t  attr;                  /* 0x0B: Attributes */
    uint8_t  nt_reserved;           /* 0x0C: Reserved (NT) */
    uint8_t  create_time_tenth;     /* 0x0D: Creation time (10ms units) */
    uint16_t create_time;           /* 0x0E: Creation time */
    uint16_t create_date;           /* 0x10: Creation date */
    uint16_t access_date;           /* 0x12: Last access date */
    uint16_t cluster_hi;            /* 0x14: High word of cluster (FAT32) */
    uint16_t modify_time;           /* 0x16: Modification time */
    uint16_t modify_date;           /* 0x18: Modification date */
    uint16_t cluster_lo;            /* 0x1A: Low word of cluster */
    uint32_t file_size;             /* 0x1C: File size in bytes */
} UFT_PACKED uft_fat_dirent_t;
UFT_PACKED_END

/* Directory entry attributes */
#define UFT_FAT_ATTR_READ_ONLY  0x01
#define UFT_FAT_ATTR_HIDDEN     0x02
#define UFT_FAT_ATTR_SYSTEM     0x04
#define UFT_FAT_ATTR_VOLUME_ID  0x08
#define UFT_FAT_ATTR_DIRECTORY  0x10
#define UFT_FAT_ATTR_ARCHIVE    0x20
#define UFT_FAT_ATTR_LFN        0x0F  /* Long filename entry */

/* Special filename markers */
#define UFT_FAT_ENTRY_FREE      0xE5  /* Deleted entry */
#define UFT_FAT_ENTRY_END       0x00  /* End of directory */
#define UFT_FAT_ENTRY_KANJI     0x05  /* First char is E5 (Kanji) */

/*============================================================================
 * LONG FILENAME ENTRY (LFN)
 *============================================================================*/

UFT_PACKED_BEGIN
typedef struct uft_fat_lfn_entry {
    uint8_t  ordinal;               /* 0x00: Sequence number (0x40 = last) */
    uint16_t name1[5];              /* 0x01: Characters 1-5 (UCS-2) */
    uint8_t  attr;                  /* 0x0B: Attributes (always 0x0F) */
    uint8_t  type;                  /* 0x0C: Type (always 0) */
    uint8_t  checksum;              /* 0x0D: Checksum of 8.3 name */
    uint16_t name2[6];              /* 0x0E: Characters 6-11 (UCS-2) */
    uint16_t cluster;               /* 0x1A: Cluster (always 0) */
    uint16_t name3[2];              /* 0x1C: Characters 12-13 (UCS-2) */
} UFT_PACKED uft_fat_lfn_entry_t;
UFT_PACKED_END

#define UFT_FAT_LFN_LAST        0x40  /* Last LFN entry marker */
#define UFT_FAT_LFN_DELETED     0x80  /* Deleted LFN entry */

/*============================================================================
 * FAT ENTRY VALUES
 *============================================================================*/

/* FAT12 special values (12-bit) */
#define UFT_FAT12_FREE          0x000
#define UFT_FAT12_RESERVED      0x001
#define UFT_FAT12_BAD           0xFF7
#define UFT_FAT12_EOC_MIN       0xFF8
#define UFT_FAT12_EOC           0xFFF

/* FAT16 special values (16-bit) */
#define UFT_FAT16_FREE          0x0000
#define UFT_FAT16_RESERVED      0x0001
#define UFT_FAT16_BAD           0xFFF7
#define UFT_FAT16_EOC_MIN       0xFFF8
#define UFT_FAT16_EOC           0xFFFF

/* FAT32 special values (28-bit, upper 4 bits reserved) */
#define UFT_FAT32_FREE          0x00000000
#define UFT_FAT32_RESERVED      0x00000001
#define UFT_FAT32_BAD           0x0FFFFFF7
#define UFT_FAT32_EOC_MIN       0x0FFFFFF8
#define UFT_FAT32_EOC           0x0FFFFFFF
#define UFT_FAT32_MASK          0x0FFFFFFF

/*============================================================================
 * MEDIA DESCRIPTOR TYPES
 *============================================================================*/

#define UFT_MEDIA_FIXED         0xF8  /* Fixed disk (HDD) */
#define UFT_MEDIA_REMOVABLE     0xF0  /* Removable 3.5" 1.44M */
#define UFT_MEDIA_F9_1440       0xF9  /* 3.5" 720K or 5.25" 1.2M */
#define UFT_MEDIA_FD_360        0xFD  /* 5.25" 360K */
#define UFT_MEDIA_FF_320        0xFF  /* 5.25" 320K */
#define UFT_MEDIA_FC_180        0xFC  /* 5.25" 180K */
#define UFT_MEDIA_FE_160        0xFE  /* 5.25" 160K */

/*============================================================================
 * FAT TYPE DETECTION
 *============================================================================*/

typedef enum {
    UFT_FAT_TYPE_UNKNOWN = 0,
    UFT_FAT_TYPE_FAT12,
    UFT_FAT_TYPE_FAT16,
    UFT_FAT_TYPE_FAT32,
    UFT_FAT_TYPE_EXFAT
} uft_fat_type_t;

/**
 * @brief Detect FAT type from BPB
 * 
 * Per Microsoft FAT specification:
 * - Count of clusters determines FAT type
 * - < 4085 clusters: FAT12
 * - < 65525 clusters: FAT16
 * - >= 65525 clusters: FAT32
 */
static inline uft_fat_type_t uft_fat_detect_type(const uft_fat_bpb_t *bpb) {
    uint32_t total_sectors = bpb->total_sectors_16 ? 
                             bpb->total_sectors_16 : bpb->total_sectors_32;
    
    uint32_t fat_size = bpb->fat_size_16 ? 
                        bpb->fat_size_16 : 
                        ((const uft_fat32_bpb_t*)bpb)->fat_size_32;
    
    uint32_t root_dir_sectors = ((bpb->root_entries * 32) + 
                                 (bpb->bytes_per_sector - 1)) / 
                                bpb->bytes_per_sector;
    
    uint32_t data_sectors = total_sectors - 
                            (bpb->reserved_sectors + 
                             (bpb->num_fats * fat_size) + 
                             root_dir_sectors);
    
    uint32_t count_of_clusters = data_sectors / bpb->sectors_per_cluster;
    
    if (count_of_clusters < 4085)
        return UFT_FAT_TYPE_FAT12;
    else if (count_of_clusters < 65525)
        return UFT_FAT_TYPE_FAT16;
    else
        return UFT_FAT_TYPE_FAT32;
}

/*============================================================================
 * FAT12 CLUSTER CHAIN HELPERS
 *============================================================================*/

/**
 * @brief Read FAT12 entry (12-bit packed values)
 * 
 * FAT12 stores two 12-bit values in 3 bytes:
 * Byte 0: Low 8 bits of entry N
 * Byte 1: High 4 bits of entry N (low nibble), Low 4 bits of entry N+1 (high nibble)
 * Byte 2: High 8 bits of entry N+1
 * 
 * From FAT12Extractor:
 * - Even clusters: bits [11:4] = byte[1] low nibble, bits [3:0] = byte[0]
 * - Odd clusters: bits [11:4] = byte[2], bits [3:0] = byte[1] high nibble
 */
static inline uint16_t uft_fat12_get_entry(const uint8_t *fat, uint16_t cluster) {
    uint32_t offset = cluster + (cluster / 2);  /* cluster * 1.5 */
    uint16_t value = fat[offset] | (fat[offset + 1] << 8);
    
    if (cluster & 1) {
        /* Odd cluster: use high 12 bits */
        return value >> 4;
    } else {
        /* Even cluster: use low 12 bits */
        return value & 0x0FFF;
    }
}

/**
 * @brief Check if FAT12 cluster is end-of-chain
 */
static inline bool uft_fat12_is_eoc(uint16_t entry) {
    return entry >= UFT_FAT12_EOC_MIN;
}

/**
 * @brief Check if FAT12 cluster is bad
 */
static inline bool uft_fat12_is_bad(uint16_t entry) {
    return entry == UFT_FAT12_BAD;
}

/*============================================================================
 * FAT16 CLUSTER CHAIN HELPERS
 *============================================================================*/

static inline uint16_t uft_fat16_get_entry(const uint8_t *fat, uint16_t cluster) {
    return ((const uint16_t*)fat)[cluster];
}

static inline bool uft_fat16_is_eoc(uint16_t entry) {
    return entry >= UFT_FAT16_EOC_MIN;
}

static inline bool uft_fat16_is_bad(uint16_t entry) {
    return entry == UFT_FAT16_BAD;
}

/*============================================================================
 * FAT32 CLUSTER CHAIN HELPERS
 *============================================================================*/

static inline uint32_t uft_fat32_get_entry(const uint8_t *fat, uint32_t cluster) {
    return ((const uint32_t*)fat)[cluster] & UFT_FAT32_MASK;
}

static inline bool uft_fat32_is_eoc(uint32_t entry) {
    return entry >= UFT_FAT32_EOC_MIN;
}

static inline bool uft_fat32_is_bad(uint32_t entry) {
    return entry == UFT_FAT32_BAD;
}

/*============================================================================
 * DATE/TIME DECODING
 *============================================================================*/

/**
 * @brief FAT time format: HHHHHMMMMMMSSSSS
 * - Hours: bits 15-11 (0-23)
 * - Minutes: bits 10-5 (0-59)
 * - Seconds: bits 4-0 (0-29, multiply by 2)
 */
static inline void uft_fat_decode_time(uint16_t time, 
                                        uint8_t *hours, 
                                        uint8_t *minutes, 
                                        uint8_t *seconds) {
    *hours = (time >> 11) & 0x1F;
    *minutes = (time >> 5) & 0x3F;
    *seconds = (time & 0x1F) * 2;
}

/**
 * @brief FAT date format: YYYYYYYMMMMDDDDD
 * - Year: bits 15-9 (0-127, add 1980)
 * - Month: bits 8-5 (1-12)
 * - Day: bits 4-0 (1-31)
 */
static inline void uft_fat_decode_date(uint16_t date,
                                        uint16_t *year,
                                        uint8_t *month,
                                        uint8_t *day) {
    *year = ((date >> 9) & 0x7F) + 1980;
    *month = (date >> 5) & 0x0F;
    *day = date & 0x1F;
}

/*============================================================================
 * DIRECTORY ENTRY VALIDATION
 *============================================================================*/

/**
 * @brief Check if directory entry is valid (from FAT12Extractor)
 */
static inline bool uft_fat_dirent_is_valid(const uft_fat_dirent_t *entry) {
    uint8_t first = entry->name[0];
    
    /* Empty or end of directory */
    if (first == UFT_FAT_ENTRY_END || first == 0x20)
        return false;
    
    /* Deleted entry */
    if (first == UFT_FAT_ENTRY_FREE || first == UFT_FAT_ENTRY_KANJI)
        return false;
    
    /* Volume label (not a file/directory) */
    if (entry->attr & UFT_FAT_ATTR_VOLUME_ID)
        return false;
    
    /* Must have a cluster assigned */
    if (entry->cluster_lo == 0 && entry->cluster_hi == 0)
        return false;
    
    return true;
}

/**
 * @brief Check if entry is a directory
 */
static inline bool uft_fat_dirent_is_dir(const uft_fat_dirent_t *entry) {
    return (entry->attr & UFT_FAT_ATTR_DIRECTORY) != 0;
}

/**
 * @brief Check if entry is a long filename entry
 */
static inline bool uft_fat_dirent_is_lfn(const uft_fat_dirent_t *entry) {
    return (entry->attr & UFT_FAT_ATTR_LFN) == UFT_FAT_ATTR_LFN;
}

/**
 * @brief Get full cluster number from directory entry
 */
static inline uint32_t uft_fat_dirent_cluster(const uft_fat_dirent_t *entry) {
    return ((uint32_t)entry->cluster_hi << 16) | entry->cluster_lo;
}

/*============================================================================
 * FILESYSTEM LAYOUT CALCULATION
 *============================================================================*/

/**
 * @brief Calculate filesystem layout from BPB
 */
typedef struct uft_fat_layout {
    uft_fat_type_t type;
    uint32_t bytes_per_sector;
    uint32_t sectors_per_cluster;
    uint32_t cluster_size;          /* bytes_per_sector * sectors_per_cluster */
    uint32_t reserved_sectors;
    uint32_t fat_sectors;           /* Sectors per FAT */
    uint32_t num_fats;
    uint32_t root_dir_sectors;
    uint32_t root_dir_entries;
    uint32_t first_fat_sector;
    uint32_t first_root_dir_sector;
    uint32_t first_data_sector;
    uint32_t data_sectors;
    uint32_t total_clusters;
    uint32_t root_cluster;          /* FAT32 only */
} uft_fat_layout_t;

static inline void uft_fat_calc_layout(const void *bpb_raw, uft_fat_layout_t *layout) {
    const uft_fat_bpb_t *bpb = (const uft_fat_bpb_t*)bpb_raw;
    const uft_fat32_bpb_t *bpb32 = (const uft_fat32_bpb_t*)bpb_raw;
    
    memset(layout, 0, sizeof(*layout));
    
    layout->bytes_per_sector = bpb->bytes_per_sector;
    layout->sectors_per_cluster = bpb->sectors_per_cluster;
    layout->cluster_size = layout->bytes_per_sector * layout->sectors_per_cluster;
    layout->reserved_sectors = bpb->reserved_sectors;
    layout->num_fats = bpb->num_fats;
    layout->root_dir_entries = bpb->root_entries;
    
    /* FAT size */
    layout->fat_sectors = bpb->fat_size_16 ? bpb->fat_size_16 : bpb32->fat_size_32;
    
    /* Root directory sectors (FAT12/16 only) */
    layout->root_dir_sectors = ((bpb->root_entries * 32) + 
                                (bpb->bytes_per_sector - 1)) / 
                               bpb->bytes_per_sector;
    
    /* Sector calculations */
    layout->first_fat_sector = bpb->reserved_sectors;
    layout->first_root_dir_sector = layout->first_fat_sector + 
                                    (layout->num_fats * layout->fat_sectors);
    layout->first_data_sector = layout->first_root_dir_sector + 
                                layout->root_dir_sectors;
    
    /* Total sectors */
    uint32_t total_sectors = bpb->total_sectors_16 ? 
                             bpb->total_sectors_16 : bpb->total_sectors_32;
    
    /* Data sectors and clusters */
    layout->data_sectors = total_sectors - layout->first_data_sector;
    layout->total_clusters = layout->data_sectors / layout->sectors_per_cluster;
    
    /* Detect type */
    layout->type = uft_fat_detect_type(bpb);
    
    /* FAT32 root cluster */
    if (layout->type == UFT_FAT_TYPE_FAT32) {
        layout->root_cluster = bpb32->root_cluster;
    }
}

/**
 * @brief Convert cluster number to sector number
 */
static inline uint32_t uft_fat_cluster_to_sector(const uft_fat_layout_t *layout,
                                                  uint32_t cluster) {
    /* Clusters start at 2 */
    return layout->first_data_sector + 
           ((cluster - 2) * layout->sectors_per_cluster);
}

/*============================================================================
 * BPB VALIDATION
 *============================================================================*/

/**
 * @brief Validate boot sector signature and basic BPB
 */
static inline bool uft_fat_validate_bpb(const uft_fat_bpb_t *bpb) {
    /* Check boot signature */
    if (bpb->signature != 0xAA55)
        return false;
    
    /* Check bytes per sector (must be power of 2, 512-4096) */
    uint16_t bps = bpb->bytes_per_sector;
    if (bps < 512 || bps > 4096 || (bps & (bps - 1)) != 0)
        return false;
    
    /* Check sectors per cluster (must be power of 2, 1-128) */
    uint8_t spc = bpb->sectors_per_cluster;
    if (spc == 0 || spc > 128 || (spc & (spc - 1)) != 0)
        return false;
    
    /* Check number of FATs */
    if (bpb->num_fats < 1 || bpb->num_fats > 2)
        return false;
    
    /* Check media type */
    if (bpb->media_type != 0xF0 && bpb->media_type < 0xF8)
        return false;
    
    return true;
}

/*============================================================================
 * GUI PARAMETER INTEGRATION
 *============================================================================*/

/**
 * @brief FAT Recovery Parameters for GUI
 */
typedef struct uft_fat_recovery_params {
    bool     recover_deleted;       /**< Attempt to recover deleted files */
    bool     rebuild_fat;           /**< Rebuild damaged FAT entries */
    bool     check_cross_links;     /**< Detect cross-linked clusters */
    bool     check_lost_chains;     /**< Find lost cluster chains */
    bool     fix_directory;         /**< Fix directory inconsistencies */
    uint8_t  fat_copy;              /**< Which FAT copy to use (0=primary) */
    bool     verify_checksums;      /**< Verify all checksums */
    bool     deep_scan;             /**< Scan all sectors for signatures */
} uft_fat_recovery_params_t;

static inline void uft_fat_recovery_params_init(uft_fat_recovery_params_t *p) {
    p->recover_deleted = true;
    p->rebuild_fat = false;
    p->check_cross_links = true;
    p->check_lost_chains = true;
    p->fix_directory = false;
    p->fat_copy = 0;
    p->verify_checksums = true;
    p->deep_scan = false;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_FILESYSTEM_H */
