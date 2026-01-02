/**
 * @file uft_fm_decoder_v2.c
 * @brief FM Decoder - Unified Registry Version
 * 
 * Single Density FM (Frequency Modulation):
 * - 8" SD disks
 * - Early 5.25" SD disks
 * - Some TRS-80 formats
 */

#include "uft/uft_decoder_registry.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// FM Constants
// ============================================================================

#define FM_SYNC_PATTERN     0xF57E      // Data address mark clock/data
#define FM_IDAM_MARK        0xFE        // ID Address Mark
#define FM_DAM_MARK         0xFB        // Data Address Mark
#define FM_DDAM_MARK        0xF8        // Deleted Data Address Mark
#define FM_IAM_MARK         0xFC        // Index Address Mark

#define FM_SD_CELL_NS       8000        // 8µs cell time (125kbit/s)

// ============================================================================
// FM PLL State
// ============================================================================

typedef struct {
    double      cell_time;
    double      nominal;
    double      min_cell;
    double      max_cell;
    double      adjust_rate;
} fm_pll_state_t;

static void fm_pll_init(fm_pll_state_t* pll, double nominal_ns) {
    pll->cell_time = nominal_ns;
    pll->nominal = nominal_ns;
    pll->min_cell = nominal_ns * 0.70;
    pll->max_cell = nominal_ns * 1.30;
    pll->adjust_rate = 0.05;
}

static int fm_pll_process(fm_pll_state_t* pll, uint32_t delta_ns) {
    double cells = (double)delta_ns / pll->cell_time;
    int num_cells = (int)(cells + 0.5);
    
    if (num_cells < 1) num_cells = 1;
    if (num_cells > 4) num_cells = 4;
    
    double error = delta_ns - num_cells * pll->cell_time;
    pll->cell_time += (error / num_cells) * pll->adjust_rate;
    
    if (pll->cell_time < pll->min_cell) pll->cell_time = pll->min_cell;
    if (pll->cell_time > pll->max_cell) pll->cell_time = pll->max_cell;
    
    return num_cells;
}

// ============================================================================
// FM CRC-16
// ============================================================================

static uint16_t fm_crc16_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }
    return crc;
}

// ============================================================================
// FM Bit Stream Processing
// ============================================================================

static size_t fm_flux_to_bits(const uft_flux_revolution_t* rev,
                               fm_pll_state_t* pll,
                               uint8_t* bits,
                               size_t max_bits) {
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < rev->count && bit_pos < max_bits; i++) {
        int cells = fm_pll_process(pll, rev->transitions[i].delta_ns);
        
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

/**
 * @brief Find FM sync pattern
 * FM has a clock bit before every data bit
 * Sync is: FFFFC7FC for IDAM (missing clock before FE)
 */
static ssize_t fm_find_sync(const uint8_t* bits, size_t bit_count, size_t start) {
    // Looking for the unique clock pattern before address mark
    // In FM, each byte has alternating clock/data
    // The sync breaks this pattern
    
    uint32_t buffer = 0;
    
    for (size_t pos = start; pos < bit_count; pos++) {
        buffer = (buffer << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        
        // Look for specific pattern indicating address mark
        // Standard FM: clock pattern break + FC/FE/FB/F8
        if ((buffer & 0xFFFF) == 0xF57E ||   // FE with clock break
            (buffer & 0xFFFF) == 0xF56F ||   // FB with clock break
            (buffer & 0xFFFF) == 0xF56A) {   // F8 with clock break
            return (ssize_t)(pos + 1);
        }
    }
    
    return -1;
}

/**
 * @brief Read FM-encoded bytes
 * FM: C0 D0 C1 D1 C2 D2 C3 D3 C4 D4 C5 D5 C6 D6 C7 D7
 */
static size_t fm_read_bytes(const uint8_t* bits, size_t bit_count,
                             size_t start, uint8_t* data, size_t max_bytes) {
    size_t bytes = 0;
    size_t pos = start;
    
    while (bytes < max_bytes && pos + 15 < bit_count) {
        uint8_t byte = 0;
        for (int b = 0; b < 8; b++) {
            size_t data_pos = pos + 1 + b * 2;  // Data bits at odd positions
            if (data_pos < bit_count) {
                int bit = (bits[data_pos / 8] >> (7 - (data_pos % 8))) & 1;
                byte = (byte << 1) | bit;
            }
        }
        data[bytes++] = byte;
        pos += 16;
    }
    
    return bytes;
}

// ============================================================================
// Probe Function
// ============================================================================

static int fm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0 || !confidence) {
        return 0;
    }
    
    *confidence = 0;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count < 500) return 0;
    
    // Calculate average transition time
    uint64_t total = 0;
    size_t count = rev->count < 5000 ? rev->count : 5000;
    for (size_t i = 0; i < count; i++) {
        total += rev->transitions[i].delta_ns;
    }
    double avg = (double)total / count;
    
    // FM SD: ~8000ns cell time, average transition ~12000-16000ns
    if (avg < 8000 || avg > 24000) {
        return 0;
    }
    
    *confidence = 40;  // Timing plausible for FM
    
    // Try to find sync patterns
    fm_pll_state_t pll;
    fm_pll_init(&pll, FM_SD_CELL_NS);
    
    size_t max_bits = rev->count * 3;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return 0;
    
    size_t bit_count = fm_flux_to_bits(rev, &pll, bits, max_bits);
    
    int sync_count = 0;
    ssize_t pos = 0;
    for (int i = 0; i < 20 && pos >= 0 && pos < (ssize_t)bit_count; i++) {
        pos = fm_find_sync(bits, bit_count, (size_t)pos);
        if (pos > 0) {
            sync_count++;
            pos += 100;
        }
    }
    
    free(bits);
    
    if (sync_count >= 8) {
        *confidence = 90;
    } else if (sync_count >= 4) {
        *confidence = 75;
    } else if (sync_count >= 1) {
        *confidence = 55;
    }
    
    return (*confidence >= 50) ? 1 : 0;
}

