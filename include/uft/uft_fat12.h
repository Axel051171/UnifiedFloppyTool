/**
 * @file uft_fat12.h
 * @brief UnifiedFloppyTool - FAT12 Filesystem Support
 * @version 3.1.4.007
 *
 * Complete FAT12 filesystem implementation for floppy disk images.
 * Supports reading, writing, and modifying FAT12 filesystems.
 *
 * Sources analyzed:
 * - DiskImageTool FAT12.vb, BiosParameterBlock.vb
 * - hp150_fat.py (HP-150 specific)
 *
 * Features:
 * - FAT12 decode/encode
 * - BPB parsing and validation
 * - Cluster chain management
 * - Bad sector tracking
 */

#ifndef UFT_FAT12_H
#define UFT_FAT12_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * FAT12 Constants
 *============================================================================*/

/** FAT12 cluster values */
#define UFT_FAT12_FREE              0x000
#define UFT_FAT12_RESERVED_START    0xFF0
#define UFT_FAT12_RESERVED_END      0xFF6
#define UFT_FAT12_BAD_CLUSTER       0xFF7
#define UFT_FAT12_LAST_START        0xFF8
#define UFT_FAT12_LAST_END          0xFFF

/** Valid FAT12 media descriptors */
static const uint8_t uft_fat12_valid_media[] = {
    0xF0,  /* 3.5" 1.44M or 2.88M, or generic */
    0xF8,  /* Hard disk */
    0xF9,  /* 3.5" 720K or 5.25" 1.2M */
    0xFA,  /* 5.25" 320K single-sided */
    0xFB,  /* 5.25" 640K double-sided */
    0xFC,  /* 5.25" 180K single-sided */
    0xFD,  /* 5.25" 360K or 8" 250K */
    0xFE,  /* 5.25" 160K or 8" 500K */
    0xFF   /* 5.25" 320K double-sided */
};
#define UFT_FAT12_MEDIA_COUNT 9

/** Directory entry attributes */
#define UFT_FAT_ATTR_READONLY       0x01
#define UFT_FAT_ATTR_HIDDEN         0x02
#define UFT_FAT_ATTR_SYSTEM         0x04
#define UFT_FAT_ATTR_VOLUME_LABEL   0x08
#define UFT_FAT_ATTR_DIRECTORY      0x10
#define UFT_FAT_ATTR_ARCHIVE        0x20
#define UFT_FAT_ATTR_LFN            0x0F  /* Long filename entry */

/** Directory entry sizes */
#define UFT_FAT_DIR_ENTRY_SIZE      32

/** Maximum values */
#define UFT_FAT12_MAX_CLUSTERS      4084  /* 0x0FF4 */

/*============================================================================
 * BPB (BIOS Parameter Block)
 *============================================================================*/

/**
 * @brief DOS 2.0 BPB (13 bytes at offset 0x0B)
 */
typedef struct __attribute__((packed)) {
    uint16_t bytes_per_sector;        /**< 512, 1024, 2048, 4096 */
    uint8_t  sectors_per_cluster;     /**< 1, 2, 4, 8, 16, 32, 64, 128 */
    uint16_t reserved_sectors;        /**< Usually 1 */
    uint8_t  fat_count;               /**< Usually 2 */
    uint16_t root_entry_count;        /**< Usually 224 or 112 */
    uint16_t total_sectors_16;        /**< 0 for FAT32 */
    uint8_t  media_descriptor;        /**< 0xF0, 0xF8-0xFF */
    uint16_t sectors_per_fat;         /**< FAT12/16 only */
} uft_bpb_dos20_t;

/**
 * @brief DOS 3.31 BPB extension (8 additional bytes)
 */
typedef struct __attribute__((packed)) {
    uint16_t sectors_per_track;       /**< Sectors per track */
    uint16_t head_count;              /**< Number of heads */
    uint32_t hidden_sectors;          /**< Hidden sectors before partition */
} uft_bpb_dos331_t;

/**
 * @brief DOS 4.0 BPB extension (for disks > 32MB)
 */
typedef struct __attribute__((packed)) {
    uint32_t total_sectors_32;        /**< Total sectors if > 65535 */
} uft_bpb_dos40_t;

/**
 * @brief Boot sector with BPB
 */
