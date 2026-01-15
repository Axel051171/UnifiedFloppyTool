/**
 * @file uft_unified_decoder.h
 * @brief Unified Decoder Interface for All Disk Encodings
 * 
 * P0-DC-001: Unified-Decoder-Interface
 * 
 * Provides a consistent interface for all disk encoding types:
 * - MFM (IBM PC, Atari ST, Amiga)
 * - FM (older systems)
 * - GCR (Apple, Commodore, Victor 9000)
 * - Apple (6+2, ProDOS)
 * - And custom/protection encodings
 * 
 * Design Goals:
 * - Single API for all encodings
 * - Pluggable decoder registration
 * - Auto-detection support
 * - Consistent error handling
 * - Confidence scoring
 */

#ifndef UFT_UNIFIED_DECODER_H
#define UFT_UNIFIED_DECODER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

typedef struct uft_bitstream uft_bitstream_t;

/*===========================================================================
 * Error Codes
 *===========================================================================*/

typedef enum {
    UFT_DEC_OK = 0,             /**< Success */
    UFT_DEC_ERR_INVALID_ARG,    /**< Invalid argument */
    UFT_DEC_ERR_NO_MEMORY,      /**< Memory allocation failed */
    UFT_DEC_ERR_NOT_DETECTED,   /**< Encoding not detected */
    UFT_DEC_ERR_DECODE_FAILED,  /**< Decode operation failed */
    UFT_DEC_ERR_ENCODE_FAILED,  /**< Encode operation failed */
    UFT_DEC_ERR_CRC_ERROR,      /**< CRC/checksum error */
    UFT_DEC_ERR_NO_SYNC,        /**< Sync pattern not found */
    UFT_DEC_ERR_TRUNCATED,      /**< Data truncated */
    UFT_DEC_ERR_NOT_REGISTERED, /**< Decoder not registered */
    UFT_DEC_ERR_UNSUPPORTED,    /**< Operation not supported */
} uft_dec_error_t;

/*===========================================================================
 * Encoding Types
 *===========================================================================*/

/**
 * @brief Disk encoding types
 */
typedef enum {
    /* Frequency Modulation */
    UFT_ENC_FM = 0,             /**< Single-density FM */
    
    /* Modified FM */
    UFT_ENC_MFM,                /**< Double-density MFM (IBM) */
    UFT_ENC_MFM_HD,             /**< High-density MFM */
    UFT_ENC_MFM_AMIGA,          /**< Amiga MFM (word-aligned) */
    
    /* Group Coded Recording */
    UFT_ENC_GCR_APPLE_525,      /**< Apple II 5.25" (6+2) */
    UFT_ENC_GCR_APPLE_35,       /**< Apple 3.5" (GCR) */
    UFT_ENC_GCR_C64,            /**< Commodore 64 GCR */
    UFT_ENC_GCR_C128,           /**< Commodore 128 GCR */
    UFT_ENC_GCR_VICTOR9K,       /**< Victor 9000 GCR */
    UFT_ENC_GCR_MAC,            /**< Macintosh GCR */
    
    /* Special */
    UFT_ENC_RAW,                /**< Raw bitstream (no encoding) */
    UFT_ENC_CUSTOM,             /**< Custom/unknown encoding */
    UFT_ENC_PROTECTED,          /**< Copy-protected encoding */
    
    UFT_ENC_COUNT               /**< Number of encoding types */
} uft_encoding_t;

/*===========================================================================
 * Core Structures
 *===========================================================================*/

/**
 * @brief Sector data structure
 */
struct uft_sector {
    uint8_t  track;             /**< Physical track number */
    uint8_t  side;              /**< Disk side (0 or 1) */
    uint8_t  sector;            /**< Sector number */
    uint8_t  size_code;         /**< Size code (0=128, 1=256, 2=512, 3=1024) */
    
    uint8_t  *data;             /**< Sector data */
    size_t   data_size;         /**< Data size in bytes */
    
    /* Metadata */
    uint16_t calculated_crc;    /**< Calculated CRC */
    uint16_t stored_crc;        /**< CRC from disk */
    bool     crc_valid;         /**< CRC validation result */
    bool     deleted;           /**< Deleted data mark */
    
    /* Quality metrics */
    float    confidence;        /**< Decode confidence (0-1) */
    uint16_t bit_errors;        /**< Estimated bit errors */
    uint16_t weak_bits;         /**< Weak bit count */
    
    /* Position information */
    uint32_t start_bit;         /**< Start position in bitstream */
    uint32_t end_bit;           /**< End position in bitstream */
};

/**
 * @brief Bitstream structure
 */
