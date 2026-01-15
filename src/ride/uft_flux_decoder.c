/**
 * @file uft_flux_decoder.c
 * @brief UFT Flux Decoder Implementation
 * 
 * Core flux decoding algorithms ported from RIDE.
 */

#include "uft/ride/uft_flux_decoder.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

/*============================================================================
 * PRIVATE CONSTANTS
 *============================================================================*/

/* CRC-CCITT lookup table */
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

/* Mining cancel flag */
static volatile bool g_mining_cancel = false;

/*============================================================================
 * FLUX BUFFER MANAGEMENT
 *============================================================================*/

uft_flux_buffer_t *uft_flux_create(size_t capacity) {
    uft_flux_buffer_t *flux = calloc(1, sizeof(uft_flux_buffer_t));
    if (!flux) return NULL;
    
    flux->times = calloc(capacity, sizeof(uft_logtime_t));
    if (!flux->times) {
        free(flux);
        return NULL;
    }
    
    flux->index_times = calloc(UFT_REVOLUTION_MAX + 1, sizeof(uft_logtime_t));
    if (!flux->index_times) {
        free(flux->times);
        free(flux);
        return NULL;
    }
    
    flux->capacity = capacity;
    flux->count = 0;
    flux->index_count = 0;
    flux->sample_clock = UFT_GW_SAMPLE_CLOCK_NS;
    flux->detected_enc = UFT_ENC_UNKNOWN;
    flux->detected_den = UFT_DENSITY_UNKNOWN;
    
    return flux;
}

void uft_flux_free(uft_flux_buffer_t *flux) {
    if (!flux) return;
    free(flux->times);
    free(flux->index_times);
    free(flux);
}

bool uft_flux_add_transition(uft_flux_buffer_t *flux, uft_logtime_t time_ns) {
    if (!flux || flux->count >= flux->capacity) return false;
    flux->times[flux->count++] = time_ns;
    return true;
}

void uft_flux_mark_index(uft_flux_buffer_t *flux, uft_logtime_t time_ns) {
    if (!flux || flux->index_count >= UFT_REVOLUTION_MAX + 1) return;
    flux->index_times[flux->index_count++] = time_ns;
}

size_t uft_flux_count(const uft_flux_buffer_t *flux) {
    return flux ? flux->count : 0;
}

uint8_t uft_flux_revolutions(const uft_flux_buffer_t *flux) {
    if (!flux || flux->index_count < 2) return 0;
    return flux->index_count - 1;
}

uft_logtime_t uft_flux_total_time(const uft_flux_buffer_t *flux) {
    if (!flux || flux->count == 0) return 0;
    return flux->times[flux->count - 1];
}

/*============================================================================
 * CRC CALCULATION
 *============================================================================*/

uint16_t uft_crc_ccitt(const uint8_t *data, size_t len) {
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc = (crc << 8) ^ crc_table[((crc >> 8) ^ *data++) & 0xFF];
    }
    return crc;
}

/*============================================================================
 * PLL FUNCTIONS
 *============================================================================*/

void uft_pll_init(uft_pll_state_t *pll, uint32_t clock_period) {
    if (!pll) return;
    
    pll->clock_period = clock_period;
    pll->frequency = 1.0;
    pll->phase = 0.0;
    pll->gain_p = 1.0;      /* Proportional gain */
    pll->gain_i = 0.25;     /* Integral gain */
    pll->error_integral = 0.0;
    
    /* Set inspection window bounds (±15% tolerance) */
    pll->window_min = clock_period * 85 / 100;
    pll->window_max = clock_period * 115 / 100;
    pll->window_current = clock_period;
}

void uft_pll_reset(uft_pll_state_t *pll) {
    if (!pll) return;
    pll->frequency = 1.0;
    pll->phase = 0.0;
    pll->error_integral = 0.0;
    pll->window_current = pll->clock_period;
}

void uft_pll_set_gains(uft_pll_state_t *pll, float gain_p, float gain_i) {
    if (!pll) return;
    pll->gain_p = gain_p;
    pll->gain_i = gain_i;
}

/**
 * @brief Process flux transition through PLL (Keir Fraser algorithm)
 * 
 * This implements the DPLL (Digital Phase-Locked Loop) algorithm used
 * in Greaseweazle and RIDE. The PLL adapts its frequency to match the
 * incoming flux timing.
 */
