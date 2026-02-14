/**
 * @file uft_dmf.h
 * @brief DMF (Distribution Media Format) and Superformat Support
 * @version 3.1.4.002
 *
 * Support for high-capacity PC floppy formats:
 * - DMF (1.68MB) - Microsoft Distribution Media Format
 * - XDF (1.86MB) - IBM XDF format
 * - 2M (1.80MB) - 2M format
 * - Custom superformats (up to 1.72MB)
 *
 * Based on floppinux DMF documentation by dscp46
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_DMF_H
#define UFT_DMF_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Format Constants
 *============================================================================*/

/** Standard 1.44MB HD format */
#define UFT_FMT_HD_144      0

/** DMF 1.68MB format */
#define UFT_FMT_DMF         1

/** XDF 1.86MB format */
#define UFT_FMT_XDF         2

/** 2M 1.80MB format */
#define UFT_FMT_2M          3

/** Custom superformat */
#define UFT_FMT_SUPER       4

/*============================================================================
 * Geometry Structures
 *============================================================================*/

/** Floppy geometry */
typedef struct {
    uint16_t    cylinders;      /**< Number of cylinders */
    uint8_t     heads;          /**< Number of heads */
    uint8_t     sectors;        /**< Sectors per track */
    uint16_t    sector_size;    /**< Bytes per sector */
    uint8_t     gap3;           /**< Gap3 length */
    uint8_t     interleave;     /**< Sector interleave */
    uint8_t     skew;           /**< Track skew */
    
    /* FAT parameters */
    uint8_t     cluster_size;   /**< Sectors per cluster */
    uint16_t    root_entries;   /**< Root directory entries */
    uint8_t     media_byte;     /**< Media descriptor */
} uft_floppy_geometry_t;

/*============================================================================
 * Predefined Geometries
 *============================================================================*/

/** Standard 1.44MB (80/2/18/512) */
extern const uft_floppy_geometry_t UFT_GEOM_HD_144;

/** DMF format (80/2/21/512) - 1.68MB */
extern const uft_floppy_geometry_t UFT_GEOM_DMF;

/** Maximum bootable format (83/2/21/512) - 1.72MB */
extern const uft_floppy_geometry_t UFT_GEOM_SUPER_1743;

/** 720KB DD (80/2/9/512) */
extern const uft_floppy_geometry_t UFT_GEOM_DD_720;

/** 360KB DD (40/2/9/512) */
extern const uft_floppy_geometry_t UFT_GEOM_DD_360;

/** 1.2MB HD 5.25" (80/2/15/512) */
extern const uft_floppy_geometry_t UFT_GEOM_HD_120;

/*============================================================================
 * Linux Floppy Device Nodes
 *============================================================================*/

/**
 * @brief Linux floppy minor device numbers for superformats
 *
 * These correspond to /dev/fd0uXXXX device nodes.
 */
typedef enum {
    UFT_FDMINOR_360   = 4,    /**< 360KB 5.25" */
    UFT_FDMINOR_720   = 16,   /**< 720KB 3.5" */
    UFT_FDMINOR_1200  = 8,    /**< 1.2MB 5.25" */
    UFT_FDMINOR_1440  = 28,   /**< 1.44MB 3.5" */
    UFT_FDMINOR_1680  = 44,   /**< 1.68MB DMF */
    UFT_FDMINOR_1722  = 60,   /**< 1.72MB (82/2/21) */
    UFT_FDMINOR_1743  = 76,   /**< 1.74MB (83/2/21) */
    UFT_FDMINOR_1760  = 96,   /**< 1.76MB (80/2/22) */
    UFT_FDMINOR_1840  = 116,  /**< 1.84MB (80/2/23) */
} uft_fd_minor_t;

/*============================================================================
 * API Functions
 *============================================================================*/

/**
 * @brief Get geometry for format type
 * @param type Format type (UFT_FMT_*)
 * @return Geometry pointer or NULL
 */
const uft_floppy_geometry_t *uft_dmf_get_geometry(int type);

/**
 * @brief Calculate capacity for geometry
 * @param geom Geometry
 * @return Capacity in bytes
 */
