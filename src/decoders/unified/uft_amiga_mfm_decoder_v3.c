#include "uft/compat/uft_platform.h"
#include "uft_error.h"
#include "uft_error_compat.h"
#include "uft_track.h"
/**
 * @file uft_amiga_mfm_decoder_v3.c
 * @brief Amiga MFM Decoder v3 - With Proper Checksum Verification
 * 
 * AUDIT FIX: Implements proper header and data checksum verification
 * that was marked as TODO in v2.
 */

#include "uft/decoders/uft_amiga_mfm_decoder.h"
#include "uft/uft_sector.h"
#include "uft/core/uft_platform.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

// ============================================================================
// CONSTANTS
// ============================================================================

#define AMIGA_SYNC_WORD     0x4489
#define AMIGA_SECTOR_SIZE   512
#define AMIGA_SECTORS_TRACK 11
#define AMIGA_GAP_SIZE      2

// ============================================================================
// CHECKSUM FUNCTIONS
// ============================================================================

static inline uint32_t amiga_decode_long(uint32_t odd, uint32_t even) {
    return ((odd & 0x55555555) << 1) | (even & 0x55555555);
}

static uint32_t amiga_calc_checksum(const uint32_t* data, size_t count) {
    uint32_t checksum = 0;
    for (size_t i = 0; i < count; i++) {
        checksum ^= data[i];
    }
    return checksum & 0x55555555;
}

static bool amiga_verify_header(uint32_t info_odd, uint32_t info_even,
                                 const uint32_t label[8],
                                 uint32_t hdr_chk_odd, uint32_t hdr_chk_even) {
    uint32_t checksum = info_odd ^ info_even;
    
    for (int i = 0; i < 8; i++) {
        checksum ^= label[i];
    }
    
    checksum &= 0x55555555;
    uint32_t stored = amiga_decode_long(hdr_chk_odd, hdr_chk_even);
    
    return (checksum == stored);
}

static bool amiga_verify_data(const uint32_t odd[128], const uint32_t even[128],
                               uint32_t dat_chk_odd, uint32_t dat_chk_even) {
    uint32_t checksum = 0;
    
    for (int i = 0; i < 128; i++) {
        checksum ^= odd[i];
        checksum ^= even[i];
    }
    
    checksum &= 0x55555555;
    uint32_t stored = amiga_decode_long(dat_chk_odd, dat_chk_even);
    
    return (checksum == stored);
}

// ============================================================================
// BIT READING
// ============================================================================

static uint32_t read_mfm_long(const uint8_t* bits, size_t bit_count, size_t* pos) {
    if (*pos + 32 > bit_count) return 0;
    
    uint32_t value = 0;
    for (int i = 0; i < 32; i++) {
        size_t byte_idx = (*pos + i) / 8;
        size_t bit_idx = 7 - ((*pos + i) % 8);
        if (bits[byte_idx] & (1 << bit_idx)) {
            value |= (1U << (31 - i));
        }
    }
    
    *pos += 32;
    return value;
}

static ssize_t find_sync(const uint8_t* bits, size_t bit_count, size_t start) {
    for (size_t i = start; i + 16 <= bit_count; i++) {
        uint16_t word = 0;
        for (int j = 0; j < 16; j++) {
            size_t byte_idx = (i + j) / 8;
            size_t bit_idx = 7 - ((i + j) % 8);
            if (bits[byte_idx] & (1 << bit_idx)) {
                word |= (1U << (15 - j));
            }
        }
        if (word == AMIGA_SYNC_WORD) {
            // Check for double sync
            if (i + 32 <= bit_count) {
                uint16_t word2 = 0;
                for (int j = 0; j < 16; j++) {
                    size_t byte_idx = (i + 16 + j) / 8;
                    size_t bit_idx = 7 - ((i + 16 + j) % 8);
                    if (bits[byte_idx] & (1 << bit_idx)) {
                        word2 |= (1U << (15 - j));
                    }
                }
                if (word2 == AMIGA_SYNC_WORD) {
                    return (ssize_t)(i + 32);  // After double sync
                }
            }
        }
    }
    return -1;
}