int uft_pll_process(uft_pll_state_t *pll, uft_logtime_t flux_time, 
                    uint8_t *bits_out) {
    if (!pll || !bits_out) return 0;
    
    /* Calculate expected bit count from flux time */
    double expected_bits = (double)flux_time / (pll->clock_period * pll->frequency);
    int bit_count = (int)(expected_bits + 0.5);  /* Round to nearest */
    
    if (bit_count < 2) bit_count = 2;  /* Minimum 2T */
    if (bit_count > 4) bit_count = 4;  /* Maximum 4T */
    
    /* Calculate phase error */
    double expected_time = bit_count * pll->clock_period * pll->frequency;
    double error = (flux_time - expected_time) / pll->clock_period;
    
    /* Apply proportional correction to phase */
    pll->phase += error * pll->gain_p;
    
    /* Apply integral correction to frequency */
    pll->error_integral += error;
    pll->frequency = 1.0 + pll->error_integral * pll->gain_i;
    
    /* Clamp frequency to reasonable range */
    if (pll->frequency < 0.8) pll->frequency = 0.8;
    if (pll->frequency > 1.2) pll->frequency = 1.2;
    
    /* Update inspection window */
    pll->window_current = (uint32_t)(pll->clock_period * pll->frequency);
    
    /* Generate output bits: 1 followed by (bit_count-1) zeros */
    bits_out[0] = 1;
    for (int i = 1; i < bit_count; i++) {
        bits_out[i] = 0;
    }
    
    return bit_count;
}

/*============================================================================
 * DECODER CONFIGURATION
 *============================================================================*/

void uft_decoder_init_default(uft_decoder_config_t *config) {
    if (!config) return;
    
    config->algorithm = UFT_DECODER_KEIR_FRASER;
    config->encoding = UFT_ENC_MFM;
    config->density = UFT_DENSITY_DD;
    config->reset_on_index = true;
    config->revolutions = 2;
    config->clock_period = 0;  /* Auto-detect */
    config->pll_gain_p = 1.0f;
    config->pll_gain_i = 0.25f;
    config->strict_timing = false;
}

/*============================================================================
 * MFM DECODING
 *============================================================================*/

/**
 * @brief Find MFM sync pattern (A1 with missing clock)
 * @return Bit position of sync, or -1 if not found
 */
static int find_mfm_sync(const uint8_t *bits, size_t bit_count, size_t start) {
    /* A1 sync pattern in MFM: 0100010010001001 = 0x4489 */
    const uint16_t sync = 0x4489;
    uint16_t window = 0;
    
    for (size_t i = start; i < bit_count; i++) {
        window = (window << 1) | (bits[i / 8] >> (7 - (i % 8)) & 1);
        if (window == sync) {
            return (int)(i - 15);  /* Return start of sync */
        }
    }
    return -1;
}

/**
 * @brief Decode MFM bits to data bytes
 */
