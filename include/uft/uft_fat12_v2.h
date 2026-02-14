/**
 * @file uft_fat12.h
 * @brief FAT12 Filesystem Support for UFT
 * 
 * Support for reading and writing FAT12 filesystems commonly used
 * specifications.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_FAT12_H
#define UFT_FAT12_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FAT12 Constants
 *============================================================================*/

/** Standard sector size */
#define UFT_FAT12_SECTOR_SIZE       512

/** BIOS Parameter Block size */
#define UFT_FAT12_BPB_SIZE          25

/** Boot sector signature */
#define UFT_FAT12_BOOT_SIG          0xAA55

/** Empty directory entry marker */
#define UFT_FAT12_DIR_EMPTY         0xE5

/** End of directory marker */
#define UFT_FAT12_DIR_END           0x00

/** Long filename marker */
#define UFT_FAT12_DIR_LFN           0x0F

/** Directory entry size */
#define UFT_FAT12_DIR_ENTRY_SIZE    32

/** Maximum filename length (8.3 format) */
#define UFT_FAT12_NAME_LEN          8
#define UFT_FAT12_EXT_LEN           3

/*============================================================================
 * FAT12 Cluster Values
 *============================================================================*/

/** Free cluster */
#define UFT_FAT12_FREE              0x000

/** Reserved cluster range start */
#define UFT_FAT12_RESERVED_START    0xFF0

/** Bad cluster marker */
#define UFT_FAT12_BAD_CLUSTER       0xFF7

/** End of chain marker (minimum) */
#define UFT_FAT12_EOC_MIN           0xFF8

/** End of chain marker (standard) */
#define UFT_FAT12_EOC               0xFFF

/*============================================================================
 * FAT12 File Attributes
 *============================================================================*/

/** Read-only file */
#define UFT_FAT12_ATTR_READONLY     0x01

/** Hidden file */
#define UFT_FAT12_ATTR_HIDDEN       0x02

/** System file */
#define UFT_FAT12_ATTR_SYSTEM       0x04

/** Volume label */
#define UFT_FAT12_ATTR_VOLUME       0x08

/** Directory */
#define UFT_FAT12_ATTR_DIRECTORY    0x10

/** Archive flag */
#define UFT_FAT12_ATTR_ARCHIVE      0x20

/** Long filename entry */
#define UFT_FAT12_ATTR_LFN          0x0F

/*============================================================================
 * FAT12 Media Descriptor Bytes
 *============================================================================*/

/** 3.5" 1.44MB */
#define UFT_FAT12_MEDIA_144MB       0xF0

/** 3.5" 2.88MB */
#define UFT_FAT12_MEDIA_288MB       0xF0

/** 5.25" 1.2MB */
#define UFT_FAT12_MEDIA_12MB        0xF9

/** 3.5" 720KB */
#define UFT_FAT12_MEDIA_720KB       0xF9

/** 5.25" 360KB */
#define UFT_FAT12_MEDIA_360KB       0xFD

/** 5.25" 320KB */
#define UFT_FAT12_MEDIA_320KB       0xFF

/** 5.25" 180KB */
#define UFT_FAT12_MEDIA_180KB       0xFC

/** 5.25" 160KB */
#define UFT_FAT12_MEDIA_160KB       0xFE

/** 8" SD */
#define UFT_FAT12_MEDIA_8SD         0xFE

/** 8" DD */
#define UFT_FAT12_MEDIA_8DD         0xFD

/*============================================================================
 * FAT12 Structures
 *============================================================================*/

/**
 * @brief BIOS Parameter Block (BPB)
 */
#pragma pack(push, 1)
typedef struct {
    uint16_t bytes_per_sector;      /**< Bytes per sector (usually 512) */
    uint8_t  sectors_per_cluster;   /**< Sectors per cluster */
    uint16_t reserved_sectors;      /**< Reserved sectors (including boot) */
    uint8_t  num_fats;              /**< Number of FAT copies */
    uint16_t root_entries;          /**< Root directory entries */
    uint16_t total_sectors_16;      /**< Total sectors (16-bit) */
    uint8_t  media_descriptor;      /**< Media descriptor byte */
    uint16_t sectors_per_fat;       /**< Sectors per FAT */
    uint16_t sectors_per_track;     /**< Sectors per track */
    uint16_t num_heads;             /**< Number of heads */
    uint32_t hidden_sectors;        /**< Hidden sectors */
    uint32_t total_sectors_32;      /**< Total sectors (32-bit) */
} uft_fat12_bpb_t;
#pragma pack(pop)

/**
 * @brief Boot sector structure
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];               /**< Jump instruction (EB xx 90) */
    char     oem_name[8];           /**< OEM name */
    uft_fat12_bpb_t bpb;            /**< BIOS Parameter Block */
    uint8_t  drive_number;          /**< Drive number */
    uint8_t  reserved1;             /**< Reserved */
    uint8_t  boot_signature;        /**< Extended boot signature (0x29) */
    uint32_t volume_serial;         /**< Volume serial number */
    char     volume_label[11];      /**< Volume label */
    char     fs_type[8];            /**< Filesystem type ("FAT12   ") */
    uint8_t  boot_code[448];        /**< Boot code */
    uint16_t signature;             /**< Boot signature (0xAA55) */
} uft_fat12_boot_t;
#pragma pack(pop)

