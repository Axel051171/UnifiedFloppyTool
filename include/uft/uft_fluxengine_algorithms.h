/**
 * @file uft_fluxengine_algorithms.h
 * @brief FluxEngine + FlashFloppy Algorithms Collection
 * @version 3.4.0
 *
 * Extracted from:
 * - FluxEngine by David Given (public domain)
 * - FlashFloppy by Keir Fraser (public domain)
 *
 * Contains:
 * - PLL Algorithm (SamDisk-derived)
 * - MFM/FM Encoding/Decoding Tables
 * - GCR Encoding (Apple II, Macintosh, C64)
 * - Amiga MFM Interleaving
 * - CRC16 CCITT (Table-based)
 * - Flux to Bitcell Conversion
 * - Precompensation
 *
 * SPDX-License-Identifier: Unlicense
 */

#ifndef UFT_FLUXENGINE_ALGORITHMS_H
#define UFT_FLUXENGINE_ALGORITHMS_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TIMING CONSTANTS (from FluxEngine protocol.h)
 *============================================================================*/

/* FluxEngine Hardware Timing */
#define UFT_FE_TICK_FREQUENCY       12000000    /* 12 MHz */
#define UFT_FE_TICKS_PER_US         (UFT_FE_TICK_FREQUENCY / 1000000)
#define UFT_FE_TICKS_PER_MS         (UFT_FE_TICK_FREQUENCY / 1000)
#define UFT_FE_NS_PER_TICK          (1000000000.0 / (double)UFT_FE_TICK_FREQUENCY)
#define UFT_FE_US_PER_TICK          (1000000.0 / (double)UFT_FE_TICK_FREQUENCY)

/* Precompensation threshold: 2.25µs in ticks */
#define UFT_FE_PRECOMP_THRESHOLD    ((int)(2.25 * UFT_FE_TICKS_PER_US))

/* Flux encoding flags */
#define UFT_FE_BIT_PULSE            0x80
#define UFT_FE_BIT_INDEX            0x40
#define UFT_FE_DESYNC               0x00
#define UFT_FE_EOF                  0x100   /* Synthetic, library-only */

/*============================================================================
 * PLL ALGORITHM (from FluxEngine fluxdecoder.cc - SamDisk derived)
 *============================================================================*/

/**
 * @brief PLL Decoder State
 * 
 * Port of SamDisk FluxDecoder algorithm:
 * https://github.com/simonowen/samdisk/blob/master/src/FluxDecoder.cpp
 */
typedef struct uft_fe_pll {
    /* Configuration */
    double pll_phase;           /**< Phase adjustment (0.0-1.0) */
    double pll_adjust;          /**< Frequency adjustment (0.0-1.0) */
    double flux_scale;          /**< Flux timing scale factor */
    
    /* Clock tracking */
    double clock;               /**< Current clock period (ns) */
    double clock_centre;        /**< Nominal clock period (ns) */
    double clock_min;           /**< Minimum allowed clock (ns) */
    double clock_max;           /**< Maximum allowed clock (ns) */
    
    /* State */
    double flux;                /**< Accumulated flux time (ns) */
    unsigned int clocked_zeroes;/**< Consecutive zero bits */
    unsigned int goodbits;      /**< Good bits since last sync loss */
    bool sync_lost;             /**< Sync loss detected */
} uft_fe_pll_t;

/**
 * @brief Initialize PLL with default FluxEngine settings
 * 
 * Default values from FluxEngine decoders.proto:
 * - pll_phase: 0.75 (75%)
 * - pll_adjust: 0.05 (5%)
 * - flux_scale: 1.0
 */
static inline void uft_fe_pll_init(uft_fe_pll_t *pll, double bitcell_ns) {
    pll->pll_phase = 0.75;
    pll->pll_adjust = 0.05;
    pll->flux_scale = 1.0;
    
    pll->clock = bitcell_ns;
    pll->clock_centre = bitcell_ns;
    pll->clock_min = bitcell_ns * (1.0 - pll->pll_adjust);
    pll->clock_max = bitcell_ns * (1.0 + pll->pll_adjust);
    
    pll->flux = 0;
    pll->clocked_zeroes = 0;
    pll->goodbits = 0;
    pll->sync_lost = false;
}

/**
 * @brief Initialize PLL with custom settings
 */
