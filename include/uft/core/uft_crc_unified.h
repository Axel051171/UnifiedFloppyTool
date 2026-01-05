/**
 * @file uft_crc_unified.h
 * @brief Unified CRC Interface (P2-ARCH-002)
 * 
 * Centralized CRC calculation for all floppy formats.
 * Combines functionality from multiple scattered implementations:
 * - uft_crc.h (basic CRC-16)
 * - uft_crc_polys.h (polynomial database)
 * - uft_crc_cached.h (caching layer)
 * - Various format-specific implementations
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_CRC_UNIFIED_H
#define UFT_CRC_UNIFIED_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * CRC Type Enumeration
 *===========================================================================*/

/**
 * @brief Supported CRC types
 */
typedef enum uft_crc_type {
    /* CRC-16 variants */
    UFT_CRC_CCITT_FALSE = 0,    /**< CRC-16/CCITT-FALSE (IBM, Amiga, etc.) */
    UFT_CRC_CCITT_TRUE,         /**< CRC-16/CCITT (X.25, HDLC) */
    UFT_CRC_IBM_ARC,            /**< CRC-16/IBM (ARC, reflected) */
    UFT_CRC_MODBUS,             /**< CRC-16/MODBUS */
    UFT_CRC_XMODEM,             /**< CRC-16/XMODEM */
    
    /* Format-specific CRC-16 */
    UFT_CRC_MFM,                /**< MFM sector CRC (CCITT with sync bytes) */
    UFT_CRC_AMIGA,              /**< Amiga header/data checksum (XOR-based) */
    UFT_CRC_GCR_C64,            /**< C64 GCR checksum (XOR) */
    UFT_CRC_GCR_APPLE,          /**< Apple II GCR checksum */
    UFT_CRC_OMTI16,             /**< OMTI 8247 header CRC-16 */
    
    /* CRC-32 variants */
    UFT_CRC32_ISO,              /**< CRC-32/ISO-HDLC (zip, ethernet) */
    UFT_CRC32_POSIX,            /**< CRC-32/POSIX (cksum) */
    UFT_CRC32_CCSDS,            /**< CRC-32/CCSDS (DEC VAX) */
    UFT_CRC32_WD,               /**< Western Digital CRC-32 */
    UFT_CRC32_SEAGATE,          /**< Seagate CRC-32 */
    UFT_CRC32_OMTI_HDR,         /**< OMTI header CRC-32 */
    UFT_CRC32_OMTI_DATA,        /**< OMTI data CRC-32 */
    
    /* Special types */
    UFT_CRC_ECC_FIRE,           /**< FIRE code ECC (used by some formats) */
    
    UFT_CRC_TYPE_COUNT
} uft_crc_type_t;

/*===========================================================================
 * CRC Configuration
 *===========================================================================*/

/**
 * @brief CRC algorithm parameters
 */
typedef struct uft_crc_params {
    uint32_t poly;          /**< Polynomial */
    uint32_t init;          /**< Initial value */
    uint32_t xor_out;       /**< XOR value for final result */
    uint8_t  width;         /**< CRC width in bits (8, 16, 32) */
    bool     reflect_in;    /**< Reflect input bytes */
    bool     reflect_out;   /**< Reflect output CRC */
} uft_crc_params_t;

/*===========================================================================
 * Basic CRC Functions
 *===========================================================================*/

/**
 * @brief Calculate CRC for data buffer
 * @param type  CRC type to use
 * @param data  Input data
 * @param len   Data length
 * @return Calculated CRC value
 */
uint32_t uft_crc_calc(uft_crc_type_t type, const void *data, size_t len);

/**
 * @brief Initialize incremental CRC calculation
 * @param type  CRC type to use
 * @return Initial CRC value
 */
uint32_t uft_crc_init(uft_crc_type_t type);

/**
 * @brief Update CRC with more data
 * @param type  CRC type
 * @param crc   Current CRC value
 * @param data  Data to add
 * @param len   Data length
 * @return Updated CRC value
 */
uint32_t uft_crc_update(uft_crc_type_t type, uint32_t crc, 
                        const void *data, size_t len);

/**
 * @brief Update CRC with single byte
 * @param type  CRC type
 * @param crc   Current CRC value
 * @param byte  Byte to add
 * @return Updated CRC value
 */
uint32_t uft_crc_update_byte(uft_crc_type_t type, uint32_t crc, uint8_t byte);

/**
 * @brief Finalize CRC calculation
 * @param type  CRC type
 * @param crc   Current CRC value
 * @return Final CRC value
 */