// ============================================================================
// Decode Function
// ============================================================================

static uft_error_t fm_decode_track(const uft_flux_track_data_t* flux,
                                    uft_track_t* sectors,
                                    const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    if (flux->revolution_count == 0) return UFT_ERROR_NO_DATA;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    double nominal = FM_SD_CELL_NS;
    if (opts && opts->pll_initial_period_us > 0) {
        nominal = opts->pll_initial_period_us * 1000.0;
    }
    
    fm_pll_state_t pll;
    fm_pll_init(&pll, nominal);
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    
    size_t max_bits = rev->count * 3;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return UFT_ERROR_NO_MEMORY;
    
    size_t bit_count = fm_flux_to_bits(rev, &pll, bits, max_bits);
    
    size_t max_sectors = 26;  // FM typically has fewer sectors
    sectors->sectors = calloc(max_sectors, sizeof(uft_sector_t));
    if (!sectors->sectors) {
        free(bits);
        return UFT_ERROR_NO_MEMORY;
    }
    
    ssize_t bit_pos = 0;
    
    while (bit_pos >= 0 && sectors->sector_count < max_sectors) {
        bit_pos = fm_find_sync(bits, bit_count, (size_t)bit_pos);
        if (bit_pos < 0) break;
        
        uint8_t header[7];
        size_t read = fm_read_bytes(bits, bit_count, (size_t)bit_pos, header, 7);
        if (read < 7) {
            bit_pos += 16;
            continue;
        }
        
        if (header[0] != FM_IDAM_MARK) {
            bit_pos += 16;
            continue;
        }
        
        uint8_t cyl = header[1];
        uint8_t head = header[2];
        uint8_t sec = header[3];
        uint8_t size_code = header[4];
        uint16_t id_crc = ((uint16_t)header[5] << 8) | header[6];
        
        // FM typically uses 128 or 256 byte sectors
        size_t sector_size = 128u << (size_code & 3);
        if (sector_size > 1024) {
            bit_pos += 16;
            continue;
        }
        
        bit_pos += 7 * 16;
        
        // Find data sync
        ssize_t data_pos = fm_find_sync(bits, bit_count, (size_t)bit_pos);
        if (data_pos < 0 || (data_pos - bit_pos) > 500) {
            continue;
        }
        
        uint8_t dam;
        if (fm_read_bytes(bits, bit_count, (size_t)data_pos, &dam, 1) < 1) {
            continue;
        }
        
        bool deleted = (dam == FM_DDAM_MARK);
        if (dam != FM_DAM_MARK && dam != FM_DDAM_MARK) {
            bit_pos = data_pos + 16;
            continue;
        }
        
        uint8_t* data_buf = malloc(sector_size + 2);
        if (!data_buf) {
            free(bits);
            return UFT_ERROR_NO_MEMORY;
        }
        
        read = fm_read_bytes(bits, bit_count, (size_t)data_pos + 16, data_buf, sector_size + 2);
        if (read < sector_size + 2) {
            free(data_buf);
            bit_pos = data_pos + 16;
            continue;
        }
        
        uint16_t data_crc = ((uint16_t)data_buf[sector_size] << 8) | data_buf[sector_size + 1];
        
        // Verify CRC
        uint16_t calc_crc = 0xFFFF;
        calc_crc = fm_crc16_update(calc_crc, dam);
        for (size_t i = 0; i < sector_size; i++) {
            calc_crc = fm_crc16_update(calc_crc, data_buf[i]);
        }
        bool data_ok = (calc_crc == data_crc);
        
        uft_sector_t* s = &sectors->sectors[sectors->sector_count];
        memset(s, 0, sizeof(*s));
        
        s->id.cylinder = cyl;
        s->id.head = head;
        s->id.sector = sec;
        s->id.size_code = size_code;
        s->id.crc = id_crc;
        s->id.crc_ok = true;  // Simplified
        
        s->data = malloc(sector_size);
        if (s->data) {
            memcpy(s->data, data_buf, sector_size);
            s->data_size = sector_size;
        }
        s->data_crc = data_crc;
        
        s->status = UFT_SECTOR_OK;
        if (!data_ok) s->status |= UFT_SECTOR_CRC_ERROR;
        if (deleted) s->status |= UFT_SECTOR_DELETED;
        
        sectors->sector_count++;
        free(data_buf);
        
        bit_pos = data_pos + (sector_size + 2) * 16;
    }
    
    free(bits);
    return UFT_OK;
}

// ============================================================================
// Default Options
// ============================================================================

static void fm_get_default_options(uft_decode_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->struct_size = sizeof(*opts);
    opts->pll_initial_period_us = 8.0;  // FM SD
    opts->pll_period_tolerance = 0.25;
    opts->pll_phase_adjust = 0.05;
    opts->max_retries = 3;
    opts->use_multiple_revolutions = true;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_decoder_ops_t uft_decoder_fm_v2 = {
    .name = "FM",
    .description = "Single Density FM (8\", SD)",
    .version = 0x00020000,
    .encoding = UFT_ENCODING_FM,
    .probe = fm_probe,
    .decode_track = fm_decode_track,
    .encode_track = NULL,
    .get_default_options = fm_get_default_options,
};
