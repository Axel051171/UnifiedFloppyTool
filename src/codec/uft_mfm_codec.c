/**
 * @file uft_mfm_codec.c
 * @brief MFM/FM Codec Implementation
 * 
 * EXT3-015: Complete MFM/FM encoding/decoding
 * 
 * Features:
 * - MFM encoding/decoding
 * - FM encoding/decoding
 * - Clock bit handling
 * - Sync pattern detection
 * - Bit-level operations
 */

#include "uft/codec/uft_mfm_codec.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Constants
 *===========================================================================*/

/* MFM sync patterns */
#define MFM_SYNC_A1         0x4489      /* Missing clock A1 */
#define MFM_SYNC_C2         0x5224      /* Missing clock C2 */
#define MFM_SYNC_IDAM       0x5554      /* Index Address Mark */

/* FM sync patterns */
#define FM_SYNC_IDAM        0xF57E      /* Index Address Mark (FC) */
#define FM_SYNC_IAM         0xF77A      /* Index Mark (FE) */
#define FM_SYNC_DAM         0xF56F      /* Data Address Mark (FB) */
#define FM_SYNC_DDAM        0xF56A      /* Deleted Data (F8) */

/*===========================================================================
 * MFM Encoding
 *===========================================================================*/

/**
 * Encode single byte to MFM (16 bits)
 * MFM rule: clock bit = 1 if both adjacent data bits are 0
 */
uint16_t uft_mfm_encode_byte(uint8_t data, uint8_t prev_bit)
{
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        uint8_t prev = (i == 7) ? prev_bit : ((data >> (i + 1)) & 1);
        
        /* Clock bit: 1 if both adjacent data bits are 0 */
        uint8_t clock = (prev == 0 && bit == 0) ? 1 : 0;
        
        result = (result << 2) | (clock << 1) | bit;
    }
    
    return result;
}

/**
 * Encode buffer to MFM
 */
int uft_mfm_encode(const uint8_t *data, size_t data_size,
                   uint8_t *mfm, size_t *mfm_size)
{
    if (!data || !mfm || !mfm_size) return -1;
    
    size_t needed = data_size * 2;
    if (*mfm_size < needed) {
        *mfm_size = needed;
        return -1;
    }
    
    uint8_t prev_bit = 0;
    
    for (size_t i = 0; i < data_size; i++) {
        uint16_t encoded = uft_mfm_encode_byte(data[i], prev_bit);
        
        mfm[i * 2] = (encoded >> 8) & 0xFF;
        mfm[i * 2 + 1] = encoded & 0xFF;
        
        prev_bit = data[i] & 0x01;
    }
    
    *mfm_size = needed;
    return 0;
}

/**
 * Encode with special sync byte (missing clock)
 */
uint16_t uft_mfm_encode_sync_a1(void)
{
    return MFM_SYNC_A1;  /* 0x4489 - A1 with missing clock */
}

uint16_t uft_mfm_encode_sync_c2(void)
{
    return MFM_SYNC_C2;  /* 0x5224 - C2 with missing clock */
}

/*===========================================================================
 * MFM Decoding
 *===========================================================================*/

/**
 * Decode MFM (16 bits) to single byte
 */
uint8_t uft_mfm_decode_word(uint16_t mfm)
{
    uint8_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        /* Extract data bit (odd positions) */
        int bit_pos = i * 2;
        uint8_t bit = (mfm >> bit_pos) & 1;
        result |= (bit << i);
    }
    
    return result;
}

/**
 * Decode MFM buffer
 */
int uft_mfm_decode(const uint8_t *mfm, size_t mfm_size,
                   uint8_t *data, size_t *data_size)
{
    if (!mfm || !data || !data_size) return -1;
    
    size_t out_size = mfm_size / 2;
    if (*data_size < out_size) {
        *data_size = out_size;
        return -1;
    }
    
    for (size_t i = 0; i < out_size; i++) {
        uint16_t word = (mfm[i * 2] << 8) | mfm[i * 2 + 1];
        data[i] = uft_mfm_decode_word(word);
    }
    
    *data_size = out_size;
    return 0;
}

