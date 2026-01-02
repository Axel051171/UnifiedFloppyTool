/**
 * @file uft_crc.h
 * @brief CRC helpers for floppy formats.
 */

#ifndef UFT_CRC_H
#define UFT_CRC_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief CRC-16/CCITT ("false") used by many floppy formats (IBM PC, AmigaDOS).
 *
 * Parameters:
 *  - poly:   0x1021
 *  - init:   0xFFFF
 *  - refin:  false
 *  - refout: false
 *  - xorout: 0x0000
 */
uint16_t uft_crc16_ccitt_false(const void *data, size_t len);

/**
 * @brief CRC-16/IBM (ARC) sometimes used in non-floppy contexts.
 *
 * Parameters:
 *  - poly:   0x8005 (reflected)
 *  - init:   0x0000
 *  - refin:  true
 *  - refout: true
 *  - xorout: 0x0000
 */
uint16_t uft_crc16_ibm_arc(const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CRC_H */