static inline void uft_fe_pll_init_custom(uft_fe_pll_t *pll, 
                                           double bitcell_ns,
                                           double phase_adj,
                                           double period_adj) {
    pll->pll_phase = phase_adj;
    pll->pll_adjust = period_adj;
    pll->flux_scale = 1.0;
    
    pll->clock = bitcell_ns;
    pll->clock_centre = bitcell_ns;
    pll->clock_min = bitcell_ns * (1.0 - period_adj);
    pll->clock_max = bitcell_ns * (1.0 + period_adj);
    
    pll->flux = 0;
    pll->clocked_zeroes = 0;
    pll->goodbits = 0;
    pll->sync_lost = false;
}

/**
 * @brief Process one flux interval and return decoded bit
 * 
 * This is the core PLL algorithm from FluxEngine/SamDisk.
 * 
 * @param pll PLL state
 * @param flux_ns Flux interval in nanoseconds
 * @return Decoded bit (0 or 1), or -1 if no bit ready
 */
static inline int uft_fe_pll_process(uft_fe_pll_t *pll, double flux_ns) {
    pll->flux += flux_ns * pll->flux_scale;
    
    /* Wait until we have at least half a clock */
    if (pll->flux < (pll->clock / 2.0)) {
        pll->clocked_zeroes = 0;
        return -1;  /* No bit ready */
    }
    
    pll->flux -= pll->clock;
    
    if (pll->flux >= (pll->clock / 2.0)) {
        /* Zero bit - flux transition was late */
        pll->clocked_zeroes++;
        pll->goodbits++;
        return 0;
    }
    
    /* One bit - flux transition */
    
    /* PLL adjustment based on sync state */
    if (pll->clocked_zeroes <= 3) {
        /* In sync: adjust clock based on phase error */
        pll->clock += pll->flux * pll->pll_adjust;
    } else {
        /* Out of sync: pull clock back towards centre */
        pll->clock += (pll->clock_centre - pll->clock) * pll->pll_adjust;
        
        /* Report sync loss after 256 good bits */
        if (pll->goodbits >= 256) {
            pll->sync_lost = true;
        }
        pll->goodbits = 0;
    }
    
    /* Clamp clock to allowed range */
    if (pll->clock < pll->clock_min) pll->clock = pll->clock_min;
    if (pll->clock > pll->clock_max) pll->clock = pll->clock_max;
    
    /* Phase damping: don't snap timing window to flux transition */
    pll->flux = pll->flux * (1.0 - pll->pll_phase);
    
    pll->goodbits++;
    return 1;
}

/*============================================================================
 * MFM ENCODING TABLE (from FlashFloppy mfm.c)
 *============================================================================*/

/**
 * @brief Complete MFM encoding lookup table
 * 
 * Maps each byte value to its 16-bit MFM encoding.
 * Assumes previous bit was 0. For proper encoding with
 * context, use uft_mfm_encode_byte().
 */
