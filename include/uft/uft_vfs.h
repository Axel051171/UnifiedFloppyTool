/**
 * @file uft_vfs.h
 * @brief Virtual File System Interface for UFT
 * 
 * Based on FluxEngine VFS by David Given
 * Provides filesystem access for various disk formats
 * 
 * Supported filesystems:
 * - FATFS (FAT12/16)
 * - AmigaFFS (OFS/FFS)
 * - CP/M
 * - CBM (Commodore DOS)
 * - Apple DOS 3.3 / ProDOS
 * - Acorn DFS / ADFS
 * - Brother 120/240
 * - HP LIF
 * - Macintosh HFS
 * - Roland (synthesizer)
 * - Smaky 6
 * - Z-DOS
 * - MicroDOS (Philips P2000)
 * 
 * Version: 1.0.0
 * Date: 2025-12-30
 */

#ifndef UFT_VFS_H
#define UFT_VFS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FILESYSTEM TYPES
 *============================================================================*/

typedef enum uft_vfs_type {
    UFT_VFS_UNKNOWN = 0,
    
    /* PC/Generic */
    UFT_VFS_FAT12,          /* MS-DOS FAT12 */
    UFT_VFS_FAT16,          /* MS-DOS FAT16 */
    UFT_VFS_CPM,            /* CP/M 2.2 */
    UFT_VFS_CPM3,           /* CP/M Plus */
    
    /* Commodore */
    UFT_VFS_CBM,            /* CBM DOS (D64/D71/D81) */
    UFT_VFS_GEOS,           /* GEOS */
    
    /* Apple */
    UFT_VFS_DOS33,          /* Apple DOS 3.3 */
    UFT_VFS_PRODOS,         /* Apple ProDOS */
    UFT_VFS_HFS,            /* Macintosh HFS */
    
    /* Acorn */
    UFT_VFS_DFS,            /* Acorn DFS */
    UFT_VFS_ADFS,           /* Acorn ADFS */
    
    /* Amiga */
    UFT_VFS_OFS,            /* Amiga OFS */
    UFT_VFS_FFS,            /* Amiga FFS */
    
    /* Other */
    UFT_VFS_BROTHER,        /* Brother word processor */
    UFT_VFS_LIF,            /* HP LIF */
    UFT_VFS_ROLAND,         /* Roland synthesizer */
    UFT_VFS_SMAKY,          /* Smaky 6 */
    UFT_VFS_ZDOS,           /* Z-DOS */
    UFT_VFS_MICRODOS,       /* MicroDOS (Philips P2000) */
    UFT_VFS_PHILE,          /* Philips :YES */
    
    UFT_VFS_COUNT
} uft_vfs_type_t;

/*============================================================================
 * FILE ATTRIBUTES
 *============================================================================*/

typedef enum uft_vfs_attr {
    UFT_VATTR_NONE      = 0,
    UFT_VATTR_READONLY  = 0x0001,
    UFT_VATTR_HIDDEN    = 0x0002,
    UFT_VATTR_SYSTEM    = 0x0004,
    UFT_VATTR_ARCHIVE   = 0x0008,
    UFT_VATTR_DIRECTORY = 0x0010,
    UFT_VATTR_LOCKED    = 0x0020,
    UFT_VATTR_DELETED   = 0x0040,
    UFT_VATTR_SPLAT     = 0x0080,   /* CBM "splat" file */
} uft_vfs_attr_t;

/*============================================================================
 * FILE TYPES (CBM/GEOS specific)
 *============================================================================*/

typedef enum uft_vfs_cbm_type {
    UFT_CBM_TYPE_DEL = 0,
    UFT_CBM_TYPE_SEQ = 1,
    UFT_CBM_TYPE_PRG = 2,
    UFT_CBM_TYPE_USR = 3,
    UFT_CBM_TYPE_REL = 4,
    UFT_CBM_TYPE_CBM = 5,   /* Partition */
    UFT_CBM_TYPE_DIR = 6,   /* Directory (D81) */
} uft_vfs_cbm_type_t;

/*============================================================================
 * DIRECTORY ENTRY
 *============================================================================*/

