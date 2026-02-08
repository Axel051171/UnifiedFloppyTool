/**
 * @file uft_fdi_format.h
 * @brief FDI (Formatted Disk Image) format profile
 * @author UFT Team
 * @date 2025
 * @version 1.0.0
 *
 * FDI is a sector-level disk image format used by various emulators.
 * Multiple variants exist (UKV FDI, Spectrum FDI, etc.) with slightly
 * different header formats but similar structure.
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_FDI_FORMAT_H
#define UFT_FDI_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * FDI Format Constants
 * ═══════════════════════════════════════════════════════════════════════════ */

/** @brief FDI signature "FDI" */
#define UFT_FDI_SIGNATURE           "FDI"
#define UFT_FDI_SIGNATURE_LEN       3

/** @brief FDI header size */
#define UFT_FDI_HEADER_SIZE         14

/** @brief FDI track header size */
#define UFT_FDI_TRACK_HEADER_SIZE   7

/** @brief FDI sector header size */
#define UFT_FDI_SECTOR_HEADER_SIZE  7

/** @brief Maximum tracks */
#define UFT_FDI_MAX_TRACKS          256

/** @brief Maximum sectors per track */
#define UFT_FDI_MAX_SECTORS         256

/* ═══════════════════════════════════════════════════════════════════════════
 * FDI Sector Flags
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_FDI_SECT_DELETED        0x80    /**< Deleted data */
#define UFT_FDI_SECT_CRC_ERROR      0x40    /**< CRC error in data */
#define UFT_FDI_SECT_NO_DATA        0x20    /**< No data field */

/* ═══════════════════════════════════════════════════════════════════════════
 * FDI Structures
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief FDI file header (14 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[3];           /**< "FDI" signature */
    uint8_t write_protect;          /**< Write protection flag */
    uint16_t cylinders;             /**< Number of cylinders */
    uint16_t heads;                 /**< Number of heads */
    uint16_t description_offset;    /**< Offset to description */
    uint16_t data_offset;           /**< Offset to track data */
    uint16_t extra_header_size;     /**< Extra header data size */
} uft_fdi_header_t;
#pragma pack(pop)

/**
 * @brief FDI track header (7 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint32_t offset;                /**< Offset to track data */
    uint16_t reserved;              /**< Reserved */
    uint8_t sector_count;           /**< Number of sectors */
} uft_fdi_track_header_t;
#pragma pack(pop)

/**
 * @brief FDI sector header (7 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t cylinder;               /**< Cylinder in ID */
    uint8_t head;                   /**< Head in ID */
    uint8_t sector;                 /**< Sector number */
    uint8_t size_code;              /**< Sector size code (N) */
    uint8_t flags;                  /**< Sector flags */
    uint16_t data_offset;           /**< Offset to sector data */
} uft_fdi_sector_header_t;
#pragma pack(pop)

/**
 * @brief Parsed FDI information
 */
typedef struct {
    uint16_t cylinders;
    uint16_t heads;
    uint32_t total_sectors;
    bool write_protected;
    char description[256];
} uft_fdi_info_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * Compile-time Verification
 * ═══════════════════════════════════════════════════════════════════════════ */

_Static_assert(sizeof(uft_fdi_header_t) == 14, "FDI header must be 14 bytes");
_Static_assert(sizeof(uft_fdi_track_header_t) == 7, "FDI track header must be 7 bytes");
_Static_assert(sizeof(uft_fdi_sector_header_t) == 7, "FDI sector header must be 7 bytes");

/* ═══════════════════════════════════════════════════════════════════════════
 * Helper Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

static inline uint32_t uft_fdi_size_code_to_bytes(uint8_t code) {
    return 128U << code;
}

static inline bool uft_fdi_validate_signature(const uint8_t* data, size_t size) {
    if (!data || size < UFT_FDI_HEADER_SIZE) return false;
    return memcmp(data, UFT_FDI_SIGNATURE, UFT_FDI_SIGNATURE_LEN) == 0;
}

static inline int uft_fdi_probe(const uint8_t* data, size_t size) {
    if (!uft_fdi_validate_signature(data, size)) return 0;
    
    int score = 50;
    const uft_fdi_header_t* hdr = (const uft_fdi_header_t*)data;
    
    if (hdr->cylinders > 0 && hdr->cylinders <= 255) score += 15;
    if (hdr->heads > 0 && hdr->heads <= 2) score += 15;
    if (hdr->data_offset >= UFT_FDI_HEADER_SIZE) score += 10;
    
    return (score > 100) ? 100 : score;
}

static inline bool uft_fdi_parse(const uint8_t* data, size_t size, uft_fdi_info_t* info) {
    if (!uft_fdi_validate_signature(data, size) || !info) return false;
    
    const uft_fdi_header_t* hdr = (const uft_fdi_header_t*)data;
    memset(info, 0, sizeof(*info));
    
    info->cylinders = hdr->cylinders;
    info->heads = hdr->heads;
    info->write_protected = (hdr->write_protect != 0);
    
    return true;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_FDI_FORMAT_H */