/*===========================================================================
 * FM Encoding
 *===========================================================================*/

/**
 * Encode single byte to FM (16 bits)
 * FM: clock bit always 1 between data bits
 */
uint16_t uft_fm_encode_byte(uint8_t data)
{
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        uint8_t bit = (data >> i) & 1;
        /* FM: clock always 1 */
        result = (result << 2) | (1 << 1) | bit;
    }
    
    return result;
}

/**
 * Encode buffer to FM
 */
int uft_fm_encode(const uint8_t *data, size_t data_size,
                  uint8_t *fm, size_t *fm_size)
{
    if (!data || !fm || !fm_size) return -1;
    
    size_t needed = data_size * 2;
    if (*fm_size < needed) {
        *fm_size = needed;
        return -1;
    }
    
    for (size_t i = 0; i < data_size; i++) {
        uint16_t encoded = uft_fm_encode_byte(data[i]);
        
        fm[i * 2] = (encoded >> 8) & 0xFF;
        fm[i * 2 + 1] = encoded & 0xFF;
    }
    
    *fm_size = needed;
    return 0;
}

/**
 * Encode FM with missing clock (address marks)
 */
uint16_t uft_fm_encode_mark(uint8_t mark)
{
    switch (mark) {
        case 0xFC: return FM_SYNC_IDAM;   /* Index Address Mark */
        case 0xFE: return FM_SYNC_IAM;    /* ID Address Mark */
        case 0xFB: return FM_SYNC_DAM;    /* Data Address Mark */
        case 0xF8: return FM_SYNC_DDAM;   /* Deleted Data Mark */
        default:   return uft_fm_encode_byte(mark);
    }
}

/*===========================================================================
 * FM Decoding
 *===========================================================================*/

/**
 * Decode FM (16 bits) to single byte
 */
uint8_t uft_fm_decode_word(uint16_t fm)
{
    uint8_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        int bit_pos = i * 2;
        uint8_t bit = (fm >> bit_pos) & 1;
        result |= (bit << i);
    }
    
    return result;
}

/**
 * Decode FM buffer
 */
int uft_fm_decode(const uint8_t *fm, size_t fm_size,
                  uint8_t *data, size_t *data_size)
{
    if (!fm || !data || !data_size) return -1;
    
    size_t out_size = fm_size / 2;
    if (*data_size < out_size) {
        *data_size = out_size;
        return -1;
    }
    
    for (size_t i = 0; i < out_size; i++) {
        uint16_t word = (fm[i * 2] << 8) | fm[i * 2 + 1];
        data[i] = uft_fm_decode_word(word);
    }
    
    *data_size = out_size;
    return 0;
}

/*===========================================================================
 * Sync Pattern Detection
 *===========================================================================*/

/**
 * Find MFM sync pattern in bitstream
 */
int uft_mfm_find_sync(const uint8_t *mfm, size_t mfm_size,
                      size_t start_bit, size_t *found_bit)
{
    if (!mfm || !found_bit) return -1;
    
    size_t total_bits = mfm_size * 8;
    
    for (size_t bit = start_bit; bit + 16 <= total_bits; bit++) {
        /* Extract 16 bits */
        size_t byte_idx = bit / 8;
        int bit_offset = bit % 8;
        
        uint32_t window = 0;
        for (int i = 0; i < 3 && byte_idx + i < mfm_size; i++) {
            window = (window << 8) | mfm[byte_idx + i];
        }
        
        uint16_t pattern = (window >> (8 - bit_offset)) & 0xFFFF;
        
        if (pattern == MFM_SYNC_A1) {
            *found_bit = bit;
            return 0;
        }
    }
    
    return -1;  /* Not found */
}

/**
 * Count consecutive sync bytes
 */