uint32_t uft_crc_final(uft_crc_type_t type, uint32_t crc);

/*===========================================================================
 * Convenience Functions (Direct Calculation)
 *===========================================================================*/

/**
 * @brief CRC-16/CCITT-FALSE (standard floppy CRC)
 */
static inline uint16_t uft_crc16_ccitt(const void *data, size_t len) {
    return (uint16_t)uft_crc_calc(UFT_CRC_CCITT_FALSE, data, len);
}

/**
 * @brief CRC-16/IBM (ARC)
 */
static inline uint16_t uft_crc16_ibm(const void *data, size_t len) {
    return (uint16_t)uft_crc_calc(UFT_CRC_IBM_ARC, data, len);
}

/**
 * @brief CRC-32/ISO (standard)
 */
static inline uint32_t uft_crc32(const void *data, size_t len) {
    return uft_crc_calc(UFT_CRC32_ISO, data, len);
}

/*===========================================================================
 * Format-Specific CRC Functions
 *===========================================================================*/

/**
 * @brief MFM sector CRC (IBM-style with sync bytes pre-added)
 * 
 * For MFM sectors, the CRC includes 3x 0xA1 sync bytes + address mark.
 * This function handles that automatically.
 * 
 * @param address_mark  Address mark byte (0xFE for ID, 0xFB for data)
 * @param data          Sector data
 * @param len           Data length
 * @return CRC-16 value
 */
uint16_t uft_crc_mfm_sector(uint8_t address_mark, const void *data, size_t len);

/**
 * @brief Amiga MFM checksum (XOR-based)
 * 
 * Amiga uses XOR of longwords for header/data checksums.
 * 
 * @param data  Data buffer (must be aligned to 4 bytes)
 * @param len   Length in bytes (must be multiple of 4)
 * @return Checksum value
 */
uint32_t uft_crc_amiga_checksum(const void *data, size_t len);

/**
 * @brief C64 GCR block checksum (XOR of all bytes)
 */
uint8_t uft_crc_gcr_c64(const void *data, size_t len);

/**
 * @brief Apple II GCR checksum
 */
uint8_t uft_crc_gcr_apple(const void *data, size_t len);

/**
 * @brief FM sector CRC (older single-density)
 * 
 * Similar to MFM but different sync pattern.
 */
uint16_t uft_crc_fm_sector(uint8_t address_mark, const void *data, size_t len);

/*===========================================================================
 * CRC Verification and Correction
 *===========================================================================*/

/**
 * @brief Verify CRC matches expected value
 */
static inline bool uft_crc_verify(uft_crc_type_t type, const void *data, 
                                   size_t len, uint32_t expected) {
    return uft_crc_calc(type, data, len) == expected;
}

/**
 * @brief Attempt 1-bit CRC error correction
 * 
 * Attempts to find and flip a single bit that would make
 * the CRC valid. Useful for marginal reads.
 * 
 * @param type      CRC type
 * @param data      Data buffer (will be modified if correction found)
 * @param len       Data length
 * @param stored_crc  Expected CRC from disk
 * @param bit_pos   Output: position of corrected bit (if found)
 * @return true if correction found and applied
 */
bool uft_crc_correct_1bit(uft_crc_type_t type, uint8_t *data, size_t len,
                          uint32_t stored_crc, size_t *bit_pos);

/**
 * @brief Attempt 2-bit CRC error correction
 * 
 * More expensive but can correct burst errors.
 * 
 * @param type      CRC type  
 * @param data      Data buffer (will be modified if correction found)
 * @param len       Data length (max ~256 bytes for reasonable performance)
 * @param stored_crc  Expected CRC
 * @param pos1      Output: position of first corrected bit
 * @param pos2      Output: position of second corrected bit
 * @return true if correction found and applied
 */
bool uft_crc_correct_2bit(uft_crc_type_t type, uint8_t *data, size_t len,
                          uint32_t stored_crc, size_t *pos1, size_t *pos2);

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

/**
 * @brief Get CRC type name
 */
const char* uft_crc_type_name(uft_crc_type_t type);

/**
 * @brief Get CRC parameters for a type
 */
const uft_crc_params_t* uft_crc_get_params(uft_crc_type_t type);

/**
 * @brief Get CRC width in bits
 */
static inline int uft_crc_width(uft_crc_type_t type) {
    const uft_crc_params_t *p = uft_crc_get_params(type);
    return p ? p->width : 0;
}

/**
 * @brief Calculate CRC with custom parameters
 */
uint32_t uft_crc_calc_custom(const uft_crc_params_t *params,
                             const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_UNIFIED_H */
