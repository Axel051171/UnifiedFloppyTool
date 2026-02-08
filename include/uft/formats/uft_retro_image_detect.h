/**
 * @file uft_retro_image_detect.h
 * @brief Retro Image Format Detection Module
 *
 * Provides detection of 400 retro image formats across 12 platforms:
 * Atari ST/Falcon, Amiga, C64, MSX, ZX Spectrum, Apple II,
 * Atari 8-bit, Amstrad CPC, PlayStation, GEM, Japanese PC.
 *
 * Uses multi-factor detection: magic bytes + file extension + file size.
 * Integrates with UFT's forensic signature pipeline.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef UFT_RETRO_IMAGE_DETECT_H
#define UFT_RETRO_IMAGE_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration - full type in uft_retro_image_sigs.h */
typedef struct ri_sig_entry ri_sig_entry_t;

/* ============================================================================
 * Detection Result
 * ============================================================================ */

#define RI_DETECT_MAX_CANDIDATES 16

typedef struct {
    const char    *ext;             /**< Format extension */
    const char    *name;            /**< Human-readable name */
    const char    *platform_name;   /**< Platform string */
    int            platform_id;     /**< ri_platform_t value */
    int            confidence;      /**< Detection confidence 0-100 */
    uint32_t       min_size;        /**< Expected min file size */
    uint32_t       max_size;        /**< Expected max file size */
    uint8_t        fixed_size;      /**< All known samples same size */
} ri_detect_result_t;

typedef struct {
    ri_detect_result_t candidates[RI_DETECT_MAX_CANDIDATES];
    uint16_t           count;       /**< Number of candidates */
    int                best_idx;    /**< Index of best match (-1 if none) */
} ri_detect_results_t;

/* ============================================================================
 * Database Statistics
 * ============================================================================ */

typedef struct {
    uint16_t total_formats;         /**< Total format signatures */
    uint16_t with_magic;            /**< Formats with magic bytes */
    uint16_t strong_magic;          /**< Formats with ≥4 byte magic */
    uint16_t fixed_size;            /**< Fixed-size formats */
    uint16_t platforms;             /**< Number of platforms covered */
    uint16_t per_platform[16];      /**< Formats per platform */
} ri_db_stats_t;

/* ============================================================================
 * Core API
 * ============================================================================ */

/**
 * @brief Detect retro image format from header bytes + metadata
 *
 * @param data      File header bytes (at least 16 bytes recommended)
 * @param data_len  Length of header data available
 * @param file_size Total file size (0 if unknown)
 * @param ext       File extension without dot (NULL if unknown)
 * @param results   Output: detection results with ranked candidates
 * @return          Number of candidates found
 */
int rid_detect(const uint8_t *data, size_t data_len,
               uint32_t file_size, const char *ext,
               ri_detect_results_t *results);

/**
 * @brief Quick detect - returns best match only
 *
 * @return Confidence (0-100), or 0 if no match. Fills name/ext if matched.
 */
int rid_detect_quick(const uint8_t *data, size_t data_len,
                     uint32_t file_size, const char *ext,
                     const char **out_name, const char **out_platform);

/**
 * @brief List all known formats for a platform
 *
 * @param platform_id  ri_platform_t value
 * @param results      Output array
 * @param max_results  Array capacity
 * @return             Number of formats found
 */
uint16_t rid_list_platform(int platform_id,
                           ri_detect_result_t *results,
                           uint16_t max_results);

/**
 * @brief Get database statistics
 */
void rid_get_stats(ri_db_stats_t *stats);

/**
 * @brief Get platform name string from ID
 */
const char *rid_platform_name(int platform_id);

/**
 * @brief Print detection results (debug)
 */
void rid_print_results(const ri_detect_results_t *results);

/**
 * @brief Scan a buffer for embedded retro images (carving)
 *
 * Scans through buffer looking for magic byte matches.
 * Useful for forensic recovery from disk image data.
 *
 * @param data      Buffer to scan
 * @param data_len  Buffer length
 * @param callback  Called for each match: callback(offset, sig_entry, user_data)
 * @param user_data Passed to callback
 * @return          Number of matches found
 */
typedef void (*rid_carve_callback_t)(size_t offset, const ri_sig_entry_t *sig,
                                      void *user_data);

uint32_t rid_carve_scan(const uint8_t *data, size_t data_len,
                        rid_carve_callback_t callback, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RETRO_IMAGE_DETECT_H */
