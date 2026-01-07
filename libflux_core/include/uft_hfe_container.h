/**
 * @file uft_hfe_container.h
 * @brief HFE (UFT HFE Format) Container Parser
 * 
 * Version-agnostic parser supporting HFE v1, v3, and future versions.
 * Implements strict 3-layer separation:
 * - Layer 1 (THIS): Container structure parsing
 * - Layer 2: Geometry detection (separate)
 * - Layer 3: Track/flux decoding (separate)
 * 
 * HFE Versions:
 * - v1 (0x00): Original, 512-byte header, 256-byte track encoding
 * - v3 (0x01): Extended, 1024-byte header, 512-byte encoding, weak bits
 * 
 * Key Features:
 * - NO hardcoded geometries
 * - Forward-compatible (unknown fields preserved)
 * - Version detection from header
 * 
 * @version 2.11.0
 * @date 2024-12-27
 */

#ifndef UFT_HFE_CONTAINER_H
#define UFT_HFE_CONTAINER_H

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HFE format version
 */
typedef enum {
    UFT_HFE_VERSION_1 = 0x00,       /**< HFE v1 (original) */
    UFT_HFE_VERSION_3 = 0x01,       /**< HFE v3 (extended) */
    UFT_HFE_VERSION_UNKNOWN = 0xFF  /**< Unknown/future version */
} uft_hfe_version_t;

/**
 * @brief HFE track encoding
 */
typedef enum {
    UFT_HFE_ENCODING_ISOIBM_MFM = 0x00,
    UFT_HFE_ENCODING_AMIGA_MFM = 0x01,
    UFT_HFE_ENCODING_ISOIBM_FM = 0x02,
    UFT_HFE_ENCODING_EMU_FM = 0x03,
    UFT_HFE_ENCODING_UNKNOWN = 0xFF
} uft_hfe_encoding_t;

/**
 * @brief HFE container header (version-independent fields)
 */
typedef struct {
    /* File identification */
    uint8_t signature[8];           /**< "HXCPICFE" */
    uint8_t format_revision;        /**< 0x00=v1, 0x01=v3 */
    
    /* Physical disk parameters */
    uint8_t track_count;            /**< Number of tracks */
    uint8_t side_count;             /**< Number of sides (1-2) */
    uint8_t track_encoding;         /**< See uft_hfe_encoding_t */
    uint16_t bitrate;               /**< Bitrate in Kbit/s */
    uint16_t rpm;                   /**< RPM * 100 (e.g., 300 = 30000) */
    
    /* Interface type */
    uint8_t interface_mode;         /**< 0x00=Generic, 0x04=Amiga, etc. */
    
    /* Track layout */
    uint16_t track_list_offset;     /**< Offset to track LUT */
    
    /* Version-specific sizes */
    uint32_t header_size;           /**< Actual header size (512 or 1024) */
    uint32_t track_encoding_size;   /**< 256 or 512 bytes per encoding */
    
    /* Write protection */
    bool write_allowed;
    
    /* Extended fields (v3+) */
    bool has_extended_header;
    uint8_t* extended_data;         /**< Unknown/future fields */
    size_t extended_size;
    
    /* v3-specific */
    bool single_step;               /**< v3: Single-step mode */
    uint8_t track0_s0_altencoding;  /**< v3: Alternate encoding */
    uint8_t track0_s0_encoding;     /**< v3: Track 0 encoding */
    uint8_t track0_s1_altencoding;
    uint8_t track0_s1_encoding;
    
} uft_hfe_header_t;

/**
 * @brief HFE track offset entry
 */
typedef struct {
    uint16_t offset;                /**< Offset in 512-byte blocks */
    uint16_t length;                /**< Track length in bytes */
} uft_hfe_track_offset_t;

/**
 * @brief HFE container context
 */
typedef struct {
    FILE* fp;
    
    /* Parsed header */
    uft_hfe_header_t header;
    
    /* Track lookup table */
    uft_hfe_track_offset_t* track_lut;
    uint32_t track_lut_size;
    
    /* Statistics */
    uint32_t tracks_read;
    
    /* Error context */
    uft_error_ctx_t error;
    
} uft_hfe_container_t;

/**
 * @brief Open HFE file and parse container
 * 
 * This is Layer 1 only - parses structure, NOT geometry!
 * 
 * @param[in] path Path to .hfe file
 * @param[out] container Pointer to receive container context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_container_open(
    const char* path,
    uft_hfe_container_t** container
);

/**
 * @brief Close HFE container
 * 
 * @param[in,out] container Container to close
 */
void uft_hfe_container_close(uft_hfe_container_t** container);

/**
 * @brief Get track offset information
 * 
 * @param[in] container HFE container
 * @param track Track number (0-based)
 * @param side Side number (0-1)
 * @param[out] offset Track offset entry
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_get_track_offset(
    const uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side,
    uft_hfe_track_offset_t* offset
);

/**
 * @brief Read raw track data (encoded)
 * 
 * Returns raw HFE track encoding (interleaved).
 * Does NOT decode - use Layer 3 for that!
 * 
 * @param[in] container HFE container
 * @param track Track number
 * @param side Side number
 * @param[out] data Buffer to receive encoded track (malloc'd, caller frees)
 * @param[out] size Size of encoded data
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_read_track_raw(
    uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side,
    uint8_t** data,
    size_t* size
);

/**
 * @brief Check if track exists
 * 
 * @param[in] container HFE container
 * @param track Track number
 * @param side Side number
 * @return true if track has data
 */
bool uft_hfe_has_track(
    const uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side
);

/**
 * @brief Get version string
 * 
 * @param version Version code
 * @return Human-readable version
 */
const char* uft_hfe_version_string(uft_hfe_version_t version);

/**
 * @brief Get encoding name
 * 
 * @param encoding Encoding code
 * @return Human-readable encoding
 */
const char* uft_hfe_encoding_name(uft_hfe_encoding_t encoding);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_CONTAINER_H */