/**
 * @brief Directory entry structure
 */
#pragma pack(push, 1)
typedef struct {
    char     name[8];               /**< Filename (space padded) */
    char     ext[3];                /**< Extension (space padded) */
    uint8_t  attributes;            /**< File attributes */
    uint8_t  reserved[10];          /**< Reserved (NT uses some) */
    uint16_t time;                  /**< Last modified time */
    uint16_t date;                  /**< Last modified date */
    uint16_t cluster;               /**< First cluster number */
    uint32_t size;                  /**< File size in bytes */
} uft_fat12_dirent_t;
#pragma pack(pop)

/**
 * @brief FAT12 filesystem handle
 */
typedef struct {
    /* Disk image */
    uint8_t* data;                  /**< Raw disk data */
    size_t   data_size;             /**< Disk data size */
    bool     data_owned;            /**< True if we allocated data */
    
    /* Boot sector info */
    uft_fat12_boot_t boot;          /**< Boot sector copy */
    
    /* Calculated values */
    uint16_t bytes_per_cluster;     /**< Bytes per cluster */
    uint16_t root_dir_sectors;      /**< Root directory sectors */
    uint32_t first_fat_sector;      /**< First FAT sector */
    uint32_t first_root_sector;     /**< First root directory sector */
    uint32_t first_data_sector;     /**< First data sector */
    uint32_t total_clusters;        /**< Total data clusters */
    
    /* State */
    bool     modified;              /**< True if modified */
} uft_fat12_t;

/**
 * @brief File handle for FAT12 access
 */
typedef struct {
    uft_fat12_t* fs;                /**< Filesystem */
    uft_fat12_dirent_t* dirent;     /**< Directory entry */
    uint32_t dir_sector;            /**< Sector containing dirent */
    uint16_t dir_offset;            /**< Offset in sector */
    uint16_t cluster;               /**< Current cluster */
    uint32_t position;              /**< Current position */
    uint32_t size;                  /**< File size */
    uint8_t  mode;                  /**< Open mode */
} uft_fat12_file_t;

/*============================================================================
 * Standard Format Definitions
 *============================================================================*/

/**
 * @brief Standard floppy format definition
 */
typedef struct {
    uint16_t size_kb;               /**< Size in KB */
    uint16_t total_sectors;         /**< Total sectors */
    uint8_t  sectors_per_track;     /**< Sectors per track */
    uint8_t  heads;                 /**< Number of heads */
    uint8_t  tracks;                /**< Number of tracks */
    uint8_t  sectors_per_cluster;   /**< Sectors per cluster */
    uint16_t root_entries;          /**< Root directory entries */
    uint16_t sectors_per_fat;       /**< Sectors per FAT */
    uint8_t  media_descriptor;      /**< Media descriptor */
    const char* name;               /**< Format name */
} uft_fat12_format_t;

/**
 * @brief Standard floppy formats table
 */
extern const uft_fat12_format_t uft_fat12_formats[];

/**
 * @brief Number of standard formats
 */
#define UFT_FAT12_NUM_FORMATS       10

/*============================================================================
 * Date/Time Conversion
 *============================================================================*/

/**
 * @brief Decode FAT date
 * @param fat_date FAT date value
 * @param year Output year (1980-2107)
 * @param month Output month (1-12)
 * @param day Output day (1-31)
 */
static inline void uft_fat12_decode_date(uint16_t fat_date,
                                          uint16_t* year,
                                          uint8_t* month,
                                          uint8_t* day)
{
    if (year)  *year  = ((fat_date >> 9) & 0x7F) + 1980;
    if (month) *month = (fat_date >> 5) & 0x0F;
    if (day)   *day   = fat_date & 0x1F;
}

/**
 * @brief Decode FAT time
 * @param fat_time FAT time value
 * @param hour Output hour (0-23)
 * @param minute Output minute (0-59)
 * @param second Output second (0-58, 2-second resolution)
 */
static inline void uft_fat12_decode_time(uint16_t fat_time,
                                          uint8_t* hour,
                                          uint8_t* minute,
                                          uint8_t* second)
{
    if (hour)   *hour   = (fat_time >> 11) & 0x1F;
    if (minute) *minute = (fat_time >> 5) & 0x3F;
    if (second) *second = (fat_time & 0x1F) * 2;
}

/**
 * @brief Encode FAT date
 */
static inline uint16_t uft_fat12_encode_date(uint16_t year, 
                                              uint8_t month, 
                                              uint8_t day)
{
    return ((year - 1980) << 9) | (month << 5) | day;
}

/**
 * @brief Encode FAT time
 */
static inline uint16_t uft_fat12_encode_time(uint8_t hour,
                                              uint8_t minute,
                                              uint8_t second)
{
    return (hour << 11) | (minute << 5) | (second / 2);
}