typedef struct uft_vfs_dirent {
    char        name[256];      /* Filename (null-terminated) */
    char        ext[16];        /* Extension (if separate) */
    uint32_t    size;           /* File size in bytes */
    uint32_t    blocks;         /* Size in blocks/sectors */
    uint32_t    attributes;     /* uft_vfs_attr_t flags */
    uint8_t     file_type;      /* System-specific file type */
    time_t      created;        /* Creation time (if available) */
    time_t      modified;       /* Modification time (if available) */
    
    /* System-specific */
    uint16_t    start_track;    /* Starting track */
    uint16_t    start_sector;   /* Starting sector */
    uint32_t    first_cluster;  /* FAT first cluster */
    uint8_t     user;           /* CP/M user number */
    uint8_t     record_length;  /* REL file record length */
    
    /* Internal */
    uint32_t    _internal_id;   /* Internal ID for operations */
} uft_vfs_dirent_t;

/*============================================================================
 * FILESYSTEM INFO
 *============================================================================*/

typedef struct uft_vfs_info {
    uft_vfs_type_t type;
    char        label[64];          /* Volume label */
    uint32_t    total_blocks;       /* Total blocks/sectors */
    uint32_t    free_blocks;        /* Free blocks/sectors */
    uint32_t    block_size;         /* Bytes per block */
    uint32_t    dir_entries_total;  /* Total directory entries */
    uint32_t    dir_entries_used;   /* Used directory entries */
    uint8_t     dos_version;        /* DOS version (CBM) */
    uint16_t    bam_track;          /* BAM/directory track */
    uint16_t    bam_sector;         /* BAM/directory sector */
    
    /* Format-specific info */
    uint8_t     interleave;
    uint8_t     double_sided;
    uint8_t     tracks_per_side;
} uft_vfs_info_t;

/*============================================================================
 * SECTOR INTERFACE (for VFS to access disk data)
 *============================================================================*/

/* Callback type for reading sectors */
typedef int (*uft_vfs_read_sector_fn)(
    void* context,
    int track,
    int head,
    int sector,
    uint8_t* buffer,
    size_t buffer_size
);

/* Callback type for writing sectors */
typedef int (*uft_vfs_write_sector_fn)(
    void* context,
    int track,
    int head,
    int sector,
    const uint8_t* data,
    size_t data_size
);

typedef struct uft_vfs_sector_interface {
    void* context;
    uft_vfs_read_sector_fn read;
    uft_vfs_write_sector_fn write;
    int sector_size;
    int tracks;
    int heads;
    int sectors_per_track;
} uft_vfs_sector_interface_t;

/*============================================================================
 * VFS CONTEXT
 *============================================================================*/

typedef struct uft_vfs_context {
    uft_vfs_type_t type;
    uft_vfs_sector_interface_t* sectors;
    uft_vfs_info_t info;
    
    /* Internal state */
    uint8_t* bam;               /* Block Allocation Map */
    size_t bam_size;
    uint8_t* dir_cache;         /* Directory cache */
    size_t dir_cache_size;
    int current_dir_track;
    int current_dir_sector;
    bool dirty;                 /* Needs write-back */
    
    /* Format-specific data */
    void* fs_data;
} uft_vfs_context_t;

/*============================================================================
 * VFS OPERATIONS
 *============================================================================*/

/**
 * Detect filesystem type from disk image
 * @param sectors Sector interface
 * @return Detected filesystem type or UFT_VFS_UNKNOWN
 */
uft_vfs_type_t uft_vfs_detect(uft_vfs_sector_interface_t* sectors);

/**
 * Mount filesystem
 * @param ctx VFS context to initialize
 * @param sectors Sector interface
 * @param type Filesystem type (or UFT_VFS_UNKNOWN to auto-detect)
 * @return 0 on success, negative on error
 */
int uft_vfs_mount(uft_vfs_context_t* ctx, 
                  uft_vfs_sector_interface_t* sectors,
                  uft_vfs_type_t type);

/**
 * Unmount filesystem
 * @param ctx VFS context
 * @return 0 on success, negative on error
 */
int uft_vfs_unmount(uft_vfs_context_t* ctx);

/**
 * Get filesystem info
 * @param ctx VFS context
 * @param info Output info structure
 * @return 0 on success, negative on error
 */
