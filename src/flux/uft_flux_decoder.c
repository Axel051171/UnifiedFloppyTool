/**
 * @file uft_flux_decoder.c
 * @brief Universal Flux-to-Sector Decoder Implementation
 * @version 3.9.0
 * 
 * Decodes raw flux timing data into sector data.
 */

#include "uft/flux/uft_flux_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * CRC Tables
 * ============================================================================ */

static uint16_t crc16_table[256];
static bool crc16_table_init = false;

static void init_crc16_table(void) {
    if (crc16_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
        crc16_table[i] = crc;
    }
    crc16_table_init = true;
}

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

void flux_decoder_options_init(flux_decoder_options_t *opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->encoding = FLUX_ENC_AUTO;
    opts->bitcell_ns = 0;
    opts->tolerance = FLUX_TIMING_TOLERANCE;
    opts->use_pll = true;
    opts->pll_gain = FLUX_PLL_GAIN;
    opts->revolution = 0;
    opts->decode_all_revs = true;
    opts->keep_raw_bits = false;
}

void flux_pll_init(flux_pll_t *pll, double initial_period) {
    if (!pll) return;
    memset(pll, 0, sizeof(*pll));
    pll->period = initial_period;
    pll->phase = 0;
    pll->freq_gain = 0.02;
    pll->phase_gain = 0.5;
    pll->last_transition = 0;
}

void flux_decoded_track_init(flux_decoded_track_t *track) {
    if (!track) return;
    memset(track, 0, sizeof(*track));
}

void flux_decoded_track_free(flux_decoded_track_t *track) {
    if (!track) return;
    
    for (size_t i = 0; i < track->sector_count; i++) {
        free(track->sectors[i].data);
    }
    free(track->raw_bits);
    
    memset(track, 0, sizeof(*track));
}

/* ============================================================================
 * CRC Functions
 * ============================================================================ */

uint16_t flux_crc16_ccitt(const uint8_t *data, size_t len) {
    init_crc16_table();
    
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

uint16_t flux_crc16_mfm(const uint8_t *data, size_t len) {
    /* MFM CRC includes 3 sync bytes (A1 A1 A1) in calculation */
    init_crc16_table();
    
    uint16_t crc = 0xFFFF;
    /* Include sync bytes */
    for (int i = 0; i < 3; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ 0xA1];
    }
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ crc16_table[(crc >> 8) ^ data[i]];
    }
    return crc;
}

/* ============================================================================
 * MFM Encoding/Decoding
 * ============================================================================ */

