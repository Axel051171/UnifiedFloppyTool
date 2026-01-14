/**
 * @file uft_format_verify.h
 * @brief Format-specific verify functions for WOZ, A2R, TD0
 * 
 * Part of INDUSTRIAL_UPGRADE_PLAN - Parser Matrix Verify Gap
 * 
 * @version 3.8.0
 * @date 2026-01-14
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
 * WOZ Format Verify
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

/* ═══════════════════════════════════════════════════════════════════════════════
 * A2R Format Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

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
 * TD0 Format Verify
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

/* ═══════════════════════════════════════════════════════════════════════════════
 * Generic File Verify
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Auto-detect format and verify file
 * 
 * Detects WOZ, A2R, TD0 by magic bytes and calls appropriate verifier.
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
