// SPDX-License-Identifier: MIT
/*
 * track_encoder.h - UFT Track Encoder API
 * 
 * Universal track encoder system for writing disk images to various formats
 * and hardware devices.
 * 
 * Integrates encoders from HxC Floppy Emulator (Jean-François DEL NERO)
 * with UnifiedFloppyTool's UFM architecture.
 * 
 * @version 2.7.0
 * @date 2024-12-25
 */

#ifndef UFT_TRACK_ENCODER_H
#define UFT_TRACK_ENCODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * ENCODER TYPES
 *============================================================================*/

/**
 * @brief Supported track encoding types
 */
typedef enum {
    TRACK_ENC_UNKNOWN = 0,
    
    // MFM Encodings
    TRACK_ENC_IBM_MFM,          /**< IBM PC/AT MFM (250/500 kbps) */
    TRACK_ENC_AMIGA_MFM,        /**< Amiga MFM (with copy protection support) */
    
    // GCR Encodings  
    TRACK_ENC_C64_GCR,          /**< Commodore 64 GCR */
    TRACK_ENC_APPLE_GCR,        /**< Apple II GCR (5-and-3, 6-and-2) */
    
    // FM Encoding
    TRACK_ENC_FM,               /**< Single-density FM */
    
    // Future
    TRACK_ENC_CUSTOM            /**< Custom encoder */
} track_encoder_type_t;

/*============================================================================*
 * ENCODER PARAMETERS
 *============================================================================*/

/**
 * @brief IBM MFM encoding parameters
 */
typedef struct {
    uint8_t sectors_per_track;  /**< Usually 9, 11, 18 */
    uint16_t sector_size;       /**< 128, 256, 512, 1024 bytes */
    uint16_t bitrate_kbps;      /**< 250, 300, 500 kbps */
    uint16_t rpm;               /**< 300 or 360 RPM */
    uint8_t gap3_length;        /**< GAP3 length (default: 54) */
} track_enc_ibm_params_t;

/**
 * @brief Amiga MFM encoding parameters
 */
typedef struct {
    uint8_t sectors_per_track;  /**< Usually 11 */
    uint16_t sector_size;       /**< Usually 512 */
    bool long_track;            /**< Enable long track (copy protection!) */
    uint32_t custom_length;     /**< Custom track length (0 = auto) */
} track_enc_amiga_params_t;

/**
 * @brief C64 GCR encoding parameters
 */
typedef struct {
    uint8_t track_number;       /**< 1-40 (determines sector count) */
    uint8_t sectors_per_track;  /**< 17-21 (auto from track if 0) */
} track_enc_c64_params_t;

/**
 * @brief Universal track encoder parameters
 */
typedef struct {
    track_encoder_type_t type;  /**< Encoder type */
    
    // Format-specific parameters
    union {
        track_enc_ibm_params_t ibm;
        track_enc_amiga_params_t amiga;
        track_enc_c64_params_t c64;
    } params;
    
    // Common flags
    bool preserve_weak_bits;    /**< Preserve weak bit patterns */
    bool deleted_dam;           /**< Use deleted DAM (0xF8) */
    
} track_encoder_params_t;

/*============================================================================*
 * ENCODER OUTPUT
 *============================================================================*/

/**
 * @brief Track encoder output
 */
typedef struct {
    uint8_t *bitstream;         /**< MFM/GCR/FM bitstream */
    size_t bitstream_size;      /**< Size in bytes */
    size_t bitstream_bits;      /**< Size in bits */
    
    uint32_t track_length;      /**< Track length in bytes */
    uint16_t bitrate_kbps;      /**< Actual bitrate */
    
    // Statistics
    uint32_t gap_bytes;         /**< Total gap bytes */
    uint32_t sync_marks;        /**< Number of sync marks */
    uint32_t sectors_encoded;   /**< Sectors successfully encoded */
    
} track_encoder_output_t;

/*============================================================================*
 * CORE API
 *============================================================================*/

/**
 * @brief Initialize track encoder subsystem
 * 
 * @return 0 on success, -1 on error
 */
int track_encoder_init(void);

/**
 * @brief Shutdown track encoder subsystem
 */
void track_encoder_shutdown(void);

/**
 * @brief Encode UFM track to bitstream
 * 
 * Takes logical sector data from UFM track and generates a
 * properly encoded bitstream (MFM/GCR/FM) ready for writing
 * to hardware or flux files.
 * 
 * SYNERGY WITH X-COPY:
 *   If track->cp_flags & UFM_CP_LONGTRACK is set,
 *   the encoder will create a longer track for copy protection!
 * 
 * @param track UFM track with logical sectors
 * @param params Encoder parameters
 * @param output Encoder output (allocated by this function)
 * @return 0 on success, negative on error
 */
int track_encode(
    const void *track,              // ufm_track_t pointer
    const track_encoder_params_t *params,
    track_encoder_output_t *output
);

/**
 * @brief Free encoder output
 * 
 * @param output Encoder output to free
 */
void track_encoder_free_output(track_encoder_output_t *output);

/*============================================================================*
 * ENCODER REGISTRY
 *============================================================================*/

/**
 * @brief Get default parameters for encoder type
 * 
 * @param type Encoder type
 * @param params Parameters to fill (output)
 * @return 0 on success, -1 if type not supported
 */
int track_encoder_get_defaults(
    track_encoder_type_t type,
    track_encoder_params_t *params
);

/**
 * @brief Auto-detect encoder type from UFM track
 * 
 * Uses bootblock, filesystem, and metadata to determine
 * the best encoder.
 * 
 * SYNERGY WITH BOOTBLOCK DB (v2.6.3):
 *   Detects Amiga/PC/C64 from bootblock signature!
 * 
 * @param track UFM track
 * @return Detected encoder type (TRACK_ENC_UNKNOWN if can't detect)
 */
track_encoder_type_t track_encoder_auto_detect(const void *track);

/*============================================================================*
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * @brief Get encoder type name
 */
const char* track_encoder_type_name(track_encoder_type_t type);

/**
 * @brief Calculate nominal track length
 * 
 * @param bitrate_kbps Bitrate in kbps (250, 500, etc.)
 * @param rpm RPM (300, 360)
 * @return Track length in bytes
 */
uint32_t track_encoder_calc_length(uint16_t bitrate_kbps, uint16_t rpm);

/**
 * @brief Validate encoder parameters
 * 
 * @param params Parameters to validate
 * @return 0 if valid, -1 if invalid
 */
int track_encoder_validate_params(const track_encoder_params_t *params);

/*============================================================================*
 * STATISTICS
 *============================================================================*/

/**
 * @brief Track encoder statistics
 */
typedef struct {
    uint64_t tracks_encoded;    /**< Total tracks encoded */
    uint64_t bytes_encoded;     /**< Total bytes encoded */
    uint64_t long_tracks;       /**< Long tracks (copy protection) */
    uint64_t errors;            /**< Encoding errors */
} track_encoder_stats_t;

/**
 * @brief Get encoder statistics
 */
void track_encoder_get_stats(track_encoder_stats_t *stats);

/**
 * @brief Reset encoder statistics
 */
void track_encoder_reset_stats(void);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_ENCODER_H */
