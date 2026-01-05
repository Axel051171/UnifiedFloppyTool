/**
 * @file uft_blkid.h
 * @brief Filesystem and Partition Detection (libblkid-style)
 * 
 * @version 4.2.0
 * @date 2026-01-03
 * 
 * FEATURES:
 * - Magic byte detection for 100+ formats
 * - Superblock parsing
 * - Partition table detection (MBR, GPT, APM)
 * - UUID and label extraction
 * - Confidence scoring
 * 
 * SUPPORTED:
 * - Filesystems: ext2/3/4, FAT12/16/32, exFAT, NTFS, HFS/HFS+, 
 *   ISO9660, UDF, XFS, Btrfs, ZFS, APFS, etc.
 * - Floppy formats: Amiga OFS/FFS, Atari, C64, Apple DOS
 * - Partition tables: MBR, GPT, APM, BSD, Solaris
 * - Archives: tar, cpio, zip (for disk images)
 */

#ifndef UFT_BLKID_H
#define UFT_BLKID_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Format Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    /* Unknown */
    UFT_BLKID_UNKNOWN           = 0,
    
    /* Modern PC Filesystems */
    UFT_BLKID_EXT2              = 100,
    UFT_BLKID_EXT3              = 101,
    UFT_BLKID_EXT4              = 102,
    UFT_BLKID_FAT12             = 110,
    UFT_BLKID_FAT16             = 111,
    UFT_BLKID_FAT32             = 112,
    UFT_BLKID_EXFAT             = 113,
    UFT_BLKID_NTFS              = 120,
    UFT_BLKID_REFS              = 121,
    UFT_BLKID_XFS               = 130,
    UFT_BLKID_BTRFS             = 131,
    UFT_BLKID_ZFS               = 132,
    UFT_BLKID_JFS               = 133,
    UFT_BLKID_REISERFS          = 134,
    UFT_BLKID_F2FS              = 135,
    
    /* Apple Filesystems */
    UFT_BLKID_HFS               = 200,
    UFT_BLKID_HFS_PLUS          = 201,
    UFT_BLKID_APFS              = 202,
    
    /* Optical Media */
    UFT_BLKID_ISO9660           = 300,
    UFT_BLKID_UDF               = 301,
    UFT_BLKID_CDFS              = 302,
    
    /* Unix/Linux Legacy */
    UFT_BLKID_UFS1              = 400,
    UFT_BLKID_UFS2              = 401,
    UFT_BLKID_FFS               = 402,
    UFT_BLKID_MINIX             = 403,
    UFT_BLKID_SYSV              = 404,
    UFT_BLKID_CRAMFS            = 405,
    UFT_BLKID_SQUASHFS          = 406,
    UFT_BLKID_ROMFS             = 407,
    
    /* Floppy/Retro Filesystems */
    UFT_BLKID_AMIGA_OFS         = 500,
    UFT_BLKID_AMIGA_FFS         = 501,
    UFT_BLKID_AMIGA_PFS         = 502,
    UFT_BLKID_ATARI_TOS         = 510,
    UFT_BLKID_ATARI_MINT        = 511,
    UFT_BLKID_C64_CBM           = 520,
    UFT_BLKID_APPLE_DOS         = 530,
    UFT_BLKID_APPLE_PRODOS      = 531,
    UFT_BLKID_CPM               = 540,
    UFT_BLKID_BBC_DFS           = 550,
    UFT_BLKID_BBC_ADFS          = 551,
    UFT_BLKID_MSX_DOS           = 560,
    
    /* Partition Tables */
    UFT_BLKID_PART_MBR          = 900,
    UFT_BLKID_PART_GPT          = 901,
    UFT_BLKID_PART_APM          = 902,
    UFT_BLKID_PART_BSD          = 903,
    UFT_BLKID_PART_SUN          = 904,
    UFT_BLKID_PART_SGI          = 905,
    
    /* Disk Images */
    UFT_BLKID_IMG_ADF           = 1000,
    UFT_BLKID_IMG_D64           = 1001,
    UFT_BLKID_IMG_ATR           = 1002,
    UFT_BLKID_IMG_DSK           = 1003,
    UFT_BLKID_IMG_DMK           = 1004,
    UFT_BLKID_IMG_HFE           = 1005,
    UFT_BLKID_IMG_SCP           = 1006,
    
    /* RAID/LVM */
    UFT_BLKID_LVM2              = 1100,
    UFT_BLKID_MD_RAID           = 1101,
    UFT_BLKID_DMRAID            = 1102,
    UFT_BLKID_LUKS              = 1103,
    
    /* Swap */
    UFT_BLKID_SWAP_LINUX        = 1200,
    UFT_BLKID_SWAP_BSD          = 1201,
    UFT_BLKID_SWAP_SOLARIS      = 1202,
    
} uft_blkid_type_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Magic Definition
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_blkid_type_t type;          /* Format type */
    const char *name;               /* Short name */
    const char *description;        /* Full description */
    
    uint64_t offset;                /* Magic offset in bytes */
    const uint8_t *magic;           /* Magic bytes */
    size_t magic_len;               /* Magic length */
    const uint8_t *mask;            /* Mask (NULL = all bits) */
    
    int priority;                   /* Detection priority (higher = checked first) */
    int min_size;                   /* Minimum required data size */
    
    /* Optional: secondary magic for verification */
    uint64_t magic2_offset;
    const uint8_t *magic2;
    size_t magic2_len;
} uft_blkid_magic_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Result
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    uft_blkid_type_t type;          /* Detected type */
    const char *name;               /* Type name */
    int confidence;                 /* Confidence 0-100 */
    
    /* Extracted information */
    char label[128];                /* Volume label */
    char uuid[64];                  /* UUID string */
    uint64_t size;                  /* Volume size (if available) */
    uint32_t block_size;            /* Block/sector size */
    
    /* For partition tables */
    int partition_count;
    
    /* Additional info */
    char version[32];               /* Filesystem version */
    uint64_t creation_time;         /* Creation timestamp (Unix) */
    uint64_t last_mount_time;       /* Last mount timestamp */
} uft_blkid_result_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Detection Context
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct uft_blkid_probe uft_blkid_probe_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Probe Flags
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_BLKID_FLAG_NONE         = 0,
    UFT_BLKID_FLAG_FILESYSTEMS  = 0x01,     /* Probe filesystems */
    UFT_BLKID_FLAG_PARTITIONS   = 0x02,     /* Probe partition tables */
    UFT_BLKID_FLAG_RAID         = 0x04,     /* Probe RAID signatures */
    UFT_BLKID_FLAG_IMAGES       = 0x08,     /* Probe disk image formats */
    UFT_BLKID_FLAG_RETRO        = 0x10,     /* Probe retro/floppy formats */
    UFT_BLKID_FLAG_ALL          = 0xFF
} uft_blkid_flags_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Probe Creation
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create probe context
 */
