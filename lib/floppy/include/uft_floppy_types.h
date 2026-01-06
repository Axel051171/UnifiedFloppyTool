/**
 * @file uft_floppy_types.h
 * @brief Common types and definitions for floppy disk operations
 * 
 * Part of UnifiedFloppyTool - Extracted and modernized from:
 * - discdiag (Disc Diagnostic)
 * - lbacache (LBA Cache)
 * - BootLoader-Test (FAT12)
 * - Fosfat (w32disk)
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UFT_FLOPPY_TYPES_H
#define UFT_FLOPPY_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/types.h>

/*===========================================================================
 * Version Information
 *===========================================================================*/

#define UFT_FLOPPY_VERSION_MAJOR  1
#define UFT_FLOPPY_VERSION_MINOR  0
#define UFT_FLOPPY_VERSION_PATCH  0
#define UFT_FLOPPY_VERSION_STR    "1.0.0"

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

#if defined(_WIN32) || defined(_WIN64)
    #define UFT_PLATFORM_WINDOWS 1
#elif defined(__linux__)
    #define UFT_PLATFORM_LINUX 1
#elif defined(__APPLE__)
    #define UFT_PLATFORM_MACOS 1
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
    #define UFT_PLATFORM_BSD 1
#elif defined(__DOS__) || defined(MSDOS) || defined(__MSDOS__)
    #define UFT_PLATFORM_DOS 1
#else
    #define UFT_PLATFORM_UNKNOWN 1
#endif

/*===========================================================================
 * Compiler Attributes
 *===========================================================================*/

#if defined(__GNUC__) || defined(__clang__)
    #define UFT_PACKED       __attribute__((packed))
    #define UFT_ALIGNED(x)   __attribute__((aligned(x)))
    #define UFT_UNUSED       __attribute__((unused))
    #define UFT_WARN_UNUSED  __attribute__((warn_unused_result))
    #define UFT_INLINE       static inline
    #define UFT_LIKELY(x)    __builtin_expect(!!(x), 1)
    #define UFT_UNLIKELY(x)  __builtin_expect(!!(x), 0)
#elif defined(_MSC_VER)
    #define UFT_PACKED
    #define UFT_ALIGNED(x)   __declspec(align(x))
    #define UFT_UNUSED
    #define UFT_WARN_UNUSED  _Check_return_
    #define UFT_INLINE       static __inline
    #define UFT_LIKELY(x)    (x)
    #define UFT_UNLIKELY(x)  (x)
    #pragma warning(disable: 4200)  /* zero-sized array */
#else
    #define UFT_PACKED
    #define UFT_ALIGNED(x)
    #define UFT_UNUSED
    #define UFT_WARN_UNUSED
    #define UFT_INLINE       static inline
    #define UFT_LIKELY(x)    (x)
    #define UFT_UNLIKELY(x)  (x)
#endif

/*===========================================================================
 * Fundamental Constants
 *===========================================================================*/

/** Standard sector size (512 bytes - PDP-11 legacy) */
#define UFT_SECTOR_SIZE           512

/** Maximum sectors in buffer (platform dependent) */
#ifdef UFT_PLATFORM_DOS
    #define UFT_MAX_SECTORS       16   /* DOS segment limit */
#else
    #define UFT_MAX_SECTORS       256
#endif

/** Maximum drives supported */
#define UFT_MAX_DRIVES            10

/** FAT12 cluster markers */
#define UFT_FAT12_EOF             0x0FF0
#define UFT_FAT12_BAD             0x0FF7
#define UFT_FAT12_FREE            0x0000

/** Directory entry size */
#define UFT_DIR_ENTRY_SIZE        32