uint8_t flux_mfm_decode_byte(uint16_t mfm_word) {
    uint8_t result = 0;
    /* MFM: clock bits at odd positions, data at even positions */
    for (int i = 0; i < 8; i++) {
        if (mfm_word & (1 << (14 - i*2))) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

uint16_t flux_mfm_encode_byte(uint8_t data, bool prev_bit) {
    uint16_t result = 0;
    
    for (int i = 7; i >= 0; i--) {
        bool bit = (data >> i) & 1;
        bool clock = !bit && !prev_bit;
        
        result = (result << 1) | clock;
        result = (result << 1) | bit;
        
        prev_bit = bit;
    }
    
    return result;
}

uint8_t flux_fm_decode_byte(uint16_t fm_word) {
    uint8_t result = 0;
    /* FM: clock bit, then data bit */
    for (int i = 0; i < 8; i++) {
        if (fm_word & (1 << (14 - i*2))) {
            result |= (1 << (7 - i));
        }
    }
    return result;
}

/* ============================================================================
 * PLL-based Flux to Bitstream Conversion
 * ============================================================================ */

flux_status_t flux_to_bitstream(const flux_raw_data_t *flux,
                                uint8_t *bits, size_t *bit_count,
                                double bitcell_ns, flux_pll_t *pll) {
    if (!flux || !bits || !bit_count || !pll) {
        return FLUX_ERR_INVALID;
    }
    
    /* Convert sample rate to nanoseconds per tick */
    double ns_per_tick = 1e9 / flux->sample_rate;
    
    size_t out_bits = 0;
    size_t max_bits = *bit_count;
    
    uint32_t prev_time = 0;
    
    for (size_t i = 0; i < flux->transition_count && out_bits < max_bits; i++) {
        uint32_t time = flux->transitions[i];
        uint32_t delta = time - prev_time;
        double delta_ns = delta * ns_per_tick;
        
        /* Calculate number of bit cells in this interval */
        double cells = delta_ns / pll->period;
        int num_cells = (int)(cells + 0.5);
        
        if (num_cells < 1) num_cells = 1;
        if (num_cells > 8) num_cells = 8;  /* Sanity limit */
        
        /* Output zeros for empty cells, then a one for the transition */
        for (int c = 0; c < num_cells - 1 && out_bits < max_bits; c++) {
            bits[out_bits / 8] &= ~(1 << (7 - (out_bits % 8)));
            out_bits++;
        }
        if (out_bits < max_bits) {
            bits[out_bits / 8] |= (1 << (7 - (out_bits % 8)));
            out_bits++;
        }
        
        /* PLL adjustment */
        if (pll->use_pll) {
            double expected = num_cells * pll->period;
            double error = delta_ns - expected;
            
            /* Phase adjustment */
            pll->phase += error * pll->phase_gain;
            
            /* Frequency adjustment */
            pll->period += error * pll->freq_gain / num_cells;
            
            /* Clamp period to reasonable range */
            double min_period = bitcell_ns * 0.8;
            double max_period = bitcell_ns * 1.2;
            if (pll->period < min_period) pll->period = min_period;
            if (pll->period > max_period) pll->period = max_period;
        }
        
        prev_time = time;
    }
    
    *bit_count = out_bits;
    return FLUX_OK;
}

/* ============================================================================
 * Sync Pattern Finding
 * ============================================================================ */

int flux_find_sync(const uint8_t *bits, size_t bit_count,
                   uint16_t pattern, size_t start_pos) {
    if (!bits || bit_count < 16) return -1;
    
    uint16_t window = 0;
    
    for (size_t i = start_pos; i < bit_count; i++) {
        /* Shift in next bit */
        window = (window << 1) | ((bits[i / 8] >> (7 - (i % 8))) & 1);
        
        if (i >= start_pos + 15 && window == pattern) {
            return (int)(i - 15);
        }
    }
    
    return -1;
}

/* ============================================================================
 * MFM Track Decoder
 * ============================================================================ */

static flux_status_t decode_mfm_sector(const uint8_t *bits, size_t bit_count,
                                       size_t start_pos, flux_decoded_sector_t *sector) {
    size_t pos = start_pos;
    
    /* Skip past sync bytes to address mark */
    pos += 16;  /* Skip the sync pattern we found */
    
    if (pos + 8 * 16 >= bit_count) return FLUX_ERR_UNDERFLOW;
    
    /* Read address mark and ID field: IDAM + C + H + S + N + CRC1 + CRC2 */
    uint8_t id_field[6];
    for (int i = 0; i < 6; i++) {
        uint16_t mfm_word = 0;
        for (size_t b = 0; b < 16 && pos < bit_count; b++, pos++) {
            mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        }
        id_field[i] = flux_mfm_decode_byte(mfm_word);
    }
    
    /* Verify IDAM */
    if (id_field[0] != MFM_IDAM) {
        return FLUX_ERR_NO_SYNC;
    }
    
    sector->cylinder = id_field[1];
    sector->head = id_field[2];
    sector->sector = id_field[3];
    sector->size_code = id_field[4];
    sector->id_crc = (id_field[5] << 8);
    
    /* Read CRC high byte (already have low byte position) */
    uint16_t mfm_word = 0;
    for (size_t b = 0; b < 16 && pos < bit_count; b++, pos++) {
        mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
    }
    sector->id_crc |= flux_mfm_decode_byte(mfm_word);
    
    /* Verify ID CRC */
    uint16_t calc_crc = flux_crc16_mfm(id_field, 5);
    sector->id_crc_ok = (calc_crc == sector->id_crc);
    
    sector->id_position = (uint32_t)start_pos;
    
    /* Search for data sync (up to 43 bytes gap) */
    int data_sync = flux_find_sync(bits, bit_count, MFM_SYNC_PATTERN, pos);
    if (data_sync < 0 || (size_t)data_sync > pos + 43 * 16) {
        return FLUX_ERR_NO_DATA;
    }
    
    pos = data_sync + 16;
    sector->data_position = (uint32_t)data_sync;
    
    /* Read data address mark */
    mfm_word = 0;
    for (size_t b = 0; b < 16 && pos < bit_count; b++, pos++) {
        mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
    }
    uint8_t dam = flux_mfm_decode_byte(mfm_word);
    
    if (dam == MFM_DDAM) {
        sector->deleted = true;
    } else if (dam != MFM_DAM) {
        return FLUX_ERR_NO_DATA;
    }
    
    /* Read sector data */
    size_t data_size = flux_sector_size(sector->size_code);
    sector->data = malloc(data_size);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size = data_size;
    
    for (size_t i = 0; i < data_size && pos + 16 <= bit_count; i++) {
        mfm_word = 0;
        for (int b = 0; b < 16; b++, pos++) {
            mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        }
        sector->data[i] = flux_mfm_decode_byte(mfm_word);
    }
    
    /* Read data CRC */
    uint8_t crc_bytes[2];
    for (size_t i = 0; i < 2 && pos + 16 <= bit_count; i++) {
        mfm_word = 0;
        for (int b = 0; b < 16; b++, pos++) {
            mfm_word = (mfm_word << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        }
        crc_bytes[i] = flux_mfm_decode_byte(mfm_word);
    }
    sector->data_crc = (crc_bytes[0] << 8) | crc_bytes[1];
    
    /* Verify data CRC */
    uint8_t *crc_data = malloc(1 + data_size);
    if (crc_data) {
        crc_data[0] = dam;
        memcpy(crc_data + 1, sector->data, data_size);
        calc_crc = flux_crc16_mfm(crc_data, 1 + data_size);
        sector->data_crc_ok = (calc_crc == sector->data_crc);
        free(crc_data);
    }
    
    return FLUX_OK;
}

flux_status_t flux_decode_mfm(const flux_raw_data_t *flux,
                              flux_decoded_track_t *track,
                              const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* Determine bit cell time */
    double bitcell_ns = opts->bitcell_ns;
    if (bitcell_ns == 0) {
        bitcell_ns = FLUX_MFM_DD_BITCELL_NS;  /* Default to DD */
    }
    
    /* Allocate bitstream buffer */
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    /* Initialize PLL */
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;
    
    /* Convert flux to bitstream */
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) {
        free(bits);
        return status;
    }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_MFM;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* Find and decode sectors */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        /* Find next sync pattern */
        int sync_pos = flux_find_sync(bits, bit_count, MFM_SYNC_PATTERN, pos);
        if (sync_pos < 0) break;
        
        /* Try to decode sector */
        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));
        
        status = decode_mfm_sector(bits, bit_count, sync_pos, sector);
        if (status == FLUX_OK) {
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok) track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
        } else if (status == FLUX_ERR_NO_DATA) {
            track->missing_data++;
        }
        
        pos = sync_pos + 16;
    }
    
    /* Keep raw bits if requested */
    if (opts->keep_raw_bits) {
        track->raw_bits = bits;
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    
    return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;
}

