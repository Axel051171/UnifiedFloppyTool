/**
 * @file uft_d64_bam.h
 * @brief D64 Block Allocation Map (BAM) Extended API
 * @version 1.0.0
 * 
 * Extended BAM functions for forensic analysis and disk manipulation.
 * Based on "The Little Black Book" forensic extraction techniques.
 * 
 * Features:
 * - BAM info extraction (disk name, ID, DOS version)
 * - BAM manipulation (allocate all, free all)
 * - Write-protect flag handling
 * - Free block counting
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_D64_BAM_H
#define UFT_D64_BAM_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * Forward Declarations
 * ═══════════════════════════════════════════════════════════════════════════ */

struct uft_d64_image;

/* ═══════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** BAM location for 1541 D64 */
#define UFT_D64_BAM_TRACK       18
#define UFT_D64_BAM_SECTOR      0

/** DOS version for standard 1541 */
#define UFT_D64_DOS_VERSION_1541    0x41

/** BAM offsets */
#define UFT_D64_BAM_OFF_DIR_TRACK   0x00
#define UFT_D64_BAM_OFF_DIR_SECTOR  0x01
#define UFT_D64_BAM_OFF_DOS_VERSION 0x02
#define UFT_D64_BAM_OFF_UNUSED      0x03
#define UFT_D64_BAM_OFF_ENTRIES     0x04
#define UFT_D64_BAM_OFF_DISK_NAME   0x90
#define UFT_D64_BAM_OFF_DISK_ID     0xA2
#define UFT_D64_BAM_OFF_DOS_TYPE    0xA5

/* ═══════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief BAM information structure
 */
typedef struct uft_d64_bam_info {
    uint8_t     dir_track;          /**< Directory start track (usually 18) */
    uint8_t     dir_sector;         /**< Directory start sector (usually 1) */
    uint8_t     dos_version;        /**< DOS version byte (0x41 for 1541) */
    char        disk_name[17];      /**< Disk name (PETSCII→ASCII, null-terminated) */
    char        disk_id[6];         /**< Disk ID (2 chars + padding) */
    char        dos_type[3];        /**< DOS type string (e.g., "2A") */
    uint16_t    free_blocks;        /**< Total free blocks */
    bool        is_write_protected; /**< Write-protect flag in BAM */
} uft_d64_bam_info_t;

/**
 * @brief BAM entry for a single track
 */
typedef struct uft_d64_bam_entry {
    uint8_t     track;              /**< Track number */
    uint8_t     free_count;         /**< Free sectors on this track */
    uint8_t     bitmap[3];          /**< Sector allocation bitmap */
} uft_d64_bam_entry_t;

/**
 * @brief BAM modification options
 */
typedef struct uft_d64_bam_options {
    bool        preserve_directory; /**< Keep directory sectors allocated */
    bool        preserve_bam;       /**< Keep BAM sector allocated */
    bool        dry_run;            /**< Report only, don't modify */
} uft_d64_bam_options_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * BAM Information Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read BAM information from D64 image
 * 
 * Extracts disk name, ID, DOS version, and free block count.
 * 
 * @param img       D64 image (must not be NULL)
 * @param info      Output structure (must not be NULL)
 * @return          0 on success, negative on error
 */
int uft_d64_bam_read_info(const struct uft_d64_image* img,
                          uft_d64_bam_info_t* info);

/**
 * @brief Get free block count for entire disk
 * 
 * @param img       D64 image
 * @return          Free block count, or 0 on error
 */
uint16_t uft_d64_bam_get_free_blocks(const struct uft_d64_image* img);

/**
 * @brief Get free block count for a specific track
 * 
 * @param img       D64 image
 * @param track     Track number (1-35/40/42)
 * @return          Free sectors on track, or 0 on error
 */
uint8_t uft_d64_bam_get_track_free(const struct uft_d64_image* img, 
                                    uint8_t track);

/**
 * @brief Check if a specific sector is allocated
 * 
 * @param img       D64 image
 * @param track     Track number
 * @param sector    Sector number
 * @return          true if allocated (used), false if free
 */
