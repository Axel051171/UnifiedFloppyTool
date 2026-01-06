/**
 * @file uft_fdi.h
 * @brief FAT Disk Image (FDI) Support for UFT
 * 
 * Based on analysis of FDI.C by Dave Dunfield.
 * 
 * Provides FAT12/FAT16 filesystem support for floppy disk images,
 * including directory operations and file extraction.
 * 
 * @copyright UFT Project
 */

#ifndef UFT_FDI_H
#define UFT_FDI_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Portable Packed Structure Macros
 *============================================================================*/

#ifndef UFT_PACKED_BEGIN
#ifdef _MSC_VER
    #define UFT_PACKED_BEGIN __pragma(pack(push, 1))
    #define UFT_PACKED_END   __pragma(pack(pop))
    #define UFT_PACKED_ATTR
#else
    #define UFT_PACKED_BEGIN
    #define UFT_PACKED_END
    #define UFT_PACKED_ATTR __attribute__((packed))
#endif
#endif /* UFT_PACKED_BEGIN */

/*============================================================================
 * FAT Constants
 *============================================================================*/
#define UFT_FDI_SECTOR_SIZE     512

/** BIOS Parameter Block size */
#define UFT_FDI_BPB_SIZE        17

/** Empty directory entry marker */
#define UFT_FDI_DIR_EMPTY       0xE5

/** End of directory marker */
#define UFT_FDI_DIR_END         0x00

/** Long filename entry marker */
#define UFT_FDI_DIR_LFN         0x0F

/*============================================================================
 * FAT File Attributes
 *============================================================================*/

#define UFT_FDI_ATTR_READONLY   0x01
#define UFT_FDI_ATTR_HIDDEN     0x02
#define UFT_FDI_ATTR_SYSTEM     0x04
#define UFT_FDI_ATTR_VOLUME     0x08
#define UFT_FDI_ATTR_DIRECTORY  0x10
#define UFT_FDI_ATTR_ARCHIVE    0x20

/*============================================================================
 * FAT Media Descriptor Bytes
 *============================================================================*/

#define UFT_FDI_MEDIA_160K      0xFE  /**< 160KB 5.25" SS */
#define UFT_FDI_MEDIA_180K      0xFC  /**< 180KB 5.25" SS */
#define UFT_FDI_MEDIA_320K      0xFF  /**< 320KB 5.25" DS */
#define UFT_FDI_MEDIA_360K      0xFD  /**< 360KB 5.25" DS */
#define UFT_FDI_MEDIA_720K      0xF9  /**< 720KB 3.5" DS */
#define UFT_FDI_MEDIA_1200K     0xF9  /**< 1.2MB 5.25" HD */
#define UFT_FDI_MEDIA_1440K     0xF0  /**< 1.44MB 3.5" HD */
#define UFT_FDI_MEDIA_2880K     0xF0  /**< 2.88MB 3.5" ED */

/*============================================================================
 * Standard Disk Types
 *============================================================================*/

/**
 * @brief Standard disk type definitions
 */
typedef struct {
    uint16_t total_size_kb;     /**< Total size in KB */
    uint8_t  sectors_cluster;   /**< Sectors per cluster */
    uint8_t  reserved_sectors;  /**< Reserved sectors */
    uint8_t  num_fats;          /**< Number of FATs */
    uint16_t root_entries;      /**< Root directory entries */
    uint16_t total_sectors;     /**< Total sectors (if < 65536) */
    uint8_t  media_id;          /**< Media descriptor byte */
    uint16_t sectors_fat;       /**< Sectors per FAT */
    uint8_t  sectors_track;     /**< Sectors per track */
    uint8_t  num_heads;         /**< Number of heads */
} uft_fdi_disk_type_t;

/**
 * @brief Standard disk types table
 */
extern const uft_fdi_disk_type_t uft_fdi_disk_types[];

/*============================================================================
 * FAT Structures
 *============================================================================*/

/**
 * @brief FAT Boot Sector (BIOS Parameter Block)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  jump[3];           /**< Jump instruction */
    uint8_t  oem_name[8];       /**< OEM name */
    uint16_t bytes_sector;      /**< Bytes per sector */
    uint8_t  sectors_cluster;   /**< Sectors per cluster */
    uint16_t reserved_sectors;  /**< Reserved sectors */
    uint8_t  num_fats;          /**< Number of FATs */
    uint16_t root_entries;      /**< Root directory entries */
    uint16_t total_sectors_16;  /**< Total sectors (16-bit) */
    uint8_t  media_id;          /**< Media descriptor */
    uint16_t sectors_fat;       /**< Sectors per FAT */
    uint16_t sectors_track;     /**< Sectors per track */
    uint16_t num_heads;         /**< Number of heads */
    uint32_t hidden_sectors;    /**< Hidden sectors */
    uint32_t total_sectors_32;  /**< Total sectors (32-bit) */
    
    /* Extended BPB (FAT12/16) */
    uint8_t  drive_number;      /**< BIOS drive number */
    uint8_t  reserved;          /**< Reserved */
    uint8_t  boot_signature;    /**< Extended boot signature (0x29) */
    uint32_t volume_serial;     /**< Volume serial number */
    uint8_t  volume_label[11];  /**< Volume label */
    uint8_t  fs_type[8];        /**< Filesystem type ("FAT12   ") */
} uft_fdi_boot_sector_t;
#pragma pack(pop)

