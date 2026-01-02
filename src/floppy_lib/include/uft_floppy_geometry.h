/**
 * @file uft_floppy_geometry.h
 * @brief Disk geometry and LBA/CHS conversion functions
 * 
 * Provides geometry detection and address translation between:
 * - LBA (Logical Block Addressing)
 * - CHS (Cylinder-Head-Sector)
 * 
 * Based on:
 * - lbacache CHS2LBA.ASM
 * - BootLoader-Test Floppy16.inc
 * 
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef UFT_FLOPPY_GEOMETRY_H
#define UFT_FLOPPY_GEOMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uft_floppy_types.h"

/*===========================================================================
 * Geometry Detection
 *===========================================================================*/

/**
 * @brief Detect floppy type from size
 * 
 * @param total_bytes Total disk size in bytes
 * @return Detected floppy type or UFT_FLOPPY_UNKNOWN
 */
uft_floppy_type_t uft_geometry_detect_type(uint64_t total_bytes);

/**
 * @brief Get predefined geometry for a floppy type
 * 
 * @param type Floppy type identifier
 * @param[out] geom Geometry structure to fill
 * @return UFT_OK on success, UFT_ERR_NOT_FOUND if type unknown
 */
UFT_WARN_UNUSED
uft_error_t uft_geometry_get_standard(uft_floppy_type_t type, uft_geometry_t *geom);

/**
 * @brief Create custom geometry
 * 
 * @param cylinders Number of cylinders
 * @param heads Number of heads
 * @param sectors Sectors per track
 * @param bytes_per_sector Bytes per sector
 * @param[out] geom Geometry structure to fill
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_geometry_create(uint16_t cylinders, uint8_t heads,
                                 uint8_t sectors, uint16_t bytes_per_sector,
                                 uft_geometry_t *geom);

/**
 * @brief Extract geometry from BPB
 * 
 * @param bpb Pointer to BIOS Parameter Block
 * @param[out] geom Geometry structure to fill
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_geometry_from_bpb(const uft_bpb_t *bpb, uft_geometry_t *geom);

/**
 * @brief Validate geometry parameters
 * 
 * Checks for:
 * - Valid sector count (minimum 7 sectors/track)
 * - Valid head count (minimum 1 head)
 * - Non-zero values
 * 
 * @param geom Geometry to validate
 * @return UFT_OK if valid, UFT_ERR_GEOMETRY_INVALID otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_geometry_validate(const uft_geometry_t *geom);

/*===========================================================================
 * LBA / CHS Conversion
 *===========================================================================*/

/**
 * @brief Convert CHS address to LBA
 * 
 * Formula: LBA = (C × heads + H) × sectors_per_track + (S - 1)
 * 
 * @param geom Disk geometry
 * @param chs CHS address
 * @param[out] lba Resulting LBA
 * @return UFT_OK on success, error code otherwise
 */
UFT_WARN_UNUSED
uft_error_t uft_chs_to_lba(const uft_geometry_t *geom, const uft_chs_t *chs,
                            uint32_t *lba);

/**
 * @brief Convert LBA to CHS address
 * 
 * Formulas:
 * - S = (LBA mod sectors_per_track) + 1
 * - H = (LBA / sectors_per_track) mod heads
 * - C = (LBA / sectors_per_track) / heads
 * 
 * @param geom Disk geometry
 * @param lba LBA address
 * @param[out] chs Resulting CHS address
 * @return UFT_OK on success, UFT_ERR_CHS_OVERFLOW if cylinder > 1023
 */
UFT_WARN_UNUSED
uft_error_t uft_lba_to_chs(const uft_geometry_t *geom, uint32_t lba,
                            uft_chs_t *chs);

/**
 * @brief Quick CHS to LBA conversion (inline)
 * 
 * No validation - use only when geometry is known valid.
 * 
 * @param heads Total heads
 * @param sectors Sectors per track
 * @param c Cylinder
 * @param h Head
 * @param s Sector (1-based!)
 * @return LBA address
 */
UFT_INLINE uint32_t uft_chs_to_lba_quick(uint8_t heads, uint8_t sectors,
                                          uint16_t c, uint8_t h, uint8_t s) {
    return ((uint32_t)c * heads + h) * sectors + (s - 1);
}

