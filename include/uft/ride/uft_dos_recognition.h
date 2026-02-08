/**
 * @file uft_dos_recognition.h
 * @brief UFT DOS Recognition System - extracted from RIDE
 * 
 * This module provides automatic filesystem detection and handling for
 * legacy disk formats, ported from RIDE's comprehensive DOS support.
 * 
 * Supported filesystems:
 * - MS-DOS FAT12/FAT16/FAT32
 * - ZX Spectrum: TR-DOS, +3DOS, MDOS, G+DOS, Opus Discovery
 * - Amstrad CPC: AMSDOS, CP/M
 * - CP/M 2.2, 3.0, CP/M-86
 * - Atari ST TOS
 * - Commodore DOS (1541/1571/1581)
 * - Acorn DFS/ADFS
 * - Apple DOS 3.3, ProDOS
 * 
 * @copyright Original RIDE code: (c) Tomas Nestorovic
 * @license MIT for this UFT port
 */

#ifndef UFT_DOS_RECOGNITION_H
#define UFT_DOS_RECOGNITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DOS TYPE ENUMERATION
 *============================================================================*/

/**
 * @brief DOS/Filesystem types
 */
typedef enum {
    UFT_DOS_UNKNOWN = 0,
    
    /* MS-DOS family */
    UFT_DOS_FAT12,
    UFT_DOS_FAT16,
    UFT_DOS_FAT32,
    
    /* ZX Spectrum family */
    UFT_DOS_TRDOS_503,      /**< TR-DOS 5.03 (original) */
    UFT_DOS_TRDOS_504,      /**< TR-DOS 5.04 */
    UFT_DOS_TRDOS_505,      /**< TR-DOS 5.05 (most common) */
    UFT_DOS_PLUS3DOS,       /**< Spectrum +3 DOS */
    UFT_DOS_PLUSD,          /**< +D system */
    UFT_DOS_MDOS,           /**< MDOS (Didaktik) */
    UFT_DOS_GDOS,           /**< G+DOS */
    UFT_DOS_OPUS,           /**< Opus Discovery */
    UFT_DOS_BSDOS,          /**< BS-DOS */
    
    /* Amstrad CPC */
    UFT_DOS_AMSDOS,         /**< AMSDOS */
    UFT_DOS_CPM_AMSTRAD,    /**< CP/M for Amstrad */
    
    /* CP/M variants */
    UFT_DOS_CPM22,          /**< CP/M 2.2 */
    UFT_DOS_CPM3,           /**< CP/M 3.0 (CP/M Plus) */
    UFT_DOS_CPM86,          /**< CP/M-86 */
    
    /* Atari */
    UFT_DOS_ATARI_ST,       /**< Atari ST TOS */
    UFT_DOS_ATARI_8BIT,     /**< Atari 8-bit DOS */
    
    /* Commodore */
    UFT_DOS_CBM_1541,       /**< Commodore 1541 */
    UFT_DOS_CBM_1571,       /**< Commodore 1571 */
    UFT_DOS_CBM_1581,       /**< Commodore 1581 */
    
    /* Acorn */
    UFT_DOS_DFS,            /**< Acorn DFS */
    UFT_DOS_ADFS,           /**< Acorn ADFS */
    
    /* Apple */
    UFT_DOS_APPLE_DOS33,    /**< Apple DOS 3.3 */
    UFT_DOS_PRODOS,         /**< Apple ProDOS */
    
    /* Other */
    UFT_DOS_RAW,            /**< Raw (no filesystem) */
    
    UFT_DOS_TYPE_COUNT      /**< Number of DOS types */
} uft_dos_type_t;

/*============================================================================
 * DISK GEOMETRY
 *============================================================================*/

/**
 * @brief Standard disk geometry
 */
typedef struct {
    uint8_t     cylinders;      /**< Number of cylinders */
    uint8_t     heads;          /**< Number of heads (1 or 2) */
    uint8_t     sectors;        /**< Sectors per track */
    uint16_t    sector_size;    /**< Bytes per sector */
    uint8_t     first_sector;   /**< First sector number (0 or 1) */
    uint8_t     interleave;     /**< Sector interleave */
    uint8_t     skew;           /**< Track skew */
} uft_geometry_t;