/* ============================================================================
 * FM Track Decoder
 * ============================================================================ */

flux_status_t flux_decode_fm(const flux_raw_data_t *flux,
                             flux_decoded_track_t *track,
                             const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        default_opts.bitcell_ns = FLUX_FM_BITCELL_NS;
        opts = &default_opts;
    }
    
    double bitcell_ns = opts->bitcell_ns ? opts->bitcell_ns : FLUX_FM_BITCELL_NS;
    
    /* Allocate bitstream buffer */
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) {
        free(bits);
        return status;
    }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_FM;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* FM decoding - similar to MFM but with FM sync pattern */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        int sync_pos = flux_find_sync(bits, bit_count, FM_SYNC_PATTERN, pos);
        if (sync_pos < 0) break;
        
        /* FM sector decoding would go here - similar to MFM */
        /* For now, just note we found a sync */
        pos = sync_pos + 16;
    }
    
    free(bits);
    return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;
}

/* ============================================================================
 * GCR Decoders (Stub implementations)
 * ============================================================================ */

/* C64 GCR tables */
static const uint8_t c64_gcr_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  /* 00-07 */
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  /* 08-0F */
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  /* 10-17 */
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   /* 18-1F */
};

/* C64 sectors per track by speed zone */
static const int c64_sectors_per_track[40] = {
    21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21, /* 1-17 */
    19,19,19,19,19,19,19,                                 /* 18-24 */
    18,18,18,18,18,18,                                     /* 25-30 */
    17,17,17,17,17,                                         /* 31-35 */
    17,17,17,17,17                                          /* 36-40 (extended) */
};

