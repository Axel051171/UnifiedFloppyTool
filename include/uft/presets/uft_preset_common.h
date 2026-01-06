/**
 * @file uft_preset_common.h
 * @brief Common definitions for format presets
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#ifndef UFT_PRESET_COMMON_H
#define UFT_PRESET_COMMON_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Encoding Types
 * ============================================================================ */

typedef enum uft_encoding_type {
    UFT_ENCODING_FM = 0,
    UFT_ENCODING_MFM = 1,
    UFT_ENCODING_GCR = 2,
    UFT_ENCODING_M2FM = 3,
    UFT_ENCODING_UNKNOWN = 255
} uft_encoding_type_t;

/* ============================================================================
 * Preset Flags
 * ============================================================================ */

#define UFT_PRESET_FLAG_NONE          0
#define UFT_PRESET_FLAG_SEQUENTIAL    (1u << 0)  /**< Sequential sector order */
#define UFT_PRESET_FLAG_INTERLEAVED   (1u << 1)  /**< Interleaved sector order */
#define UFT_PRESET_FLAG_READ_ONLY     (1u << 2)  /**< Read-only format */
#define UFT_PRESET_FLAG_PROTECTED     (1u << 3)  /**< Copy-protected */
#define UFT_PRESET_FLAG_SPECIAL_SYNC  (1u << 4)  /**< Special sync marks */
#define UFT_PRESET_FLAG_HAS_HEADER    (1u << 5)  /**< Has file header */
#define UFT_PRESET_FLAG_VARIABLE_SEC  (1u << 6)  /**< Variable sector size */

/* ============================================================================
 * Preset Structure
 * ============================================================================ */

/**
 * @brief Format preset definition
 */
typedef struct uft_format_preset {
    const char *name;              /**< Short name (e.g. "PC98-2HD") */
    const char *description;       /**< Full description */
    const char *category;          /**< Category (e.g. "PC-98") */
    const char *extension;         /**< Default file extension */
    
    uint16_t cylinders;            /**< Number of cylinders */
    uint8_t  heads;                /**< Number of heads/sides */
    uint8_t  sectors_per_track;    /**< Sectors per track */
    uint16_t sector_size;          /**< Bytes per sector */
    
    uft_encoding_type_t encoding;  /**< Encoding type */
    uint16_t rpm;                  /**< Rotation speed */
    uint16_t data_rate;            /**< Data rate in kbit/s */
    uint8_t  gap3;                 /**< GAP3 length */
    
    uint32_t total_size;           /**< Total image size in bytes */
    uint32_t flags;                /**< Preset flags */
} uft_format_preset_t;

/* ============================================================================
 * Helper Macros
 * ============================================================================ */

/**
 * @brief Calculate total disk size from geometry
 */
#define UFT_CALC_DISK_SIZE(c, h, s, sz) \
    ((uint32_t)(c) * (uint32_t)(h) * (uint32_t)(s) * (uint32_t)(sz))

/**
 * @brief Count presets in array
 */
#define UFT_PRESET_COUNT(arr) \
    (sizeof(arr) / sizeof((arr)[0]) - 1)

#ifdef __cplusplus
}
#endif

#endif /* UFT_PRESET_COMMON_H */