/*============================================================================
 * DIRECTORY STRUCTURES
 *============================================================================*/

/**
 * @brief File attributes (MS-DOS compatible)
 */
typedef enum {
    UFT_ATTR_READONLY   = 0x01,
    UFT_ATTR_HIDDEN     = 0x02,
    UFT_ATTR_SYSTEM     = 0x04,
    UFT_ATTR_VOLUME     = 0x08,
    UFT_ATTR_DIRECTORY  = 0x10,
    UFT_ATTR_ARCHIVE    = 0x20
} uft_file_attr_t;

/**
 * @brief Directory entry (generic)
 */
typedef struct {
    char        name[256];      /**< File name */
    char        ext[16];        /**< Extension */
    uint32_t    size;           /**< File size in bytes */
    uint32_t    start_sector;   /**< Starting sector/cluster */
    uint8_t     attributes;     /**< File attributes */
    uint32_t    created;        /**< Creation time (DOS format) */
    uint32_t    modified;       /**< Modification time */
    uint8_t     user_data[16];  /**< DOS-specific user data */
} uft_dir_entry_t;

/**
 * @brief Directory listing
 */
typedef struct {
    uft_dir_entry_t *entries;   /**< Entry array */
    size_t          count;      /**< Number of entries */
    size_t          capacity;   /**< Array capacity */
} uft_directory_t;

/*============================================================================
 * DOS RECOGNITION RESULT
 *============================================================================*/

/**
 * @brief DOS recognition result
 */
typedef struct {
    uft_dos_type_t  type;           /**< Detected DOS type */
    int             confidence;     /**< Confidence score (0-100) */
    const char     *name;           /**< DOS name string */
    const char     *description;    /**< Detailed description */
    uft_geometry_t  geometry;       /**< Detected geometry */
} uft_dos_recognition_t;

/*============================================================================
 * DOS INTERFACE (VTABLE)
 *============================================================================*/

/**
 * @brief DOS handler interface
 * 
 * Each DOS type implements this interface for filesystem operations.
 */
typedef struct uft_dos_interface {
    uft_dos_type_t  type;
    const char     *name;
    
    /** Probe disk to check if this DOS applies */
    int (*probe)(const uint8_t *boot_sector, size_t size, void *ctx);
    
    /** Get disk geometry */
    bool (*get_geometry)(void *ctx, uft_geometry_t *geom);
    
    /** Read boot sector info */
    bool (*read_boot_info)(void *ctx, char *label, size_t label_size);
    
    /** List directory contents */
    bool (*list_directory)(void *ctx, const char *path, uft_directory_t *dir);
    
    /** Read file contents */
    bool (*read_file)(void *ctx, const char *path, uint8_t **data, size_t *size);
    
    /** Write file */
    bool (*write_file)(void *ctx, const char *path, const uint8_t *data, size_t size);
    
    /** Delete file */
    bool (*delete_file)(void *ctx, const char *path);
    
    /** Create subdirectory */
    bool (*create_directory)(void *ctx, const char *path);
    
    /** Format disk */
    bool (*format)(void *ctx, const char *label, const uft_geometry_t *geom);
    
    /** Get free space */
    bool (*get_free_space)(void *ctx, uint32_t *free_bytes, uint32_t *total_bytes);
    
    /** Validate filesystem integrity */
    bool (*validate)(void *ctx, char *report, size_t report_size);
    
    /** Get sector status (used/free/bad) */
    int (*get_sector_status)(void *ctx, uint32_t sector);
    
} uft_dos_interface_t;

/*============================================================================
 * DOS RECOGNITION FUNCTIONS
 *============================================================================*/

/**
 * @brief Register a DOS handler
 * @param dos DOS interface to register
 * @return true on success
 */
bool uft_dos_register(const uft_dos_interface_t *dos);

/**
 * @brief Unregister a DOS handler
 * @param type DOS type to unregister
 * @return true on success
 */
bool uft_dos_unregister(uft_dos_type_t type);

