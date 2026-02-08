/**
 * @file uft_floppy_utils.h
 * @brief Floppy Disk Utility Functions
 */

#ifndef UFT_FLOPPY_UTILS_H
#define UFT_FLOPPY_UTILS_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Byte order conversion */
uint16_t uft_read_le16(const uint8_t *p);
uint32_t uft_read_le32(const uint8_t *p);
uint16_t uft_read_be16(const uint8_t *p);
uint32_t uft_read_be32(const uint8_t *p);

void uft_write_le16(uint8_t *p, uint16_t v);
void uft_write_le32(uint8_t *p, uint32_t v);
void uft_write_be16(uint8_t *p, uint16_t v);
void uft_write_be32(uint8_t *p, uint32_t v);

/* CRC utilities */
uint16_t uft_crc16_ccitt_byte(uint16_t crc, uint8_t byte);
uint32_t uft_crc32_byte(uint32_t crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLOPPY_UTILS_H */
