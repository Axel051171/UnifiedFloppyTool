/**
 * @file uft_scp_reader.h
 * @brief SuperCard Pro (SCP) Flux Image Reader
 * 
 * Reads SCP flux image files created by SuperCard Pro hardware
 * and other flux-level imaging tools.
 * 
 * SCP Format Specification:
 * - Header: "SCP" magic + version
 * - Disk type, flags, revolution count
 * - Track offset table (168 entries, 84 tracks * 2 sides)
 * - Per-track data: timestamps + flux transitions
 * 
 * Supported Versions:
 * - v1.0, v1.5, v2.0, v2.1 (auto-detected)
 * 
 * @version 2.11.0
 * @date 2024-12-27
 */

#ifndef UFT_SCP_READER_H
#define UFT_SCP_READER_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief SCP file version
 */
typedef enum {
    UFT_SCP_VERSION_10 = 0x10,
    UFT_SCP_VERSION_15 = 0x15,
    UFT_SCP_VERSION_20 = 0x20,
    UFT_SCP_VERSION_21 = 0x21
} uft_scp_version_t;

/**
 * @brief SCP disk type
 */
typedef enum {
    UFT_SCP_DISK_C64 = 0x00,
    UFT_SCP_DISK_AMIGA = 0x04,
    UFT_SCP_DISK_APPLE_II = 0x08,
    UFT_SCP_DISK_ATARI_ST = 0x0C,
    UFT_SCP_DISK_ATARI_810 = 0x10,
    UFT_SCP_DISK_PC_DD = 0x14,
    UFT_SCP_DISK_PC_HD = 0x18,
    UFT_SCP_DISK_CUSTOM = 0xFF
} uft_scp_disk_type_t;

/**
 * @brief SCP file flags
 */
typedef enum {
    UFT_SCP_FLAG_INDEX = (1 << 0),      /**< Index signal present */
    UFT_SCP_FLAG_TPI_96 = (1 << 1),     /**< 96 TPI drive */
    UFT_SCP_FLAG_RPM_360 = (1 << 2),    /**< 360 RPM (else 300) */
    UFT_SCP_FLAG_NORMALIZED = (1 << 3), /**< Flux normalized */
    UFT_SCP_FLAG_READ_WRITE = (1 << 4), /**< Read/write capable */
    UFT_SCP_FLAG_FOOTER = (1 << 5)      /**< Has footer */
} uft_scp_flags_t;

/**
 * @brief SCP track header
 */
typedef struct {
    uint32_t duration;          /**< Track duration (ns) */
    uint32_t index_offset;      /**< Offset to index pulse */
    uint32_t track_offset;      /**< Offset in file */
    uint32_t entry_count;       /**< Number of flux entries */
} uft_scp_track_header_t;

/**
 * @brief SCP reader context
 */
typedef struct {
    FILE* fp;
    
    /* File header */
    uint8_t version;
    uint8_t disk_type;
    uint8_t revolutions;
    uint8_t start_track;
    uint8_t end_track;
    uint8_t flags;
    uint16_t bit_cell_width;    /**< 0 = 16-bit, 1+ = variable */
    uint16_t heads;
    uint32_t checksum;
    
    /* Track offsets (168 max: 84 tracks * 2 heads) */
    uint32_t track_offsets[168];
    
    /* Statistics */
    uint32_t tracks_read;
    uint32_t total_flux_transitions;
    
    /* Error context */
    uft_error_ctx_t error;
    
} uft_scp_ctx_t;

/**
 * @brief Open SCP file
 * 
 * @param[in] path Path to .scp file
 * @param[out] ctx Pointer to receive context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_scp_open(const char* path, uft_scp_ctx_t** ctx);

/**
 * @brief Close SCP file
 * 
 * @param[in,out] ctx Context to close
 */
void uft_scp_close(uft_scp_ctx_t** ctx);

/**
 * @brief Read track flux data
 * 
 * Reads flux transition data for a specific track/head.
 * Flux times are in nanoseconds.
 * 
 * @param[in] ctx SCP context
 * @param track Physical track number (0-83)
 * @param head Head number (0-1)
 * @param revolution Revolution number (0-4, usually 0 for first)
 * @param[out] flux_ns Buffer to receive flux times (malloc'd, caller must free)
 * @param[out] count Number of flux transitions
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_scp_read_track(
    uft_scp_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uint8_t revolution,
    uint32_t** flux_ns,
    uint32_t* count
);

/**
 * @brief Get track header information
 * 
 * @param[in] ctx SCP context
 * @param track Track number
 * @param head Head number
 * @param[out] header Track header info
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_scp_get_track_header(
    uft_scp_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uft_scp_track_header_t* header
);

/**
 * @brief Check if track exists
 * 
 * @param[in] ctx SCP context
 * @param track Track number
 * @param head Head number
 * @return true if track has data
 */
bool uft_scp_has_track(
    const uft_scp_ctx_t* ctx,
    uint8_t track,
    uint8_t head
);

/**
 * @brief Get disk type name
 * 
 * @param disk_type Disk type code
 * @return Human-readable name
 */
const char* uft_scp_disk_type_name(uint8_t disk_type);

/**
 * @brief Get version string
 * 
 * @param version Version code
 * @return Version string (e.g., "2.1")
 */
const char* uft_scp_version_string(uint8_t version);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_READER_H */