/**
 * @brief Recognize DOS from boot sector
 * @param boot_sector Boot sector data
 * @param size Boot sector size
 * @param results Output recognition results (sorted by confidence)
 * @param max_results Maximum results to return
 * @return Number of matching DOS types found
 */
int uft_dos_recognize(const uint8_t *boot_sector, size_t size,
                      uft_dos_recognition_t *results, int max_results);

/**
 * @brief Get best matching DOS type
 * @param boot_sector Boot sector data
 * @param size Boot sector size
 * @return Best matching DOS type (UFT_DOS_UNKNOWN if none)
 */
uft_dos_type_t uft_dos_best_match(const uint8_t *boot_sector, size_t size);

/**
 * @brief Set custom recognition order
 * @param order Array of DOS types in priority order
 * @param count Number of entries
 */
void uft_dos_set_recognition_order(const uft_dos_type_t *order, size_t count);

/**
 * @brief Get DOS interface for type
 * @param type DOS type
 * @return DOS interface or NULL if not registered
 */
const uft_dos_interface_t *uft_dos_get_interface(uft_dos_type_t type);

/**
 * @brief Get DOS type name string
 * @param type DOS type
 * @return Static string
 */
const char *uft_dos_name(uft_dos_type_t type);

/**
 * @brief Get DOS type description
 * @param type DOS type
 * @return Static string
 */
const char *uft_dos_description(uft_dos_type_t type);

/*============================================================================
 * FAT FILESYSTEM SUPPORT
 *============================================================================*/

/**
 * @brief FAT boot sector structure (packed)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t     jump[3];            /**< Jump instruction */
    char        oem_name[8];        /**< OEM name */
    uint16_t    bytes_per_sector;   /**< Bytes per sector */
    uint8_t     sectors_per_cluster;/**< Sectors per cluster */
    uint16_t    reserved_sectors;   /**< Reserved sectors */
    uint8_t     num_fats;           /**< Number of FATs */
    uint16_t    root_entries;       /**< Root directory entries */
    uint16_t    total_sectors_16;   /**< Total sectors (16-bit) */
    uint8_t     media_type;         /**< Media type */
    uint16_t    fat_sectors_16;     /**< FAT sectors (16-bit) */
    uint16_t    sectors_per_track;  /**< Sectors per track */
    uint16_t    num_heads;          /**< Number of heads */
    uint32_t    hidden_sectors;     /**< Hidden sectors */
    uint32_t    total_sectors_32;   /**< Total sectors (32-bit) */
    /* FAT32 extended fields follow */
    union {
        struct {
            uint8_t     drive_num;
            uint8_t     reserved1;
            uint8_t     boot_sig;       /**< 0x29 if extended */
            uint32_t    volume_id;
            char        volume_label[11];
            char        fs_type[8];     /**< "FAT12   " etc */
        } fat16;
        struct {
            uint32_t    fat_sectors_32;
            uint16_t    ext_flags;
            uint16_t    fs_version;
            uint32_t    root_cluster;
            uint16_t    fs_info;
            uint16_t    backup_boot;
            uint8_t     reserved[12];
            uint8_t     drive_num;
            uint8_t     reserved1;
            uint8_t     boot_sig;
            uint32_t    volume_id;
            char        volume_label[11];
            char        fs_type[8];
        } fat32;
    };
    uint8_t     boot_code[420];
    uint16_t    signature;          /**< 0xAA55 */
} uft_fat_boot_t;
#pragma pack(pop)

/**
 * @brief Probe FAT filesystem
 * @param boot Boot sector data
 * @param size Boot sector size
 * @return Confidence score (0-100)
 */
int uft_fat_probe(const uint8_t *boot, size_t size);

/**
 * @brief Determine FAT type from boot sector
 * @param boot Parsed boot sector
 * @return FAT12, FAT16, FAT32, or UNKNOWN
 */
uft_dos_type_t uft_fat_determine_type(const uft_fat_boot_t *boot);

/*============================================================================
 * TR-DOS SUPPORT (ZX SPECTRUM)
 *============================================================================*/

/**
 * @brief TR-DOS disk format types
 */