int uft_vfs_get_info(uft_vfs_context_t* ctx, uft_vfs_info_t* info);

/**
 * Open directory for reading
 * @param ctx VFS context
 * @param path Directory path (NULL for root)
 * @return Handle or NULL on error
 */
void* uft_vfs_opendir(uft_vfs_context_t* ctx, const char* path);

/**
 * Read next directory entry
 * @param handle Directory handle from opendir
 * @param entry Output entry structure
 * @return 0 on success, 1 on end of directory, negative on error
 */
int uft_vfs_readdir(void* handle, uft_vfs_dirent_t* entry);

/**
 * Close directory
 * @param handle Directory handle
 */
void uft_vfs_closedir(void* handle);

/**
 * Read file
 * @param ctx VFS context
 * @param path File path
 * @param buffer Output buffer
 * @param size Buffer size
 * @param bytes_read Output: bytes actually read
 * @return 0 on success, negative on error
 */
int uft_vfs_read_file(uft_vfs_context_t* ctx,
                      const char* path,
                      uint8_t* buffer,
                      size_t size,
                      size_t* bytes_read);

/**
 * Write file
 * @param ctx VFS context
 * @param path File path
 * @param data Data to write
 * @param size Data size
 * @return 0 on success, negative on error
 */
int uft_vfs_write_file(uft_vfs_context_t* ctx,
                       const char* path,
                       const uint8_t* data,
                       size_t size);

/**
 * Delete file
 * @param ctx VFS context
 * @param path File path
 * @return 0 on success, negative on error
 */
int uft_vfs_delete(uft_vfs_context_t* ctx, const char* path);

/**
 * Rename file
 * @param ctx VFS context
 * @param old_path Current path
 * @param new_path New path
 * @return 0 on success, negative on error
 */
int uft_vfs_rename(uft_vfs_context_t* ctx,
                   const char* old_path,
                   const char* new_path);

/**
 * Create directory
 * @param ctx VFS context
 * @param path Directory path
 * @return 0 on success, negative on error
 */
int uft_vfs_mkdir(uft_vfs_context_t* ctx, const char* path);

/**
 * Format disk with filesystem
 * @param sectors Sector interface
 * @param type Filesystem type
 * @param label Volume label
 * @return 0 on success, negative on error
 */
int uft_vfs_format(uft_vfs_sector_interface_t* sectors,
                   uft_vfs_type_t type,
                   const char* label);

/**
 * Validate/check filesystem
 * @param ctx VFS context
 * @param fix If true, attempt to fix errors
 * @return Number of errors found (0 if clean)
 */
int uft_vfs_check(uft_vfs_context_t* ctx, bool fix);

/*============================================================================
 * CP/M SPECIFIC
 *============================================================================*/

typedef struct uft_vfs_cpm_params {
    int block_size;         /* 1024, 2048, 4096, 8192 */
    int dir_entries;        /* Number of directory entries */
    int reserved_tracks;    /* System tracks */
    int extent_mask;        /* EX mask */
    bool timestamped;       /* DateStamper active */
} uft_vfs_cpm_params_t;

/**
 * Set CP/M filesystem parameters
 * @param ctx VFS context (must be CP/M type)
 * @param params CP/M parameters
 * @return 0 on success, negative on error
 */
int uft_vfs_cpm_set_params(uft_vfs_context_t* ctx,
                           const uft_vfs_cpm_params_t* params);

/*============================================================================
 * COMMODORE SPECIFIC
 *============================================================================*/

/**
 * Get CBM file type string
 * @param type File type code
 * @return Type string (e.g., "PRG", "SEQ")
 */
const char* uft_vfs_cbm_type_string(uint8_t type);

/**
 * Read CBM BAM (Block Allocation Map)
 * @param ctx VFS context
 * @param bam Output BAM buffer
 * @param size BAM buffer size
 * @return 0 on success, negative on error
 */
int uft_vfs_cbm_read_bam(uft_vfs_context_t* ctx,
                         uint8_t* bam,
                         size_t size);

/**
 * Validate CBM disk (VALIDATE command)
 * @param ctx VFS context
 * @return 0 on success, negative on error
 */
int uft_vfs_cbm_validate(uft_vfs_context_t* ctx);