struct uft_bitstream {
    uint8_t  *bits;             /**< Raw bit data */
    size_t   bit_count;         /**< Number of bits */
    size_t   capacity;          /**< Allocated capacity (bits) */
    
    /* Timing information */
    uint32_t *timing;           /**< Optional timing data (ns per bit) */
    bool     has_timing;        /**< Timing data available */
    
    /* Track info */
    uint16_t track;             /**< Track number */
    uint8_t  side;              /**< Side */
    uint16_t rpm;               /**< Drive RPM */
    
    /* Quality */
    float    avg_confidence;    /**< Average bit confidence */
};

/* Note: struct uft_track is defined in uft/uft_track.h */
#include "uft/uft_track.h"

/*===========================================================================
 * Decoder Interface
 *===========================================================================*/

/**
 * @brief Unified decoder interface
 * 
 * Each encoding type implements this interface.
 */
typedef struct {
    /** Decoder identification */
    const char *name;           /**< Human-readable name */
    const char *description;    /**< Detailed description */
    uft_encoding_t encoding;    /**< Encoding type */
    uint16_t version;           /**< Interface version */
    
    /**
     * @brief Detect if bitstream uses this encoding
     * @param bs Input bitstream
     * @param confidence Output: detection confidence (0-1)
     * @return UFT_DEC_OK if detected, UFT_DEC_ERR_NOT_DETECTED otherwise
     */
    uft_dec_error_t (*detect)(const uft_bitstream_t *bs, float *confidence);
    
    /**
     * @brief Decode bitstream to sectors
     * @param bs Input bitstream
     * @param sectors Output sector array (caller allocates)
     * @param count In: capacity, Out: sectors decoded
     * @return UFT_DEC_OK on success
     */
    uft_dec_error_t (*decode)(const uft_bitstream_t *bs, 
                               uft_sector_t *sectors, size_t *count);
    
    /**
     * @brief Encode sectors to bitstream
     * @param sectors Input sectors
     * @param count Number of sectors
     * @param bs Output bitstream (caller allocates)
     * @return UFT_DEC_OK on success
     */
    uft_dec_error_t (*encode)(const uft_sector_t *sectors, size_t count,
                               uft_bitstream_t *bs);
    
    /**
     * @brief Validate a sector's data integrity
     * @param sector Sector to validate
     * @return UFT_DEC_OK if valid, UFT_DEC_ERR_CRC_ERROR if not
     */
    uft_dec_error_t (*validate)(const uft_sector_t *sector);
    
    /**
     * @brief Get expected sector count for track
     * @param track Track number
     * @param side Side
     * @return Expected sector count or 0 if variable
     */
    uint8_t (*expected_sectors)(uint16_t track, uint8_t side);
    
    /**
     * @brief Get expected sector size
     * @return Sector size in bytes or 0 if variable
     */
    uint16_t (*sector_size)(void);
    
    /** Optional: Advanced features */
    
    /**
     * @brief Decode with multiple interpretations
     * @param bs Input bitstream
     * @param track Output track with all candidates
     * @param max_candidates Maximum candidates per sector
     * @return UFT_DEC_OK on success
     */
    uft_dec_error_t (*decode_multi)(const uft_bitstream_t *bs,
                                     uft_track_t *track,
                                     uint8_t max_candidates);
    
    /**
     * @brief Get decoder capabilities
     * @return Capability flags
     */
    uint32_t (*capabilities)(void);
    
} uft_decoder_interface_t;

/** Decoder capability flags */
#define UFT_DEC_CAP_ENCODE          0x0001  /**< Can encode */
#define UFT_DEC_CAP_MULTI_DECODE    0x0002  /**< Multi-interpretation decode */
#define UFT_DEC_CAP_TIMING          0x0004  /**< Timing-aware decode */
#define UFT_DEC_CAP_WEAK_BITS       0x0008  /**< Weak bit detection */
#define UFT_DEC_CAP_VARIABLE_SIZE   0x0010  /**< Variable sector sizes */
#define UFT_DEC_CAP_PROTECTION      0x0020  /**< Protection detection */

/*===========================================================================
 * Registry Functions
 *===========================================================================*/

/**
 * @brief Register a decoder
 * @param decoder Decoder interface to register
 * @return UFT_DEC_OK on success
 */
uft_dec_error_t uft_dec_register(const uft_decoder_interface_t *decoder);

/**
 * @brief Unregister a decoder
 * @param encoding Encoding type to unregister
 * @return UFT_DEC_OK on success
 */
uft_dec_error_t uft_dec_unregister(uft_encoding_t encoding);

/**
 * @brief Get registered decoder
 * @param encoding Encoding type
 * @return Decoder interface or NULL if not registered
 */
const uft_decoder_interface_t *uft_dec_get(uft_encoding_t encoding);