typedef enum {
    UFT_TRDOS_DS80 = 22,    /**< Double-sided 80 tracks */
    UFT_TRDOS_DS40 = 23,    /**< Double-sided 40 tracks */
    UFT_TRDOS_SS80 = 24,    /**< Single-sided 80 tracks */
    UFT_TRDOS_SS40 = 25     /**< Single-sided 40 tracks */
} uft_trdos_format_t;

/**
 * @brief TR-DOS file extensions
 */
typedef enum {
    UFT_TRDOS_EXT_BASIC = 'B',  /**< BASIC program */
    UFT_TRDOS_EXT_DATA  = 'D',  /**< Data field */
    UFT_TRDOS_EXT_CODE  = 'C',  /**< Code block */
    UFT_TRDOS_EXT_PRINT = '#'   /**< Print file */
} uft_trdos_ext_t;

/**
 * @brief TR-DOS boot sector (sector 9, track 0)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t     zero1;              /**< End of directory (0x00) */
    uint8_t     reserved1[224];
    uint8_t     first_free_sector;
    uint8_t     first_free_track;
    uint8_t     disk_type;          /**< uft_trdos_format_t */
    uint8_t     file_count;
    uint16_t    free_sectors;
    uint8_t     trdos_id;           /**< 0x10 for TR-DOS */
    uint8_t     reserved2[2];
    char        password[9];
    uint8_t     zero2;
    uint8_t     deleted_files;
    char        label[8];
    uint8_t     reserved3[3];
} uft_trdos_boot_t;
#pragma pack(pop)

/**
 * @brief TR-DOS directory entry
 */
#pragma pack(push, 1)
typedef struct {
    char        name[8];            /**< File name (or 0x00/0x01 special) */
    uint8_t     extension;          /**< File type (B/D/C/#) */
    uint16_t    param_a;            /**< Start address or line number */
    uint16_t    param_b;            /**< Length or variable length */
    uint8_t     sector_count;       /**< Number of sectors */
    uint8_t     first_sector;
    uint8_t     first_track;
} uft_trdos_entry_t;
#pragma pack(pop)

/**
 * @brief Probe TR-DOS filesystem
 * @param sector9 Sector 9 data (boot sector)
 * @param size Sector size
 * @return Confidence score (0-100)
 */
int uft_trdos_probe(const uint8_t *sector9, size_t size);

/*============================================================================
 * +3DOS SUPPORT (ZX SPECTRUM +3)
 *============================================================================*/

/**
 * @brief +3DOS disk specification
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t     format;             /**< Format type */
    uint8_t     sidedness;          /**< 0=SS, 1=DS */
    uint8_t     tracks;             /**< Tracks per side */
    uint8_t     sectors;            /**< Sectors per track */
    uint8_t     sector_size;        /**< Log2(sector size)-7 */
    uint8_t     reserved_tracks;    /**< Reserved tracks */
    uint8_t     block_shift;        /**< Block shift */
    uint8_t     dir_blocks;         /**< Directory blocks */
    uint8_t     gap_rw;             /**< R/W gap length */
    uint8_t     gap_format;         /**< Format gap length */
    uint8_t     multitrack;         /**< Multitrack flag */
    uint8_t     freeze;             /**< Freeze flag */
} uft_plus3_spec_t;
#pragma pack(pop)

/**
 * @brief Probe +3DOS filesystem
 * @param boot Boot sector data
 * @param size Sector size
 * @return Confidence score (0-100)
 */
int uft_plus3_probe(const uint8_t *boot, size_t size);

/*============================================================================
 * AMSDOS SUPPORT (AMSTRAD CPC)
 *============================================================================*/

/**
 * @brief AMSDOS file header
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t     user;               /**< User number */
    char        name[8];            /**< File name */
    char        ext[3];             /**< Extension */
    uint8_t     unused[4];
    uint8_t     block_num;          /**< Block number */
    uint8_t     last_block;         /**< Last block number */
    uint8_t     file_type;          /**< File type */
    uint16_t    data_length;        /**< Data length */
    uint16_t    load_address;       /**< Load address */
    uint8_t     first_block;        /**< First block */
    uint16_t    logical_length;     /**< Logical length */
    uint16_t    exec_address;       /**< Execution address */
    uint8_t     unused2[36];
    uint16_t    real_length;        /**< Real file length */
    uint16_t    checksum;           /**< Header checksum */
    uint8_t     unused3[59];
} uft_amsdos_header_t;
#pragma pack(pop)