/* C64 GCR bitcell times per speed zone (ns) */
static const double c64_bitcell_ns[4] = {
    3250.0,  /* Zone 3: tracks 31-35, 17 sectors */
    3500.0,  /* Zone 2: tracks 25-30, 18 sectors */
    3750.0,  /* Zone 1: tracks 18-24, 19 sectors */
    4000.0   /* Zone 0: tracks 1-17,  21 sectors */
};

static int c64_speed_zone(int track) {
    if (track < 17) return 3;  /* zone 0 → longest bitcell */
    if (track < 24) return 2;
    if (track < 30) return 1;
    return 0;
}

static uint8_t c64_gcr_decode_byte(const uint8_t *bits, size_t pos, size_t bit_count) {
    /* Decode two 5-bit GCR groups → one byte */
    if (pos + 10 > bit_count) return 0xFF;
    
    uint8_t hi_gcr = 0, lo_gcr = 0;
    for (int i = 0; i < 5; i++) {
        size_t bp = pos + i;
        hi_gcr = (hi_gcr << 1) | ((bits[bp / 8] >> (7 - (bp % 8))) & 1);
    }
    for (int i = 0; i < 5; i++) {
        size_t bp = pos + 5 + i;
        lo_gcr = (lo_gcr << 1) | ((bits[bp / 8] >> (7 - (bp % 8))) & 1);
    }
    
    uint8_t hi = c64_gcr_decode[hi_gcr & 0x1F];
    uint8_t lo = c64_gcr_decode[lo_gcr & 0x1F];
    if (hi == 0xFF || lo == 0xFF) return 0xFF;
    return (hi << 4) | lo;
}

/* Find C64 GCR sync: 10+ consecutive 1-bits */
static int c64_find_sync(const uint8_t *bits, size_t bit_count, size_t start) {
    int ones = 0;
    for (size_t i = start; i < bit_count; i++) {
        if ((bits[i / 8] >> (7 - (i % 8))) & 1) {
            ones++;
            if (ones >= 10) {
                /* Skip remaining 1-bits */
                while (i + 1 < bit_count && 
                       ((bits[(i+1) / 8] >> (7 - ((i+1) % 8))) & 1)) {
                    i++;
                }
                return (int)(i + 1);  /* First bit after sync */
            }
        } else {
            ones = 0;
        }
    }
    return -1;
}