/**
 * @brief Quick LBA to CHS conversion (inline)
 * 
 * No validation - use only when geometry is known valid.
 * 
 * @param heads Total heads
 * @param sectors Sectors per track
 * @param lba LBA address
 * @param[out] c Cylinder
 * @param[out] h Head
 * @param[out] s Sector (1-based!)
 */
UFT_INLINE void uft_lba_to_chs_quick(uint8_t heads, uint8_t sectors, uint32_t lba,
                                      uint16_t *c, uint8_t *h, uint8_t *s) {
    *s = (uint8_t)((lba % sectors) + 1);
    uint32_t temp = lba / sectors;
    *h = (uint8_t)(temp % heads);
    *c = (uint16_t)(temp / heads);
}

/*===========================================================================
 * BIOS Int 13h Encoding
 *===========================================================================*/

/**
 * @brief Encode CHS for BIOS Int 13h (CX/DX registers)
 * 
 * CX format: CCCCCCCC CCSSSSSS
 * DX format: HHHHHHHH DDDDDDDD
 * 
 * @param chs CHS address
 * @param drive_num Drive number (0x00 or 0x80+)
 * @param[out] cx CX register value
 * @param[out] dx DX register value
 * @return UFT_OK on success, UFT_ERR_CHS_OVERFLOW if cylinder > 1023
 */
UFT_WARN_UNUSED
uft_error_t uft_chs_to_bios(const uft_chs_t *chs, uint8_t drive_num,
                             uint16_t *cx, uint16_t *dx);

/**
 * @brief Decode CHS from BIOS Int 13h registers
 * 
 * @param cx CX register value
 * @param dx DX register value
 * @param[out] chs Resulting CHS address
 * @param[out] drive_num Drive number (if not NULL)
 */
void uft_bios_to_chs(uint16_t cx, uint16_t dx, uft_chs_t *chs, uint8_t *drive_num);

/*===========================================================================
 * Cluster / Sector Conversion (FAT specific)
 *===========================================================================*/

/**
 * @brief Convert FAT cluster number to LBA
 * 
 * @param bpb Pointer to BIOS Parameter Block
 * @param cluster Cluster number (2+)
 * @return First sector of the cluster
 */
UFT_INLINE uint32_t uft_cluster_to_lba(const uft_bpb_t *bpb, uint16_t cluster) {
    uint32_t root_dir_sectors = ((bpb->root_entries * 32) + 
                                  (bpb->bytes_per_sector - 1)) / 
                                  bpb->bytes_per_sector;
    uint32_t first_data_sector = bpb->reserved_sectors + 
                                  (bpb->num_fats * bpb->sectors_per_fat) + 
                                  root_dir_sectors;
    return first_data_sector + ((cluster - 2) * bpb->sectors_per_cluster);
}

/**
 * @brief Get first root directory sector
 * 
 * @param bpb Pointer to BIOS Parameter Block
 * @return First sector of root directory
 */
UFT_INLINE uint32_t uft_root_dir_sector(const uft_bpb_t *bpb) {
    return bpb->reserved_sectors + (bpb->num_fats * bpb->sectors_per_fat);
}

/**
 * @brief Get first FAT sector
 * 
 * @param bpb Pointer to BIOS Parameter Block
 * @param fat_num FAT number (0 = primary, 1 = backup, etc.)
 * @return First sector of specified FAT
 */
UFT_INLINE uint32_t uft_fat_sector(const uft_bpb_t *bpb, uint8_t fat_num) {
    return bpb->reserved_sectors + (fat_num * bpb->sectors_per_fat);
}

/*===========================================================================
 * Geometry String Formatting
 *===========================================================================*/

/**
 * @brief Format geometry as string
 * 
 * @param geom Geometry structure
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
int uft_geometry_to_string(const uft_geometry_t *geom, char *buffer, size_t size);

/**
 * @brief Format CHS address as string
 * 
 * @param chs CHS address
 * @param buffer Output buffer
 * @param size Buffer size
 * @return Number of characters written (excluding null terminator)
 */
int uft_chs_to_string(const uft_chs_t *chs, char *buffer, size_t size);

/**
 * @brief Get floppy type name
 * 
 * @param type Floppy type
 * @return Human-readable type name
 */
const char* uft_floppy_type_name(uft_floppy_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_GEOMETRY_H */
