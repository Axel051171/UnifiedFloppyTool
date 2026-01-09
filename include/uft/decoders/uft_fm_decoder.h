/**
 * @file uft_fm_decoder.h
 * @brief FM (Single Density) Decoder
 */
#ifndef UFT_FM_DECODER_H
#define UFT_FM_DECODER_H

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
    uint8_t *data;
    size_t data_size;
} uft_fm_sector_t;

int uft_fm_decode_track(const uint8_t *raw_data, size_t len,
                        uft_fm_sector_t *sectors, int max_sectors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FM_DECODER_H */
