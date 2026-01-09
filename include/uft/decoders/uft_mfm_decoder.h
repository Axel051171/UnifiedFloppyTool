/**
 * @file uft_mfm_decoder.h
 * @brief Generic MFM Decoder
 */
#ifndef UFT_MFM_DECODER_H
#define UFT_MFM_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t cylinder;
    uint8_t head;
    uint8_t sector;
    uint8_t size_code;
    uint16_t crc;
    bool crc_ok;
    bool deleted;
    uint8_t *data;
    size_t data_size;
} uft_mfm_sector_t;

int uft_mfm_decode_track(const uint8_t *raw_data, size_t len,
                         uft_mfm_sector_t *sectors, int max_sectors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_DECODER_H */