static const uint16_t UFT_MFM_ENCODE_TABLE[256] = {
    0xaaaa, 0xaaa9, 0xaaa4, 0xaaa5, 0xaa92, 0xaa91, 0xaa94, 0xaa95, 
    0xaa4a, 0xaa49, 0xaa44, 0xaa45, 0xaa52, 0xaa51, 0xaa54, 0xaa55, 
    0xa92a, 0xa929, 0xa924, 0xa925, 0xa912, 0xa911, 0xa914, 0xa915, 
    0xa94a, 0xa949, 0xa944, 0xa945, 0xa952, 0xa951, 0xa954, 0xa955, 
    0xa4aa, 0xa4a9, 0xa4a4, 0xa4a5, 0xa492, 0xa491, 0xa494, 0xa495, 
    0xa44a, 0xa449, 0xa444, 0xa445, 0xa452, 0xa451, 0xa454, 0xa455, 
    0xa52a, 0xa529, 0xa524, 0xa525, 0xa512, 0xa511, 0xa514, 0xa515, 
    0xa54a, 0xa549, 0xa544, 0xa545, 0xa552, 0xa551, 0xa554, 0xa555, 
    0x92aa, 0x92a9, 0x92a4, 0x92a5, 0x9292, 0x9291, 0x9294, 0x9295, 
    0x924a, 0x9249, 0x9244, 0x9245, 0x9252, 0x9251, 0x9254, 0x9255, 
    0x912a, 0x9129, 0x9124, 0x9125, 0x9112, 0x9111, 0x9114, 0x9115, 
    0x914a, 0x9149, 0x9144, 0x9145, 0x9152, 0x9151, 0x9154, 0x9155, 
    0x94aa, 0x94a9, 0x94a4, 0x94a5, 0x9492, 0x9491, 0x9494, 0x9495, 
    0x944a, 0x9449, 0x9444, 0x9445, 0x9452, 0x9451, 0x9454, 0x9455, 
    0x952a, 0x9529, 0x9524, 0x9525, 0x9512, 0x9511, 0x9514, 0x9515, 
    0x954a, 0x9549, 0x9544, 0x9545, 0x9552, 0x9551, 0x9554, 0x9555, 
    0x4aaa, 0x4aa9, 0x4aa4, 0x4aa5, 0x4a92, 0x4a91, 0x4a94, 0x4a95, 
    0x4a4a, 0x4a49, 0x4a44, 0x4a45, 0x4a52, 0x4a51, 0x4a54, 0x4a55, 
    0x492a, 0x4929, 0x4924, 0x4925, 0x4912, 0x4911, 0x4914, 0x4915, 
    0x494a, 0x4949, 0x4944, 0x4945, 0x4952, 0x4951, 0x4954, 0x4955, 
    0x44aa, 0x44a9, 0x44a4, 0x44a5, 0x4492, 0x4491, 0x4494, 0x4495, 
    0x444a, 0x4449, 0x4444, 0x4445, 0x4452, 0x4451, 0x4454, 0x4455, 
    0x452a, 0x4529, 0x4524, 0x4525, 0x4512, 0x4511, 0x4514, 0x4515, 
    0x454a, 0x4549, 0x4544, 0x4545, 0x4552, 0x4551, 0x4554, 0x4555, 
    0x52aa, 0x52a9, 0x52a4, 0x52a5, 0x5292, 0x5291, 0x5294, 0x5295, 
    0x524a, 0x5249, 0x5244, 0x5245, 0x5252, 0x5251, 0x5254, 0x5255, 
    0x512a, 0x5129, 0x5124, 0x5125, 0x5112, 0x5111, 0x5114, 0x5115, 
    0x514a, 0x5149, 0x5144, 0x5145, 0x5152, 0x5151, 0x5154, 0x5155, 
    0x54aa, 0x54a9, 0x54a4, 0x54a5, 0x5492, 0x5491, 0x5494, 0x5495, 
    0x544a, 0x5449, 0x5444, 0x5445, 0x5452, 0x5451, 0x5454, 0x5455, 
    0x552a, 0x5529, 0x5524, 0x5525, 0x5512, 0x5511, 0x5514, 0x5515, 
    0x554a, 0x5549, 0x5544, 0x5545, 0x5552, 0x5551, 0x5554, 0x5555
};

/**
 * @brief Encode byte to MFM with previous bit context
 * 
 * @param byte Data byte to encode
 * @param last_bit Previous bit (0 or 1)
 * @return 16-bit MFM encoded value
 */
static inline uint16_t uft_mfm_encode_byte(uint8_t byte, bool last_bit) {
    uint16_t encoded = UFT_MFM_ENCODE_TABLE[byte];
    /* If last bit was 1 and first data bit is 0, remove clock bit */
    if (last_bit && !(byte & 0x80)) {
        encoded &= 0x7FFF;
    }
    return encoded;
}

/**
 * @brief Decode MFM word to byte (extract data bits)
 */