typedef struct __attribute__((packed)) {
    uint8_t  jmp_boot[3];             /**< Jump instruction */
    char     oem_name[8];             /**< OEM name */
    uft_bpb_dos20_t bpb;              /**< DOS 2.0 BPB */
    uft_bpb_dos331_t bpb_ext;         /**< DOS 3.31 extension */
    uft_bpb_dos40_t bpb_ext2;         /**< DOS 4.0 extension */
    uint8_t  drive_number;            /**< BIOS drive number */
    uint8_t  reserved1;               /**< Reserved */
    uint8_t  boot_signature;          /**< 0x29 for extended boot record */
    uint32_t volume_serial;           /**< Volume serial number */
    char     volume_label[11];        /**< Volume label */
    char     fs_type[8];              /**< "FAT12   " */
} uft_boot_sector_t;

/*============================================================================
 * FAT12 Runtime Structures
 *============================================================================*/

/**
 * @brief Parsed BPB information
 */
typedef struct {
    /* Basic geometry */
    uint16_t bytes_per_sector;
    uint8_t  sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t  fat_count;
    uint16_t root_entry_count;
    uint32_t total_sectors;
    uint8_t  media_descriptor;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_track;
    uint16_t head_count;
    uint32_t hidden_sectors;
    
    /* Derived values */
    uint32_t fat_start_sector;        /**< First FAT sector */
    uint32_t root_dir_start_sector;   /**< First root directory sector */
    uint32_t root_dir_sectors;        /**< Root directory sector count */
    uint32_t data_start_sector;       /**< First data sector (cluster 2) */
    uint32_t data_sectors;            /**< Data sector count */
    uint32_t cluster_count;           /**< Total clusters */
    uint32_t bytes_per_cluster;       /**< Bytes per cluster */
    
    bool     is_valid;                /**< BPB validity flag */
} uft_bpb_t;

/**
 * @brief Directory entry
 */
typedef struct __attribute__((packed)) {
    char     name[8];                 /**< Filename (space-padded) */
    char     ext[3];                  /**< Extension (space-padded) */
    uint8_t  attributes;              /**< File attributes */
    uint8_t  reserved;                /**< Reserved (NT: lowercase flags) */
    uint8_t  create_time_tenth;       /**< Creation time (10ms units) */
    uint16_t create_time;             /**< Creation time */
    uint16_t create_date;             /**< Creation date */
    uint16_t access_date;             /**< Last access date */
    uint16_t cluster_high;            /**< High word of cluster (FAT32) */
    uint16_t modify_time;             /**< Last modification time */
    uint16_t modify_date;             /**< Last modification date */
    uint16_t cluster_low;             /**< Starting cluster */
    uint32_t file_size;               /**< File size in bytes */
} uft_dir_entry_t;

/**
 * @brief FAT12 table context
 */
typedef struct {
    uint16_t *entries;                /**< FAT entries (decoded) */
    uint16_t entry_count;             /**< Number of entries */
    uint8_t  *raw_data;               /**< Raw FAT data */
    size_t   raw_size;                /**< Raw data size */
    uint8_t  fat_index;               /**< Which FAT copy (0 or 1) */
    
    /* Cluster sets */
    uint16_t *free_clusters;          /**< Free cluster list */
    size_t   free_count;
    uint16_t *bad_clusters;           /**< Bad cluster list */
    size_t   bad_count;
    uint16_t *allocated_clusters;     /**< Allocated cluster list */
    size_t   allocated_count;
} uft_fat12_t;

/**
 * @brief FAT12 filesystem context
 */
typedef struct {
    uft_bpb_t       bpb;              /**< Parsed BPB */
    uft_fat12_t     fat[2];           /**< FAT tables (up to 2 copies) */
    uft_dir_entry_t *root_dir;        /**< Root directory entries */
    uint16_t        root_entry_count;
    
    uint8_t         *image_data;      /**< Disk image data */
    size_t          image_size;       /**< Image size */
} uft_fat12_fs_t;

/*============================================================================
 * FAT12 Decode/Encode
 *============================================================================*/

/**
 * @brief Decode FAT12 table
 * @param data Raw FAT data
 * @param data_size Data size
 * @param entries Output entries array (caller allocates)
 * @param max_entries Maximum entries
 * @return Number of entries decoded
 *
 * FAT12 encoding: each 3 bytes contain 2 12-bit entries
 * Bytes: AB CD EF
 * Entry 0: DAB (low nibble of C + AB)
 * Entry 1: EFC (EF + high nibble of C)
 */