/**
 * @brief Probe AMSDOS filesystem
 * @param boot Boot sector data
 * @param size Sector size
 * @return Confidence score (0-100)
 */
int uft_amsdos_probe(const uint8_t *boot, size_t size);

/*============================================================================
 * CP/M SUPPORT
 *============================================================================*/

/**
 * @brief CP/M directory entry
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t     user;               /**< User number (0xE5 = deleted) */
    char        name[8];            /**< File name */
    char        ext[3];             /**< Extension (high bits = flags) */
    uint8_t     extent_low;         /**< Extent counter low */
    uint8_t     reserved1;
    uint8_t     extent_high;        /**< Extent counter high */
    uint8_t     record_count;       /**< Record count */
    uint8_t     alloc[16];          /**< Allocation map */
} uft_cpm_dir_t;
#pragma pack(pop)

/**
 * @brief CP/M disk parameter block
 */
typedef struct {
    uint16_t    spt;                /**< Sectors per track */
    uint8_t     bsh;                /**< Block shift */
    uint8_t     blm;                /**< Block mask */
    uint8_t     exm;                /**< Extent mask */
    uint16_t    dsm;                /**< Disk size (blocks - 1) */
    uint16_t    drm;                /**< Directory entries - 1 */
    uint8_t     al0;                /**< Allocation 0 */
    uint8_t     al1;                /**< Allocation 1 */
    uint16_t    cks;                /**< Checksum vector size */
    uint16_t    off;                /**< Track offset */
} uft_cpm_dpb_t;

/**
 * @brief Probe CP/M filesystem
 * @param dir Directory data
 * @param size Data size
 * @return Confidence score (0-100)
 */
int uft_cpm_probe(const uint8_t *dir, size_t size);

/*============================================================================
 * DIRECTORY MANAGEMENT
 *============================================================================*/

/**
 * @brief Create empty directory structure
 * @param capacity Initial capacity
 * @return Allocated directory or NULL
 */
uft_directory_t *uft_directory_create(size_t capacity);

/**
 * @brief Free directory structure
 * @param dir Directory to free
 */
void uft_directory_free(uft_directory_t *dir);

/**
 * @brief Add entry to directory
 * @param dir Directory
 * @param entry Entry to add (copied)
 * @return true on success
 */
bool uft_directory_add(uft_directory_t *dir, const uft_dir_entry_t *entry);

/**
 * @brief Sort directory entries
 * @param dir Directory
 * @param by_name true to sort by name, false by position
 */
void uft_directory_sort(uft_directory_t *dir, bool by_name);

/*============================================================================
 * INITIALIZATION
 *============================================================================*/

/**
 * @brief Initialize DOS recognition system
 * 
 * Registers all built-in DOS handlers.
 */
void uft_dos_init(void);

/**
 * @brief Shutdown DOS recognition system
 */
void uft_dos_shutdown(void);

/*============================================================================
 * SIMPLIFIED DOS DETECTION API
 *============================================================================*/

/**
 * @brief Simplified DOS type enum (for probe functions)
 */
typedef enum {
    UFT_DOS_UNKNOWN_SIMPLE = 0,
    UFT_DOS_FAT12_S,
    UFT_DOS_FAT16_S,
    UFT_DOS_FAT32_S,
    UFT_DOS_TRDOS,
    UFT_DOS_PLUS3DOS_S,
    UFT_DOS_MDOS_S,
    UFT_DOS_GDOS_S,
    UFT_DOS_AMSDOS_S,
    UFT_DOS_CPM,
    UFT_DOS_CBM,
    UFT_DOS_APPLE_DOS,
    UFT_DOS_PRODOS_S,
    UFT_DOS_ATARI_ST_S,
    UFT_DOS_DFS_S
} uft_dos_type_simple_t;

