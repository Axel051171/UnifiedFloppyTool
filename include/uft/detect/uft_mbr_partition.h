/**
 * @file uft_mbr_partition.h
 * @brief MBR/DOS Partition Table Support
 * 
 * Based on util-linux libblkid (LGPL)
 * 
 * Provides complete MBR partition table handling including:
 * - Partition entry parsing
 * - CHS/LBA conversion
 * - Extended partition traversal
 * - Partition type identification
 */

#ifndef UFT_MBR_PARTITION_H
#define UFT_MBR_PARTITION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MBR Layout Constants
 *===========================================================================*/

#define UFT_MBR_SECTOR_SIZE     512
#define UFT_MBR_PT_OFFSET       0x1BE   /**< Partition table offset */
#define UFT_MBR_MAGIC_OFFSET    510
#define UFT_MBR_MAGIC_0         0x55
#define UFT_MBR_MAGIC_1         0xAA
#define UFT_MBR_DISKID_OFFSET   440     /**< Optional disk signature */
#define UFT_MBR_MAX_PARTITIONS  4

/*===========================================================================
 * Partition Entry Structure
 *===========================================================================*/

/**
 * @brief MBR partition entry (16 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  boot_ind;      /**< 0x80 = bootable, 0x00 = not bootable */
    uint8_t  start_head;    /**< Starting head */
    uint8_t  start_sector;  /**< Starting sector (bits 0-5), cyl high (bits 6-7) */
    uint8_t  start_cyl;     /**< Starting cylinder (low 8 bits) */
    uint8_t  sys_ind;       /**< Partition type (system indicator) */
    uint8_t  end_head;      /**< Ending head */
    uint8_t  end_sector;    /**< Ending sector (bits 0-5), cyl high (bits 6-7) */
    uint8_t  end_cyl;       /**< Ending cylinder (low 8 bits) */
    uint8_t  start_lba[4];  /**< Starting LBA (little-endian) */
    uint8_t  size_lba[4];   /**< Size in sectors (little-endian) */
} uft_mbr_entry_t;

/*===========================================================================
 * Partition Types (System Indicator)
 *===========================================================================*/

typedef enum {
    /* Empty */
    UFT_PT_EMPTY                = 0x00,
    
    /* FAT variants (common on floppies) */
    UFT_PT_FAT12                = 0x01,
    UFT_PT_FAT16_SMALL          = 0x04,  /**< FAT16 < 32MB */
    UFT_PT_FAT16                = 0x06,  /**< FAT16 >= 32MB */
    UFT_PT_FAT32                = 0x0B,
    UFT_PT_FAT32_LBA            = 0x0C,
    UFT_PT_FAT16_LBA            = 0x0E,
    
    /* Extended partitions */
    UFT_PT_EXTENDED             = 0x05,
    UFT_PT_EXTENDED_LBA         = 0x0F,
    UFT_PT_LINUX_EXTENDED       = 0x85,
    
    /* NTFS/HPFS */
    UFT_PT_NTFS                 = 0x07,
    
    /* Hidden FAT variants */
    UFT_PT_FAT12_HIDDEN         = 0x11,
    UFT_PT_FAT16_SMALL_HIDDEN   = 0x14,
    UFT_PT_FAT16_HIDDEN         = 0x16,
    UFT_PT_FAT32_HIDDEN         = 0x1B,
    UFT_PT_FAT32_LBA_HIDDEN     = 0x1C,
    UFT_PT_FAT16_LBA_HIDDEN     = 0x1E,
    
    /* Unix/Linux */
    UFT_PT_LINUX_SWAP           = 0x82,
    UFT_PT_LINUX                = 0x83,
    UFT_PT_LINUX_LVM            = 0x8E,
    UFT_PT_LINUX_RAID           = 0xFD,
    
    /* BSD */
    UFT_PT_FREEBSD              = 0xA5,
    UFT_PT_OPENBSD              = 0xA6,
    UFT_PT_NETBSD               = 0xA9,
    
    /* Minix (common on floppy) */
    UFT_PT_MINIX_OLD            = 0x80,
    UFT_PT_MINIX                = 0x81,
    
    /* CP/M (floppy era) */
    UFT_PT_CPM                  = 0x52,
    UFT_PT_CPM_CTOS             = 0xDB,
    
    /* Other legacy */
    UFT_PT_XENIX_ROOT           = 0x02,
    UFT_PT_XENIX_USR            = 0x03,
    UFT_PT_QNX                  = 0x4D,
    UFT_PT_VENIX                = 0x40,
    
    /* Special */
    UFT_PT_GPT_PROTECTIVE       = 0xEE,
    UFT_PT_EFI_SYSTEM           = 0xEF
} uft_partition_type_t;