/**
 * @brief FAT Directory Entry (32 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  name[8];           /**< Filename (space-padded) */
    uint8_t  ext[3];            /**< Extension (space-padded) */
    uint8_t  attr;              /**< File attributes */
    uint8_t  reserved[10];      /**< Reserved */
    uint16_t time;              /**< Last modified time */
    uint16_t date;              /**< Last modified date */
    uint16_t cluster;           /**< First cluster */
    uint32_t size;              /**< File size in bytes */
} uft_fdi_dir_entry_t;
#pragma pack(pop)

/*============================================================================
 * Time/Date Conversion
 *============================================================================*/

/**
 * @brief Decode DOS time value
 * @param time DOS time (HHHHHMMMMMMSSSSS)
 * @param hour Output hour (0-23)
 * @param minute Output minute (0-59)
 * @param second Output second (0-58, 2-second resolution)
 */
static inline void uft_fdi_decode_time(uint16_t time, 
                                        uint8_t* hour, uint8_t* minute, uint8_t* second)
{
    if (hour) *hour = (time >> 11) & 0x1F;
    if (minute) *minute = (time >> 5) & 0x3F;
    if (second) *second = (time & 0x1F) * 2;
}

/**
 * @brief Encode DOS time value
 */
static inline uint16_t uft_fdi_encode_time(uint8_t hour, uint8_t minute, uint8_t second)
{
    return ((uint16_t)(hour & 0x1F) << 11) |
           ((uint16_t)(minute & 0x3F) << 5) |
           ((second / 2) & 0x1F);
}

/**
 * @brief Decode DOS date value
 * @param date DOS date (YYYYYYYMMMMDDDDD)
 * @param year Output year (1980-2107)
 * @param month Output month (1-12)
 * @param day Output day (1-31)
 */
static inline void uft_fdi_decode_date(uint16_t date,
                                        uint16_t* year, uint8_t* month, uint8_t* day)
{
    if (year) *year = ((date >> 9) & 0x7F) + 1980;
    if (month) *month = (date >> 5) & 0x0F;
    if (day) *day = date & 0x1F;
}

/**
 * @brief Encode DOS date value
 */
static inline uint16_t uft_fdi_encode_date(uint16_t year, uint8_t month, uint8_t day)
{
    return ((uint16_t)((year - 1980) & 0x7F) << 9) |
           ((uint16_t)(month & 0x0F) << 5) |
           (day & 0x1F);
}

/*============================================================================
 * FDI Image Structure
 *============================================================================*/

/**
 * @brief FDI disk image
 */
typedef struct {
    uint8_t* data;              /**< Raw disk data */
    size_t   size;              /**< Total size */
    
    /* BPB parameters */
    uint16_t bytes_sector;
    uint8_t  sectors_cluster;
    uint16_t reserved_sectors;
    uint8_t  num_fats;
    uint16_t root_entries;
    uint16_t sectors_fat;
    uint16_t sectors_track;
    uint8_t  num_heads;
    uint32_t total_sectors;
    uint8_t  media_id;
    
    /* Calculated values */
    uint32_t fat_start;         /**< First FAT sector */
    uint32_t root_start;        /**< Root directory sector */
    uint32_t data_start;        /**< First data sector */
    uint32_t root_sectors;      /**< Sectors in root directory */
    uint32_t data_clusters;     /**< Total data clusters */
    uint32_t cluster_size;      /**< Bytes per cluster */
    
    bool     is_fat16;          /**< FAT16 (vs FAT12) */
} uft_fdi_image_t;

/*============================================================================
 * File Control Block
 *============================================================================*/

/**
 * @brief File control block for open files
 */
typedef struct {
    uft_fdi_dir_entry_t* dir_entry;  /**< Directory entry */
    uint32_t dir_sector;        /**< Sector containing dir entry */
    uint16_t first_cluster;     /**< First cluster */
    uint16_t current_cluster;   /**< Current cluster */
    uint32_t position;          /**< Current position */
    uint32_t size;              /**< File size */
    uint8_t  sector_in_cluster; /**< Current sector in cluster */
    bool     modified;          /**< File was modified */
    bool     is_write;          /**< Opened for write */
} uft_fdi_file_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Initialize FDI image structure
 */
