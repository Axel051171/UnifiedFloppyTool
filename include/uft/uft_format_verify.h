/**
 * @file uft_format_verify.h
 * @brief Format-specific verify functions
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN - Parser Matrix Verify Gap
 * Supports: WOZ, A2R, TD0, IMG, IMD, D71, D81, HFE, D88
 * 
 * @version 3.8.6
 * @date 2026-01-15
 */

#ifndef UFT_FORMAT_VERIFY_H
#define UFT_FORMAT_VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft_write_verify.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Apple II Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify WOZ file integrity
 * 
 * Checks:
 * - Magic bytes ("WOZ1" or "WOZ2")
 * - CRC32 checksum
 * - INFO/TMAP/TRKS chunks
 * 
 * @param data      Raw WOZ file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_woz(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/**
 * @brief Verify A2R (Applesauce) file integrity
 * 
 * Checks:
 * - Magic bytes ("A2R2" or "A2R3")
 * - Header sequence
 * - Chunk structure
 * 
 * @param data      Raw A2R file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_a2r(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * PC/DOS Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify TD0 (Teledisk) file integrity
 * 
 * Checks:
 * - Magic bytes ("TD" or "td")
 * - Header CRC16
 * - Version validity
 * 
 * @param data      Raw TD0 file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_td0(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/**
 * @brief Verify IMG/IMA (raw sector) file integrity
 * 
 * Checks:
 * - Valid disk image size
 * - Optional boot sector signature
 * - Sector alignment
 * 
 * @param data      Raw IMG file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_img_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/**
 * @brief Verify IMD (ImageDisk) file integrity
 * 
 * Checks:
 * - Magic "IMD " header
 * - Header terminator (0x1A)
 * - Track record validity
 * 
 * @param data      Raw IMD file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_imd_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Commodore Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify D71 (Commodore 1571) file integrity
 * 
 * Checks:
 * - Exact file size (349696 or 351062 bytes)
 * - BAM structure at track 18
 * - Directory track reference
 * 
 * @param data      Raw D71 file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_d71_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/**
 * @brief Verify D81 (Commodore 1581) file integrity
 * 
 * Checks:
 * - Exact file size (819200 bytes)
 * - Header at track 40
 * - BAM chain structure
 * 
 * @param data      Raw D81 file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_d81_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Flux/Emulator Formats
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Verify HFE (HxC Floppy Emulator) file integrity
 * 
 * Checks:
 * - Magic "HXCPICFE" header
 * - Revision, track count, side count
 * - Track offset table validity
 * 
 * @param data      Raw HFE file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_hfe_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/**
 * @brief Verify D88 (Japanese PC formats) file integrity
 * 
 * Checks:
 * - Header structure (672+ bytes)
 * - Media type validity
 * - Track offset table
 * - Size consistency
 * 
 * @param data      Raw D88 file data
 * @param size      Size of data
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_d88_buffer(
    const uint8_t *data, 
    size_t size,
    uft_verify_result_t *result);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Generic File Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Auto-detect format and verify file
 * 
 * Detects format by magic bytes and size, calls appropriate verifier.
 * Supports: WOZ, A2R, TD0, IMG, IMD, D71, D81, HFE, D88
 * 
 * @param path      File path
 * @param result    Result structure (optional, can be NULL)
 * @return UFT_VERIFY_OK on success
 */
uft_verify_status_t uft_verify_file(
    const char *path,
    uft_verify_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMAT_VERIFY_H */