/**
 * Get CBM disk ID
 * @param ctx VFS context
 * @param id Output buffer (5 bytes: 2 ID + 2 DOS type + null)
 */
void uft_vfs_cbm_get_id(uft_vfs_context_t* ctx, char* id);

/*============================================================================
 * APPLE SPECIFIC
 *============================================================================*/

typedef struct uft_vfs_apple_catalog_entry {
    char filename[31];          /* ProDOS allows 15 chars */
    uint8_t file_type;
    uint16_t aux_type;          /* Load address, etc. */
    uint16_t blocks_used;
    time_t modified;
} uft_vfs_apple_catalog_entry_t;

/**
 * Get Apple ProDOS file type string
 * @param type File type code
 * @return Type string (e.g., "TXT", "BIN", "SYS")
 */
const char* uft_vfs_prodos_type_string(uint8_t type);

/*============================================================================
 * AMIGA SPECIFIC
 *============================================================================*/

typedef struct uft_vfs_amiga_info {
    bool is_ffs;                /* FFS vs OFS */
    bool is_intl;               /* International mode */
    bool is_dirc;               /* Directory cache */
    uint32_t root_block;
    uint32_t bitmap_blocks[25]; /* Bitmap block pointers */
} uft_vfs_amiga_info_t;

/**
 * Get Amiga filesystem extended info
 * @param ctx VFS context
 * @param info Output info structure
 * @return 0 on success, negative on error
 */
int uft_vfs_amiga_get_info(uft_vfs_context_t* ctx,
                           uft_vfs_amiga_info_t* info);

/*============================================================================
 * BROTHER SPECIFIC
 *============================================================================*/

typedef struct uft_vfs_brother_info {
    char document_name[32];
    int page_count;
    int font;
    int pitch;
} uft_vfs_brother_info_t;

/*============================================================================
 * HP LIF SPECIFIC
 *============================================================================*/

typedef struct uft_vfs_lif_info {
    uint16_t directory_start;
    uint16_t directory_length;
    uint32_t volume_version;
    char volume_label[7];
} uft_vfs_lif_info_t;

/*============================================================================
 * HELPER FUNCTIONS
 *============================================================================*/

/**
 * Get filesystem type name
 * @param type Filesystem type
 * @return Name string
 */
const char* uft_vfs_type_name(uft_vfs_type_t type);

/**
 * Convert PETSCII to ASCII
 * @param dst Destination buffer
 * @param src Source PETSCII string
 * @param len Maximum length
 */
void uft_vfs_petscii_to_ascii(char* dst, const uint8_t* src, size_t len);

/**
 * Convert ASCII to PETSCII
 * @param dst Destination buffer
 * @param src Source ASCII string
 * @param len Maximum length
 */
void uft_vfs_ascii_to_petscii(uint8_t* dst, const char* src, size_t len);

/**
 * Convert Apple ProDOS timestamp to time_t
 * @param prodos_time ProDOS timestamp (32-bit)
 * @return Unix timestamp
 */
time_t uft_vfs_prodos_to_time(uint32_t prodos_time);

/**
 * Convert time_t to Apple ProDOS timestamp
 * @param t Unix timestamp
 * @return ProDOS timestamp
 */
uint32_t uft_vfs_time_to_prodos(time_t t);

/*============================================================================
 * ERROR CODES
 *============================================================================*/

#define UFT_VFS_OK              0
#define UFT_VFS_ERR_NOMEM      -1
#define UFT_VFS_ERR_IO         -2
#define UFT_VFS_ERR_NOTFOUND   -3
#define UFT_VFS_ERR_EXISTS     -4
#define UFT_VFS_ERR_FULL       -5
#define UFT_VFS_ERR_DIRFULL    -6
#define UFT_VFS_ERR_READONLY   -7
#define UFT_VFS_ERR_BADNAME    -8
#define UFT_VFS_ERR_CORRUPT    -9
#define UFT_VFS_ERR_BADTYPE    -10
#define UFT_VFS_ERR_NOTEMPTY   -11
#define UFT_VFS_ERR_NOTDIR     -12
#define UFT_VFS_ERR_ISDIR      -13
#define UFT_VFS_ERR_BADFS      -14

#ifdef __cplusplus
}
#endif

#endif /* UFT_VFS_H */
