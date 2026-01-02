/**
 * @file uft_hfe.h
 * @brief HFE Complete Reader (All 3 Layers)
 * 
 * Combines:
 * - Layer 1: Container parsing (uft_hfe_container.h)
 * - Layer 2: Geometry detection (THIS)
 * - Layer 3: Track decoding (THIS)
 * 
 * @version 2.11.0
 * @date 2024-12-27
 */

#ifndef UFT_HFE_H
#define UFT_HFE_H

#include "uft_hfe_container.h"
#include "uft/uft_geometry.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief HFE complete context (all layers)
 */
typedef struct {
    /* Layer 1: Container */
    uft_hfe_container_t* container;
    
    /* Layer 2: Geometry */
    uft_geometry_t geometry;
    bool geometry_detected;
    
    /* Layer 3: Flux profile */
    const uft_flux_profile_t* flux_profile;
    
    /* Capabilities */
    bool supports_track_api;
    bool supports_sector_api;
    
} uft_hfe_ctx_t;

/**
 * @brief LAYER 2: Detect geometry from HFE container
 * 
 * Uses multi-source heuristics:
 * 1. Header fields (track_count, side_count)
 * 2. Track analysis
 * 3. Encoding type
 * 
 * @param[in] container HFE container (Layer 1)
 * @param[out] geometry Detected geometry
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_detect_geometry(
    const uft_hfe_container_t* container,
    uft_geometry_t* geometry
);

/**
 * @brief LAYER 3: Decode HFE track to bitstream
 * 
 * HFE uses interleaved encoding:
 * - Bytes are interleaved (side 0, side 1, side 0, side 1...)
 * - v1: 256-byte blocks
 * - v3: 512-byte blocks
 * 
 * @param[in] container HFE container
 * @param track Track number
 * @param side Side number
 * @param[out] bitstream Decoded bitstream (malloc'd, caller frees)
 * @param[out] bit_count Number of bits
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_decode_track_bitstream(
    uft_hfe_container_t* container,
    uint8_t track,
    uint8_t side,
    uint8_t** bitstream,
    uint32_t* bit_count
);

/**
 * @brief Open HFE file (complete - all layers)
 * 
 * @param[in] path Path to .hfe file
 * @param[out] ctx Complete HFE context
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_open(const char* path, uft_hfe_ctx_t** ctx);

/**
 * @brief Close HFE file
 * 
 * @param[in,out] ctx HFE context
 */
void uft_hfe_close(uft_hfe_ctx_t** ctx);

/**
 * @brief Read sector (logical API)
 * 
 * Decodes track and extracts sector.
 * 
 * @param[in] ctx HFE context
 * @param cylinder Cylinder number
 * @param head Head number
 * @param sector Sector number
 * @param[out] buffer Sector data buffer
 * @param buffer_size Buffer size
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_read_sector(
    uft_hfe_ctx_t* ctx,
    uint32_t cylinder,
    uint32_t head,
    uint32_t sector,
    uint8_t* buffer,
    size_t buffer_size
);

/**
 * @brief Read track (physical API)
 * 
 * Returns decoded track data.
 * 
 * @param[in] ctx HFE context
 * @param track Track number
 * @param head Head number
 * @param[out] bitstream Track bitstream
 * @param[out] bit_count Bit count
 * @return UFT_SUCCESS or error code
 */
uft_rc_t uft_hfe_read_track(
    uft_hfe_ctx_t* ctx,
    uint8_t track,
    uint8_t head,
    uint8_t** bitstream,
    uint32_t* bit_count
);

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_H */