int uft_mfm_count_sync(const uint8_t *mfm, size_t mfm_size,
                       size_t start_bit, int *count)
{
    if (!mfm || !count) return -1;
    
    *count = 0;
    size_t bit = start_bit;
    
    while (bit + 16 <= mfm_size * 8) {
        size_t found;
        if (uft_mfm_find_sync(mfm, mfm_size, bit, &found) != 0) {
            break;
        }
        
        if (found != bit) break;  /* Gap found */
        
        (*count)++;
        bit += 16;
    }
    
    return 0;
}

/*===========================================================================
 * Bit Stream Operations
 *===========================================================================*/

/**
 * Extract bits from buffer
 */
uint32_t uft_bits_extract(const uint8_t *data, size_t bit_offset, int num_bits)
{
    if (!data || num_bits > 32) return 0;
    
    uint32_t result = 0;
    
    for (int i = 0; i < num_bits; i++) {
        size_t byte_idx = (bit_offset + i) / 8;
        int bit_idx = 7 - ((bit_offset + i) % 8);
        
        if ((data[byte_idx] >> bit_idx) & 1) {
            result |= (1 << (num_bits - 1 - i));
        }
    }
    
    return result;
}

/**
 * Insert bits into buffer
 */
int uft_bits_insert(uint8_t *data, size_t bit_offset, uint32_t value, int num_bits)
{
    if (!data || num_bits > 32) return -1;
    
    for (int i = 0; i < num_bits; i++) {
        size_t byte_idx = (bit_offset + i) / 8;
        int bit_idx = 7 - ((bit_offset + i) % 8);
        
        if ((value >> (num_bits - 1 - i)) & 1) {
            data[byte_idx] |= (1 << bit_idx);
        } else {
            data[byte_idx] &= ~(1 << bit_idx);
        }
    }
    
    return 0;
}

/*===========================================================================
 * PLL Simulation
 *===========================================================================*/

/**
 * Simple PLL for clock recovery
 */
typedef struct {
    double phase;           /* Current phase (0-1) */
    double frequency;       /* Current frequency estimate */
    double phase_gain;      /* Phase correction gain */
    double freq_gain;       /* Frequency correction gain */
    double nominal_freq;    /* Nominal bit cell frequency */
} uft_pll_t;

int uft_pll_init(uft_pll_t *pll, double nominal_freq)
{
    if (!pll) return -1;
    
    pll->phase = 0.0;
    pll->frequency = nominal_freq;
    pll->nominal_freq = nominal_freq;
    pll->phase_gain = 0.1;
    pll->freq_gain = 0.01;
    
    return 0;
}

/**
 * Process flux transition and return bit value
 */
int uft_pll_process(uft_pll_t *pll, double flux_time, uint8_t *bit)
{
    if (!pll || !bit) return -1;
    
    /* Calculate expected transition time */
    double expected = pll->phase + (1.0 / pll->frequency);
    
    /* Phase error */
    double error = flux_time - expected;
    
    /* Determine bit value based on timing */
    double half_cell = 0.5 / pll->frequency;
    
    if (error < -half_cell) {
        *bit = 0;  /* Early - missing transition */
    } else if (error > half_cell) {
        *bit = 1;  /* Late - extra transition */
    } else {
        *bit = 1;  /* On time - normal transition */
    }
    
    /* Update PLL state */
    pll->phase = flux_time + pll->phase_gain * error;
    pll->frequency += pll->freq_gain * error;
    
    /* Limit frequency drift */
    if (pll->frequency < pll->nominal_freq * 0.9) {
        pll->frequency = pll->nominal_freq * 0.9;
    }
    if (pll->frequency > pll->nominal_freq * 1.1) {
        pll->frequency = pll->nominal_freq * 1.1;
    }
    
    return 0;
}

/*===========================================================================
 * CRC Calculation
 *===========================================================================*/

static uint16_t crc_ccitt_table[256];
static bool crc_table_init = false;