static flux_status_t decode_c64_gcr_sector(const uint8_t *bits, size_t bit_count,
                                            size_t data_start, 
                                            flux_decoded_sector_t *sector) {
    size_t pos = data_start;
    
    /* Read header: 0x08, checksum, sector, track, id2, id1, 0x0F, 0x0F */
    uint8_t header[8];
    for (int i = 0; i < 8; i++) {
        header[i] = c64_gcr_decode_byte(bits, pos, bit_count);
        pos += 10;
        if (header[i] == 0xFF && i < 6) return FLUX_ERR_NO_SYNC;
    }
    
    if (header[0] != 0x08) return FLUX_ERR_NO_SYNC;  /* Not a header block */
    
    uint8_t hdr_checksum = header[1];
    sector->sector = header[2];
    sector->cylinder = header[3] > 0 ? header[3] - 1 : 0;  /* 1-based → 0-based */
    sector->head = 0;
    sector->size_code = 1;  /* 256 bytes */
    sector->id_position = (uint32_t)data_start;
    
    /* Verify header checksum: XOR of sector, track, id2, id1 */
    uint8_t calc_hdr = header[2] ^ header[3] ^ header[4] ^ header[5];
    sector->id_crc = hdr_checksum;
    sector->id_crc_ok = (calc_hdr == hdr_checksum);
    
    /* Find data block sync */
    int data_sync = c64_find_sync(bits, bit_count, pos);
    if (data_sync < 0) return FLUX_ERR_NO_DATA;
    pos = (size_t)data_sync;
    
    /* Read data block marker */
    uint8_t marker = c64_gcr_decode_byte(bits, pos, bit_count);
    pos += 10;
    if (marker != 0x07) return FLUX_ERR_NO_DATA;  /* Not a data block */
    
    sector->data_position = (uint32_t)pos;
    
    /* Read 256 data bytes */
    sector->data = malloc(256);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size = 256;
    
    uint8_t checksum = 0;
    for (int i = 0; i < 256; i++) {
        sector->data[i] = c64_gcr_decode_byte(bits, pos, bit_count);
        checksum ^= sector->data[i];
        pos += 10;
    }
    
    /* Read data checksum byte */
    uint8_t read_checksum = c64_gcr_decode_byte(bits, pos, bit_count);
    sector->data_crc = read_checksum;
    sector->data_crc_ok = (checksum == read_checksum);
    sector->deleted = false;
    
    return FLUX_OK;
}

flux_status_t flux_decode_gcr_c64(const flux_raw_data_t *flux,
                                  flux_decoded_track_t *track,
                                  const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }
    
    /* C64 default bitcell: zone 0 (tracks 1-17) = 4000ns */
    double bitcell_ns = opts->bitcell_ns;
    if (bitcell_ns == 0) bitcell_ns = 4000.0;
    
    /* Allocate bitstream buffer */
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;
    
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) { free(bits); return status; }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_GCR_C64;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* Find and decode sectors */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        int sync_pos = c64_find_sync(bits, bit_count, pos);
        if (sync_pos < 0) break;
        
        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));
        
        status = decode_c64_gcr_sector(bits, bit_count, (size_t)sync_pos, sector);
        if (status == FLUX_OK) {
            sector->bitrate = 1e9 / pll.period;
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok) track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
            pos = sync_pos + 256 * 10;  /* Skip past data */
        } else {
            if (status == FLUX_ERR_NO_DATA) track->missing_data++;
            pos = sync_pos + 10;  /* Try next sync */
        }
    }
    
    if (opts->keep_raw_bits) {
        track->raw_bits = bits;
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    
    return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;
}

/* Apple II 6-and-2 GCR decode table (disk byte → 6-bit value) */
static const uint8_t apple_gcr62_decode[256] = {
    [0x96]=0x00,[0x97]=0x01,[0x9A]=0x02,[0x9B]=0x03,[0x9D]=0x04,[0x9E]=0x05,[0x9F]=0x06,
    [0xA6]=0x07,[0xA7]=0x08,[0xAB]=0x09,[0xAC]=0x0A,[0xAD]=0x0B,[0xAE]=0x0C,[0xAF]=0x0D,
    [0xB2]=0x0E,[0xB3]=0x0F,[0xB4]=0x10,[0xB5]=0x11,[0xB6]=0x12,[0xB7]=0x13,[0xB9]=0x14,
    [0xBA]=0x15,[0xBB]=0x16,[0xBC]=0x17,[0xBD]=0x18,[0xBE]=0x19,[0xBF]=0x1A,
    [0xCB]=0x1B,[0xCD]=0x1C,[0xCE]=0x1D,[0xCF]=0x1E,
    [0xD3]=0x1F,[0xD6]=0x20,[0xD7]=0x21,[0xD9]=0x22,[0xDA]=0x23,[0xDB]=0x24,
    [0xDC]=0x25,[0xDD]=0x26,[0xDE]=0x27,[0xDF]=0x28,
    [0xE5]=0x29,[0xE6]=0x2A,[0xE7]=0x2B,[0xE9]=0x2C,[0xEA]=0x2D,[0xEB]=0x2E,
    [0xEC]=0x2F,[0xED]=0x30,[0xEE]=0x31,[0xEF]=0x32,
    [0xF2]=0x33,[0xF3]=0x34,[0xF4]=0x35,[0xF5]=0x36,[0xF6]=0x37,[0xF7]=0x38,
    [0xF9]=0x39,[0xFA]=0x3A,[0xFB]=0x3B,[0xFC]=0x3C,[0xFD]=0x3D,[0xFE]=0x3E,[0xFF]=0x3F
};

