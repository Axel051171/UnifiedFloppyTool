/**
 * @file uft_fatx.h
 * @brief Xbox FATX Filesystem Structures
 * 
 * Based on FATXTools
 * Supports Xbox (LE) and Xbox 360 (BE) platforms
 */

#ifndef UFT_FATX_H
#define UFT_FATX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_FATX_SECTOR_SIZE        0x200       /**< 512 bytes */
#define UFT_FATX_PAGE_SIZE          0x1000      /**< 4096 bytes */
#define UFT_FATX_RESERVED_BYTES     UFT_FATX_PAGE_SIZE
#define UFT_FATX_RESERVED_CLUSTERS  1

#define UFT_FATX_SIGNATURE          0x58544146  /**< "FATX" in LE */
#define UFT_FATX_DIRENT_SIZE        64          /**< Directory entry size */
#define UFT_FATX_MAX_FILENAME       42          /**< Max filename length */

/** Directory entry status markers */
#define UFT_FATX_DIRENT_NEVER_USED   0x00
#define UFT_FATX_DIRENT_NEVER_USED2  0xFF
#define UFT_FATX_DIRENT_DELETED      0xE5

/** FAT32 cluster markers */
#define UFT_FATX_CLUSTER_AVAILABLE   0x00000000
#define UFT_FATX_CLUSTER_RESERVED    0xFFFFFFF0
#define UFT_FATX_CLUSTER_BAD         0xFFFFFFF7
#define UFT_FATX_CLUSTER_MEDIA       0xFFFFFFF8
#define UFT_FATX_CLUSTER_LAST        0xFFFFFFFF

/** FAT16 cluster markers */
#define UFT_FATX16_CLUSTER_AVAILABLE 0x0000
#define UFT_FATX16_CLUSTER_RESERVED  0xFFF0
#define UFT_FATX16_CLUSTER_BAD       0xFFF7
#define UFT_FATX16_CLUSTER_MEDIA     0xFFF8
#define UFT_FATX16_CLUSTER_LAST      0xFFFF

/** File attributes */
#define UFT_FATX_ATTR_READONLY       0x01
#define UFT_FATX_ATTR_HIDDEN         0x02
#define UFT_FATX_ATTR_SYSTEM         0x04
#define UFT_FATX_ATTR_DIRECTORY      0x10
#define UFT_FATX_ATTR_ARCHIVE        0x20
#define UFT_FATX_ATTR_DEVICE         0x40

/*===========================================================================
 * Platform Types
 *===========================================================================*/

typedef enum {
    UFT_FATX_PLATFORM_XBOX   = 0,   /**< Original Xbox (little-endian) */
    UFT_FATX_PLATFORM_X360   = 1    /**< Xbox 360 (big-endian) */
} uft_fatx_platform_t;

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief FATX volume header (on-disk)
 */
typedef struct __attribute__((packed)) {
    uint32_t signature;             /**< "FATX" (0x58544146) */
    uint32_t volume_id;             /**< Volume serial number */
    uint32_t sectors_per_cluster;   /**< Cluster size in sectors */
    uint32_t root_dir_cluster;      /**< First cluster of root directory */
    /* Padding to sector boundary */
} uft_fatx_header_t;

/**
 * @brief Xbox timestamp format
 * 
 * Bits 0-4:   Seconds / 2 (0-29)
 * Bits 5-10:  Minutes (0-59)
 * Bits 11-15: Hours (0-23)
 * Bits 16-20: Day (1-31)
 * Bits 21-24: Month (1-12)
 * Bits 25-31: Year - 2000
 */
typedef uint32_t uft_fatx_time_t;

/**
 * @brief Xbox 360 timestamp format
 * 
 * Bits 0-4:   Seconds / 2 (0-29)
 * Bits 5-10:  Minutes (0-59)
 * Bits 11-15: Hours (0-23)
 * Bits 16-20: Day (1-31)
 * Bits 21-24: Month (1-12)
 * Bits 25-31: Year - 1980
 */
typedef uint32_t uft_fatx360_time_t;

/**
 * @brief FATX directory entry (64 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  filename_length;       /**< 0x00 = never used, 0xE5 = deleted, 0xFF = end */
    uint8_t  attributes;            /**< File attributes */
    uint8_t  filename[42];          /**< Filename (ASCII, not null-terminated) */
    uint32_t first_cluster;         /**< First cluster number */
    uint32_t file_size;             /**< File size in bytes */
    uint32_t creation_time;         /**< Creation timestamp */
    uint32_t last_write_time;       /**< Last write timestamp */
    uint32_t last_access_time;      /**< Last access timestamp */
} uft_fatx_dirent_t;

/**
 * @brief FATX volume context
 */
