#include "uft/compat/uft_platform.h"
#include "uft_error.h"
#include "uft_error_compat.h"
#include "uft_track.h"
/**
 * @file uft_amiga_mfm_decoder_v2.c
 * @brief Amiga MFM Decoder - Unified Registry Version
 * 
 * AUDIT FIX v3.3.3: Implemented proper checksum verification
 */

#include "uft/uft_decoder_registry.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>

#define AMIGA_SYNC          0x4489
#define AMIGA_SECTORS_DD    11
#define AMIGA_SECTORS_HD    22
#define AMIGA_SECTOR_SIZE   512
#define AMIGA_DD_CELL_NS    2000
#define AMIGA_HD_CELL_NS    1000

typedef struct {
    double cell_time, nominal, min_cell, max_cell, adjust;
} amiga_pll_t;

static void amiga_pll_init(amiga_pll_t* pll, bool hd) {
    pll->nominal = hd ? AMIGA_HD_CELL_NS : AMIGA_DD_CELL_NS;
    pll->cell_time = pll->nominal;
    pll->min_cell = pll->nominal * 0.75;
    pll->max_cell = pll->nominal * 1.25;
    pll->adjust = 0.05;
}

static int amiga_pll_process(amiga_pll_t* pll, uint32_t delta_ns) {
    double cells = (double)delta_ns / pll->cell_time;
    int n = (int)(cells + 0.5);
    if (n < 1) n = 1;
    if (n > 5) n = 5;
    double err = delta_ns - n * pll->cell_time;
    pll->cell_time += (err / n) * pll->adjust;
    if (pll->cell_time < pll->min_cell) pll->cell_time = pll->min_cell;
    if (pll->cell_time > pll->max_cell) pll->cell_time = pll->max_cell;
    return n;
}

static size_t amiga_flux_to_bits(const uft_flux_revolution_t* rev,
                                  amiga_pll_t* pll,
                                  uint8_t* bits, size_t max_bits) {
    size_t bit_pos = 0;
    for (size_t i = 0; i < rev->count && bit_pos < max_bits; i++) {
        int cells = amiga_pll_process(pll, rev->transitions[i].delta_ns);
        for (int c = 0; c < cells - 1 && bit_pos < max_bits; c++) {
            bits[bit_pos / 8] &= ~(0x80 >> (bit_pos % 8));
            bit_pos++;
        }
        if (bit_pos < max_bits) {
            bits[bit_pos / 8] |= (0x80 >> (bit_pos % 8));
            bit_pos++;
        }
    }
    return bit_pos;
}

static ssize_t amiga_find_sync(const uint8_t* bits, size_t bit_count, size_t start) {
    uint32_t buffer = 0;
    for (size_t pos = start; pos < bit_count; pos++) {
        buffer = (buffer << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        if ((buffer & 0xFFFF) == AMIGA_SYNC) {
            if (pos + 1 < bit_count && ((bits[(pos+1)/8] >> (7-((pos+1)%8))) & 1) == 0) {
                return (ssize_t)(pos + 1);
            }
        }
    }
    return -1;
}

static uint32_t read_mfm_long(const uint8_t* bits, size_t bit_count, size_t* pos) {
    uint32_t val = 0;
    for (size_t i = 0; i < 32 && *pos < bit_count; i++) {
        val = (val << 1) | ((bits[*pos / 8] >> (7 - (*pos % 8))) & 1);
        (*pos)++;
    }
    return val;
}

static uint32_t amiga_decode_long(uint32_t odd, uint32_t even) {
    return ((odd & 0x55555555) << 1) | (even & 0x55555555);
}

/* ============================================================================
 * CHECKSUM VERIFICATION
 * ============================================================================ */

/**
 * @brief Calculate Amiga XOR checksum over MFM data
 * @param data Array of MFM-encoded 32-bit words (odd and even interleaved)
 * @param count Number of word pairs
 * @return Calculated checksum (masked to data bits)
 */
static uint32_t amiga_calc_checksum_raw(const uint32_t* words, size_t count) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < count; i++) {
        checksum ^= words[i];
    }
    return checksum & 0x55555555;
}

/**
 * @brief Verify Amiga sector header checksum
 * 
 * Header layout:
 * - info_odd (1 long)
 * - info_even (1 long)  
 * - label (8 longs: 4 odd + 4 even)
 * Total: 10 longs XORed for checksum
 */