/** Maximum filename length (8.3 format) */
#define UFT_FILENAME_LEN          11

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum uft_error {
    UFT_OK                    =  0,   /**< Success */
    UFT_ERR_INVALID_PARAM     = -1,   /**< Invalid parameter */
    UFT_ERR_NOT_INITIALIZED   = -2,   /**< Not initialized */
    UFT_ERR_DRIVE_NOT_SET     = -3,   /**< Drive not set */
    UFT_ERR_OPEN_FAILED       = -4,   /**< Failed to open device */
    UFT_ERR_READ_FAILED       = -5,   /**< Read operation failed */
    UFT_ERR_WRITE_FAILED      = -6,   /**< Write operation failed */
    UFT_ERR_SEEK_FAILED       = -7,   /**< Seek operation failed */
    UFT_ERR_PERMISSION        = -8,   /**< Permission denied */
    UFT_ERR_NO_MEMORY         = -9,   /**< Out of memory */
    UFT_ERR_NOT_FOUND         = -10,  /**< File/entry not found */
    UFT_ERR_INVALID_FORMAT    = -11,  /**< Invalid disk format */
    UFT_ERR_CHS_OVERFLOW      = -12,  /**< CHS addressing overflow */
    UFT_ERR_GEOMETRY_INVALID  = -13,  /**< Invalid disk geometry */
    UFT_ERR_BUFFER_TOO_SMALL  = -14,  /**< Buffer too small */
    UFT_ERR_END_OF_FILE       = -15,  /**< End of file reached */
    UFT_ERR_DISK_FULL         = -16,  /**< Disk full */
    UFT_ERR_DIR_NOT_EMPTY     = -17,  /**< Directory not empty */
    UFT_ERR_EXISTS            = -18,  /**< File/dir already exists */
    UFT_ERR_PROTECTED         = -19,  /**< Write protected */
    UFT_ERR_IO                = -20,  /**< Generic I/O error */
    UFT_ERR_UNSUPPORTED       = -21,  /**< Operation not supported */
} uft_error_t;

/*===========================================================================
 * Floppy Disk Types
 *===========================================================================*/

typedef enum uft_floppy_type {
    UFT_FLOPPY_UNKNOWN        = 0,    /**< Unknown format */
    
    /* 5.25" formats */
    UFT_FLOPPY_525_160K       = 1,    /**< 5.25" SS/DD 160KB */
    UFT_FLOPPY_525_180K       = 2,    /**< 5.25" SS/DD 180KB */
    UFT_FLOPPY_525_320K       = 3,    /**< 5.25" DS/DD 320KB */
    UFT_FLOPPY_525_360K       = 4,    /**< 5.25" DS/DD 360KB */
    UFT_FLOPPY_525_1200K      = 5,    /**< 5.25" DS/HD 1.2MB */
    
    /* 3.5" formats */
    UFT_FLOPPY_35_720K        = 10,   /**< 3.5" DS/DD 720KB */
    UFT_FLOPPY_35_1440K       = 11,   /**< 3.5" DS/HD 1.44MB */
    UFT_FLOPPY_35_2880K       = 12,   /**< 3.5" DS/ED 2.88MB */
    
    /* Amiga formats */
    UFT_FLOPPY_AMIGA_DD       = 20,   /**< Amiga DD 880KB */
    UFT_FLOPPY_AMIGA_HD       = 21,   /**< Amiga HD 1.76MB */
    
    /* Commodore formats */
    UFT_FLOPPY_C64_1541       = 30,   /**< C64 1541 170KB */
    UFT_FLOPPY_C64_1571       = 31,   /**< C64 1571 340KB */
    UFT_FLOPPY_C64_1581       = 32,   /**< C64 1581 800KB */
    
    /* Apple formats */
    UFT_FLOPPY_APPLE_400K     = 40,   /**< Apple 400KB GCR */
    UFT_FLOPPY_APPLE_800K     = 41,   /**< Apple 800KB GCR */
    UFT_FLOPPY_APPLE_1440K    = 42,   /**< Apple 1.44MB MFM */
    
    /* Mac formats */
    UFT_FLOPPY_MAC_400K       = 50,   /**< Mac 400KB GCR */
    UFT_FLOPPY_MAC_800K       = 51,   /**< Mac 800KB GCR */
    UFT_FLOPPY_MAC_1440K      = 52,   /**< Mac 1.44MB MFM */
    
} uft_floppy_type_t;