static inline uint8_t uft_mfm_decode_word(uint16_t mfm) {
    uint8_t result = 0;
    /* Extract odd bits (data bits) */
    for (int i = 0; i < 8; i++) {
        if (mfm & (1 << (14 - i*2))) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

/*============================================================================
 * FM ENCODING (from FlashFloppy)
 *============================================================================*/

/**
 * @brief Create FM sync byte with custom clock pattern
 * 
 * FM encoding puts clock bits in odd positions, data in even.
 * Special sync bytes use non-standard clock patterns.
 * 
 * @param data Data byte
 * @param clock Clock pattern byte
 * @return 16-bit FM encoded value
 */
static inline uint16_t uft_fm_sync(uint8_t data, uint8_t clock) {
    uint16_t _data = UFT_MFM_ENCODE_TABLE[data] & 0x5555;
    uint16_t _clock = (UFT_MFM_ENCODE_TABLE[clock] & 0x5555) << 1;
    return _clock | _data;
}

/*============================================================================
 * IBM MFM SYNC PATTERNS (from FluxEngine ibm/decoder.cc)
 *============================================================================*/

/* MFM record separator: 0xA1 with missing clock
 * Normal 0xA1:  01 00 01 00 10 10 10 01 = 0x44A9
 * Special 0xA1: 01 00 01 00 10 00 10 01 = 0x4489
 *                           ^^^^^
 * The missing clock creates an illegal MFM pattern that cannot
 * occur in normal data, making it detectable as a sync mark.
 */
#define UFT_MFM_SYNC_A1             0x4489
#define UFT_MFM_SYNC_A1A1A1         0x448944894489ULL

/* IAM separator: 0xC2 with missing clock
 * Normal 0xC2:  01 01 00 10 10 10 01 00 = 0x5254
 * Special 0xC2: 01 01 00 10 00 10 01 00 = 0x5224
 */
#define UFT_MFM_SYNC_C2             0x5224

/* FM patterns (from FluxEngine) */
#define UFT_FM_IDAM_PATTERN         0xF57E  /* 0xFE with clock 0xC7 */
#define UFT_FM_DAM1_PATTERN         0xF56A  /* 0xF8 with clock 0xC7 */
#define UFT_FM_DAM2_PATTERN         0xF56F  /* 0xFB with clock 0xC7 */
#define UFT_FM_IAM_PATTERN          0xF77A  /* 0xFC with clock 0xD7 */

/* TRS-80 special DAM patterns */
#define UFT_FM_TRS80_DAM1           0xF56B  /* 0xF9 with clock 0xC7 */
#define UFT_FM_TRS80_DAM2           0xF56E  /* 0xFA with clock 0xC7 */

/*============================================================================
 * GCR ENCODING - COMMODORE 64 (from FluxEngine c64/)
 *============================================================================*/

/* C64 GCR uses 5 bits to encode 4 bits of data */
#define UFT_C64_SECTOR_RECORD       0xFFD49  /* Sync + 0x08 */
#define UFT_C64_DATA_RECORD         0xFFD57  /* Sync + 0x07 */
#define UFT_C64_SECTOR_LENGTH       256

/* C64 GCR encoding table: 4-bit value -> 5-bit GCR */
static const uint8_t UFT_C64_GCR_ENCODE[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/* C64 GCR decoding table: 5-bit GCR -> 4-bit value (-1 = invalid) */
static const int8_t UFT_C64_GCR_DECODE[32] = {
    -1, -1, -1, -1, -1, -1, -1, -1,  /* 00-07 */
    -1, 0x8, 0x0, 0x1, -1, 0xC, 0x4, 0x5,  /* 08-0F */
    -1, -1, 0x2, 0x3, -1, 0xF, 0x6, 0x7,  /* 10-17 */
    -1, 0x9, 0xA, 0xB, -1, 0xD, 0xE, -1   /* 18-1F */
};

static inline int uft_c64_gcr_decode(uint8_t gcr) {
    return (gcr < 32) ? UFT_C64_GCR_DECODE[gcr] : -1;
}

/*============================================================================
 * GCR ENCODING - APPLE II (from FluxEngine apple2/)
 *============================================================================*/

#define UFT_APPLE2_SECTOR_RECORD    0xD5AA96
#define UFT_APPLE2_DATA_RECORD      0xD5AAAD
#define UFT_APPLE2_SECTOR_LENGTH    256
#define UFT_APPLE2_ENCODED_LENGTH   342

/* Apple II 6&2 GCR encoding table */
static const uint8_t UFT_APPLE2_GCR_ENCODE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/* Apple II 6&2 GCR decoding table (0x96-0xFF only valid range) */
static const int8_t UFT_APPLE2_GCR_DECODE[256] = {
    /* 0x00-0x95: Invalid */
    [0x00 ... 0x95] = -1,
    /* 0x96-0xFF: Valid GCR values */
    [0x96] = 0x00, [0x97] = 0x01, [0x9A] = 0x02, [0x9B] = 0x03,
    [0x9D] = 0x04, [0x9E] = 0x05, [0x9F] = 0x06, [0xA6] = 0x07,
    [0xA7] = 0x08, [0xAB] = 0x09, [0xAC] = 0x0A, [0xAD] = 0x0B,
    [0xAE] = 0x0C, [0xAF] = 0x0D, [0xB2] = 0x0E, [0xB3] = 0x0F,
    [0xB4] = 0x10, [0xB5] = 0x11, [0xB6] = 0x12, [0xB7] = 0x13,
    [0xB9] = 0x14, [0xBA] = 0x15, [0xBB] = 0x16, [0xBC] = 0x17,
    [0xBD] = 0x18, [0xBE] = 0x19, [0xBF] = 0x1A, [0xCB] = 0x1B,
    [0xCD] = 0x1C, [0xCE] = 0x1D, [0xCF] = 0x1E, [0xD3] = 0x1F,
    [0xD6] = 0x20, [0xD7] = 0x21, [0xD9] = 0x22, [0xDA] = 0x23,
    [0xDB] = 0x24, [0xDC] = 0x25, [0xDD] = 0x26, [0xDE] = 0x27,
    [0xDF] = 0x28, [0xE5] = 0x29, [0xE6] = 0x2A, [0xE7] = 0x2B,
    [0xE9] = 0x2C, [0xEA] = 0x2D, [0xEB] = 0x2E, [0xEC] = 0x2F,
    [0xED] = 0x30, [0xEE] = 0x31, [0xEF] = 0x32, [0xF2] = 0x33,
    [0xF3] = 0x34, [0xF4] = 0x35, [0xF5] = 0x36, [0xF6] = 0x37,
    [0xF7] = 0x38, [0xF9] = 0x39, [0xFA] = 0x3A, [0xFB] = 0x3B,
    [0xFC] = 0x3C, [0xFD] = 0x3D, [0xFE] = 0x3E, [0xFF] = 0x3F
};

/*============================================================================
 * GCR ENCODING - MACINTOSH (from FluxEngine macintosh/)
 *============================================================================*/

#define UFT_MAC_SECTOR_RECORD       0xD5AA96
#define UFT_MAC_DATA_RECORD         0xD5AAAD
#define UFT_MAC_SECTOR_LENGTH       524     /* 12 tag + 512 data */
#define UFT_MAC_ENCODED_LENGTH      703

/* Macintosh uses same GCR encoding as Apple II */

/*============================================================================
 * AMIGA MFM (from FluxEngine amiga/)
 *============================================================================*/

#define UFT_AMIGA_SECTOR_RECORD     0xAAAA44894489ULL
#define UFT_AMIGA_TRACKS_PER_DISK   80
#define UFT_AMIGA_SECTORS_PER_TRACK 11
#define UFT_AMIGA_RECORD_SIZE       0x21C   /* 540 bytes */

/**
 * @brief Amiga checksum calculation
 * 
 * XOR all 32-bit words, mask with 0x55555555
 */
static inline uint32_t uft_amiga_checksum(const uint8_t *data, size_t len) {
    uint32_t checksum = 0;
    for (size_t i = 0; i + 3 < len; i += 4) {
        uint32_t word = ((uint32_t)data[i] << 24) |
                        ((uint32_t)data[i+1] << 16) |
                        ((uint32_t)data[i+2] << 8) |
                        data[i+3];
        checksum ^= word;
    }
    return checksum & 0x55555555;
}

/**
 * @brief Amiga MFM interleaving (odd/even bit separation)
 * 
 * Amiga stores data with odd bits first, then even bits.
 * This function interleaves data for writing.
 * 
 * @param input Input data
 * @param output Output buffer (must be 2x input length)
 * @param len Input length in bytes
 */
static inline void uft_amiga_interleave(const uint8_t *input, 
                                         uint8_t *output, 
                                         size_t len) {
    size_t half = len;
    uint8_t *odds = output;
    uint8_t *evens = output + half;
    
    for (size_t i = 0; i < len; i += 2) {
        uint16_t word = ((uint16_t)input[i] << 8) | input[i+1];
        
        /* Extract odd bits */
        uint8_t odd = 0;
        uint16_t x = word & 0xAAAA;
        x |= x >> 1;
        /* Compress bits */
        odd = ((x >> 9) & 0x08) | ((x >> 6) & 0x04) |
              ((x >> 3) & 0x02) | (x & 0x01);
        odd |= ((x >> 5) & 0x80) | ((x >> 2) & 0x40) |
               ((x << 1) & 0x20) | ((x << 4) & 0x10);
        *odds++ = odd;
        
        /* Extract even bits */
        uint8_t even = 0;
        x = word & 0x5555;
        x |= x << 1;
        even = ((x >> 8) & 0x08) | ((x >> 5) & 0x04) |
               ((x >> 2) & 0x02) | ((x << 1) & 0x01);
        even |= ((x >> 4) & 0x80) | ((x >> 1) & 0x40) |
                ((x << 2) & 0x20) | ((x << 5) & 0x10);
        *evens++ = even;
    }
}

/**
 * @brief Amiga MFM deinterleaving
 * 
 * Reconstructs data from odd/even separated format.
 * Uses 64-bit multiply bit interleaving trick.
 * 
 * @param odds Pointer to odd bits
 * @param evens Pointer to even bits
 * @param output Output buffer
 * @param len Output length in bytes (half of input)
 */
static inline void uft_amiga_deinterleave(const uint8_t *odds,
                                           const uint8_t *evens,
                                           uint8_t *output,
                                           size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t o = *odds++;
        uint8_t e = *evens++;
        
        /* Bit interleave using 64-bit multiply trick */
        uint16_t result =
            (((e * 0x0101010101010101ULL & 0x8040201008040201ULL) *
                    0x0102040810204081ULL >> 49) & 0x5555) |
            (((o * 0x0101010101010101ULL & 0x8040201008040201ULL) *
                    0x0102040810204081ULL >> 48) & 0xAAAA);
        
        output[i*2] = result >> 8;
        output[i*2 + 1] = result & 0xFF;
    }
}

/*============================================================================
 * CRC16-CCITT (from FlashFloppy crc.c)
 *============================================================================*/

static const uint16_t UFT_CRC16_CCITT_TABLE[256] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
    0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
    0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
    0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
    0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
    0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
    0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
    0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
    0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
    0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
    0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
    0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
    0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
    0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
    0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
    0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
    0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
    0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
    0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
    0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
    0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
    0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
    0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
    0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
    0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
    0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
    0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
    0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
};

/**
 * @brief Calculate CRC16-CCITT
 * 
 * @param data Input data
 * @param len Data length
 * @param init Initial CRC value (usually 0xFFFF)
 * @return CRC16 value
 */
static inline uint16_t uft_crc16_ccitt(const uint8_t *data, size_t len, 
                                        uint16_t init) {
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = UFT_CRC16_CCITT_TABLE[(crc >> 8) ^ data[i]] ^ (crc << 8);
    }
    return crc;
}