static inline size_t uft_fat12_decode(const uint8_t *data, size_t data_size,
                                       uint16_t *entries, size_t max_entries)
{
    size_t count = 0;
    size_t i = 0;
    
    while (i + 2 < data_size && count + 1 < max_entries) {
        /* Even entry: low byte + low nibble of middle byte */
        entries[count++] = data[i] | ((data[i+1] & 0x0F) << 8);
        
        /* Odd entry: high nibble of middle byte + high byte */
        if (count < max_entries) {
            entries[count++] = (data[i+1] >> 4) | (data[i+2] << 4);
        }
        
        i += 3;
    }
    
    return count;
}

/**
 * @brief Encode FAT12 table
 * @param entries FAT entries
 * @param entry_count Number of entries
 * @param data Output data buffer
 * @param data_size Buffer size
 * @return Bytes written
 */
static inline size_t uft_fat12_encode(const uint16_t *entries, size_t entry_count,
                                       uint8_t *data, size_t data_size)
{
    size_t bytes = 0;
    size_t i = 0;
    
    /* Round up to even count */
    size_t padded_count = (entry_count + 1) & ~1;
    
    while (i < padded_count && bytes + 3 <= data_size) {
        uint16_t e0 = (i < entry_count) ? entries[i] : 0;
        uint16_t e1 = (i + 1 < entry_count) ? entries[i + 1] : 0;
        
        data[bytes++] = e0 & 0xFF;
        data[bytes++] = ((e0 >> 8) & 0x0F) | ((e1 & 0x0F) << 4);
        data[bytes++] = e1 >> 4;
        
        i += 2;
    }
    
    return bytes;
}

/*============================================================================
 * BPB Parsing
 *============================================================================*/

/**
 * @brief Parse BPB from boot sector
 * @param boot_sector Boot sector data (512 bytes minimum)
 * @param bpb Output parsed BPB
 * @return true if valid
 */
bool uft_bpb_parse(const uint8_t *boot_sector, uft_bpb_t *bpb);

/**
 * @brief Validate media descriptor
 */
static inline bool uft_bpb_valid_media(uint8_t media)
{
    for (size_t i = 0; i < UFT_FAT12_MEDIA_COUNT; i++) {
        if (uft_fat12_valid_media[i] == media) return true;
    }
    return false;
}

/**
 * @brief Convert cluster number to sector number
 */
static inline uint32_t uft_bpb_cluster_to_sector(const uft_bpb_t *bpb, uint16_t cluster)
{
    if (cluster < 2) return 0;
    return bpb->data_start_sector + (cluster - 2) * bpb->sectors_per_cluster;
}

/**
 * @brief Convert cluster number to byte offset
 */
static inline uint32_t uft_bpb_cluster_to_offset(const uft_bpb_t *bpb, uint16_t cluster)
{
    return uft_bpb_cluster_to_sector(bpb, cluster) * bpb->bytes_per_sector;
}

/**
 * @brief Convert sector number to byte offset
 */
static inline uint32_t uft_bpb_sector_to_offset(const uft_bpb_t *bpb, uint32_t sector)
{
    return sector * bpb->bytes_per_sector;
}

/*============================================================================
 * FAT12 Operations
 *============================================================================*/

/**
 * @brief Get FAT entry value
 */
static inline uint16_t uft_fat12_get_entry(const uft_fat12_t *fat, uint16_t cluster)
{
    if (cluster >= fat->entry_count) return UFT_FAT12_BAD_CLUSTER;
    return fat->entries[cluster];
}

/**
 * @brief Set FAT entry value
 */
static inline void uft_fat12_set_entry(uft_fat12_t *fat, uint16_t cluster, uint16_t value)
{
    if (cluster < fat->entry_count) {
        fat->entries[cluster] = value & 0x0FFF;
    }
}

/**
 * @brief Check if cluster is free
 */
static inline bool uft_fat12_is_free(uint16_t value)
{
    return value == UFT_FAT12_FREE;
}

/**
 * @brief Check if cluster is bad
 */
static inline bool uft_fat12_is_bad(uint16_t value)
{
    return value == UFT_FAT12_BAD_CLUSTER;
}

/**
 * @brief Check if cluster is last in chain
 */
static inline bool uft_fat12_is_last(uint16_t value)
{
    return value >= UFT_FAT12_LAST_START && value <= UFT_FAT12_LAST_END;
}

/**
 * @brief Check if cluster is reserved
 */
