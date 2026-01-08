/**
 * @file uft_mfm_flux.h
 * @brief MFM Flux Decoding
 */

#ifndef UFT_MFM_FLUX_H
#define UFT_MFM_FLUX_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MFM constants */
#define MFM_SYNC_WORD 0x4489
#define MFM_GAP_BYTE 0x4E

/* CRC-16-CCITT for MFM */
uint16_t uft_mfm_crc16(const uint8_t *data, size_t len);
void uft_mfm_crc16_init(uint16_t *crc);
void uft_mfm_crc16_update(uint16_t *crc, uint8_t byte);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_FLUX_H */
