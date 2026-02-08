/**
 * @file uft_crc.h
 * @brief CRC helpers for floppy formats.
 */

#ifndef UFT_CRC_H
#define UFT_CRC_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC type enumeration
 */
#ifndef UFT_CRC_TYPE_DEFINED
#define UFT_CRC_TYPE_DEFINED
typedef enum {
    CRC_TYPE_NONE = 0,
    CRC_TYPE_CRC16_CCITT,
    CRC_TYPE_CRC16_IBM,
    CRC_TYPE_CRC32,
    CRC_TYPE_CHECKSUM
} uft_crc_type_t;
#endif /* UFT_CRC_TYPE_DEFINED */

/**
 * @brief Maximum error positions for CRC correction
 */
#define UFT_CRC_MAX_ERRORS 8

/**
 * @brief CRC correction result
 */
typedef struct {
    bool corrected;                         /**< True if CRC corrected successfully */
    int error_count;                        /**< Number of errors found */
    int error_positions[UFT_CRC_MAX_ERRORS]; /**< Bit positions of errors */
    double confidence;                      /**< Correction confidence 0.0-1.0 */
} uft_crc_result_t;

/**
 * @brief Get CRC type name
 */
const char* uft_crc_type_name(uft_crc_type_t type);

/**
 * @brief Calculate CRC for data
 */
uint32_t uft_crc_calculate(uft_crc_type_t type, const uint8_t* data, size_t length);

/**
 * @brief Verify CRC matches expected value
 */
bool uft_crc_verify(uft_crc_type_t type, const uint8_t* data, size_t length, uint32_t expected);

/**
 * @brief Attempt to correct CRC errors
 */
int uft_crc_correct(uft_crc_type_t type, uint8_t* data, size_t length, int max_errors, uft_crc_result_t* result);

/**
 * @brief Calculate CRC syndrome (for error detection)
 */
uint32_t uft_crc_syndrome(uft_crc_type_t type, const uint8_t* data, size_t length);

/**
 * @brief CRC-16/CCITT ("false") used by many floppy formats (IBM PC, AmigaDOS).
 */
uint16_t uft_crc16_ccitt_false(const void *data, size_t len);

/**
 * @brief CRC-16/IBM (ARC) sometimes used in non-floppy contexts.
 */
uint16_t uft_crc16_ibm_arc(const void *data, size_t len);

/**
 * @brief CRC-32 calculation
 */
uint32_t uft_crc32(const void *data, size_t len);

/**
 * @brief Simple checksum
 */
uint16_t uft_checksum(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_H */