bool uft_d64_bam_is_allocated(const struct uft_d64_image* img,
                               uint8_t track, uint8_t sector);

/**
 * @brief Read BAM entry for a specific track
 * 
 * @param img       D64 image
 * @param track     Track number (1-35/40/42)
 * @param entry     Output entry structure
 * @return          0 on success, negative on error
 */
int uft_d64_bam_read_entry(const struct uft_d64_image* img,
                           uint8_t track,
                           uft_d64_bam_entry_t* entry);

/* ═══════════════════════════════════════════════════════════════════════════
 * BAM Modification Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Write DOS version byte to BAM
 * 
 * The DOS version byte is at offset 0x02 in the BAM sector.
 * Standard value is 0x41 ('A') for 1541.
 * Some copy protections modify this byte.
 * 
 * @param img       D64 image
 * @param version   DOS version byte (typically 0x41)
 * @return          0 on success, negative on error
 */
int uft_d64_bam_write_dos_version(struct uft_d64_image* img, 
                                   uint8_t version);

/**
 * @brief Allocate (mark as used) a specific sector
 * 
 * @param img       D64 image
 * @param track     Track number
 * @param sector    Sector number
 * @return          0 on success, negative on error
 */
int uft_d64_bam_allocate_sector(struct uft_d64_image* img,
                                 uint8_t track, uint8_t sector);

/**
 * @brief Free (mark as available) a specific sector
 * 
 * @param img       D64 image
 * @param track     Track number
 * @param sector    Sector number
 * @return          0 on success, negative on error
 */
int uft_d64_bam_free_sector(struct uft_d64_image* img,
                             uint8_t track, uint8_t sector);

/**
 * @brief Allocate all sectors on disk
 * 
 * Marks every sector as allocated (used), optionally preserving
 * BAM and directory sectors. This is a forensic/protection technique
 * described in "The Little Black Book".
 * 
 * @param img       D64 image
 * @param options   Modification options (NULL for defaults)
 * @return          0 on success, negative on error
 */
int uft_d64_bam_allocate_all(struct uft_d64_image* img,
                              const uft_d64_bam_options_t* options);

/**
 * @brief Free all sectors on disk
 * 
 * Marks every sector as free, optionally preserving BAM and directory.
 * 
 * @param img       D64 image
 * @param options   Modification options (NULL for defaults)
 * @return          0 on success, negative on error
 */
int uft_d64_bam_free_all(struct uft_d64_image* img,
                          const uft_d64_bam_options_t* options);

/**
 * @brief Remove write-protection from disk
 * 
 * Restores DOS version byte to 0x41 (standard 1541).
 * Some copy protections set this to other values to indicate
 * write-protection or as a protection mechanism.
 * 
 * @param img       D64 image
 * @return          0 on success, negative on error
 */
int uft_d64_bam_unwrite_protect(struct uft_d64_image* img);

/**
 * @brief Set disk name in BAM
 * 
 * @param img       D64 image
 * @param name      Disk name (max 16 chars, will be converted to PETSCII)
 * @return          0 on success, negative on error
 */
int uft_d64_bam_set_disk_name(struct uft_d64_image* img, 
                               const char* name);

/**
 * @brief Set disk ID in BAM
 * 
 * @param img       D64 image
 * @param id        Disk ID (2 characters)
 * @return          0 on success, negative on error
 */
int uft_d64_bam_set_disk_id(struct uft_d64_image* img,
                             const char* id);

/* ═══════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get default BAM options
 * 
 * @return Default options structure
 */
uft_d64_bam_options_t uft_d64_bam_default_options(void);

/**
 * @brief Recalculate free counts in BAM
 * 
 * Updates the free-count bytes in BAM entries to match
 * the actual bitmap state.
 * 
 * @param img       D64 image
 * @return          0 on success, negative on error
 */
int uft_d64_bam_recalculate_free_counts(struct uft_d64_image* img);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_BAM_H */