/*===========================================================================
 * Disk Geometry
 *===========================================================================*/

/**
 * @brief CHS (Cylinder-Head-Sector) address
 */
typedef struct uft_chs {
    uint16_t cylinder;              /**< Cylinder number (0-based) */
    uint8_t  head;                  /**< Head number (0-based) */
    uint8_t  sector;                /**< Sector number (1-based!) */
} uft_chs_t;

/**
 * @brief Disk geometry parameters
 */
typedef struct uft_geometry {
    uint16_t cylinders;             /**< Total cylinders */
    uint8_t  heads;                 /**< Heads per cylinder */
    uint8_t  sectors_per_track;     /**< Sectors per track */
    uint16_t bytes_per_sector;      /**< Bytes per sector (usually 512) */
    uint32_t total_sectors;         /**< Total sectors on disk */
    uint32_t total_bytes;           /**< Total bytes on disk */
    
    uft_floppy_type_t type;         /**< Floppy type identifier */
    const char *description;        /**< Human-readable description */
} uft_geometry_t;

/*===========================================================================
 * BIOS Parameter Block (BPB)
 *===========================================================================*/

/**
 * @brief BIOS Parameter Block for FAT12/16 volumes
 * 
 * This structure is found at offset 0x0B in the boot sector.
 * All multi-byte values are little-endian.
 */
typedef struct UFT_PACKED uft_bpb {
    uint16_t bytes_per_sector;      /**< 0x0B: Bytes per sector (usually 512) */
    uint8_t  sectors_per_cluster;   /**< 0x0D: Sectors per allocation unit */
    uint16_t reserved_sectors;      /**< 0x0E: Reserved sectors (boot sector) */
    uint8_t  num_fats;              /**< 0x10: Number of FAT copies */
    uint16_t root_entries;          /**< 0x11: Root directory entries */
    uint16_t total_sectors_16;      /**< 0x13: Total sectors (16-bit) */
    uint8_t  media_type;            /**< 0x15: Media descriptor */
    uint16_t sectors_per_fat;       /**< 0x16: Sectors per FAT */
    uint16_t sectors_per_track;     /**< 0x18: Sectors per track (CHS) */
    uint16_t heads;                 /**< 0x1A: Number of heads (CHS) */
    uint32_t hidden_sectors;        /**< 0x1C: Hidden sectors */
    uint32_t total_sectors_32;      /**< 0x20: Total sectors (32-bit) */
} uft_bpb_t;

/**
 * @brief Extended Boot Record for FAT12/16
 */
typedef struct UFT_PACKED uft_ebr {
    uint8_t  drive_number;          /**< 0x24: BIOS drive number */
    uint8_t  reserved;              /**< 0x25: Reserved */
    uint8_t  boot_signature;        /**< 0x26: Extended boot signature (0x29) */
    uint32_t volume_id;             /**< 0x27: Volume serial number */
    char     volume_label[11];      /**< 0x2B: Volume label */
    char     fs_type[8];            /**< 0x36: File system type */
} uft_ebr_t;

/**
 * @brief Complete Boot Sector structure
 */
typedef struct UFT_PACKED uft_boot_sector {
    uint8_t   jump[3];              /**< 0x00: Jump instruction */
    char      oem_name[8];          /**< 0x03: OEM identifier */
    uft_bpb_t bpb;                  /**< 0x0B: BIOS Parameter Block */
    uft_ebr_t ebr;                  /**< 0x24: Extended Boot Record */
    uint8_t   boot_code[448];       /**< 0x3E: Boot code */
    uint16_t  signature;            /**< 0x1FE: Boot signature (0xAA55) */
} uft_boot_sector_t;