int uft_fdi_init(uft_fdi_image_t* img);

/**
 * @brief Free FDI image resources
 */
void uft_fdi_free(uft_fdi_image_t* img);

/**
 * @brief Read disk image and parse BPB
 * @param filename Input filename
 * @param img Output image structure
 * @return 0 on success
 */
int uft_fdi_read(const char* filename, uft_fdi_image_t* img);

/**
 * @brief Read from memory buffer
 */
int uft_fdi_read_mem(const uint8_t* data, size_t size, uft_fdi_image_t* img);

/**
 * @brief Write disk image to file
 */
int uft_fdi_write(const char* filename, const uft_fdi_image_t* img);

/**
 * @brief Create new formatted disk image
 * @param img Image structure
 * @param type_index Index into uft_fdi_disk_types
 * @return 0 on success
 */
int uft_fdi_format(uft_fdi_image_t* img, int type_index);

/**
 * @brief Read sector from image
 * @param img Disk image
 * @param sector Sector number (0-based)
 * @param buffer Output buffer (must be sector size)
 * @return 0 on success
 */
int uft_fdi_read_sector(const uft_fdi_image_t* img, uint32_t sector, uint8_t* buffer);

/**
 * @brief Write sector to image
 */
int uft_fdi_write_sector(uft_fdi_image_t* img, uint32_t sector, const uint8_t* buffer);

/**
 * @brief Get FAT entry for cluster
 * @param img Disk image
 * @param cluster Cluster number
 * @return Next cluster or 0xFFF/0xFFFF for end
 */
uint16_t uft_fdi_get_fat(const uft_fdi_image_t* img, uint16_t cluster);

/**
 * @brief Set FAT entry for cluster
 */
int uft_fdi_set_fat(uft_fdi_image_t* img, uint16_t cluster, uint16_t value);

/**
 * @brief Find free cluster
 * @param img Disk image
 * @return Free cluster number or 0 if disk full
 */
uint16_t uft_fdi_find_free_cluster(const uft_fdi_image_t* img);

/**
 * @brief Open directory for reading
 * @param img Disk image
 * @param path Directory path (NULL or "" for root)
 * @param first_cluster Output: first cluster of directory
 * @return 0 on success
 */
int uft_fdi_open_dir(const uft_fdi_image_t* img, const char* path,
                     uint16_t* first_cluster);

/**
 * @brief Read next directory entry
 * @param img Disk image
 * @param cluster Current cluster (0 for root)
 * @param index Entry index within directory
 * @param entry Output entry
 * @return 0 on success, 1 for end of directory, -1 on error
 */
int uft_fdi_read_dir_entry(const uft_fdi_image_t* img, uint16_t cluster,
                           uint16_t index, uft_fdi_dir_entry_t* entry);

/**
 * @brief Find file by name
 * @param img Disk image
 * @param path Full path to file
 * @param entry Output directory entry
 * @return 0 on success
 */
int uft_fdi_find_file(const uft_fdi_image_t* img, const char* path,
                      uft_fdi_dir_entry_t* entry);

/**
 * @brief Extract file to buffer
 * @param img Disk image
 * @param entry Directory entry
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Bytes read or -1 on error
 */
int uft_fdi_extract_file(const uft_fdi_image_t* img,
                         const uft_fdi_dir_entry_t* entry,
                         uint8_t* buffer, size_t size);

/**
 * @brief List directory contents
 * @param img Disk image
 * @param path Directory path
 * @param callback Function called for each entry
 * @param user_data User data passed to callback
 */
typedef void (*uft_fdi_dir_callback_t)(const uft_fdi_dir_entry_t* entry,
                                        void* user_data);
int uft_fdi_list_dir(const uft_fdi_image_t* img, const char* path,
                     uft_fdi_dir_callback_t callback, void* user_data);

/**
 * @brief Print filesystem information
 */
void uft_fdi_print_info(const uft_fdi_image_t* img, bool verbose);

/**
 * @brief Convert 8.3 name to readable string
 * @param entry Directory entry
 * @param buffer Output buffer (at least 13 bytes)
 */
void uft_fdi_name_to_string(const uft_fdi_dir_entry_t* entry, char* buffer);

/**
 * @brief Convert string to 8.3 format
 * @param name Input filename
 * @param entry Directory entry (name and ext fields set)
 * @return 0 on success
 */
int uft_fdi_string_to_name(const char* name, uft_fdi_dir_entry_t* entry);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDI_H */