/*===========================================================================
 * Helper Functions - Little Endian Access
 *===========================================================================*/

/**
 * @brief Read 32-bit little-endian value from unaligned bytes
 */
static inline uint32_t uft_mbr_get_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/**
 * @brief Write 32-bit little-endian value to unaligned bytes
 */
static inline void uft_mbr_set_le32(uint8_t *p, uint32_t val)
{
    p[0] = val & 0xFF;
    p[1] = (val >> 8) & 0xFF;
    p[2] = (val >> 16) & 0xFF;
    p[3] = (val >> 24) & 0xFF;
}

/*===========================================================================
 * Partition Entry Access
 *===========================================================================*/

/**
 * @brief Get partition entry from MBR sector
 */
static inline uft_mbr_entry_t *uft_mbr_get_entry(uint8_t *mbr, int index)
{
    if (index < 0 || index >= UFT_MBR_MAX_PARTITIONS) return NULL;
    return (uft_mbr_entry_t *)(mbr + UFT_MBR_PT_OFFSET + 
                               (index * sizeof(uft_mbr_entry_t)));
}

/**
 * @brief Get partition start LBA
 */
static inline uint32_t uft_mbr_entry_start(const uft_mbr_entry_t *e)
{
    return uft_mbr_get_le32(e->start_lba);
}

/**
 * @brief Get partition size in sectors
 */
static inline uint32_t uft_mbr_entry_size(const uft_mbr_entry_t *e)
{
    return uft_mbr_get_le32(e->size_lba);
}

/**
 * @brief Set partition start LBA
 */
static inline void uft_mbr_entry_set_start(uft_mbr_entry_t *e, uint32_t lba)
{
    uft_mbr_set_le32(e->start_lba, lba);
}

/**
 * @brief Set partition size in sectors
 */
static inline void uft_mbr_entry_set_size(uft_mbr_entry_t *e, uint32_t sectors)
{
    uft_mbr_set_le32(e->size_lba, sectors);
}

/*===========================================================================
 * CHS Extraction/Encoding
 *===========================================================================*/

/**
 * @brief CHS address
 */
typedef struct {
    uint16_t cylinder;  /**< 0-1023 */
    uint8_t  head;      /**< 0-255 */
    uint8_t  sector;    /**< 1-63 */
} uft_chs_t;

/**
 * @brief Extract CHS from partition entry start
 */
static inline void uft_mbr_get_start_chs(const uft_mbr_entry_t *e, uft_chs_t *chs)
{
    chs->head = e->start_head;
    chs->sector = e->start_sector & 0x3F;
    chs->cylinder = e->start_cyl | ((e->start_sector & 0xC0) << 2);
}

/**
 * @brief Extract CHS from partition entry end
 */
static inline void uft_mbr_get_end_chs(const uft_mbr_entry_t *e, uft_chs_t *chs)
{
    chs->head = e->end_head;
    chs->sector = e->end_sector & 0x3F;
    chs->cylinder = e->end_cyl | ((e->end_sector & 0xC0) << 2);
}

/**
 * @brief Set CHS in partition entry start
 */
static inline void uft_mbr_set_start_chs(uft_mbr_entry_t *e, const uft_chs_t *chs)
{
    e->start_head = chs->head;
    e->start_sector = (chs->sector & 0x3F) | ((chs->cylinder >> 2) & 0xC0);
    e->start_cyl = chs->cylinder & 0xFF;
}

/**
 * @brief Convert LBA to CHS
 * @param lba Logical Block Address
 * @param geom Disk geometry (heads, sectors per track)
 * @param chs Output CHS address
 */
static inline void uft_lba_to_chs(uint32_t lba, uint8_t heads, 
                                   uint8_t sectors_per_track, uft_chs_t *chs)
{
    uint32_t temp = lba / sectors_per_track;
    chs->sector = (lba % sectors_per_track) + 1;
    chs->head = temp % heads;
    chs->cylinder = temp / heads;
    
    /* Clamp to maximum CHS values */
    if (chs->cylinder > 1023) {
        chs->cylinder = 1023;
        chs->head = heads - 1;
        chs->sector = sectors_per_track;
    }
}