uint32_t uft_dmf_capacity(const uft_floppy_geometry_t *geom);

/**
 * @brief Detect format from image size
 * @param size Image file size
 * @return Best matching geometry or NULL
 */
const uft_floppy_geometry_t *uft_dmf_detect_by_size(size_t size);

/**
 * @brief Detect format from boot sector
 * @param boot_sector First 512 bytes of image
 * @return Detected geometry or NULL
 */
const uft_floppy_geometry_t *uft_dmf_detect_by_bpb(const uint8_t *boot_sector);

/**
 * @brief Check if geometry is bootable
 * 
 * Some superformats won't boot on all BIOSes.
 *
 * @param geom Geometry to check
 * @return true if likely bootable
 */
bool uft_dmf_is_bootable(const uft_floppy_geometry_t *geom);

/**
 * @brief Check USB FDD compatibility
 *
 * USB floppy drives are limited to standard formats.
 *
 * @param geom Geometry to check
 * @return true if USB FDD compatible
 */
bool uft_dmf_usb_compatible(const uft_floppy_geometry_t *geom);

/**
 * @brief Get Linux device minor number
 * @param geom Geometry
 * @return Minor number for /dev/fd0uXXXX
 */
int uft_dmf_linux_minor(const uft_floppy_geometry_t *geom);

/**
 * @brief Format disk with mformat command line
 * @param geom Geometry
 * @param buffer Output buffer for command
 * @param bufsize Buffer size
 * @return Command string length
 */
int uft_dmf_mformat_cmd(const uft_floppy_geometry_t *geom,
                         char *buffer, size_t bufsize);

/**
 * @brief Create empty image file
 * @param path Output path
 * @param geom Geometry
 * @param fill Fill byte (typically 0xF6 or 0x00)
 * @return 0 on success
 */
int uft_dmf_create_image(const char *path,
                          const uft_floppy_geometry_t *geom,
                          uint8_t fill);

/**
 * @brief Verify image matches geometry
 * @param path Image path
 * @param geom Expected geometry
 * @return 0 if match, -1 on error, 1 if mismatch
 */
int uft_dmf_verify_image(const char *path,
                          const uft_floppy_geometry_t *geom);

/*============================================================================
 * BPB (BIOS Parameter Block) Functions
 *============================================================================*/

/** BPB structure (DOS 3.0+) */
#pragma pack(push, 1)
typedef struct {
    uint8_t     jmp[3];         /**< Jump instruction */
    char        oem[8];         /**< OEM name */
    uint16_t    bytes_per_sect; /**< Bytes per sector */
    uint8_t     sects_per_clust;/**< Sectors per cluster */
    uint16_t    reserved_sects; /**< Reserved sectors */
    uint8_t     num_fats;       /**< Number of FATs */
    uint16_t    root_entries;   /**< Root directory entries */
    uint16_t    total_sects_16; /**< Total sectors (16-bit) */
    uint8_t     media_type;     /**< Media descriptor */
    uint16_t    sects_per_fat;  /**< Sectors per FAT */
    uint16_t    sects_per_track;/**< Sectors per track */
    uint16_t    num_heads;      /**< Number of heads */
    uint32_t    hidden_sects;   /**< Hidden sectors */
    uint32_t    total_sects_32; /**< Total sectors (32-bit) */
} uft_bpb_t;
#pragma pack(pop)

/**
 * @brief Parse BPB from boot sector
 * @param boot_sector Boot sector data
 * @param bpb Output BPB structure
 * @return 0 on success, -1 if invalid
 */
int uft_bpb_parse(const uint8_t *boot_sector, uft_bpb_t *bpb);

/**
 * @brief Create BPB for geometry
 * @param geom Geometry
 * @param bpb Output BPB
 * @param volume_label Volume label (11 chars, space padded)
 */
void uft_bpb_create(const uft_floppy_geometry_t *geom,
                    uft_bpb_t *bpb, const char *volume_label);

/**
 * @brief Extract geometry from BPB
 * @param bpb BPB structure
 * @param geom Output geometry
 * @return 0 on success
 */
int uft_bpb_to_geometry(const uft_bpb_t *bpb, uft_floppy_geometry_t *geom);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DMF_H */