typedef struct {
    uft_fatx_platform_t platform;
    
    /* Volume geometry */
    uint64_t volume_offset;         /**< Start offset in image */
    uint64_t volume_size;           /**< Total volume size */
    uint32_t sectors_per_cluster;
    uint32_t cluster_size;          /**< In bytes */
    uint32_t fat_offset;            /**< FAT table offset */
    uint32_t fat_size;              /**< FAT table size */
    uint32_t data_offset;           /**< Data area offset */
    uint32_t total_clusters;
    uint32_t root_cluster;
    
    /* FAT type */
    bool is_fat16;                  /**< true = FAT16, false = FAT32 */
    
    /* Volume info */
    uint32_t volume_id;
} uft_fatx_volume_t;

/*===========================================================================
 * Xbox Partition Layout
 *===========================================================================*/

/** Original Xbox partition table (fixed layout) */
typedef struct {
    const char *name;
    uint64_t offset;
    uint64_t size;
} uft_xbox_partition_t;

static const uft_xbox_partition_t uft_xbox_partitions[] = {
    { "X (Cache)",   0x00080000,    0x2EE00000 },  /* 750 MB */
    { "Y (Cache)",   0x2EE80000,    0x2EE00000 },  /* 750 MB */
    { "Z (Cache)",   0x5DC80000,    0x2EE00000 },  /* 750 MB */
    { "C (System)",  0x8CA80000,    0x1F400000 },  /* 500 MB */
    { "E (Data)",    0xABE80000,    0x131F00000 }, /* Rest of disk */
    { NULL, 0, 0 }
};

/** Xbox 360 DEVKIT partition offsets */
#define UFT_X360_SYSTEMAUX_OFFSET   0x00080000
#define UFT_X360_SYSTEM_OFFSET      0x10080000
#define UFT_X360_DATA_OFFSET        0x20080000

/*===========================================================================
 * Timestamp Conversion
 *===========================================================================*/

/**
 * @brief Decode Xbox timestamp to components
 */
static inline void uft_fatx_decode_time(uft_fatx_time_t ts,
                                         int *year, int *month, int *day,
                                         int *hour, int *minute, int *second)
{
    *second = (ts & 0x1F) * 2;
    *minute = (ts >> 5) & 0x3F;
    *hour   = (ts >> 11) & 0x1F;
    *day    = (ts >> 16) & 0x1F;
    *month  = (ts >> 21) & 0x0F;
    *year   = ((ts >> 25) & 0x7F) + 2000;
}

/**
 * @brief Decode Xbox 360 timestamp to components
 */
static inline void uft_fatx360_decode_time(uft_fatx360_time_t ts,
                                            int *year, int *month, int *day,
                                            int *hour, int *minute, int *second)
{
    *second = (ts & 0x1F) * 2;
    *minute = (ts >> 5) & 0x3F;
    *hour   = (ts >> 11) & 0x1F;
    *day    = (ts >> 16) & 0x1F;
    *month  = (ts >> 21) & 0x0F;
    *year   = ((ts >> 25) & 0x7F) + 1980;
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Check if buffer contains FATX signature
 */
static inline bool uft_fatx_check_signature(const uint8_t *data)
{
    return (data[0] == 'F' && data[1] == 'A' && 
            data[2] == 'T' && data[3] == 'X');
}

/**
 * @brief Initialize FATX volume from header
 */
int uft_fatx_init(uft_fatx_volume_t *vol, const uint8_t *header,
                  uint64_t volume_offset, uint64_t volume_size,
                  uft_fatx_platform_t platform);

/**
 * @brief Read FAT entry
 */
uint32_t uft_fatx_read_fat(const uft_fatx_volume_t *vol, 
                           const uint8_t *fat_data, uint32_t cluster);

/**
 * @brief Check if cluster is end-of-chain
 */
bool uft_fatx_is_last_cluster(const uft_fatx_volume_t *vol, uint32_t cluster);

/**
 * @brief Check if cluster is available
 */
bool uft_fatx_is_free_cluster(const uft_fatx_volume_t *vol, uint32_t cluster);

/**
 * @brief Calculate offset for cluster data
 */
uint64_t uft_fatx_cluster_offset(const uft_fatx_volume_t *vol, uint32_t cluster);

/**
 * @brief Parse directory entry
 */
int uft_fatx_parse_dirent(uft_fatx_dirent_t *dirent, const uint8_t *data,
                          uft_fatx_platform_t platform);

/**
 * @brief Check if directory entry is valid
 */
bool uft_fatx_dirent_is_valid(const uft_fatx_dirent_t *dirent);

/**
 * @brief Check if directory entry is deleted
 */
static inline bool uft_fatx_dirent_is_deleted(const uft_fatx_dirent_t *dirent)
{
    return dirent->filename_length == UFT_FATX_DIRENT_DELETED;
}

/**
 * @brief Check if directory entry is a directory
 */
static inline bool uft_fatx_dirent_is_dir(const uft_fatx_dirent_t *dirent)
{
    return (dirent->attributes & UFT_FATX_ATTR_DIRECTORY) != 0;
}

/**
 * @brief Get filename from directory entry
 * @param dirent Directory entry
 * @param buf Output buffer (at least 43 bytes)
 * @return Filename length
 */
int uft_fatx_get_filename(const uft_fatx_dirent_t *dirent, char *buf);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FATX_H */