uft_blkid_probe_t *uft_blkid_new_probe(void);

/**
 * @brief Create probe from file
 */
uft_blkid_probe_t *uft_blkid_new_probe_from_filename(const char *filename);

/**
 * @brief Set data to probe
 */
int uft_blkid_set_data(uft_blkid_probe_t *probe,
                       const uint8_t *data,
                       size_t size);

/**
 * @brief Set device offset
 */
int uft_blkid_set_offset(uft_blkid_probe_t *probe, uint64_t offset);

/**
 * @brief Set device size
 */
int uft_blkid_set_size(uft_blkid_probe_t *probe, uint64_t size);

/**
 * @brief Set probe flags
 */
void uft_blkid_set_flags(uft_blkid_probe_t *probe, uint32_t flags);

/**
 * @brief Destroy probe context
 */
void uft_blkid_free_probe(uft_blkid_probe_t *probe);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Run all enabled probes
 * @return 0 on success, -1 if nothing found
 */
int uft_blkid_do_probe(uft_blkid_probe_t *probe);

/**
 * @brief Run specific probe type
 */
int uft_blkid_do_safeprobe(uft_blkid_probe_t *probe);

/**
 * @brief Run probe for specific type
 */
int uft_blkid_probe_type(uft_blkid_probe_t *probe, uft_blkid_type_t type);

/**
 * @brief Get probe result
 */
int uft_blkid_get_result(const uft_blkid_probe_t *probe,
                         uft_blkid_result_t *result);

/**
 * @brief Get all matching results
 */
int uft_blkid_get_results(const uft_blkid_probe_t *probe,
                          uft_blkid_result_t *results,
                          int max_results);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Simple Detection
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Quick detection from data
 * @param data Raw data
 * @param size Data size
 * @param result Output result
 * @return 0 on success
 */
int uft_blkid_detect(const uint8_t *data, size_t size,
                     uft_blkid_result_t *result);

/**
 * @brief Quick detection from file
 */
int uft_blkid_detect_file(const char *filename,
                          uft_blkid_result_t *result);

/**
 * @brief Get type name string
 */
const char *uft_blkid_type_name(uft_blkid_type_t type);

/**
 * @brief Get type description
 */
const char *uft_blkid_type_description(uft_blkid_type_t type);

/**
 * @brief Get type by name
 */
uft_blkid_type_t uft_blkid_type_by_name(const char *name);