// ============================================================================
// MAIN DECODER
// ============================================================================

/**
 * @brief Simple software PLL for flux-to-bits conversion
 */
static size_t pll_flux_to_bits(const uft_flux_t* flux, uint8_t* bits, size_t max_bits) {
    if (!flux || !flux->flux_data || !bits || flux->flux_count == 0) {
        return 0;
    }
    
    /* Calculate average flux interval from first ~1000 samples */
    uint64_t sum = 0;
    size_t sample_count = (flux->flux_count < 1000) ? flux->flux_count : 1000;
    for (size_t i = 0; i < sample_count; i++) {
        sum += flux->flux_data[i];
    }
    uint32_t avg_interval = (uint32_t)(sum / sample_count);
    
    /* Calculate bit cell time (Amiga = 2µs at 300 RPM) */
    /* Adjust based on flux sample rate if known */
    uint32_t bit_cell = avg_interval;
    if (bit_cell < 10) bit_cell = 10;  /* Sanity check */
    
    /* PLL state */
    int32_t phase = 0;
    uint32_t freq = bit_cell;
    const int32_t phase_adjust = 5;    /* Phase correction strength (%) */
    const int32_t freq_adjust = 1;     /* Frequency correction strength (%) */
    
    size_t bit_pos = 0;
    int32_t accumulated = 0;
    
    for (size_t i = 0; i < flux->flux_count && bit_pos < max_bits; i++) {
        int32_t interval = (int32_t)flux->flux_data[i];
        accumulated += interval;
        
        /* Process all bit cells in this interval */
        while (accumulated > (int32_t)(freq / 2) && bit_pos < max_bits) {
            int32_t cell_center = (int32_t)freq;
            
            if (accumulated >= cell_center) {
                /* We're past the cell center - this could be a 1 */
                int32_t error = accumulated - cell_center;
                
                if (error < (int32_t)(freq * 3 / 4)) {
                    /* Flux transition detected in this cell window */
                    size_t byte_idx = bit_pos / 8;
                    size_t bit_idx = 7 - (bit_pos % 8);
                    bits[byte_idx] |= (uint8_t)(1 << bit_idx);
                    
                    /* Apply PLL corrections */
                    phase += (error * phase_adjust) / 100;
                    freq = (uint32_t)((int32_t)freq + (error * freq_adjust) / 100);
                    
                    /* Clamp frequency to reasonable range */
                    uint32_t min_freq = avg_interval * 80 / 100;
                    uint32_t max_freq = avg_interval * 120 / 100;
                    if (freq < min_freq) freq = min_freq;
                    if (freq > max_freq) freq = max_freq;
                    
                    accumulated = 0;
                } else {
                    /* No transition - output 0 */
                }
            }
            
            bit_pos++;
            accumulated -= (int32_t)freq;
            
            /* Prevent runaway accumulation */
            if (accumulated < -(int32_t)(freq * 2)) {
                accumulated = -(int32_t)freq;
            }
        }
    }
    
    return bit_pos;
}

