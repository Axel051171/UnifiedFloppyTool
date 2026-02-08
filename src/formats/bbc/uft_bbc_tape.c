/**
 * @file uft_bbc_tape.c
 * @brief BBC Micro Tape Audio Decoding Implementation
 * 
 * EXT-010: BBC tape format support
 * Based on bbctapedisc by W.H.Scholten, R.Schmidt, Thomas Harte, Jon Welch
 * 
 * @version 1.0.0
 * @date 2026-01-04
 * @author UFT Team
 * 
 * SPDX-License-Identifier: MIT
 */

#include "uft/formats/uft_bbc_tape.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*===========================================================================
 * CRC-16 (BBC Tape uses CRC-16-CCITT, MSB first)
 *===========================================================================*/

static const uint16_t crc16_table[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
    0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
    0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
    0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
    0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
    0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
    0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
    0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
    0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
    0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
    0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
    0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
    0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
    0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
    0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
    0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
    0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

static uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    return (crc << 8) ^ crc16_table[((crc >> 8) ^ byte) & 0xFF];
}

static uint16_t crc16_block(const uint8_t *data, size_t len) {
    uint16_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc = crc16_update(crc, data[i]);
    }
    return crc;
}

/*===========================================================================
 * WAV File Handling
 *===========================================================================*/

/**
 * @brief Parse WAV file header
 */
int uft_wav_parse_header(const uint8_t *data, size_t size,
                         uft_wav_header_t *header)
{
    if (!data || !header || size < 44) {
        return -1;
    }
    
    if (!uft_wav_is_valid(data, size)) {
        return -2;
    }
    
    /* Copy header */
    memcpy(header, data, sizeof(uft_wav_header_t));
    
    /* Validate */
    if (header->audio_format != UFT_WAV_FORMAT_PCM) {
        return -3;  /* Only PCM supported */
    }
    
    if (header->bits_per_sample != 8 && header->bits_per_sample != 16) {
        return -4;  /* Only 8/16-bit supported */
    }
    
    if (header->num_channels < 1 || header->num_channels > 2) {
        return -5;  /* Only mono/stereo supported */
    }
    
    return 0;
}

/**
 * @brief Get audio samples from WAV data (normalized to 8-bit unsigned)
 */
size_t uft_wav_get_samples(const uint8_t *data, size_t size,
                           uint8_t *samples, size_t max_samples)
{
    uft_wav_header_t header;
    
    if (uft_wav_parse_header(data, size, &header) != 0) {
        return 0;
    }
    
    /* Find data chunk (may not be at offset 44 if extra chunks present) */
    size_t data_offset = 12;  /* After RIFF header */
    
    while (data_offset + 8 < size) {
        uint32_t chunk_size = data[data_offset + 4] | 
                              (data[data_offset + 5] << 8) |
                              ((uint32_t)data[data_offset + 6] << 16) |
                              ((uint32_t)data[data_offset + 7] << 24);
        
        if (memcmp(&data[data_offset], "data", 4) == 0) {
            data_offset += 8;
            break;
        }
        
        data_offset += 8 + chunk_size;
        if (chunk_size & 1) data_offset++;  /* Pad to word boundary */
    }
    
    if (data_offset >= size) {
        return 0;
    }
    
    /* Extract samples */
    const uint8_t *audio = data + data_offset;
    size_t audio_size = size - data_offset;
    size_t sample_count = 0;
    size_t pos = 0;
    
    int bytes_per_sample = header.bits_per_sample / 8;
    int channels = header.num_channels;
    
    while (pos + bytes_per_sample * channels <= audio_size && 
           sample_count < max_samples) {
        int sample;
        
        if (header.bits_per_sample == 8) {
            /* 8-bit unsigned */
            sample = audio[pos];
        } else {
            /* 16-bit signed, convert to 8-bit unsigned */
            int16_t s16 = (int16_t)(audio[pos] | (audio[pos + 1] << 8));
            sample = (s16 >> 8) + 128;
        }
        
        /* Average channels if stereo */
        if (channels == 2) {
            int sample2;
            if (header.bits_per_sample == 8) {
                sample2 = audio[pos + 1];
            } else {
                int16_t s16 = (int16_t)(audio[pos + 2] | (audio[pos + 3] << 8));
                sample2 = (s16 >> 8) + 128;
            }
            sample = (sample + sample2) / 2;
        }
        
        samples[sample_count++] = (uint8_t)sample;
        pos += bytes_per_sample * channels;
    }
    
    return sample_count;
}

/*===========================================================================
 * BBC Tape Decoder
 *===========================================================================*/

/**
 * @brief Initialize tape decoder
 */