static inline bool uft_fat12_is_reserved(uint16_t value)
{
    return (value >= UFT_FAT12_RESERVED_START && value <= UFT_FAT12_RESERVED_END)
           || value == 1;
}

/**
 * @brief Check if cluster is allocated (in use)
 */
static inline bool uft_fat12_is_allocated(uint16_t value, uint16_t max_cluster)
{
    return (value >= 2 && value <= max_cluster) || uft_fat12_is_last(value);
}

/*============================================================================
 * Cluster Chain Operations
 *============================================================================*/

/**
 * @brief Get cluster chain
 * @param fat FAT table
 * @param start_cluster Starting cluster
 * @param chain Output chain array
 * @param max_length Maximum chain length
 * @return Chain length
 */
size_t uft_fat12_get_chain(const uft_fat12_t *fat, uint16_t start_cluster,
                           uint16_t *chain, size_t max_length);

/**
 * @brief Allocate cluster chain
 * @param fat FAT table
 * @param count Number of clusters needed
 * @param chain Output allocated clusters
 * @return Clusters allocated (may be less than requested)
 */
size_t uft_fat12_alloc_chain(uft_fat12_t *fat, size_t count, uint16_t *chain);

/**
 * @brief Free cluster chain
 * @param fat FAT table
 * @param start_cluster Starting cluster
 * @return Clusters freed
 */
size_t uft_fat12_free_chain(uft_fat12_t *fat, uint16_t start_cluster);

/*============================================================================
 * Directory Operations
 *============================================================================*/

/**
 * @brief Parse 8.3 filename from directory entry
 */
static inline void uft_fat_parse_filename(const uft_dir_entry_t *entry,
                                           char *filename, size_t size)
{
    if (size < 13) return;
    
    /* Copy name (trim trailing spaces) */
    int len = 8;
    while (len > 0 && entry->name[len-1] == ' ') len--;
    memcpy(filename, entry->name, len);
    
    /* Add extension if present */
    int ext_len = 3;
    while (ext_len > 0 && entry->ext[ext_len-1] == ' ') ext_len--;
    
    if (ext_len > 0) {
        filename[len++] = '.';
        memcpy(filename + len, entry->ext, ext_len);
        len += ext_len;
    }
    
    filename[len] = '\0';
}

/**
 * @brief Check if directory entry is deleted
 */
static inline bool uft_fat_is_deleted(const uft_dir_entry_t *entry)
{
    return entry->name[0] == 0xE5;
}

/**
 * @brief Check if directory entry is end marker
 */
static inline bool uft_fat_is_end(const uft_dir_entry_t *entry)
{
    return entry->name[0] == 0x00;
}

/**
 * @brief Check if directory entry is volume label
 */
static inline bool uft_fat_is_volume_label(const uft_dir_entry_t *entry)
{
    return (entry->attributes & UFT_FAT_ATTR_VOLUME_LABEL) != 0;
}

/**
 * @brief Check if directory entry is subdirectory
 */
static inline bool uft_fat_is_directory(const uft_dir_entry_t *entry)
{
    return (entry->attributes & UFT_FAT_ATTR_DIRECTORY) != 0;
}

/**
 * @brief Check if directory entry is long filename
 */
static inline bool uft_fat_is_lfn(const uft_dir_entry_t *entry)
{
    return (entry->attributes & UFT_FAT_ATTR_LFN) == UFT_FAT_ATTR_LFN;
}

/*============================================================================
 * Filesystem Operations
 *============================================================================*/

/**
 * @brief Initialize FAT12 filesystem from image data
 */
int uft_fat12_fs_init(uft_fat12_fs_t *fs, uint8_t *image_data, size_t image_size);

/**
 * @brief Get free space in bytes
 */
uint32_t uft_fat12_fs_free_space(const uft_fat12_fs_t *fs);

/**
 * @brief Find file in root directory
 * @param fs Filesystem context
 * @param filename 8.3 filename (uppercase)
 * @return Directory entry index, or -1 if not found
 */
int uft_fat12_fs_find_file(const uft_fat12_fs_t *fs, const char *filename);

/**
 * @brief Read file data
 * @param fs Filesystem context
 * @param entry Directory entry
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes read
 */
size_t uft_fat12_fs_read_file(const uft_fat12_fs_t *fs, 
                              const uft_dir_entry_t *entry,
                              uint8_t *buffer, size_t buffer_size);

/**
 * @brief Cleanup filesystem context
 */
void uft_fat12_fs_free(uft_fat12_fs_t *fs);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT12_H */
