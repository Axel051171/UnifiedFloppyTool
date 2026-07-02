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
#ifndef UFT_CRC_TYPE_T_DEFINED
#define UFT_CRC_TYPE_T_DEFINED
typedef enum {
    CRC_TYPE_NONE = 0,
    CRC_TYPE_CRC16_CCITT,
    CRC_TYPE_CRC16_IBM,
    CRC_TYPE_CRC32,
    CRC_TYPE_CHECKSUM
} uft_crc_type_t;
#endif /* UFT_CRC_TYPE_T_DEFINED */
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
 * @brief Attempt to correct CRC errors
 */
int uft_crc_correct(uft_crc_type_t type, uint8_t* data, size_t length, int max_errors, uft_crc_result_t* result);






#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_H */