/* Apple address prologue: D5 AA 96, data prologue: D5 AA AD */
#define APPLE_PROLOG1   0xD5
#define APPLE_PROLOG2   0xAA
#define APPLE_ADDR_P3   0x96
#define APPLE_DATA_P3   0xAD
#define APPLE_EPILOG1   0xDE
#define APPLE_EPILOG2   0xAA
#define APPLE_GCR_BITCELL_NS  4000.0

static uint8_t apple_read_byte(const uint8_t *bits, size_t pos, size_t bit_count) {
    if (pos + 8 > bit_count) return 0;
    uint8_t val = 0;
    for (int i = 0; i < 8; i++) {
        size_t bp = pos + i;
        val = (val << 1) | ((bits[bp / 8] >> (7 - (bp % 8))) & 1);
    }
    return val;
}

/* Find Apple 3-byte prologue sequence in bitstream */
static int apple_find_prologue(const uint8_t *bits, size_t bit_count, 
                                size_t start, uint8_t p3) {
    for (size_t i = start; i + 24 <= bit_count; i++) {
        uint8_t b1 = apple_read_byte(bits, i, bit_count);
        if (b1 == APPLE_PROLOG1) {
            uint8_t b2 = apple_read_byte(bits, i + 8, bit_count);
            if (b2 == APPLE_PROLOG2) {
                uint8_t b3 = apple_read_byte(bits, i + 16, bit_count);
                if (b3 == p3) return (int)i;
            }
        }
    }
    return -1;
}

static uint8_t apple_decode_44(const uint8_t *bits, size_t pos, size_t bit_count) {
    /* Apple 4-and-4 encoding: two bytes, even bits in first, odd in second */
    uint8_t b1 = apple_read_byte(bits, pos, bit_count);
    uint8_t b2 = apple_read_byte(bits, pos + 8, bit_count);
    return ((b1 << 1) | 1) & b2;
}

