/**
 * @file uft_fat_detect.h
 * @brief P1-5: FAT Format Detection API
 * 
 * @author UFT Team
 * @date 2025
 */

#ifndef UFT_FAT_DETECT_H
#define UFT_FAT_DETECT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft/fs/fat_bpb.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief FAT detection result
 */
typedef struct uft_fat_detect_result {
    bool            is_fat;         /**< True if FAT format detected */
    int             confidence;     /**< Confidence score 0-100 */
    uft_fat_type_t  fat_type;       /**< FAT12, FAT16, or FAT32 */
    uft_fat_bpb_t   bpb;            /**< Parsed BPB (if is_fat) */
    char            reason[128];    /**< Detection reason/description */
} uft_fat_detect_result_t;

/**
 * @brief Detect FAT format from buffer with confidence scoring
 * 
 * This function:
 * - Rejects known non-FAT formats (D64, ADF, SCP, HFE, G64, IPF)
 * - Validates FAT BPB structure
 * - Computes confidence score based on:
 *   - Boot signature (0x55AA)
 *   - Valid BPB fields
 *   - Common floppy sizes
 *   - Geometry hints
 * 
 * @param data Image data (at least 512 bytes)
 * @param size Image size in bytes
 * @param result Detection result (output)
 * @return 0 if FAT detected, -1 if not FAT or error
 * 
 * @example
 * ```c
 * uft_fat_detect_result_t result;
 * if (uft_fat_detect(image_data, image_size, &result) == 0) {
 *     printf("Detected %s with %d%% confidence\n",
 *            uft_fat_type_name(result.fat_type),
 *            result.confidence);
 * }
 * ```
 */
int uft_fat_detect(const uint8_t *data, size_t size, 
                   uft_fat_detect_result_t *result);

/**
 * @brief Get FAT type name string
 */
const char* uft_fat_type_name(uft_fat_type_t type);

/**
 * @brief Check if size matches common FAT floppy format
 */
bool uft_fat_is_floppy_size(size_t size);

/* ═══════════════════════════════════════════════════════════════════════════
 * Confidence Score Breakdown
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Component                        Points
 * ────────────────────────────────────────
 * Boot signature (0x55AA)           +20
 * Valid bytes_per_sector            +10
 * Valid sectors_per_cluster         +10
 * Valid reserved_sectors             +5
 * FAT count == 2                     +5
 * Valid media descriptor             +5
 * Valid sectors_per_fat              +5
 * Valid sectors_per_track            +5
 * Valid heads                        +5
 * Common floppy size                +15
 * ────────────────────────────────────────
 * Maximum                            85
 * 
 * Interpretation:
 * - 70-100: High confidence (definitely FAT)
 * - 50-69:  Medium confidence (likely FAT)
 * - 30-49:  Low confidence (might be FAT)
 * - 0-29:   Very low (probably not FAT)
 * 
 * ═══════════════════════════════════════════════════════════════════════════ */

#ifdef __cplusplus
}
#endif

#endif /* UFT_FAT_DETECT_H */
