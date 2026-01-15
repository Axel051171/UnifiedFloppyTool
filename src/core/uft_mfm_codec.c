/**
 * @file uft_mfm_codec.c
 * @brief MFM/FM Encoder and Decoder Implementation
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#include "uft/uft_mfm_codec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*===========================================================================
 * CRC-CCITT TABLE
 *===========================================================================*/

static const uint16_t crc_table[256] = {
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

/*===========================================================================
 * CODEC CONTEXT
 *===========================================================================*/

struct uft_mfm_codec {
    uft_codec_options_t opts;
    uint32_t bit_time_ns;       /* Bit cell time in nanoseconds */
};

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

void uft_mfm_codec_default_options(uft_codec_options_t *opts) {
    if (!opts) return;
    
    opts->encoding = UFT_ENC_MFM;
    opts->data_rate = UFT_RATE_500K;
    opts->rpm = 300;
    opts->use_pll = true;
    opts->pll_window = 15;      /* 15% window */
    opts->strict_crc = false;
    opts->ignore_weak = false;
}

uft_mfm_codec_t* uft_mfm_codec_create(void) {
    uft_codec_options_t opts;
    uft_mfm_codec_default_options(&opts);
    return uft_mfm_codec_create_ex(&opts);
}

uft_mfm_codec_t* uft_mfm_codec_create_ex(const uft_codec_options_t *opts) {
    uft_mfm_codec_t *codec = (uft_mfm_codec_t *)calloc(1, sizeof(*codec));
    if (!codec) return NULL;
    
    if (opts) {
        codec->opts = *opts;
    } else {
        uft_mfm_codec_default_options(&codec->opts);
    }
    
    /* Calculate bit time */
    codec->bit_time_ns = 1000000000UL / codec->opts.data_rate;
    
    return codec;
}

void uft_mfm_codec_destroy(uft_mfm_codec_t *codec) {
    free(codec);
}

int uft_mfm_codec_set_options(uft_mfm_codec_t *codec,
                               const uft_codec_options_t *opts) {
    if (!codec || !opts) return -1;
    codec->opts = *opts;
    codec->bit_time_ns = 1000000000UL / codec->opts.data_rate;
    return 0;
}

/*===========================================================================
 * CRC CALCULATION
 *===========================================================================*/

uint16_t uft_disk_crc_init(void) {
    return 0xFFFF;
}

uint16_t uft_disk_crc_update(uint16_t crc, uint8_t byte) {
    return (crc << 8) ^ crc_table[(crc >> 8) ^ byte];
}

uint16_t uft_disk_crc_final(uint16_t crc) {
    return crc;
}

uint16_t uft_disk_crc(const uint8_t *data, size_t len) {
    uint16_t crc = uft_disk_crc_init();
    for (size_t i = 0; i < len; i++) {
        crc = uft_disk_crc_update(crc, data[i]);
    }
    return uft_disk_crc_final(crc);
}

/*===========================================================================
 * MFM ENCODING
 *===========================================================================*/

uint16_t uft_mfm_encode_byte(uint8_t data, int prev_bit) {
    uint16_t mfm = 0;
    
    for (int i = 7; i >= 0; i--) {
        int bit = (data >> i) & 1;
        int clock;
        
        /* MFM rule: clock = 1 only if both data bits are 0 */
        if (!bit && !prev_bit) {
            clock = 1;
        } else {
            clock = 0;
        }
        
        mfm = (mfm << 2) | (clock << 1) | bit;
        prev_bit = bit;
    }
    
    return mfm;
}

uint16_t uft_mfm_encode_sync(void) {
    /* 0xA1 with missing clock = 0x4489 */
    /* Normal 0xA1 would be 0x44A9 */
    return 0x4489;
}

int uft_mfm_encode(const uint8_t *data, size_t data_len,
                   uint8_t *mfm, size_t mfm_size) {
    if (!data || !mfm || mfm_size < data_len * 2) return -1;
    
    int prev_bit = 0;
    size_t out_pos = 0;
    
    for (size_t i = 0; i < data_len; i++) {
        uint16_t encoded = uft_mfm_encode_byte(data[i], prev_bit);
        
        mfm[out_pos++] = (encoded >> 8) & 0xFF;
        mfm[out_pos++] = encoded & 0xFF;
        
        prev_bit = data[i] & 1;
    }
    
    return (int)out_pos;
}

/*===========================================================================
 * FM ENCODING
 *===========================================================================*/

uint16_t uft_fm_encode_byte(uint8_t data) {
    uint16_t fm = 0;
    
    /* FM: clock always 1, data bits interleaved */
    for (int i = 7; i >= 0; i--) {
        int bit = (data >> i) & 1;
        fm = (fm << 2) | (1 << 1) | bit;  /* Clock always 1 */
    }
    
    return fm;
}

uint16_t uft_fm_encode_mark(uint8_t mark) {
    /* Address marks have specific clock patterns */
    switch (mark) {
        case 0xFC: return 0xD7FC;  /* Index AM */
        case 0xFE: return 0xF57E;  /* ID AM */
        case 0xFB: return 0xF56F;  /* Data AM */
        case 0xF8: return 0xF56A;  /* Deleted AM */
        default:   return uft_fm_encode_byte(mark);
    }
}

int uft_fm_encode(const uint8_t *data, size_t data_len,
                  uint8_t *fm, size_t fm_size) {
    if (!data || !fm || fm_size < data_len * 2) return -1;
    
    size_t out_pos = 0;
    
    for (size_t i = 0; i < data_len; i++) {
        uint16_t encoded = uft_fm_encode_byte(data[i]);
        
        fm[out_pos++] = (encoded >> 8) & 0xFF;
        fm[out_pos++] = encoded & 0xFF;
    }
    
    return (int)out_pos;
}

/*===========================================================================
 * MFM DECODING
 *===========================================================================*/

uint8_t uft_mfm_decode_byte(uint16_t mfm) {
    uint8_t data = 0;
    
    /* Extract data bits (odd positions) */
    for (int i = 0; i < 8; i++) {
        int bit_pos = (7 - i) * 2;  /* Position of data bit */
        data = (data << 1) | ((mfm >> bit_pos) & 1);
    }
    
    return data;
}

int uft_mfm_decode(const uint8_t *mfm, size_t mfm_bits,
                   uint8_t *data, size_t data_size) {
    if (!mfm || !data) return -1;
    
    size_t data_bytes = mfm_bits / 16;
    if (data_bytes > data_size) data_bytes = data_size;
    
    for (size_t i = 0; i < data_bytes; i++) {
        size_t byte_pos = i * 2;
        uint16_t word = (mfm[byte_pos] << 8) | mfm[byte_pos + 1];
        data[i] = uft_mfm_decode_byte(word);
    }
    
    return (int)data_bytes;
}

/*===========================================================================
 * FM DECODING
 *===========================================================================*/

uint8_t uft_fm_decode_byte(uint16_t fm) {
    return uft_mfm_decode_byte(fm);  /* Same extraction */
}

int uft_fm_decode(const uint8_t *fm, size_t fm_bits,
                  uint8_t *data, size_t data_size) {
    return uft_mfm_decode(fm, fm_bits, data, data_size);
}

/*===========================================================================
 * SYNC DETECTION
 *===========================================================================*/

/**
 * @brief Get bit from bitstream
 */
static int get_bit(const uint8_t *data, int bit) {
    return (data[bit / 8] >> (7 - (bit % 8))) & 1;
}

/**
 * @brief Get 16-bit word from bitstream
 */
static uint16_t get_word(const uint8_t *data, int bit) {
    uint16_t word = 0;
    for (int i = 0; i < 16; i++) {
        word = (word << 1) | get_bit(data, bit + i);
    }
    return word;
}

int uft_mfm_find_sync(const uint8_t *mfm, size_t mfm_bits, int start_bit) {
    if (!mfm || mfm_bits < 16) return -1;
    
    /* Look for three consecutive sync words (0x4489) */
    for (size_t bit = start_bit; bit + 48 <= mfm_bits; bit++) {
        uint16_t w1 = get_word(mfm, bit);
        uint16_t w2 = get_word(mfm, bit + 16);
        uint16_t w3 = get_word(mfm, bit + 32);
        
        if (w1 == 0x4489 && w2 == 0x4489 && w3 == 0x4489) {
            return (int)bit;
        }
    }
    
    return -1;
}

int uft_mfm_find_address_mark(const uint8_t *mfm, size_t mfm_bits,
                               int start_bit, uint8_t *mark) {
    int sync = uft_mfm_find_sync(mfm, mfm_bits, start_bit);
    if (sync < 0) return -1;
    
    /* Skip three sync words */
    int mark_bit = sync + 48;
    if (mark_bit + 16 > (int)mfm_bits) return -1;
    
    /* Decode address mark byte */
    uint16_t mark_word = get_word(mfm, mark_bit);
    if (mark) *mark = uft_mfm_decode_byte(mark_word);
    
    return mark_bit;
}

/*===========================================================================
 * TRACK DECODING
 *===========================================================================*/

int uft_mfm_decode_track(uft_mfm_codec_t *codec,
                          const uint8_t *mfm, size_t mfm_bits,
                          uft_track_data_t *track) {
    if (!codec || !mfm || !track) return -1;
    
    memset(track, 0, sizeof(*track));
    track->encoding = UFT_ENC_MFM;
    track->total_bits = (int)mfm_bits;
    track->data_rate = codec->opts.data_rate;
    
    /* Allocate sectors array */
    track->sectors = (uft_sector_t *)calloc(64, sizeof(uft_sector_t));
    if (!track->sectors) return -1;
    
    int bit_pos = 0;
    int sector_count = 0;
    
    while (bit_pos + 256 < (int)mfm_bits && sector_count < 64) {
        /* Find ID address mark */
        uint8_t mark;
        int mark_pos = uft_mfm_find_address_mark(mfm, mfm_bits, bit_pos, &mark);
        if (mark_pos < 0) break;
        
        if (mark != UFT_AM_ID) {
            bit_pos = mark_pos + 16;
            continue;
        }
        
        /* Read ID field: C H S N CRC */
        int id_start = mark_pos + 16;
        if (id_start + 80 > (int)mfm_bits) break;
        
        uint8_t id_data[6];
        for (int i = 0; i < 6; i++) {
            uint16_t word = get_word(mfm, id_start + i * 16);
            id_data[i] = uft_mfm_decode_byte(word);
        }
        
        uft_sector_t *sec = &track->sectors[sector_count];
        sec->id.cylinder = id_data[0];
        sec->id.head = id_data[1];
        sec->id.sector = id_data[2];
        sec->id.size_code = id_data[3];
        sec->id.crc = (id_data[4] << 8) | id_data[5];
        
        /* Verify ID CRC */
        uint8_t crc_data[5] = {mark, id_data[0], id_data[1], id_data[2], id_data[3]};
        uint16_t calc_crc = uft_disk_crc(crc_data, 5);
        sec->id.crc_ok = (calc_crc == sec->id.crc);
        
        /* Look for data address mark */
        int data_search = id_start + 96;  /* After ID field + gap */
        int data_mark_pos = uft_mfm_find_address_mark(mfm, mfm_bits, 
                                                       data_search, &mark);
        
        if (data_mark_pos >= 0 && 
            (mark == UFT_AM_DATA || mark == UFT_AM_DEL_DATA)) {
            sec->data_mark = mark;
            sec->bit_offset = mark_pos;
            
            /* Calculate sector size */
            int sector_size = uft_sector_size_from_code(sec->id.size_code);
            sec->data_size = sector_size;
            
            /* Read data */
            int data_start = data_mark_pos + 16;
            if (data_start + (sector_size + 2) * 16 <= (int)mfm_bits) {
                sec->data = (uint8_t *)malloc(sector_size);
                if (sec->data) {
                    for (int i = 0; i < sector_size; i++) {
                        uint16_t word = get_word(mfm, data_start + i * 16);
                        sec->data[i] = uft_mfm_decode_byte(word);
                    }
                    
                    /* Read data CRC */
                    uint16_t crc_hi = get_word(mfm, data_start + sector_size * 16);
                    uint16_t crc_lo = get_word(mfm, data_start + (sector_size + 1) * 16);
                    sec->data_crc = (uft_mfm_decode_byte(crc_hi) << 8) |
                                     uft_mfm_decode_byte(crc_lo);
                    
                    /* Verify data CRC */
                    uint16_t data_crc_init = uft_disk_crc_init();
                    data_crc_init = uft_disk_crc_update(data_crc_init, mark);
                    for (int i = 0; i < sector_size; i++) {
                        data_crc_init = uft_disk_crc_update(data_crc_init, sec->data[i]);
                    }
                    sec->data_crc_ok = (uft_disk_crc_final(data_crc_init) == sec->data_crc);
                }
            }
            
            bit_pos = data_mark_pos + (sector_size + 3) * 16;
        } else {
            bit_pos = id_start + 96;
        }
        
        /* Update track info */
        if (sector_count == 0) {
            track->track_num = sec->id.cylinder;
            track->head = sec->id.head;
        }
        
        sector_count++;
    }
    
    track->sector_count = sector_count;
    return sector_count;
}

/*===========================================================================
 * FLUX CONVERSION
 *===========================================================================*/

int uft_mfm_to_flux(const uint8_t *mfm, size_t mfm_bits,
                    uint32_t data_rate,
                    uint32_t *flux, size_t max_flux) {
    if (!mfm || !flux || mfm_bits == 0 || data_rate == 0) return 0;
    
    uint32_t bit_time_ns = 1000000000UL / data_rate;
    size_t flux_count = 0;
    uint32_t accumulated = 0;
    
    for (size_t bit = 0; bit < mfm_bits && flux_count < max_flux; bit++) {
        int bit_val = get_bit(mfm, bit);
        accumulated += bit_time_ns;
        
        if (bit_val) {
            flux[flux_count++] = accumulated;
            accumulated = 0;
        }
    }
    
    return (int)flux_count;
}

int uft_flux_to_mfm(const uint32_t *flux, size_t flux_count,
                    uint32_t data_rate,
                    uint8_t *mfm, size_t max_bytes,
                    int *out_bits) {
    if (!flux || !mfm || flux_count == 0 || data_rate == 0) return -1;
    
    uint32_t bit_time_ns = 1000000000UL / data_rate;
    uint32_t half_bit = bit_time_ns / 2;
    
    memset(mfm, 0, max_bytes);
    
    size_t bit_pos = 0;
    size_t max_bits = max_bytes * 8;
    
    for (size_t i = 0; i < flux_count && bit_pos < max_bits; i++) {
        /* Quantize flux interval to bit cells */
        int cells = (flux[i] + half_bit) / bit_time_ns;
        if (cells < 1) cells = 1;
        
        /* Write zeros then a one */
        bit_pos += (cells - 1);
        
        if (bit_pos < max_bits) {
            mfm[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            bit_pos++;
        }
    }
    
    if (out_bits) *out_bits = (int)bit_pos;
    return 0;
}

/*===========================================================================
 * UTILITIES
 *===========================================================================*/

int uft_sector_size_from_code(int code) {
    switch (code) {
        case 0: return 128;
        case 1: return 256;
        case 2: return 512;
        case 3: return 1024;
        case 4: return 2048;
        case 5: return 4096;
        case 6: return 8192;
        default: return 512;
    }
}

int uft_sector_code_from_size(int size) {
    switch (size) {
        case 128:  return 0;
        case 256:  return 1;
        case 512:  return 2;
        case 1024: return 3;
        case 2048: return 4;
        case 4096: return 5;
        case 8192: return 6;
        default:   return 2;
    }
}

void uft_track_data_free(uft_track_data_t *track) {
    if (!track) return;
    
    if (track->sectors) {
        for (int i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    memset(track, 0, sizeof(*track));
}

void uft_track_data_print(const uft_track_data_t *track) {
    if (!track) return;
    
    printf("Track %d.%d: %d sectors, %d bits\n",
           track->track_num, track->head,
           track->sector_count, track->total_bits);
    
    for (int i = 0; i < track->sector_count; i++) {
        const uft_sector_t *s = &track->sectors[i];
        printf("  Sector C=%d H=%d S=%d N=%d: %s %s\n",
               s->id.cylinder, s->id.head, s->id.sector, s->id.size_code,
               s->id.crc_ok ? "ID_OK" : "ID_BAD",
               s->data_crc_ok ? "DATA_OK" : "DATA_BAD");
    }
}

const char* uft_encoding_name(uft_encoding_t enc) {
    switch (enc) {
        case UFT_ENC_FM:        return "FM";
        case UFT_ENC_MFM:       return "MFM";
        case UFT_ENC_M2FM:      return "M2FM";
        case UFT_ENC_GCR_APPLE: return "Apple GCR";
        case UFT_ENC_GCR_C64:   return "C64 GCR";
        default:                return "Unknown";
    }
}

uint8_t uft_reverse_bits(uint8_t b) {
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

int uft_popcount(uint32_t v) {
    v = v - ((v >> 1) & 0x55555555);
    v = (v & 0x33333333) + ((v >> 2) & 0x33333333);
    return ((v + (v >> 4) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/*===========================================================================
 * AMIGA MFM
 *===========================================================================*/

uint32_t uft_amiga_checksum(const uint32_t *data, int longs) {
    uint32_t checksum = 0;
    for (int i = 0; i < longs; i++) {
        checksum ^= data[i];
    }
    return checksum & 0x55555555;  /* Mask clock bits */
}