uft_error_t uft_amiga_mfm_decode_track_v3(const uft_flux_t* flux,
                                           uft_track_sectors_t* sectors,
                                           uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    (void)opts;
    
    /* Allocate bit buffer - Amiga track is ~105000 raw bits */
    size_t max_bits = 150000;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return UFT_ERROR_MEMORY;
    
    /* Convert flux to bits using PLL */
    size_t bit_count = 0;
    if (flux->flux_data && flux->flux_count > 0) {
        bit_count = pll_flux_to_bits(flux, bits, max_bits);
    }
    
    if (bit_count == 0) {
        free(bits);
        return UFT_ERROR_NO_DATA;
    }
    
    // Initialize sectors
    memset(sectors, 0, sizeof(*sectors));
    sectors->sectors = calloc(AMIGA_SECTORS_TRACK, sizeof(uft_sector_t));
    if (!sectors->sectors) {
        free(bits);
        return UFT_ERROR_MEMORY;
    }
    
    // Find and decode sectors
    ssize_t bit_pos = 0;
    int sectors_found = 0;
    
    while (sectors_found < AMIGA_SECTORS_TRACK) {
        bit_pos = find_sync(bits, bit_count, (size_t)bit_pos);
        if (bit_pos < 0) break;
        
        size_t pos = (size_t)bit_pos;
        
        // Need at least header + data
        if (pos + (1 + 1 + 8 + 2 + 256 + 256) * 32 > bit_count) break;
        
        // Read header
        uint32_t info_odd = read_mfm_long(bits, bit_count, &pos);
        uint32_t info_even = read_mfm_long(bits, bit_count, &pos);
        uint32_t info = amiga_decode_long(info_odd, info_even);
        
        // Read label (8 longs = 4 odd + 4 even)
        uint32_t label[8];
        for (int i = 0; i < 8; i++) {
            label[i] = read_mfm_long(bits, bit_count, &pos);
        }
        
        // Read header checksum
        uint32_t hdr_chk_odd = read_mfm_long(bits, bit_count, &pos);
        uint32_t hdr_chk_even = read_mfm_long(bits, bit_count, &pos);
        
        // VERIFY HEADER CHECKSUM (FIX for TODO)
        bool header_ok = amiga_verify_header(info_odd, info_even, label,
                                              hdr_chk_odd, hdr_chk_even);
        
        // Read data checksum
        uint32_t dat_chk_odd = read_mfm_long(bits, bit_count, &pos);
        uint32_t dat_chk_even = read_mfm_long(bits, bit_count, &pos);
        
        // Parse info
        uint8_t format = (info >> 24) & 0xFF;
        uint8_t track_num = (info >> 16) & 0xFF;
        uint8_t sector_num = (info >> 8) & 0xFF;
        uint8_t sectors_to_gap = info & 0xFF;
        (void)format; (void)sectors_to_gap;
        
        // Read data (128 odd longs, 128 even longs)
        uint32_t odd_data[128], even_data[128];
        uint8_t data[512];
        
        for (int i = 0; i < 128; i++) {
            odd_data[i] = read_mfm_long(bits, bit_count, &pos);
        }
        for (int i = 0; i < 128; i++) {
            even_data[i] = read_mfm_long(bits, bit_count, &pos);
        }
        
        // VERIFY DATA CHECKSUM (FIX for TODO)
        bool data_ok = amiga_verify_data(odd_data, even_data, 
                                          dat_chk_odd, dat_chk_even);
        
        // Decode data
        for (int i = 0; i < 128; i++) {
            uint32_t val = amiga_decode_long(odd_data[i], even_data[i]);
            data[i*4+0] = (val >> 24) & 0xFF;
            data[i*4+1] = (val >> 16) & 0xFF;
            data[i*4+2] = (val >> 8) & 0xFF;
            data[i*4+3] = val & 0xFF;
        }
        
        // Store sector
        uft_sector_t* s = &sectors->sectors[sectors->sector_count];
        memset(s, 0, sizeof(*s));
        s->id.cylinder = track_num / 2;
        s->id.head = track_num % 2;
        s->id.sector = sector_num;
        s->id.size_code = 2;  // 512 bytes
        
        // Set CRC status based on checksums (FIXED!)
        s->id.crc_ok = header_ok;
        
        s->data = malloc(512);
        if (s->data) {
            memcpy(s->data, data, 512);
            s->data_size = 512;
        }
        
        // Set status based on both checksums
        if (header_ok && data_ok) {
            s->status = UFT_SECTOR_OK;
        } else if (header_ok) {
            s->status = UFT_SECTOR_DATA_CRC_ERROR;
        } else {
            s->status = UFT_SECTOR_HEADER_CRC_ERROR;
        }
        
        sectors->sector_count++;
        sectors_found++;
        
        bit_pos = (ssize_t)pos;
    }
    
    free(bits);
    return UFT_OK;
}