/**
 * @brief Calculate CRC16-CCITT with standard init
 */
static inline uint16_t uft_crc16_ccitt_std(const uint8_t *data, size_t len) {
    return uft_crc16_ccitt(data, len, 0xFFFF);
}

/*============================================================================
 * HFE FORMAT CONSTANTS (from FlashFloppy hfe.c)
 *============================================================================*/

/* HFE track encoding types */
typedef enum {
    UFT_HFE_ENC_ISOIBM_MFM  = 0,
    UFT_HFE_ENC_AMIGA_MFM   = 1,
    UFT_HFE_ENC_ISOIBM_FM   = 2,
    UFT_HFE_ENC_EMU_FM      = 3,
    UFT_HFE_ENC_UNKNOWN     = 0xFF
} uft_hfe_encoding_t;

/* HFE interface modes */
typedef enum {
    UFT_HFE_IFM_IBMPC_DD    = 0,
    UFT_HFE_IFM_IBMPC_HD    = 1,
    UFT_HFE_IFM_ATARI_DD    = 2,
    UFT_HFE_IFM_ATARI_HD    = 3,
    UFT_HFE_IFM_AMIGA_DD    = 4,
    UFT_HFE_IFM_AMIGA_HD    = 5,
    UFT_HFE_IFM_CPC_DD      = 6,
    UFT_HFE_IFM_GENERIC_DD  = 7,
    UFT_HFE_IFM_IBMPC_ED    = 8,
    UFT_HFE_IFM_MSX2_DD     = 9,
    UFT_HFE_IFM_C64_DD      = 10,
    UFT_HFE_IFM_EMU_DD      = 11,
    UFT_HFE_IFM_S950_DD     = 12,
    UFT_HFE_IFM_S950_HD     = 13,
    UFT_HFE_IFM_DISABLE     = 0xFE
} uft_hfe_interface_t;

/* HFEv3 opcodes (bit-reversed to match raw HFE bit order) */
typedef enum {
    UFT_HFE_OP_NOP          = 0x0F,
    UFT_HFE_OP_INDEX        = 0x8F,
    UFT_HFE_OP_BITRATE      = 0x4F,
    UFT_HFE_OP_SKIPBITS     = 0xCF,
    UFT_HFE_OP_RAND         = 0x2F
} uft_hfe_opcode_t;

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUXENGINE_ALGORITHMS_H */