size_t uft_decode_mfm_bits(const uint8_t *bits, size_t bit_count,
                           uint8_t *data, size_t data_size) {
    if (!bits || !data || bit_count < 16) return 0;
    
    size_t data_pos = 0;
    size_t bit_pos = 0;
    
    /* MFM: every other bit is clock, data bits are at odd positions */
    while (bit_pos + 16 <= bit_count && data_pos < data_size) {
        uint8_t byte = 0;
        for (int i = 0; i < 8; i++) {
            /* Data bit is at position 1, 3, 5, 7, 9, 11, 13, 15 */
            size_t pos = bit_pos + (i * 2) + 1;
            if (pos < bit_count) {
                byte = (byte << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
            }
        }
        data[data_pos++] = byte;
        bit_pos += 16;
    }
    
    return data_pos;
}

/**
 * @brief Encode data bytes to MFM bits
 */
size_t uft_encode_mfm_bits(const uint8_t *data, size_t data_size,
                           uint8_t *bits, size_t max_bits) {
    if (!data || !bits || max_bits < data_size * 16) return 0;
    
    memset(bits, 0, (max_bits + 7) / 8);
    
    uint8_t prev_bit = 0;
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < data_size && bit_pos + 16 <= max_bits; i++) {
        for (int j = 7; j >= 0; j--) {
            uint8_t data_bit = (data[i] >> j) & 1;
            
            /* MFM clock rule: insert 1 if both adjacent data bits are 0 */
            uint8_t clock_bit = (prev_bit == 0 && data_bit == 0) ? 1 : 0;
            
            /* Write clock bit */
            if (clock_bit) {
                bits[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            }
            bit_pos++;
            
            /* Write data bit */
            if (data_bit) {
                bits[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
            }
            bit_pos++;
            
            prev_bit = data_bit;
        }
    }
    
    return bit_pos;
}

/*============================================================================
 * FM DECODING
 *============================================================================*/

size_t uft_decode_fm_bits(const uint8_t *bits, size_t bit_count,
                          uint8_t *data, size_t data_size) {
    if (!bits || !data || bit_count < 16) return 0;
    
    size_t data_pos = 0;
    size_t bit_pos = 0;
    
    /* FM: every other bit is clock (always 1), data at odd positions */
    while (bit_pos + 16 <= bit_count && data_pos < data_size) {
        uint8_t byte = 0;
        for (int i = 0; i < 8; i++) {
            size_t pos = bit_pos + (i * 2) + 1;
            if (pos < bit_count) {
                byte = (byte << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
            }
        }
        data[data_pos++] = byte;
        bit_pos += 16;
    }
    
    return data_pos;
}

/*============================================================================
 * TRACK DECODING
 *============================================================================*/

/**
 * @brief Get clock period for density
 */
static uint32_t get_clock_period(uft_density_t density) {
    switch (density) {
        case UFT_DENSITY_SD: return UFT_FM_CELL_SD_NS;
        case UFT_DENSITY_DD: return UFT_MFM_CELL_DD_NS;
        case UFT_DENSITY_HD: return UFT_MFM_CELL_HD_NS;
        case UFT_DENSITY_ED: return UFT_MFM_CELL_ED_NS;
        default:             return UFT_MFM_CELL_DD_NS;
    }
}

/**
 * @brief Decode flux transitions to bit stream using PLL
 */
static size_t flux_to_bits(const uft_flux_buffer_t *flux,
                           const uft_decoder_config_t *config,
                           uint8_t *bits, size_t max_bits,
                           uft_logtime_t start_time, uft_logtime_t end_time) {
    if (!flux || !bits || flux->count < 2) return 0;
    
    /* Initialize PLL */
    uft_pll_state_t pll;
    uint32_t clock_period = config->clock_period;
    if (clock_period == 0) {
        clock_period = get_clock_period(config->density);
    }
    uft_pll_init(&pll, clock_period);
    uft_pll_set_gains(&pll, config->pll_gain_p, config->pll_gain_i);
    
    memset(bits, 0, (max_bits + 7) / 8);
    
    size_t bit_pos = 0;
    uft_logtime_t prev_time = 0;
    uint8_t bit_buf[8];
    
    /* Find starting position */
    size_t start_idx = 0;
    while (start_idx < flux->count && flux->times[start_idx] < start_time) {
        start_idx++;
    }
    
    /* Process flux transitions */
    for (size_t i = start_idx; i < flux->count && bit_pos < max_bits; i++) {
        uft_logtime_t cur_time = flux->times[i];
        
        if (end_time > 0 && cur_time > end_time) break;
        
        if (prev_time > 0) {
            uft_logtime_t delta = cur_time - prev_time;
            
            /* Use PLL to decode */
            int n_bits = uft_pll_process(&pll, delta, bit_buf);
            
            /* Add bits to output */
            for (int b = 0; b < n_bits && bit_pos < max_bits; b++) {
                if (bit_buf[b]) {
                    bits[bit_pos / 8] |= (1 << (7 - (bit_pos % 8)));
                }
                bit_pos++;
            }
        }
        prev_time = cur_time;
    }
    
    return bit_pos;
}

/**
 * @brief Parse MFM sector from bit stream
 */
static bool parse_mfm_sector(const uint8_t *bits, size_t bit_count, 
                             size_t *bit_pos, uft_sector_t *sector) {
    if (!bits || !sector || *bit_pos + 640 > bit_count) return false;
    
    /* Temporary buffer for decoded data */
    uint8_t data[1024];
    
    /* Decode ID field: IDAM + C + H + R + N + CRC (6 bytes + CRC) */
    size_t id_bits = 7 * 16;  /* 7 bytes in MFM */
    if (*bit_pos + id_bits > bit_count) return false;
    
    size_t decoded = uft_decode_mfm_bits(bits + (*bit_pos / 8), id_bits, data, 7);
    if (decoded < 7) return false;
    
    /* Verify IDAM */
    if (data[0] != UFT_AM_IDAM) return false;
    
    /* Extract ID */
    sector->header.id.cylinder = data[1];
    sector->header.id.head = data[2];
    sector->header.id.sector = data[3];
    sector->header.id.size_code = data[4];
    sector->header.header_crc = (data[5] << 8) | data[6];
    
    /* Verify header CRC */
    uint16_t calc_crc = uft_crc_ccitt(data, 5);
    sector->header.header_crc_ok = (calc_crc == sector->header.header_crc);
    
    *bit_pos += id_bits;
    
    /* Skip Gap2 and look for DAM/DDAM */
    size_t max_gap2 = 50 * 16;  /* Max ~50 bytes */
    size_t gap2_end = *bit_pos + max_gap2;
    if (gap2_end > bit_count) gap2_end = bit_count;
    
    /* Look for sync + DAM/DDAM */
    int dam_pos = find_mfm_sync(bits, gap2_end, *bit_pos);
    if (dam_pos < 0) {
        sector->fdc_status.reg2 |= UFT_FDC_ST2_NOT_DAM;
        return false;
    }
    
    *bit_pos = dam_pos + 16;  /* Skip sync */
    
    /* Decode DAM byte */
    if (*bit_pos + 16 > bit_count) return false;
    decoded = uft_decode_mfm_bits(bits + (*bit_pos / 8), 16, data, 1);
    if (decoded < 1) return false;
    
    sector->data_mark = data[0];
    if (sector->data_mark != UFT_AM_DAM && sector->data_mark != UFT_AM_DDAM) {
        return false;
    }
    
    *bit_pos += 16;
    
    /* Calculate data size */
    sector->data_size = uft_sector_size_from_code(sector->header.id.size_code);
    
    /* Decode data field + CRC */
    size_t data_bits = (sector->data_size + 2) * 16;
    if (*bit_pos + data_bits > bit_count) return false;
    
    sector->data = malloc(sector->data_size);
    if (!sector->data) return false;
    
    decoded = uft_decode_mfm_bits(bits + (*bit_pos / 8), data_bits, 
                                   data, sector->data_size + 2);
    if (decoded < (size_t)(sector->data_size + 2)) {
        free(sector->data);
        sector->data = NULL;
        return false;
    }
    
    memcpy(sector->data, data, sector->data_size);
    sector->data_crc = (data[sector->data_size] << 8) | data[sector->data_size + 1];
    
    /* Verify data CRC */
    /* CRC includes DAM byte */
    uint8_t crc_buf[8192];
    crc_buf[0] = sector->data_mark;
    memcpy(crc_buf + 1, sector->data, sector->data_size);
    calc_crc = uft_crc_ccitt(crc_buf, sector->data_size + 1);
    sector->data_crc_ok = (calc_crc == sector->data_crc);
    
    if (!sector->data_crc_ok) {
        sector->fdc_status.reg2 |= UFT_FDC_ST2_CRC_ERROR;
    }
    
    *bit_pos += data_bits;
    
    return true;
}

bool uft_decode_track(const uft_flux_buffer_t *flux,
                      const uft_decoder_config_t *config,
                      uft_track_t *track) {
    if (!flux || !config || !track) return false;
    
    memset(track, 0, sizeof(uft_track_t));
    track->encoding = config->encoding;
    track->density = config->density;
    
    /* Allocate bit buffer */
    size_t max_bits = flux->count * 4;  /* Conservative estimate */
    uint8_t *bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return false;
    
    /* Determine time range */
    uft_logtime_t start_time = 0;
    uft_logtime_t end_time = 0;
    
    if (flux->index_count >= 2) {
        start_time = flux->index_times[0];
        end_time = flux->index_times[1];
        track->index_time = end_time - start_time;
        track->revolution_count = 1;
    }
    
    /* Convert flux to bits */
    size_t bit_count = flux_to_bits(flux, config, bits, max_bits, 
                                     start_time, end_time);
    if (bit_count == 0) {
        free(bits);
        return false;
    }
    
    track->track_length = (uint32_t)bit_count;
    
    /* Find and decode sectors */
    size_t bit_pos = 0;
    while (bit_pos < bit_count && track->sector_count < UFT_SECTORS_MAX) {
        /* Look for sync */
        int sync_pos = find_mfm_sync(bits, bit_count, bit_pos);
        if (sync_pos < 0) break;
        
        bit_pos = sync_pos + 3 * 16;  /* Skip 3x A1 sync */
        
        /* Try to parse sector */
        uft_sector_t *sector = &track->sectors[track->sector_count];
        memset(sector, 0, sizeof(uft_sector_t));
        
        if (parse_mfm_sector(bits, bit_count, &bit_pos, sector)) {
            sector->revolution = 0;
            sector->confidence = sector->data_crc_ok ? 1.0f : 0.5f;
            
            if (sector->data_crc_ok) {
                track->healthy_sectors++;
            } else {
                track->bad_sectors++;
            }
            
            track->sector_count++;
        } else {
            /* Clean up failed parse */
            if (sector->data) {
                free(sector->data);
                sector->data = NULL;
            }
        }
    }
    
    free(bits);
    
    /* Calculate bit rate */
    if (track->index_time > 0) {
        track->bit_rate = (uint32_t)((uint64_t)bit_count * 1000000000ULL / 
                                      track->index_time);
    }
    
    return true;
}

/*============================================================================
 * SECTOR/TRACK UTILITIES
 *============================================================================*/

bool uft_sector_verify(const uft_sector_t *sector) {
    if (!sector) return false;
    return sector->header.header_crc_ok && sector->data_crc_ok;
}

void uft_sector_free(uft_sector_t *sector) {
    if (!sector) return;
    if (sector->data) {
        free(sector->data);
        sector->data = NULL;
    }
}

void uft_track_free(uft_track_t *track) {
    if (!track) return;
    for (int i = 0; i < track->sector_count; i++) {
        uft_sector_free(&track->sectors[i]);
    }
    track->sector_count = 0;
}

uint16_t uft_sector_size_from_code(uint8_t code) {
    if (code > 7) code = 7;
    return 128 << code;
}

uint8_t uft_size_code_from_size(uint16_t size) {
    uint8_t code = 0;
    while (size > 128 && code < 7) {
        size >>= 1;
        code++;
    }
    return code;
}

/*============================================================================
 * ENCODING/DENSITY DETECTION
 *============================================================================*/

uft_encoding_t uft_detect_encoding(const uft_flux_buffer_t *flux) {
    if (!flux || flux->count < 100) return UFT_ENC_UNKNOWN;
    
    /* Build histogram of flux times */
    uint32_t bins[16] = {0};
    const uint32_t bin_width = 1000;  /* 1µs bins */
    
    uft_logtime_t prev = 0;
    for (size_t i = 0; i < flux->count && i < 10000; i++) {
        if (prev > 0) {
            uft_logtime_t delta = flux->times[i] - prev;
            int bin = delta / bin_width;
            if (bin < 16) bins[bin]++;
        }
        prev = flux->times[i];
    }
    
    /* MFM has peaks at ~4µs, ~6µs, ~8µs (bins 4, 6, 8 for DD) */
    /* FM has peaks at ~8µs, ~16µs (bins 8, 16 for SD) */
    
    uint32_t mfm_score = bins[3] + bins[4] + bins[5] + bins[6] + bins[7] + bins[8];
    uint32_t fm_score = bins[7] + bins[8] + bins[9] + bins[15];
    
    if (mfm_score > fm_score * 2) {
        return UFT_ENC_MFM;
    } else if (fm_score > mfm_score) {
        return UFT_ENC_FM;
    }
    
    return UFT_ENC_MFM;  /* Default to MFM */
}

uft_density_t uft_detect_density(const uft_flux_buffer_t *flux) {
    if (!flux || flux->count < 100) return UFT_DENSITY_UNKNOWN;
    
    /* Estimate bit rate from flux count and revolution time */
    if (flux->index_count >= 2) {
        uft_logtime_t rev_time = flux->index_times[1] - flux->index_times[0];
        if (rev_time > 0) {
            /* Count flux transitions in first revolution */
            size_t count = 0;
            for (size_t i = 0; i < flux->count; i++) {
                if (flux->times[i] >= flux->index_times[0] &&
                    flux->times[i] < flux->index_times[1]) {
                    count++;
                }
            }
            
            /* Estimate bits (roughly 1.5x flux count for MFM) */
            uint64_t bits = count * 3 / 2;
            uint64_t bit_rate = bits * 1000000000ULL / rev_time;
            
            if (bit_rate < 150000) return UFT_DENSITY_SD;
            if (bit_rate < 350000) return UFT_DENSITY_DD;
            if (bit_rate < 700000) return UFT_DENSITY_HD;
            return UFT_DENSITY_ED;
        }
    }
    
    return UFT_DENSITY_DD;  /* Default to DD */
}

void uft_flux_histogram(const uft_flux_buffer_t *flux,
                        uint32_t *bins, size_t bin_count, uint32_t bin_width) {
    if (!flux || !bins) return;
    
    memset(bins, 0, bin_count * sizeof(uint32_t));
    
    uft_logtime_t prev = 0;
    for (size_t i = 0; i < flux->count; i++) {
        if (prev > 0) {
            uft_logtime_t delta = flux->times[i] - prev;
            size_t bin = delta / bin_width;
            if (bin < bin_count) bins[bin]++;
        }
        prev = flux->times[i];
    }
}

/*============================================================================
 * TRACK MINING
 *============================================================================*/

void uft_mining_init_default(uft_mining_target_t *target) {
    if (!target) return;
    
    target->min_healthy = 9;  /* Typical 9 sectors for DD */
    target->require_all_headers = true;
    target->require_all_data = false;
    target->timeout_ms = 60000;  /* 1 minute */
    target->max_retries = 100;
    target->head_calibration_every = 10;  /* Calibrate every 10 attempts */
    target->stop_on_success = true;
}

void uft_mine_cancel(void) {
    g_mining_cancel = true;
}

bool uft_mine_track(void *device, uint8_t cyl, uint8_t head,
                    const uft_mining_target_t *target,
                    const uft_decoder_config_t *config,
                    uft_mining_result_t *result,
                    void (*progress_cb)(int attempt, int best_healthy, void *ctx),
                    void *cb_ctx) {
    if (!target || !config || !result) return false;
    
    memset(result, 0, sizeof(uft_mining_result_t));
    g_mining_cancel = false;
    
    /* Note: Actual device I/O must be provided by caller through device pointer */
    /* This is a framework that the UFT device layer should implement */
    
    (void)device;
    (void)cyl;
    (void)head;
    
    result->success = false;
    result->attempts = 0;
    result->best_healthy = 0;
    
    /* TODO: Implement mining loop with device I/O integration */
    /* The mining loop should:
     * 1. Read track from device
     * 2. Decode with config
     * 3. Count healthy sectors
     * 4. If target met, success
     * 5. Keep best result
     * 6. Repeat until timeout/cancelled
     */
    
    if (progress_cb) {
        progress_cb(0, 0, cb_ctx);
    }
    
    return result->success;
}

bool uft_merge_revolutions(const uft_track_t *tracks, size_t count,
                           uft_track_t *output) {
    if (!tracks || !output || count == 0) return false;
    
    memset(output, 0, sizeof(uft_track_t));
    
    /* Copy basic info from first track */
    output->cylinder = tracks[0].cylinder;
    output->head = tracks[0].head;
    output->encoding = tracks[0].encoding;
    output->density = tracks[0].density;
    
    /* For each sector ID, find best version across revolutions */
    for (size_t t = 0; t < count; t++) {
        const uft_track_t *track = &tracks[t];
        
        for (int s = 0; s < track->sector_count; s++) {
            const uft_sector_t *sector = &track->sectors[s];
            
            /* Check if we already have this sector */
            bool found = false;
            for (int o = 0; o < output->sector_count; o++) {
                uft_sector_t *existing = &output->sectors[o];
                
                if (existing->header.id.cylinder == sector->header.id.cylinder &&
                    existing->header.id.head == sector->header.id.head &&
                    existing->header.id.sector == sector->header.id.sector &&
                    existing->header.id.size_code == sector->header.id.size_code) {
                    
                    found = true;
                    
                    /* Replace if new one is better */
                    if (sector->confidence > existing->confidence) {
                        uft_sector_free(existing);
                        memcpy(existing, sector, sizeof(uft_sector_t));
                        
                        /* Deep copy data */
                        if (sector->data) {
                            existing->data = malloc(sector->data_size);
                            if (existing->data) {
                                memcpy(existing->data, sector->data, sector->data_size);
                            }
                        }
                        existing->revolution = (uint8_t)t;
                    }
                    break;
                }
            }
            
            /* Add new sector */
            if (!found && output->sector_count < UFT_SECTORS_MAX) {
                uft_sector_t *new_sector = &output->sectors[output->sector_count];
                memcpy(new_sector, sector, sizeof(uft_sector_t));
                
                if (sector->data) {
                    new_sector->data = malloc(sector->data_size);
                    if (new_sector->data) {
                        memcpy(new_sector->data, sector->data, sector->data_size);
                    }
                }
                new_sector->revolution = (uint8_t)t;
                output->sector_count++;
            }
        }
    }
    
    /* Update statistics */
    for (int s = 0; s < output->sector_count; s++) {
        if (output->sectors[s].data_crc_ok) {
            output->healthy_sectors++;
        } else {
            output->bad_sectors++;
        }
    }
    
    output->revolution_count = (uint8_t)count;
    output->consistent = (output->bad_sectors == 0);
    
    return true;
}

/*============================================================================
 * STRING UTILITIES
 *============================================================================*/

const char *uft_encoding_name(uft_encoding_t enc) {
    switch (enc) {
        case UFT_ENC_FM:        return "FM";
        case UFT_ENC_MFM:       return "MFM";
        case UFT_ENC_GCR_APPLE: return "GCR-Apple";
        case UFT_ENC_GCR_C64:   return "GCR-C64";
        case UFT_ENC_GCR_AMIGA: return "GCR-Amiga";
        case UFT_ENC_RLL:       return "RLL";
        default:                return "Unknown";
    }
}

const char *uft_density_name(uft_density_t den) {
    switch (den) {
        case UFT_DENSITY_SD: return "SD (125kbps)";
        case UFT_DENSITY_DD: return "DD (250kbps)";
        case UFT_DENSITY_QD: return "QD (500kbps)";
        case UFT_DENSITY_HD: return "HD (500kbps)";
        case UFT_DENSITY_ED: return "ED (1Mbps)";
        default:             return "Unknown";
    }
}

const char *uft_decoder_name(uft_decoder_algo_t algo) {
    switch (algo) {
        case UFT_DECODER_SIMPLE:       return "Simple";
        case UFT_DECODER_PLL_FIXED:    return "PLL-Fixed";
        case UFT_DECODER_PLL_ADAPTIVE: return "PLL-Adaptive";
        case UFT_DECODER_KEIR_FRASER:  return "Keir Fraser";
        case UFT_DECODER_MARK_OGDEN:   return "Mark Ogden";
        default:                       return "None";
    }
}

void uft_sector_status_string(const uft_sector_t *sector,
                               char *buffer, size_t size) {
    if (!sector || !buffer || size == 0) return;
    
    snprintf(buffer, size, 
             "C=%u H=%u R=%u N=%u [%s] %s%s",
             sector->header.id.cylinder,
             sector->header.id.head,
             sector->header.id.sector,
             sector->header.id.size_code,
             sector->data_mark == UFT_AM_DDAM ? "DEL" : "DAT",
             sector->header.header_crc_ok ? "ID:OK " : "ID:BAD ",
             sector->data_crc_ok ? "DATA:OK" : "DATA:BAD");
}

void uft_track_summary_string(const uft_track_t *track,
                               char *buffer, size_t size) {
    if (!track || !buffer || size == 0) return;
    
    snprintf(buffer, size,
             "C%u/H%u: %u sectors (%u OK, %u bad) %s %s",
             track->cylinder, track->head,
             track->sector_count, track->healthy_sectors, track->bad_sectors,
             uft_encoding_name(track->encoding),
             uft_density_name(track->density));
}

bool uft_fdc_has_error(const uft_fdc_status_t *status) {
    if (!status) return false;
    return (status->reg1 & 0x25) != 0 || (status->reg2 & 0x61) != 0;
}

bool uft_fdc_missing_id(const uft_fdc_status_t *status) {
    if (!status) return false;
    return (status->reg1 & UFT_FDC_ST1_NO_AM) != 0;
}

bool uft_fdc_crc_error(const uft_fdc_status_t *status) {
    if (!status) return false;
    return (status->reg1 & UFT_FDC_ST1_DATA_ERROR) != 0 ||
           (status->reg2 & UFT_FDC_ST2_CRC_ERROR) != 0;
}
