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
        for (int b = 0; b < 16 && pos < bit_count; b++, pos++) {
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
    for (int b = 0; b < 16 && pos < bit_count; b++, pos++) {
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
    for (int b = 0; b < 16 && pos < bit_count; b++, pos++) {
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
    for (int i = 0; i < 2 && pos + 16 <= bit_count; i++) {
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

flux_status_t flux_decode_gcr_c64(const flux_raw_data_t *flux,
                                  flux_decoded_track_t *track,
                                  const flux_decoder_options_t *opts) {
    (void)flux;
    (void)track;
    (void)opts;
    /* TODO: Implement C64 GCR decoding */
    return FLUX_ERR_INVALID;
}

flux_status_t flux_decode_gcr_apple(const flux_raw_data_t *flux,
                                    flux_decoded_track_t *track,
                                    const flux_decoder_options_t *opts) {
    (void)flux;
    (void)track;
    (void)opts;
    /* TODO: Implement Apple II GCR decoding */
    return FLUX_ERR_INVALID;
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