int uft_bbc_tape_decoder_init(uft_bbc_tape_decoder_t *decoder,
                               int sample_rate)
{
    if (!decoder) {
        return -1;
    }
    
    if (sample_rate < UFT_BBC_MIN_SAMPLE_RATE) {
        return -2;
    }
    
    memset(decoder, 0, sizeof(*decoder));
    
    decoder->sample_rate = sample_rate;
    decoder->bit_length = (float)sample_rate / UFT_BBC_BAUD_RATE;
    decoder->average_flank = decoder->bit_length / 2.0f;
    decoder->bit_flank_sign = 1;
    decoder->top = 192;
    decoder->bottom = 64;
    
    /* Allocate circular buffer */
    decoder->buffer_size = 256;
    decoder->buffer = (int*)calloc(decoder->buffer_size, sizeof(int));
    if (!decoder->buffer) {
        return -3;
    }
    
    return 0;
}

/**
 * @brief Free decoder resources
 */
void uft_bbc_tape_decoder_free(uft_bbc_tape_decoder_t *decoder)
{
    if (!decoder) return;
    
    if (decoder->buffer) {
        free(decoder->buffer);
        decoder->buffer = NULL;
    }
}

/**
 * @brief Find zero crossing in audio samples
 */
static int find_zero_crossing(const uint8_t *samples, size_t num_samples,
                               size_t start, int threshold, int direction)
{
    int mid = 128;
    
    for (size_t i = start; i + 1 < num_samples; i++) {
        int s1 = (int)samples[i] - mid;
        int s2 = (int)samples[i + 1] - mid;
        
        if (direction > 0) {
            /* Looking for positive crossing */
            if (s1 <= threshold && s2 > threshold) {
                return (int)i;
            }
        } else {
            /* Looking for negative crossing */
            if (s1 >= -threshold && s2 < -threshold) {
                return (int)i;
            }
        }
    }
    
    return -1;  /* Not found */
}

/**
 * @brief Decode a single bit from audio
 */
static int decode_bit(uft_bbc_tape_decoder_t *decoder,
                      const uint8_t *samples, size_t num_samples,
                      size_t *pos)
{
    /* Find two zero crossings */
    int cross1 = find_zero_crossing(samples, num_samples, *pos, 10, 
                                     decoder->bit_flank_sign);
    if (cross1 < 0) return -1;
    
    int cross2 = find_zero_crossing(samples, num_samples, cross1 + 1, 10,
                                     -decoder->bit_flank_sign);
    if (cross2 < 0) return -1;
    
    int half_cycle = cross2 - cross1;
    
    /* Update average */
    decoder->average_flank = 0.9f * decoder->average_flank + 0.1f * half_cycle;
    
    /* Decide bit value based on half-cycle length */
    /* '0' = one cycle of 1200 Hz (long half-cycles) */
    /* '1' = two cycles of 2400 Hz (short half-cycles) */
    float threshold = decoder->bit_length / 3.0f;
    
    *pos = cross2;
    
    if (half_cycle > threshold) {
        /* Long half-cycle = part of '0' bit */
        /* Need another half-cycle to complete */
        int cross3 = find_zero_crossing(samples, num_samples, cross2 + 1, 10,
                                         decoder->bit_flank_sign);
        if (cross3 >= 0) {
            *pos = cross3;
        }
        return 0;
    } else {
        /* Short half-cycle = part of '1' bit */
        /* Skip the second 2400 Hz cycle */
        int cross3 = find_zero_crossing(samples, num_samples, cross2 + 1, 10,
                                         -decoder->bit_flank_sign);
        int cross4 = find_zero_crossing(samples, num_samples, 
                                         cross3 >= 0 ? cross3 + 1 : cross2 + 1,
                                         10, decoder->bit_flank_sign);
        if (cross4 >= 0) {
            *pos = cross4;
        }
        return 1;
    }
}

/**
 * @brief Decode a byte from audio
 */
static int decode_byte(uft_bbc_tape_decoder_t *decoder,
                       const uint8_t *samples, size_t num_samples,
                       size_t *pos, uint8_t *byte_out)
{
    /* Start bit (should be 0) */
    int start = decode_bit(decoder, samples, num_samples, pos);
    if (start != 0) {
        return -1;  /* No valid start bit */
    }
    
    /* Data bits (LSB first) */
    uint8_t value = 0;
    for (int i = 0; i < 8; i++) {
        int bit = decode_bit(decoder, samples, num_samples, pos);
        if (bit < 0) return -2;
        if (bit) value |= (1 << i);
    }
    
    /* Stop bit (should be 1) */
    int stop = decode_bit(decoder, samples, num_samples, pos);
    if (stop != 1) {
        return -3;  /* Invalid stop bit */
    }
    
    *byte_out = value;
    return 0;
}