/* Alias for backward compatibility */
#define UFT_DOS_FAT12     UFT_DOS_FAT12_S
#define UFT_DOS_FAT16     UFT_DOS_FAT16_S
#define UFT_DOS_FAT32     UFT_DOS_FAT32_S

/**
 * @brief DOS detection result
 */
typedef struct {
    uft_dos_type_simple_t dos_type;     /**< Detected DOS type */
    char     dos_name[32];              /**< Human-readable name */
    int      confidence;                /**< Detection confidence (0-100) */
    uint16_t sector_size;               /**< Bytes per sector */
    uint32_t cluster_size;              /**< Bytes per cluster */
    uint32_t total_sectors;             /**< Total sectors */
    uint16_t root_entries;              /**< Root directory entries (FAT) */
    uint16_t file_count;                /**< Number of files */
    uint16_t free_sectors;              /**< Free sectors */
    char     volume_label[32];          /**< Volume label */
} uft_dos_info_t;

/**
 * @brief Detect DOS type from boot/system sectors
 * @param data Boot sector or first track data
 * @param size Data size in bytes
 * @param results Array to receive detection results
 * @param max_results Maximum results to return
 * @return Number of DOS types detected (sorted by confidence)
 */
int uft_dos_detect(const uint8_t *data, size_t size,
                    uft_dos_info_t *results, int max_results);

/**
 * @brief Probe for FAT filesystem
 * @param boot_sector 512-byte boot sector
 * @param size Size of boot sector
 * @param info Output info structure
 * @return 1 if FAT detected, 0 otherwise
 */
int uft_dos_probe_fat(const uint8_t *boot_sector, size_t size,
                       uft_dos_info_t *info);

/**
 * @brief Probe for TR-DOS filesystem
 * @param sector0 Track 0 sector 0
 * @param sector8 Track 0 sector 8 (info sector)
 * @param size Sector size
 * @param info Output info structure
 * @return 1 if TR-DOS detected, 0 otherwise
 */
int uft_dos_probe_trdos(const uint8_t *sector0, const uint8_t *sector8,
                         size_t size, uft_dos_info_t *info);

/**
 * @brief Probe for CP/M filesystem
 * @param directory Directory track data
 * @param size Data size
 * @param info Output info structure
 * @return 1 if CP/M detected, 0 otherwise
 */
int uft_dos_probe_cpm(const uint8_t *directory, size_t size,
                       uft_dos_info_t *info);

/**
 * @brief Probe for Commodore 1541 DOS
 * @param bam BAM sector (track 18, sector 0)
 * @param size Sector size
 * @param info Output info structure
 * @return 1 if CBM DOS detected, 0 otherwise
 */
int uft_dos_probe_cbm(const uint8_t *bam, size_t size,
                       uft_dos_info_t *info);

/**
 * @brief Probe for Apple DOS 3.3
 * @param vtoc VTOC sector (track 17, sector 0)
 * @param size Sector size
 * @param info Output info structure
 * @return 1 if Apple DOS detected, 0 otherwise
 */
int uft_dos_probe_apple_dos(const uint8_t *vtoc, size_t size,
                             uft_dos_info_t *info);

/**
 * @brief Probe for ProDOS
 * @param block2 Block 2 (volume directory header)
 * @param size Block size
 * @param info Output info structure
 * @return 1 if ProDOS detected, 0 otherwise
 */
int uft_dos_probe_prodos(const uint8_t *block2, size_t size,
                          uft_dos_info_t *info);

/**
 * @brief Probe for BBC Micro DFS
 * @param sector0 Sector 0
 * @param sector1 Sector 1
 * @param size Sector size
 * @param info Output info structure
 * @return 1 if DFS detected, 0 otherwise
 */
int uft_dos_probe_dfs(const uint8_t *sector0, const uint8_t *sector1,
                       size_t size, uft_dos_info_t *info);

/**
 * @brief Get DOS type name string
 * @param type DOS type
 * @return Human-readable name
 */
const char *uft_dos_type_name(uft_dos_type_simple_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DOS_RECOGNITION_H */