static flux_status_t decode_apple_gcr_sector(const uint8_t *bits, size_t bit_count,
                                              size_t addr_start,
                                              flux_decoded_sector_t *sector) {
    size_t pos = addr_start + 24;  /* Skip past D5 AA 96 */
    
    /* Address field: volume(4&4), track(4&4), sector(4&4), checksum(4&4) */
    uint8_t volume = apple_decode_44(bits, pos, bit_count); pos += 16;
    uint8_t track  = apple_decode_44(bits, pos, bit_count); pos += 16;
    uint8_t sect   = apple_decode_44(bits, pos, bit_count); pos += 16;
    uint8_t cksum  = apple_decode_44(bits, pos, bit_count); pos += 16;
    
    sector->cylinder = track;
    sector->head = 0;
    sector->sector = sect;
    sector->size_code = 1;  /* 256 bytes */
    sector->id_position = (uint32_t)addr_start;
    
    uint8_t calc_cksum = volume ^ track ^ sect;
    sector->id_crc = cksum;
    sector->id_crc_ok = (calc_cksum == cksum);
    
    /* Find data prologue: D5 AA AD */
    int data_start = apple_find_prologue(bits, bit_count, pos, APPLE_DATA_P3);
    if (data_start < 0 || (size_t)data_start > pos + 500 * 8) {
        return FLUX_ERR_NO_DATA;
    }
    pos = (size_t)data_start + 24;  /* Skip past D5 AA AD */
    sector->data_position = (uint32_t)data_start;
    
    /* Read 342 + 1 disk bytes (6-and-2 encoded) */
    uint8_t disk_bytes[343];
    for (int i = 0; i < 343; i++) {
        disk_bytes[i] = apple_read_byte(bits, pos, bit_count);
        pos += 8;
    }
    
    /* Decode 6-and-2: first XOR-decode the stream */
    uint8_t decoded[343];
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        uint8_t val = apple_gcr62_decode[disk_bytes[i]];
        decoded[i] = val ^ prev;
        prev = decoded[i];
    }
    /* Checksum byte */
    uint8_t data_cksum = apple_gcr62_decode[disk_bytes[342]];
    sector->data_crc = data_cksum;
    sector->data_crc_ok = (prev == data_cksum);
    
    /* Reassemble 256 bytes from 6-and-2 */
    sector->data = malloc(256);
    if (!sector->data) return FLUX_ERR_OVERFLOW;
    sector->data_size = 256;
    
    for (int i = 0; i < 256; i++) {
        uint8_t lo2;
        /* Low 2 bits are in decoded[i % 86] */
        int aux_idx = i % 86;
        int shift = (i / 86) * 2;
        lo2 = (decoded[aux_idx] >> shift) & 0x03;
        /* High 6 bits are in decoded[86 + i] */
        sector->data[i] = (decoded[86 + i] << 2) | lo2;
    }
    
    sector->deleted = false;
    return FLUX_OK;
}

flux_status_t flux_decode_gcr_apple(const flux_raw_data_t *flux,
                                    flux_decoded_track_t *track,
                                    const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoder_options_t default_opts;
    if (!opts) {
        flux_decoder_options_init(&default_opts);
        opts = &default_opts;
    }
    
    double bitcell_ns = opts->bitcell_ns;
    if (bitcell_ns == 0) bitcell_ns = APPLE_GCR_BITCELL_NS;
    
    size_t max_bits = FLUX_MAX_TRACK_SIZE * 8;
    uint8_t *bits = calloc(max_bits / 8 + 1, 1);
    if (!bits) return FLUX_ERR_OVERFLOW;
    
    flux_pll_t pll;
    flux_pll_init(&pll, bitcell_ns);
    pll.use_pll = opts->use_pll;
    pll.freq_gain = opts->pll_gain;
    
    size_t bit_count = max_bits;
    flux_status_t status = flux_to_bitstream(flux, bits, &bit_count, bitcell_ns, &pll);
    if (status != FLUX_OK) { free(bits); return status; }
    
    track->track_length_bits = (uint32_t)bit_count;
    track->detected_encoding = FLUX_ENC_GCR_APPLE;
    track->avg_bitrate = 1e9 / pll.period;
    
    /* Find and decode sectors */
    size_t pos = 0;
    while (pos < bit_count && track->sector_count < FLUX_MAX_SECTORS) {
        /* Find address prologue D5 AA 96 */
        int addr_pos = apple_find_prologue(bits, bit_count, pos, APPLE_ADDR_P3);
        if (addr_pos < 0) break;
        
        flux_decoded_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(*sector));
        
        status = decode_apple_gcr_sector(bits, bit_count, (size_t)addr_pos, sector);
        if (status == FLUX_OK) {
            sector->bitrate = 1e9 / pll.period;
            track->sector_count++;
            track->good_sectors++;
            if (!sector->id_crc_ok) track->bad_id_crc++;
            if (!sector->data_crc_ok) track->bad_data_crc++;
            pos = addr_pos + 343 * 8;  /* Skip past data */
        } else {
            if (status == FLUX_ERR_NO_DATA) track->missing_data++;
            pos = addr_pos + 24;  /* Skip prologue, try next */
        }
    }
    
    if (opts->keep_raw_bits) {
        track->raw_bits = bits;
        track->raw_bit_count = bit_count;
    } else {
        free(bits);
    }
    
    return (track->sector_count > 0) ? FLUX_OK : FLUX_ERR_NO_SYNC;
}