/**
 * @brief Convert CHS to LBA
 */
static inline uint32_t uft_chs_to_lba(const uft_chs_t *chs, uint8_t heads,
                                       uint8_t sectors_per_track)
{
    return (chs->cylinder * heads + chs->head) * sectors_per_track + 
           (chs->sector - 1);
}

/*===========================================================================
 * MBR Validation
 *===========================================================================*/

/**
 * @brief Check if MBR has valid signature
 */
static inline bool uft_mbr_is_valid(const uint8_t *mbr)
{
    return mbr[510] == UFT_MBR_MAGIC_0 && mbr[511] == UFT_MBR_MAGIC_1;
}

/**
 * @brief Set MBR signature
 */
static inline void uft_mbr_set_signature(uint8_t *mbr)
{
    mbr[510] = UFT_MBR_MAGIC_0;
    mbr[511] = UFT_MBR_MAGIC_1;
}

/**
 * @brief Get disk signature (optional 4-byte ID at offset 440)
 */
static inline uint32_t uft_mbr_get_disk_id(const uint8_t *mbr)
{
    return uft_mbr_get_le32(&mbr[UFT_MBR_DISKID_OFFSET]);
}

/**
 * @brief Set disk signature
 */
static inline void uft_mbr_set_disk_id(uint8_t *mbr, uint32_t id)
{
    uft_mbr_set_le32(&mbr[UFT_MBR_DISKID_OFFSET], id);
}

/*===========================================================================
 * Partition Type Helpers
 *===========================================================================*/

/**
 * @brief Check if partition type is extended
 */
static inline bool uft_pt_is_extended(uint8_t type)
{
    return type == UFT_PT_EXTENDED || 
           type == UFT_PT_EXTENDED_LBA ||
           type == UFT_PT_LINUX_EXTENDED;
}

/**
 * @brief Check if partition type is FAT
 */
static inline bool uft_pt_is_fat(uint8_t type)
{
    return type == UFT_PT_FAT12 ||
           type == UFT_PT_FAT16_SMALL ||
           type == UFT_PT_FAT16 ||
           type == UFT_PT_FAT32 ||
           type == UFT_PT_FAT32_LBA ||
           type == UFT_PT_FAT16_LBA;
}

/**
 * @brief Check if partition is bootable
 */
static inline bool uft_mbr_entry_is_bootable(const uft_mbr_entry_t *e)
{
    return e->boot_ind == 0x80;
}

/**
 * @brief Get partition type name
 */
const char *uft_partition_type_name(uint8_t type);

/*===========================================================================
 * High-Level API
 *===========================================================================*/

/**
 * @brief Parsed partition info
 */
typedef struct {
    uint8_t  index;         /**< Partition number (1-4 for primary) */
    uint8_t  type;          /**< System indicator */
    bool     bootable;      /**< Boot flag set */
    bool     extended;      /**< Is extended partition */
    uint32_t start_lba;     /**< Start sector (LBA) */
    uint32_t size_sectors;  /**< Size in sectors */
    uft_chs_t start_chs;    /**< Start CHS */
    uft_chs_t end_chs;      /**< End CHS */
} uft_partition_info_t;

/**
 * @brief Parse MBR and extract partition info
 * @param mbr MBR sector (512 bytes)
 * @param parts Output array (4 entries)
 * @return Number of non-empty partitions, or -1 if invalid MBR
 */
int uft_mbr_parse(const uint8_t *mbr, uft_partition_info_t parts[4]);

/**
 * @brief Create empty MBR
 * @param mbr Output buffer (512 bytes)
 * @param disk_id Optional disk signature (0 for none)
 */
void uft_mbr_init(uint8_t *mbr, uint32_t disk_id);

/**
 * @brief Add partition to MBR
 * @param mbr MBR buffer
 * @param index Partition index (0-3)
 * @param type Partition type
 * @param start_lba Start sector
 * @param size_sectors Size in sectors
 * @param bootable Set boot flag
 * @param heads Disk geometry for CHS
 * @param sectors_per_track Disk geometry for CHS
 * @return 0 on success, -1 on error
 */
int uft_mbr_add_partition(uint8_t *mbr, int index, uint8_t type,
                          uint32_t start_lba, uint32_t size_sectors,
                          bool bootable, uint8_t heads, uint8_t sectors_per_track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MBR_PARTITION_H */