/**
 * @brief Get decoder by name
 * @param name Decoder name
 * @return Decoder interface or NULL if not found
 */
const uft_decoder_interface_t *uft_dec_get_by_name(const char *name);

/**
 * @brief List all registered decoders
 * @param encodings Output array (caller allocates)
 * @param max_count Array capacity
 * @return Number of registered decoders
 */
size_t uft_dec_list(uft_encoding_t *encodings, size_t max_count);

/**
 * @brief Initialize all built-in decoders
 * @return Number of decoders registered
 */
int uft_dec_init_builtins(void);

/*===========================================================================
 * Auto-Detection
 *===========================================================================*/

/**
 * @brief Detection result
 */
typedef struct {
    uft_encoding_t encoding;    /**< Detected encoding */
    float confidence;           /**< Detection confidence */
    const uft_decoder_interface_t *decoder; /**< Matched decoder */
} uft_detection_result_t;

/**
 * @brief Auto-detect encoding from bitstream
 * @param bs Input bitstream
 * @param results Output array (caller allocates)
 * @param max_results Maximum results
 * @return Number of possible encodings detected
 * 
 * Results are sorted by confidence (highest first).
 */
size_t uft_dec_auto_detect(const uft_bitstream_t *bs,
                           uft_detection_result_t *results,
                           size_t max_results);

/**
 * @brief Get best encoding match
 * @param bs Input bitstream
 * @param confidence Output: detection confidence
 * @return Best matching encoding or UFT_ENC_RAW if unknown
 */
uft_encoding_t uft_dec_detect_best(const uft_bitstream_t *bs, float *confidence);

/*===========================================================================
 * High-Level Decode Functions
 *===========================================================================*/

/**
 * @brief Decode track with auto-detection
 * @param bs Input bitstream
 * @param track Output track structure
 * @return UFT_DEC_OK on success
 */
uft_dec_error_t uft_dec_decode_track(const uft_bitstream_t *bs,
                                      uft_track_t *track);

/**
 * @brief Decode track with specified encoding
 * @param bs Input bitstream
 * @param encoding Encoding to use
 * @param track Output track structure
 * @return UFT_DEC_OK on success
 */
uft_dec_error_t uft_dec_decode_track_as(const uft_bitstream_t *bs,
                                         uft_encoding_t encoding,
                                         uft_track_t *track);

/**
 * @brief Encode track to bitstream
 * @param track Input track
 * @param encoding Encoding to use
 * @param bs Output bitstream
 * @return UFT_DEC_OK on success
 */
uft_dec_error_t uft_dec_encode_track(const uft_track_t *track,
                                      uft_encoding_t encoding,
                                      uft_bitstream_t *bs);

/*===========================================================================
 * Memory Management
 *===========================================================================*/

/**
 * @brief Allocate bitstream
 * @param bit_capacity Capacity in bits
 * @param with_timing Include timing array
 * @return Allocated bitstream or NULL
 */
uft_bitstream_t *uft_bitstream_alloc(size_t bit_capacity, bool with_timing);

/**
 * @brief Free bitstream
 * @param bs Bitstream to free
 */
void uft_bitstream_free(uft_bitstream_t *bs);

/**
 * @brief Allocate sector
 * @param data_size Data capacity
 * @return Allocated sector or NULL
 */
uft_sector_t *uft_sector_alloc(size_t data_size);

/**
 * @brief Free sector
 * @param sector Sector to free
 */
void uft_sector_free(uft_sector_t *sector);

/**
 * @brief Allocate track
 * @param sector_capacity Maximum sectors
 * @return Allocated track or NULL
 */
uft_track_t *uft_track_alloc(size_t sector_capacity);

/**
 * @brief Free track
 * @param track Track to free (including sectors)
 */
void uft_track_free(uft_track_t *track);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get encoding name
 * @param encoding Encoding type
 * @return Human-readable name
 */
const char *uft_encoding_name(uft_encoding_t encoding);

/**
 * @brief Get error message
 * @param error Error code
 * @return Human-readable error message
 */
const char *uft_dec_error_str(uft_dec_error_t error);

/**
 * @brief Calculate sector data size from size code
 * @param size_code Size code (0-3)
 * @return Size in bytes (128, 256, 512, 1024)
 */
static inline uint16_t uft_sector_size_from_code(uint8_t size_code)
{
    return 128u << (size_code & 0x03);
}

/**
 * @brief Get size code from data size
 * @param size Size in bytes
 * @return Size code (0-3) or 0xFF if invalid
 */
static inline uint8_t uft_size_code_from_size(uint16_t size)
{
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        default:   return 0xFF;
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_UNIFIED_DECODER_H */