static bool amiga_verify_header(uint32_t info_odd, uint32_t info_even,
                                 const uint32_t label[8],
                                 uint32_t chk_odd, uint32_t chk_even) {
    uint32_t calc = 0;
    
    /* XOR info */
    calc ^= info_odd;
    calc ^= info_even;
    
    /* XOR label (8 longs) */
    for (int i = 0; i < 8; i++) {
        calc ^= label[i];
    }
    
    calc &= 0x55555555;
    
    /* Decode stored checksum and compare */
    uint32_t stored = amiga_decode_long(chk_odd, chk_even);
    return (calc == stored);
}

/**
 * @brief Verify Amiga sector data checksum
 * 
 * Data: 128 odd longs + 128 even longs = 256 longs total
 */
static bool amiga_verify_data(const uint32_t odd_data[128],
                               const uint32_t even_data[128],
                               uint32_t chk_odd, uint32_t chk_even) {
    uint32_t calc = 0;
    
    /* XOR all data longs */
    for (int i = 0; i < 128; i++) {
        calc ^= odd_data[i];
        calc ^= even_data[i];
    }
    
    calc &= 0x55555555;
    
    /* Decode stored checksum and compare */
    uint32_t stored = amiga_decode_long(chk_odd, chk_even);
    return (calc == stored);
}

/* ============================================================================ */

static int amiga_mfm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0 || !confidence) return 0;
    *confidence = 0;
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count < 1000) return 0;
    
    uint64_t total = 0;
    size_t count = rev->count < 5000 ? rev->count : 5000;
    for (size_t i = 0; i < count; i++) total += rev->transitions[i].delta_ns;
    double avg = (double)total / count;
    
    bool hd = (avg < 4000);
    if (avg < 1500 || avg > 8000) return 0;
    *confidence = 45;
    
    amiga_pll_t pll;
    amiga_pll_init(&pll, hd);
    size_t max_bits = rev->count * 5;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return 0;
    
    size_t bit_count = amiga_flux_to_bits(rev, &pll, bits, max_bits);
    int sync_count = 0;
    ssize_t pos = 0;
    for (int i = 0; i < 30 && pos >= 0; i++) {
        pos = amiga_find_sync(bits, bit_count, (size_t)pos);
        if (pos > 0) { sync_count++; pos += 100; }
    }
    free(bits);
    
    if (sync_count >= 20) *confidence = 92;
    else if (sync_count >= 10) *confidence = 80;
    else if (sync_count >= 5) *confidence = 65;
    
    return (*confidence >= 50) ? 1 : 0;
}

