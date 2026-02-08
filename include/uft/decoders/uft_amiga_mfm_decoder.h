/**
 * @file uft_amiga_mfm_decoder.h
 * @brief Amiga MFM Decoder
 */
#ifndef UFT_AMIGA_MFM_DECODER_H
#define UFT_AMIGA_MFM_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AMIGA_SECTOR_SIZE 512
#define AMIGA_SECTORS_PER_TRACK 11
#define AMIGA_MFM_SYNC 0x4489

typedef struct {
    uint8_t format;
    uint8_t track;
    uint8_t sector;
    uint8_t sectors_to_gap;
    uint32_t header_checksum;
    uint32_t data_checksum;
    uint8_t data[AMIGA_SECTOR_SIZE];
    bool header_ok;
    bool data_ok;
} uft_amiga_sector_t;

int uft_amiga_decode_track(const uint8_t *mfm_data, size_t len,
                           uft_amiga_sector_t *sectors, int max_sectors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_MFM_DECODER_H */