/*===========================================================================
 * FAT Directory Entry
 *===========================================================================*/

/** File attributes */
typedef enum uft_file_attr {
    UFT_ATTR_READ_ONLY  = 0x01,
    UFT_ATTR_HIDDEN     = 0x02,
    UFT_ATTR_SYSTEM     = 0x04,
    UFT_ATTR_VOLUME_ID  = 0x08,
    UFT_ATTR_DIRECTORY  = 0x10,
    UFT_ATTR_ARCHIVE    = 0x20,
    UFT_ATTR_LONG_NAME  = 0x0F,     /**< LFN entry marker */
} uft_file_attr_t;

/**
 * @brief FAT directory entry (32 bytes)
 */
typedef struct UFT_PACKED uft_dir_entry {
    char     name[8];               /**< 0x00: Filename (space-padded) */
    char     ext[3];                /**< 0x08: Extension (space-padded) */
    uint8_t  attr;                  /**< 0x0B: File attributes */
    uint8_t  nt_reserved;           /**< 0x0C: Reserved for NT */
    uint8_t  create_time_tenth;     /**< 0x0D: Creation time (tenths of sec) */
    uint16_t create_time;           /**< 0x0E: Creation time */
    uint16_t create_date;           /**< 0x10: Creation date */
    uint16_t access_date;           /**< 0x12: Last access date */
    uint16_t cluster_high;          /**< 0x14: High word of cluster (FAT32) */
    uint16_t modify_time;           /**< 0x16: Last modification time */
    uint16_t modify_date;           /**< 0x18: Last modification date */
    uint16_t cluster_low;           /**< 0x1A: Low word of starting cluster */
    uint32_t file_size;             /**< 0x1C: File size in bytes */
} uft_dir_entry_t;

/*===========================================================================
 * Media Descriptor Values
 *===========================================================================*/

#define UFT_MEDIA_FIXED_DISK      0xF8    /**< Fixed disk */
#define UFT_MEDIA_35_1440K        0xF0    /**< 3.5" 1.44MB */
#define UFT_MEDIA_35_720K         0xF9    /**< 3.5" 720KB */
#define UFT_MEDIA_525_1200K       0xF9    /**< 5.25" 1.2MB */
#define UFT_MEDIA_525_360K        0xFD    /**< 5.25" 360KB */
#define UFT_MEDIA_525_320K        0xFF    /**< 5.25" 320KB */
#define UFT_MEDIA_525_180K        0xFC    /**< 5.25" 180KB */
#define UFT_MEDIA_525_160K        0xFE    /**< 5.25" 160KB */

/*===========================================================================
 * Predefined Geometry Table
 *===========================================================================*/

/**
 * @brief Standard floppy geometries
 */
static const uft_geometry_t UFT_GEOMETRIES[] = {
    /* 5.25" formats */
    { 40, 1,  8, 512, 320,   163840, UFT_FLOPPY_525_160K, "5.25\" 160KB SS/DD" },
    { 40, 1,  9, 512, 360,   184320, UFT_FLOPPY_525_180K, "5.25\" 180KB SS/DD" },
    { 40, 2,  8, 512, 640,   327680, UFT_FLOPPY_525_320K, "5.25\" 320KB DS/DD" },
    { 40, 2,  9, 512, 720,   368640, UFT_FLOPPY_525_360K, "5.25\" 360KB DS/DD" },
    { 80, 2, 15, 512, 2400, 1228800, UFT_FLOPPY_525_1200K, "5.25\" 1.2MB DS/HD" },
    
    /* 3.5" formats */
    { 80, 2,  9, 512, 1440,  737280, UFT_FLOPPY_35_720K,  "3.5\" 720KB DS/DD" },
    { 80, 2, 18, 512, 2880, 1474560, UFT_FLOPPY_35_1440K, "3.5\" 1.44MB DS/HD" },
    { 80, 2, 36, 512, 5760, 2949120, UFT_FLOPPY_35_2880K, "3.5\" 2.88MB DS/ED" },
    
    /* Amiga formats */
    { 80, 2, 11, 512, 1760,  901120, UFT_FLOPPY_AMIGA_DD, "Amiga DD 880KB" },
    { 80, 2, 22, 512, 3520, 1802240, UFT_FLOPPY_AMIGA_HD, "Amiga HD 1.76MB" },
    
    /* Terminator */
    { 0, 0, 0, 0, 0, 0, UFT_FLOPPY_UNKNOWN, NULL }
};