/**
 * @brief Wait for carrier tone
 */
static int wait_for_carrier(uft_bbc_tape_decoder_t *decoder,
                            const uint8_t *samples, size_t num_samples,
                            size_t *pos)
{
    int consecutive_ones = 0;
    
    while (*pos < num_samples && consecutive_ones < 16) {
        int bit = decode_bit(decoder, samples, num_samples, pos);
        if (bit == 1) {
            consecutive_ones++;
        } else if (bit == 0) {
            consecutive_ones = 0;
        } else {
            break;
        }
    }
    
    return (consecutive_ones >= 16) ? 0 : -1;
}

/**
 * @brief Wait for sync byte (0x2A)
 */
static int wait_for_sync(uft_bbc_tape_decoder_t *decoder,
                         const uint8_t *samples, size_t num_samples,
                         size_t *pos)
{
    uint8_t byte;
    int attempts = 0;
    
    while (*pos < num_samples && attempts < 1000) {
        if (decode_byte(decoder, samples, num_samples, pos, &byte) == 0) {
            if (byte == UFT_BBC_SYNC_BYTE) {
                return 0;
            }
        }
        attempts++;
    }
    
    return -1;
}

/*===========================================================================
 * UEF Format Support
 *===========================================================================*/

/**
 * @brief Parse UEF file and extract tape blocks
 */
int uft_uef_parse(const uint8_t *data, size_t size,
                  uint8_t *output, size_t max_output, size_t *output_size)
{
    if (!data || !output || !output_size) {
        return -1;
    }
    
    if (!uft_uef_is_valid(data, size)) {
        return -2;
    }
    
    *output_size = 0;
    
    /* Skip header */
    size_t pos = 12;
    
    while (pos + 6 <= size) {
        uint16_t chunk_type = data[pos] | (data[pos + 1] << 8);
        uint32_t chunk_len = data[pos + 2] | (data[pos + 3] << 8) |
                             ((uint32_t)data[pos + 4] << 16) | ((uint32_t)data[pos + 5] << 24);
        
        pos += 6;
        
        if (pos + chunk_len > size) {
            break;
        }
        
        switch (chunk_type) {
            case UFT_UEF_IMPLICIT_DATA:
            case UFT_UEF_DEFINED_FORMAT:
                /* Copy raw data */
                if (*output_size + chunk_len <= max_output) {
                    memcpy(output + *output_size, data + pos, chunk_len);
                    *output_size += chunk_len;
                }
                break;
                
            case UFT_UEF_CARRIER_TONE:
            case UFT_UEF_INTEGER_GAP:
            case UFT_UEF_FLOAT_GAP:
                /* Gap/carrier - skip */
                break;
                
            default:
                /* Unknown chunk - skip */
                break;
        }
        
        pos += chunk_len;
    }
    
    return 0;
}

/*===========================================================================
 * CSW Format Support
 *===========================================================================*/

/**
 * @brief Parse CSW file header
 */
int uft_csw_parse_header(const uint8_t *data, size_t size,
                         uft_csw_header_t *header)
{
    if (!data || !header || size < sizeof(uft_csw_header_t)) {
        return -1;
    }
    
    if (!uft_csw_is_valid(data, size)) {
        return -2;
    }
    
    memcpy(header, data, sizeof(uft_csw_header_t));
    return 0;
}

/**
 * @brief Decompress CSW RLE data to samples
 */
size_t uft_csw_decompress_rle(const uint8_t *data, size_t data_size,
                               uint8_t *samples, size_t max_samples)
{
    size_t pos = 0;
    size_t sample_count = 0;
    uint8_t level = 0;  /* Toggle between 0 and 255 */
    
    while (pos < data_size && sample_count < max_samples) {
        uint32_t run_length;
        
        if (data[pos] != 0) {
            run_length = data[pos];
            pos++;
        } else {
            /* Extended run length (little-endian 32-bit) */
            if (pos + 5 > data_size) break;
            run_length = data[pos + 1] | (data[pos + 2] << 8) |
                         ((uint32_t)data[pos + 3] << 16) | ((uint32_t)data[pos + 4] << 24);
            pos += 5;
        }
        
        /* Fill samples with current level */
        while (run_length > 0 && sample_count < max_samples) {
            samples[sample_count++] = level;
            run_length--;
        }
        
        /* Toggle level */
        level = (level == 0) ? 255 : 0;
    }
    
    return sample_count;
}