/* ═══════════════════════════════════════════════════════════════════════════════
 * API Functions - Value Extraction
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get string value
 */
int uft_blkid_lookup_value(const uft_blkid_probe_t *probe,
                           const char *name,
                           char *value,
                           size_t value_size);

/**
 * @brief Get integer value
 */
int uft_blkid_lookup_int(const uft_blkid_probe_t *probe,
                         const char *name,
                         int64_t *value);

/**
 * @brief Standard value names */
#define UFT_BLKID_TAG_TYPE          "TYPE"
#define UFT_BLKID_TAG_LABEL         "LABEL"
#define UFT_BLKID_TAG_UUID          "UUID"
#define UFT_BLKID_TAG_VERSION       "VERSION"
#define UFT_BLKID_TAG_USAGE         "USAGE"
#define UFT_BLKID_TAG_BLOCK_SIZE    "BLOCK_SIZE"
#define UFT_BLKID_TAG_PART_ENTRY    "PART_ENTRY_TYPE"

/* ═══════════════════════════════════════════════════════════════════════════════
 * Magic Byte Database
 * ═══════════════════════════════════════════════════════════════════════════════ */

/* Example magics - full database would be much larger */

/* ext2/3/4 superblock magic at offset 0x438 */
static const uint8_t UFT_MAGIC_EXT2[] = {0x53, 0xEF};

/* FAT boot sector signature at offset 0x1FE */
static const uint8_t UFT_MAGIC_FAT_BOOT[] = {0x55, 0xAA};

/* NTFS OEM ID at offset 3 */
static const uint8_t UFT_MAGIC_NTFS[] = {'N','T','F','S',' ',' ',' ',' '};

/* HFS+ volume header magic at offset 0x400 */
static const uint8_t UFT_MAGIC_HFSPLUS[] = {'H','+'};

/* ISO9660 at offset 0x8001 */
static const uint8_t UFT_MAGIC_ISO9660[] = {'C','D','0','0','1'};

/* GPT signature at offset 0x200 */
static const uint8_t UFT_MAGIC_GPT[] = {'E','F','I',' ','P','A','R','T'};

/* XFS magic at offset 0 */
static const uint8_t UFT_MAGIC_XFS[] = {'X','F','S','B'};

/* Btrfs magic at offset 0x10040 */
static const uint8_t UFT_MAGIC_BTRFS[] = {'_','B','H','R','f','S','_','M'};

/* Amiga DOS magic at offset 0 */
static const uint8_t UFT_MAGIC_AMIGA_DOS[] = {'D','O','S'};

/* ATR header */
static const uint8_t UFT_MAGIC_ATR[] = {0x96, 0x02};

/* HFE header */
static const uint8_t UFT_MAGIC_HFE[] = {'H','X','C','P','I','C','F','E'};

/* SCP header */
static const uint8_t UFT_MAGIC_SCP[] = {'S','C','P'};

/**
 * @brief Get magic database
 * @return Array of magic definitions (terminated by type=UNKNOWN)
 */
const uft_blkid_magic_t *uft_blkid_get_magics(void);

/**
 * @brief Get magic count
 */
int uft_blkid_get_magic_count(void);

/**
 * @brief Register custom magic
 */
int uft_blkid_register_magic(const uft_blkid_magic_t *magic);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Partition Table Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct {
    int index;                      /* Partition index (0-based) */
    uint64_t start;                 /* Start offset in bytes */
    uint64_t size;                  /* Size in bytes */
    uint8_t type;                   /* Partition type (MBR) */
    char type_uuid[64];             /* Type GUID (GPT) */
    char uuid[64];                  /* Partition GUID */
    char label[128];                /* Partition label */
    bool bootable;                  /* Bootable flag */
} uft_blkid_partition_t;

/**
 * @brief Probe partition table
 */
int uft_blkid_probe_partitions(uft_blkid_probe_t *probe);

/**
 * @brief Get partition count
 */
int uft_blkid_partition_count(const uft_blkid_probe_t *probe);

/**
 * @brief Get partition info
 */
int uft_blkid_get_partition(const uft_blkid_probe_t *probe,
                            int index,
                            uft_blkid_partition_t *part);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse UUID string
 */
int uft_blkid_parse_uuid(const char *str, uint8_t *uuid);

/**
 * @brief Format UUID as string
 */
void uft_blkid_format_uuid(const uint8_t *uuid, char *str, size_t size);

/**
 * @brief Check if data matches magic
 */
bool uft_blkid_check_magic(const uint8_t *data, size_t size,
                           const uft_blkid_magic_t *magic);

/**
 * @brief Calculate score for detection
 */
int uft_blkid_score(const uint8_t *data, size_t size,
                    uft_blkid_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BLKID_H */