/*===========================================================================
 * Utility Macros
 *===========================================================================*/

/** Convert LBA to byte offset */
#define UFT_LBA_TO_OFFSET(lba, sector_size) ((uint64_t)(lba) * (sector_size))

/** Calculate root directory size in sectors */
#define UFT_ROOT_DIR_SECTORS(bpb) \
    (((bpb)->root_entries * 32 + (bpb)->bytes_per_sector - 1) / (bpb)->bytes_per_sector)

/** Calculate first data sector */
#define UFT_FIRST_DATA_SECTOR(bpb) \
    ((bpb)->reserved_sectors + ((bpb)->num_fats * (bpb)->sectors_per_fat) + \
     UFT_ROOT_DIR_SECTORS(bpb))

/** Calculate cluster to sector */
#define UFT_CLUSTER_TO_SECTOR(bpb, cluster) \
    (UFT_FIRST_DATA_SECTOR(bpb) + ((cluster) - 2) * (bpb)->sectors_per_cluster)

/** Get total sectors from BPB */
#define UFT_TOTAL_SECTORS(bpb) \
    ((bpb)->total_sectors_16 ? (bpb)->total_sectors_16 : (bpb)->total_sectors_32)

/** Check if entry is free */
#define UFT_ENTRY_IS_FREE(entry) \
    ((entry)->name[0] == 0x00 || (entry)->name[0] == (char)0xE5)

/** Check if entry is deleted */
#define UFT_ENTRY_IS_DELETED(entry) ((entry)->name[0] == (char)0xE5)

/** Check if entry is end of directory */
#define UFT_ENTRY_IS_END(entry) ((entry)->name[0] == 0x00)

/** Check if entry is a directory */
#define UFT_ENTRY_IS_DIR(entry) ((entry)->attr & UFT_ATTR_DIRECTORY)

/** Check if entry is a volume label */
#define UFT_ENTRY_IS_VOLUME(entry) ((entry)->attr & UFT_ATTR_VOLUME_ID)

/** Check if entry is long filename */
#define UFT_ENTRY_IS_LFN(entry) (((entry)->attr & UFT_ATTR_LONG_NAME) == UFT_ATTR_LONG_NAME)

/*===========================================================================
 * Endianness Helpers
 *===========================================================================*/

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define UFT_BIG_ENDIAN 1
#else
    #define UFT_LITTLE_ENDIAN 1
#endif

UFT_INLINE uint16_t uft_le16_to_cpu(uint16_t val) {
#ifdef UFT_BIG_ENDIAN
    return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8);
#else
    return val;
#endif
}

UFT_INLINE uint32_t uft_le32_to_cpu(uint32_t val) {
#ifdef UFT_BIG_ENDIAN
    return ((val & 0x000000FF) << 24) |
           ((val & 0x0000FF00) << 8)  |
           ((val & 0x00FF0000) >> 8)  |
           ((val & 0xFF000000) >> 24);
#else
    return val;
#endif
}

UFT_INLINE uint16_t uft_cpu_to_le16(uint16_t val) {
    return uft_le16_to_cpu(val);
}

UFT_INLINE uint32_t uft_cpu_to_le32(uint32_t val) {
    return uft_le32_to_cpu(val);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_TYPES_H */