static void init_crc_table(void)
{
    if (crc_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = i << 8;
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        crc_ccitt_table[i] = crc;
    }
    
    crc_table_init = true;
}

uint16_t uft_mfm_crc(const uint8_t *data, size_t size)
{
    init_crc_table();
    
    uint16_t crc = 0xFFFF;
    
    for (size_t i = 0; i < size; i++) {
        crc = (crc << 8) ^ crc_ccitt_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

/**
 * Calculate CRC including sync bytes
 */
uint16_t uft_mfm_sector_crc(const uint8_t *data, size_t size)
{
    init_crc_table();
    
    /* Start with 3x A1 sync bytes */
    uint16_t crc = 0xFFFF;
    crc = (crc << 8) ^ crc_ccitt_table[(crc >> 8) ^ 0xA1];
    crc = (crc << 8) ^ crc_ccitt_table[(crc >> 8) ^ 0xA1];
    crc = (crc << 8) ^ crc_ccitt_table[(crc >> 8) ^ 0xA1];
    
    /* Add data */
    for (size_t i = 0; i < size; i++) {
        crc = (crc << 8) ^ crc_ccitt_table[(crc >> 8) ^ data[i]];
    }
    
    return crc;
}

/*===========================================================================
 * Track Analysis
 *===========================================================================*/

int uft_mfm_analyze_track(const uint8_t *mfm, size_t mfm_size,
                          uft_mfm_analysis_t *analysis)
{
    if (!mfm || !analysis) return -1;
    
    memset(analysis, 0, sizeof(*analysis));
    analysis->encoding = UFT_ENCODING_MFM;
    
    /* Count sync patterns */
    size_t bit = 0;
    while (bit < mfm_size * 8) {
        size_t found;
        if (uft_mfm_find_sync(mfm, mfm_size, bit, &found) == 0) {
            analysis->sync_count++;
            bit = found + 16;
        } else {
            break;
        }
    }
    
    /* Estimate sector count */
    /* Each sector has at least 2 sync sequences (IDAM + DAM) */
    analysis->estimated_sectors = analysis->sync_count / 6;  /* 3 syncs per mark */
    
    /* Check for FM encoding */
    int fm_clocks = 0;
    for (size_t i = 0; i < mfm_size; i += 2) {
        uint16_t word = (mfm[i] << 8) | mfm[i + 1];
        /* FM has clock on every other bit */
        if ((word & 0xAAAA) == 0xAAAA) {
            fm_clocks++;
        }
    }
    
    if (fm_clocks > mfm_size / 8) {
        analysis->encoding = UFT_ENCODING_FM;
    }
    
    /* Calculate data rate estimate */
    analysis->bit_count = mfm_size * 8;
    
    return 0;
}

/*===========================================================================
 * Codec Context
 *===========================================================================*/

int uft_mfm_codec_init(uft_mfm_codec_t *codec, uft_encoding_t encoding)
{
    if (!codec) return -1;
    
    memset(codec, 0, sizeof(*codec));
    codec->encoding = encoding;
    codec->prev_bit = 0;
    
    return 0;
}

int uft_mfm_codec_encode_byte(uft_mfm_codec_t *codec, uint8_t data,
                               uint16_t *encoded)
{
    if (!codec || !encoded) return -1;
    
    if (codec->encoding == UFT_ENCODING_FM) {
        *encoded = uft_fm_encode_byte(data);
    } else {
        *encoded = uft_mfm_encode_byte(data, codec->prev_bit);
    }
    
    codec->prev_bit = data & 0x01;
    return 0;
}

int uft_mfm_codec_decode_byte(uft_mfm_codec_t *codec, uint16_t encoded,
                               uint8_t *data)
{
    if (!codec || !data) return -1;
    
    if (codec->encoding == UFT_ENCODING_FM) {
        *data = uft_fm_decode_word(encoded);
    } else {
        *data = uft_mfm_decode_word(encoded);
    }
    
    return 0;
}