/* ============================================================================
 * Encoding Detection
 * ============================================================================ */

flux_encoding_t flux_detect_encoding(const flux_raw_data_t *flux) {
    if (!flux || flux->transition_count < 100) {
        return FLUX_ENC_AUTO;
    }
    
    /* Calculate average transition time */
    double ns_per_tick = 1e9 / flux->sample_rate;
    double total_time = 0;
    uint32_t prev = 0;
    size_t count = 0;
    
    for (size_t i = 0; i < flux->transition_count && i < 1000; i++) {
        if (i > 0) {
            total_time += (flux->transitions[i] - prev) * ns_per_tick;
            count++;
        }
        prev = flux->transitions[i];
    }
    
    if (count == 0) return FLUX_ENC_AUTO;
    
    double avg_interval = total_time / count;
    
    /* Classify based on average interval */
    if (avg_interval < 1500) {
        return FLUX_ENC_MFM;  /* HD MFM (~1µs bit cell, ~1.5µs avg interval) */
    } else if (avg_interval < 3000) {
        return FLUX_ENC_MFM;  /* DD MFM (~2µs bit cell, ~3µs avg interval) */
    } else if (avg_interval < 5000) {
        return FLUX_ENC_FM;   /* FM (~4µs bit cell) */
    } else {
        return FLUX_ENC_GCR_C64;  /* Slower - probably GCR */
    }
}

/* ============================================================================
 * Main Decoder Entry Point
 * ============================================================================ */

flux_status_t flux_decode_track(const flux_raw_data_t *flux,
                                flux_decoded_track_t *track,
                                const flux_decoder_options_t *opts) {
    if (!flux || !track) return FLUX_ERR_INVALID;
    
    flux_decoded_track_init(track);
    
    flux_decoder_options_t local_opts;
    if (!opts) {
        flux_decoder_options_init(&local_opts);
        opts = &local_opts;
    }
    
    flux_encoding_t encoding = opts->encoding;
    if (encoding == FLUX_ENC_AUTO) {
        encoding = flux_detect_encoding(flux);
    }
    
    switch (encoding) {
        case FLUX_ENC_MFM:
        case FLUX_ENC_AMIGA:
            return flux_decode_mfm(flux, track, opts);
        
        case FLUX_ENC_FM:
            return flux_decode_fm(flux, track, opts);
        
        case FLUX_ENC_GCR_C64:
            return flux_decode_gcr_c64(flux, track, opts);
        
        case FLUX_ENC_GCR_APPLE:
            return flux_decode_gcr_apple(flux, track, opts);
        
        default:
            return FLUX_ERR_INVALID;
    }
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char* flux_encoding_name(flux_encoding_t enc) {
    switch (enc) {
        case FLUX_ENC_AUTO:      return "Auto";
        case FLUX_ENC_MFM:       return "MFM";
        case FLUX_ENC_FM:        return "FM";
        case FLUX_ENC_GCR_C64:   return "GCR (C64)";
        case FLUX_ENC_GCR_APPLE: return "GCR (Apple)";
        case FLUX_ENC_AMIGA:     return "Amiga MFM";
        case FLUX_ENC_RAW:       return "Raw";
        default:                 return "Unknown";
    }
}

const char* flux_status_name(flux_status_t status) {
    switch (status) {
        case FLUX_OK:           return "OK";
        case FLUX_ERR_NO_SYNC:  return "No sync pattern";
        case FLUX_ERR_BAD_CRC:  return "CRC error";
        case FLUX_ERR_NO_DATA:  return "No data field";
        case FLUX_ERR_WEAK_BITS: return "Weak/unreliable bits";
        case FLUX_ERR_OVERFLOW: return "Buffer overflow";
        case FLUX_ERR_UNDERFLOW: return "Not enough data";
        case FLUX_ERR_INVALID:  return "Invalid parameters";
        default:                return "Unknown error";
    }
}