static uft_error_t amiga_mfm_decode_track(const uft_flux_track_data_t* flux,
                                           uft_track_t* sectors,
                                           const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    if (flux->revolution_count == 0) return UFT_ERROR_NO_DATA;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    uint64_t total = 0;
    for (size_t i = 0; i < rev->count && i < 5000; i++) total += rev->transitions[i].delta_ns;
    double avg = (double)total / (rev->count < 5000 ? rev->count : 5000);
    bool hd = (avg < 4000);
    
    amiga_pll_t pll;
    amiga_pll_init(&pll, hd);
    if (opts && opts->pll_initial_period_us > 0) {
        pll.nominal = opts->pll_initial_period_us * 1000.0;
        pll.cell_time = pll.nominal;
    }
    
    size_t max_bits = rev->count * 5;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return UFT_ERROR_NO_MEMORY;
    
    size_t bit_count = amiga_flux_to_bits(rev, &pll, bits, max_bits);
    int max_sectors = hd ? AMIGA_SECTORS_HD : AMIGA_SECTORS_DD;
    sectors->sectors = calloc(max_sectors + 2, sizeof(uft_sector_t));
    if (!sectors->sectors) { free(bits); return UFT_ERROR_NO_MEMORY; }
    
    ssize_t bit_pos = 0;
    while (bit_pos >= 0 && sectors->sector_count < (size_t)(max_sectors + 2)) {
        bit_pos = amiga_find_sync(bits, bit_count, (size_t)bit_pos);
        if (bit_pos < 0) break;
        
        size_t pos = (size_t)bit_pos;
        
        /* Skip second sync if present */
        uint32_t peek = read_mfm_long(bits, bit_count, &pos);
        if ((peek >> 16) == AMIGA_SYNC) {
            pos -= 16;
        } else {
            pos -= 32;
        }
        
        /* Read header info */
        uint32_t info_odd = read_mfm_long(bits, bit_count, &pos);
        uint32_t info_even = read_mfm_long(bits, bit_count, &pos);
        uint32_t info = amiga_decode_long(info_odd, info_even);
        
        /* Read label (8 longs: 4 odd + 4 even, but stored as 8 sequential) */
        uint32_t label[8];
        for (int i = 0; i < 8; i++) {
            label[i] = read_mfm_long(bits, bit_count, &pos);
        }
        
        /* Read header checksum */
        uint32_t hdr_chk_odd = read_mfm_long(bits, bit_count, &pos);
        uint32_t hdr_chk_even = read_mfm_long(bits, bit_count, &pos);
        
        /* Verify header checksum */
        bool hdr_ok = amiga_verify_header(info_odd, info_even, label, 
                                          hdr_chk_odd, hdr_chk_even);
        
        /* Read data checksum */
        uint32_t dat_chk_odd = read_mfm_long(bits, bit_count, &pos);
        uint32_t dat_chk_even = read_mfm_long(bits, bit_count, &pos);
        
        /* Parse info */
        uint8_t format = (info >> 24) & 0xFF;
        uint8_t track_num = (info >> 16) & 0xFF;
        uint8_t sector_num = (info >> 8) & 0xFF;
        uint8_t sectors_to_gap = info & 0xFF;
        (void)format; (void)sectors_to_gap;
        
        /* Read 512 bytes data (128 odd longs + 128 even longs) */
        uint8_t data[512];
        uint32_t odd_data[128], even_data[128];
        
        for (size_t i = 0; i < 128; i++) odd_data[i] = read_mfm_long(bits, bit_count, &pos);
        for (size_t i = 0; i < 128; i++) even_data[i] = read_mfm_long(bits, bit_count, &pos);
        
        /* Verify data checksum */
        bool data_ok = amiga_verify_data(odd_data, even_data, dat_chk_odd, dat_chk_even);
        
        /* Decode data */
        for (int i = 0; i < 128; i++) {
            uint32_t val = amiga_decode_long(odd_data[i], even_data[i]);
            data[i*4+0] = (val >> 24) & 0xFF;
            data[i*4+1] = (val >> 16) & 0xFF;
            data[i*4+2] = (val >> 8) & 0xFF;
            data[i*4+3] = val & 0xFF;
        }
        
        /* Store sector */
        uft_sector_t* s = &sectors->sectors[sectors->sector_count];
        memset(s, 0, sizeof(*s));
        s->id.cylinder = track_num / 2;
        s->id.head = track_num % 2;
        s->id.sector = sector_num;
        s->id.size_code = 2;  /* 512 bytes */
        s->id.crc_ok = hdr_ok;  /* Header CRC verification */
        
        s->data = malloc(512);
        if (s->data) { 
            memcpy(s->data, data, 512); 
            s->data_size = 512; 
        }
        
        /* Set status based on checksum verification */
        if (hdr_ok && data_ok) {
            s->status = UFT_SECTOR_OK;
        } else if (hdr_ok && !data_ok) {
            s->status = UFT_SECTOR_DATA_CRC_ERROR;
        } else {
            s->status = UFT_SECTOR_HEADER_CRC_ERROR;
        }
        
        sectors->sector_count++;
        bit_pos = (ssize_t)pos;
    }
    
    free(bits);
    return UFT_OK;
}

static void amiga_mfm_get_default_options(uft_decode_options_t* opts) {
    if (!opts) return;
    memset(opts, 0, sizeof(*opts));
    opts->struct_size = sizeof(*opts);
    opts->pll_initial_period_us = 2.0;
    opts->pll_period_tolerance = 0.25;
    opts->pll_phase_adjust = 0.05;
    opts->max_retries = 3;
    opts->use_multiple_revolutions = true;
}

const uft_decoder_ops_t uft_decoder_amiga_mfm_v2 = {
    .name = "Amiga-MFM",
    .description = "Amiga MFM (880K DD, 1.76MB HD)",
    .version = 0x00020001,  /* v2.0.1 - checksum fix */
    .encoding = UFT_ENCODING_MFM,
    .probe = amiga_mfm_probe,
    .decode_track = amiga_mfm_decode_track,
    .encode_track = NULL,
    .get_default_options = amiga_mfm_get_default_options,
};