/*============================================================================
 * FAT12 API Functions
 *============================================================================*/

/**
 * @brief Initialize FAT12 filesystem from disk image
 * @param fs Filesystem handle to initialize
 * @param data Disk image data
 * @param size Data size
 * @param copy If true, make a copy of the data
 * @return 0 on success
 */
int uft_fat12_init(uft_fat12_t* fs, uint8_t* data, size_t size, bool copy);

/**
 * @brief Free FAT12 filesystem resources
 */
void uft_fat12_free(uft_fat12_t* fs);

/**
 * @brief Create new FAT12 filesystem
 * @param fs Filesystem handle
 * @param format Format definition
 * @return 0 on success
 */
int uft_fat12_format(uft_fat12_t* fs, const uft_fat12_format_t* format);

/**
 * @brief Detect format from disk image
 * @param data Disk image data
 * @param size Data size
 * @return Pointer to matching format or NULL
 */
const uft_fat12_format_t* uft_fat12_detect_format(const uint8_t* data, 
                                                   size_t size);

/**
 * @brief Read FAT entry
 * @param fs Filesystem
 * @param cluster Cluster number
 * @return FAT entry value or -1 on error
 */
int uft_fat12_read_fat(uft_fat12_t* fs, uint16_t cluster);

/**
 * @brief Write FAT entry
 * @param fs Filesystem
 * @param cluster Cluster number
 * @param value Value to write
 * @return 0 on success
 */
int uft_fat12_write_fat(uft_fat12_t* fs, uint16_t cluster, uint16_t value);

/**
 * @brief Find free cluster
 * @param fs Filesystem
 * @return Cluster number or 0 if full
 */
uint16_t uft_fat12_find_free_cluster(uft_fat12_t* fs);

/**
 * @brief Read cluster data
 * @param fs Filesystem
 * @param cluster Cluster number
 * @param buffer Output buffer
 * @return Bytes read or -1 on error
 */
int uft_fat12_read_cluster(uft_fat12_t* fs, uint16_t cluster, uint8_t* buffer);

/**
 * @brief Write cluster data
 */
int uft_fat12_write_cluster(uft_fat12_t* fs, uint16_t cluster, 
                            const uint8_t* buffer);

/*============================================================================
 * Directory Functions
 *============================================================================*/

/**
 * @brief List directory contents
 * @param fs Filesystem
 * @param path Directory path (NULL or "" for root)
 * @param callback Called for each entry
 * @param user_data User data passed to callback
 * @return Number of entries or -1 on error
 */
int uft_fat12_list_dir(uft_fat12_t* fs, const char* path,
                       void (*callback)(const uft_fat12_dirent_t* entry,
                                        void* user_data),
                       void* user_data);

/**
 * @brief Find file in directory
 * @param fs Filesystem
 * @param path File path
 * @param entry Output directory entry
 * @return 0 if found, -1 if not found
 */
int uft_fat12_find_file(uft_fat12_t* fs, const char* path,
                        uft_fat12_dirent_t* entry);

/**
 * @brief Create directory entry
 */
int uft_fat12_create_entry(uft_fat12_t* fs, const char* path,
                           uint8_t attributes);

/**
 * @brief Delete file or directory
 */
int uft_fat12_delete(uft_fat12_t* fs, const char* path);

/*============================================================================
 * File Functions
 *============================================================================*/

/**
 * @brief Open file
 * @param fs Filesystem
 * @param path File path
 * @param mode Open mode ('r', 'w', 'a')
 * @return File handle or NULL on error
 */
uft_fat12_file_t* uft_fat12_fopen(uft_fat12_t* fs, const char* path, 
                                   const char* mode);

/**
 * @brief Close file
 */
void uft_fat12_fclose(uft_fat12_file_t* file);

/**
 * @brief Read from file
 * @param buffer Output buffer
 * @param size Bytes to read
 * @param file File handle
 * @return Bytes read
 */
size_t uft_fat12_fread(void* buffer, size_t size, uft_fat12_file_t* file);

/**
 * @brief Write to file
 */
size_t uft_fat12_fwrite(const void* buffer, size_t size, uft_fat12_file_t* file);

/**
 * @brief Seek in file
 */
int uft_fat12_fseek(uft_fat12_file_t* file, long offset, int whence);

/**
 * @brief Get file position
 */
long uft_fat12_ftell(uft_fat12_file_t* file);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * @brief Convert 8.3 filename to string
 */
void uft_fat12_name_to_str(const uft_fat12_dirent_t* entry, char* buffer);

/**
 * @brief Convert string to 8.3 filename
 */
int uft_fat12_str_to_name(const char* str, char* name, char* ext);

/**
 * @brief Get filesystem statistics
 */
void uft_fat12_get_stats(uft_fat12_t* fs, uint32_t* total_clusters,
                         uint32_t* free_clusters, uint32_t* bad_clusters);

/**
 * @brief Print filesystem information
 */
void uft_fat12_print_info(uft_fat12_t* fs, bool verbose);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT12_H */
