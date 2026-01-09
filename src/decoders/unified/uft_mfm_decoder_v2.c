#include "uft/compat/uft_platform.h"
#include "uft_error.h"
#include "uft_error_compat.h"
#include "uft_track.h"
/**
 * @file uft_mfm_decoder_v2.c
 * @brief MFM Decoder - Unified Registry Version
 * 
 * IBM-kompatibles MFM (Modified Frequency Modulation):
 * - PC 3.5"/5.25" DD/HD
 * - Amiga DD/HD
 * - Atari ST DD
 * 
 * Integration mit uft_decoder_registry.h
 */

#include "uft/uft_decoder_registry.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include "uft/uft_pll.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// MFM Constants
// ============================================================================

#define MFM_SYNC_WORD       0x4489      // A1 mit Clock-Fehler
#define MFM_SYNC_COUNT      3           // Dreimal $A1
#define MFM_IDAM_MARK       0xFE        // ID Address Mark
#define MFM_DAM_MARK        0xFB        // Data Address Mark
#define MFM_DDAM_MARK       0xF8        // Deleted Data Address Mark

#define MFM_DD_CELL_NS      4000        // 4µs cell time DD
#define MFM_HD_CELL_NS      2000        // 2µs cell time HD
#define MFM_ED_CELL_NS      1000        // 1µs cell time ED

// ============================================================================
// Internal PLL State
// ============================================================================

typedef struct {
    double      cell_time;      // Current bit cell time (ns)
    double      nominal;        // Nominal cell time
    double      min_cell;       // Minimum allowed
    double      max_cell;       // Maximum allowed
    double      adjust_rate;    // PLL adjustment rate (0.05 = 5%)
    uint64_t    bit_buffer;     // For sync detection
    int         bits_in_buffer;
} mfm_pll_state_t;

static void pll_init(mfm_pll_state_t* pll, double nominal_ns) {
    pll->cell_time = nominal_ns;
    pll->nominal = nominal_ns;
    pll->min_cell = nominal_ns * 0.70;
    pll->max_cell = nominal_ns * 1.30;
    pll->adjust_rate = 0.05;
    pll->bit_buffer = 0;
    pll->bits_in_buffer = 0;
}

static void pll_reset_phase(mfm_pll_state_t* pll) {
    pll->bit_buffer = 0;
    pll->bits_in_buffer = 0;
}

/**
 * @brief Process a flux transition and return number of bit cells
 */
static int pll_process_transition(mfm_pll_state_t* pll, uint32_t delta_ns) {
    double cells = (double)delta_ns / pll->cell_time;
    int num_cells = (int)(cells + 0.5);
    
    if (num_cells < 1) num_cells = 1;
    if (num_cells > 5) num_cells = 5;  // Limit run length
    
    // PLL adjustment
    double error = delta_ns - num_cells * pll->cell_time;
    double correction = (error / num_cells) * pll->adjust_rate;
    
    pll->cell_time += correction;
    
    // Clamp
    if (pll->cell_time < pll->min_cell) pll->cell_time = pll->min_cell;
    if (pll->cell_time > pll->max_cell) pll->cell_time = pll->max_cell;
    
    return num_cells;
}

/**
 * @brief Add bits to buffer (for sync detection)
 */
static void pll_add_bits(mfm_pll_state_t* pll, int num_cells) {
    // Each transition ends with a '1', preceded by (num_cells-1) zeros
    for (int i = 0; i < num_cells - 1; i++) {
        pll->bit_buffer = (pll->bit_buffer << 1);
        pll->bits_in_buffer++;
    }
    pll->bit_buffer = (pll->bit_buffer << 1) | 1;
    pll->bits_in_buffer++;
}

// ============================================================================
// CRC-16 CCITT
// ============================================================================

static uint16_t crc16_update(uint16_t crc, uint8_t byte) {
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000)
            crc = (crc << 1) ^ 0x1021;
        else
            crc <<= 1;
    }
    return crc;
}

static uint16_t crc16_mfm_init(void) {
    // After 3x A1 sync bytes
    uint16_t crc = 0xFFFF;
    crc = crc16_update(crc, 0xA1);
    crc = crc16_update(crc, 0xA1);
    crc = crc16_update(crc, 0xA1);
    return crc;
}

static uint16_t crc16_mfm_block(const uint8_t* data, size_t len) {
    uint16_t crc = crc16_mfm_init();
    for (size_t i = 0; i < len; i++) {
        crc = crc16_update(crc, data[i]);
    }
    return crc;
}

// ============================================================================
// MFM Bit Stream Decoding
// ============================================================================

/**
 * @brief Convert flux to bitstream using PLL
 */
static size_t flux_to_bits(const uft_flux_revolution_t* rev,
                           mfm_pll_state_t* pll,
                           uint8_t* bits,
                           size_t max_bits) {
    size_t bit_pos = 0;
    
    pll_reset_phase(pll);
    
    for (size_t i = 0; i < rev->count && bit_pos < max_bits; i++) {
        uint32_t delta = rev->transitions[i].delta_ns;
        int cells = pll_process_transition(pll, delta);
        
        // Output bits
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
 * @brief Find sync pattern in bitstream
 * @return Bit position after sync, or -1 if not found
 */
static ssize_t find_sync_pattern(const uint8_t* bits, size_t bit_count, size_t start_bit) {
    // Looking for: 0x4489 0x4489 0x4489 (three A1 sync marks with clock violation)
    // In raw bits this is: 0100 0100 1000 1001 repeated 3 times = 48 bits
    
    uint64_t sync_pattern = 0x448944894489ULL;
    uint64_t mask = 0xFFFFFFFFFFFFULL;  // 48 bits
    uint64_t buffer = 0;
    
    for (size_t pos = start_bit; pos < bit_count; pos++) {
        buffer = (buffer << 1) | ((bits[pos / 8] >> (7 - (pos % 8))) & 1);
        
        if (pos - start_bit >= 47) {  // Have 48 bits
            if ((buffer & mask) == sync_pattern) {
                return (ssize_t)(pos + 1);  // Return position after sync
            }
        }
    }
    
    return -1;
}

/**
 * @brief Read MFM-encoded bytes from bitstream
 */
static size_t read_mfm_bytes(const uint8_t* bits, size_t bit_count,
                              size_t start_bit, uint8_t* data, size_t max_bytes) {
    size_t bytes_read = 0;
    size_t bit_pos = start_bit;
    
    while (bytes_read < max_bytes && bit_pos + 15 < bit_count) {
        uint8_t byte = 0;
        
        // MFM: clock-data-clock-data... We want the data bits (odd positions)
        for (int b = 0; b < 8; b++) {
            size_t data_bit_pos = bit_pos + 1 + b * 2;  // Odd positions
            if (data_bit_pos < bit_count) {
                int bit_val = (bits[data_bit_pos / 8] >> (7 - (data_bit_pos % 8))) & 1;
                byte = (byte << 1) | bit_val;
            }
        }
        
        data[bytes_read++] = byte;
        bit_pos += 16;  // 16 bits per byte in MFM
    }
    
    return bytes_read;
}

// ============================================================================
// Probe Function
// ============================================================================

static int mfm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0 || !confidence) {
        return 0;
    }
    
    *confidence = 0;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count < 1000) return 0;
    
    // Calculate average transition time
    uint64_t total = 0;
    size_t samples = rev->count < 10000 ? rev->count : 10000;
    for (size_t i = 0; i < samples; i++) {
        total += rev->transitions[i].delta_ns;
    }
    double avg_ns = (double)total / samples;
    
    // MFM timing: DD ~4000ns, HD ~2000ns average cell time
    // But transitions span 2-4 cells typically
    bool timing_ok = false;
    double nominal = 0;
    
    if (avg_ns >= 5000 && avg_ns <= 12000) {
        // DD MFM range
        nominal = MFM_DD_CELL_NS;
        timing_ok = true;
    } else if (avg_ns >= 2500 && avg_ns <= 6000) {
        // HD MFM range
        nominal = MFM_HD_CELL_NS;
        timing_ok = true;
    }
    
    if (!timing_ok) return 0;
    
    *confidence = 50;  // Timing looks right
    
    // Try to find sync patterns
    mfm_pll_state_t pll;
    pll_init(&pll, nominal);
    
    // Allocate bitstream buffer
    size_t max_bits = rev->count * 5;  // Conservative estimate
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return 0;
    
    size_t bit_count = flux_to_bits(rev, &pll, bits, max_bits);
    
    // Count sync patterns
    int sync_count = 0;
    ssize_t pos = 0;
    for (int i = 0; i < 30 && pos >= 0; i++) {
        pos = find_sync_pattern(bits, bit_count, (size_t)pos);
        if (pos > 0) {
            sync_count++;
            pos += 100;  // Skip ahead
        }
    }
    
    free(bits);
    
    if (sync_count >= 10) {
        *confidence = 95;
    } else if (sync_count >= 5) {
        *confidence = 85;
    } else if (sync_count >= 2) {
        *confidence = 70;
    }
    
    return (*confidence >= 50) ? 1 : 0;
}

// ============================================================================
// Decode Function
// ============================================================================

static uft_error_t mfm_decode_track(const uft_flux_track_data_t* flux,
                                     uft_track_t* sectors,
                                     const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    if (flux->revolution_count == 0) return UFT_ERROR_NO_DATA;
    
    // Initialize output
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    // Determine nominal cell time
    double nominal = MFM_DD_CELL_NS;
    if (opts && opts->pll_initial_period_us > 0) {
        nominal = opts->pll_initial_period_us * 1000.0;
    } else {
        // Auto-detect from flux
        const uft_flux_revolution_t* rev = &flux->revolutions[0];
        uint64_t total = 0;
        size_t count = rev->count < 5000 ? rev->count : 5000;
        for (size_t i = 0; i < count; i++) {
            total += rev->transitions[i].delta_ns;
        }
        double avg = (double)total / count;
        
        // Estimate cell time (average transition is ~2.5 cells)
        nominal = avg / 2.5;
        if (nominal < 1500) nominal = MFM_HD_CELL_NS;
        else if (nominal < 3000) nominal = MFM_DD_CELL_NS;
    }
    
    mfm_pll_state_t pll;
    pll_init(&pll, nominal);
    
    if (opts) {
        if (opts->pll_period_tolerance > 0) {
            pll.min_cell = nominal * (1.0 - opts->pll_period_tolerance);
            pll.max_cell = nominal * (1.0 + opts->pll_period_tolerance);
        }
        if (opts->pll_phase_adjust > 0) {
            pll.adjust_rate = opts->pll_phase_adjust;
        }
    }
    
    // Use first revolution (or merge if requested)
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    
    // Convert to bitstream
    size_t max_bits = rev->count * 5;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return UFT_ERROR_NO_MEMORY;
    
    size_t bit_count = flux_to_bits(rev, &pll, bits, max_bits);
    
    // Allocate sectors array
    size_t max_sectors = 32;
    sectors->sectors = calloc(max_sectors, sizeof(uft_sector_t));
    if (!sectors->sectors) {
        free(bits);
        return UFT_ERROR_NO_MEMORY;
    }
    
    // Find and decode sectors
    ssize_t bit_pos = 0;
    
    while (bit_pos >= 0 && sectors->sector_count < max_sectors) {
        // Find sync
        bit_pos = find_sync_pattern(bits, bit_count, (size_t)bit_pos);
        if (bit_pos < 0) break;
        
        // Read address mark
        uint8_t header[7];
        size_t read = read_mfm_bytes(bits, bit_count, (size_t)bit_pos, header, 7);
        if (read < 7) {
            bit_pos += 16;
            continue;
        }
        
        // Check for IDAM
        if (header[0] != MFM_IDAM_MARK) {
            bit_pos += 16;
            continue;
        }
        
        // Parse ID field
        uint8_t cyl = header[1];
        uint8_t head = header[2];
        uint8_t sec = header[3];
        uint8_t size_code = header[4];
        uint16_t id_crc_read = ((uint16_t)header[5] << 8) | header[6];
        
        // Verify ID CRC
        uint16_t id_crc_calc = crc16_mfm_block(header, 5);
        bool id_crc_ok = (id_crc_read == id_crc_calc);
        
        // Sector size
        size_t sector_size = 128u << (size_code & 7);
        if (sector_size > 8192) {
            bit_pos += 16;
            continue;
        }
        
        bit_pos += 7 * 16;  // Skip ID field
        
        // Look for data sync (within reasonable gap)
        ssize_t data_sync_pos = find_sync_pattern(bits, bit_count, (size_t)bit_pos);
        if (data_sync_pos < 0 || (data_sync_pos - bit_pos) > 1000) {
            continue;
        }
        
        // Read DAM
        uint8_t dam;
        if (read_mfm_bytes(bits, bit_count, (size_t)data_sync_pos, &dam, 1) < 1) {
            continue;
        }
        
        bool deleted = (dam == MFM_DDAM_MARK);
        if (dam != MFM_DAM_MARK && dam != MFM_DDAM_MARK) {
            bit_pos = data_sync_pos + 16;
            continue;
        }
        
        // Read data + CRC
        uint8_t* data_buf = malloc(sector_size + 2);
        if (!data_buf) {
            free(bits);
            return UFT_ERROR_NO_MEMORY;
        }
        
        size_t data_pos = (size_t)data_sync_pos + 16;  // After DAM
        read = read_mfm_bytes(bits, bit_count, data_pos, data_buf, sector_size + 2);
        
        if (read < sector_size + 2) {
            free(data_buf);
            bit_pos = data_sync_pos + 16;
            continue;
        }
        
        // Verify data CRC
        uint16_t data_crc_read = ((uint16_t)data_buf[sector_size] << 8) | data_buf[sector_size + 1];
        
        uint16_t data_crc_calc = crc16_mfm_init();
        data_crc_calc = crc16_update(data_crc_calc, dam);
        for (size_t i = 0; i < sector_size; i++) {
            data_crc_calc = crc16_update(data_crc_calc, data_buf[i]);
        }
        bool data_crc_ok = (data_crc_read == data_crc_calc);
        
        // Store sector
        uft_sector_t* s = &sectors->sectors[sectors->sector_count];
        memset(s, 0, sizeof(*s));
        
        s->id.cylinder = cyl;
        s->id.head = head;
        s->id.sector = sec;
        s->id.size_code = size_code;
        s->id.crc = id_crc_read;
        s->id.crc_ok = id_crc_ok;
        
        s->data = malloc(sector_size);
        if (s->data) {
            memcpy(s->data, data_buf, sector_size);
            s->data_size = sector_size;
        }
        s->data_crc = data_crc_read;
        
        s->status = UFT_SECTOR_OK;
        if (!id_crc_ok) s->status |= UFT_SECTOR_ID_CRC_ERROR;
        if (!data_crc_ok) s->status |= UFT_SECTOR_CRC_ERROR;
        if (deleted) s->status |= UFT_SECTOR_DELETED;
        
        sectors->sector_count++;
        
        free(data_buf);
        
        // Move past this sector
        bit_pos = data_pos + (sector_size + 2) * 16;
    }
    
    free(bits);
    
    return UFT_OK;
}

// ============================================================================
// Encode Function - delegates to uft_mfm_encoder.c
// ============================================================================

/* Forward declaration from uft_mfm_encoder.c */
extern uft_error_t uft_mfm_encode_track(const uft_sector_t* sectors,
                                         int sector_count,
                                         uint8_t* buf,
                                         size_t buf_size,
                                         size_t* out_bits);

extern uft_error_t uft_mfm_to_flux(const uint8_t* mfm_bits,
                                    size_t bit_count,
                                    uint32_t bit_cell_ns,
                                    uint32_t* flux,
                                    size_t max_flux,
                                    size_t* out_count);

static uft_error_t mfm_encode_track(const uft_track_t* sectors,
                                     uft_flux_track_data_t* flux,
                                     const uft_encode_options_t* opts) {
    if (!sectors || !flux) {
        return UFT_ERROR_NULL_POINTER;
    }
    
    /* Determine bit cell time from options or use DD default */
    uint32_t bit_cell_ns = 2000;  /* DD MFM = 2µs */
    if (opts && opts->bit_cell_ns > 0) {
        bit_cell_ns = opts->bit_cell_ns;
    }
    
    /* Allocate buffer for MFM bitstream (~12500 bytes for DD track) */
    size_t buf_size = 16384;
    uint8_t* mfm_buf = calloc(buf_size, 1);
    if (!mfm_buf) {
        return UFT_ERROR_NO_MEMORY;
    }
    
    /* Encode sectors to MFM bitstream */
    size_t out_bits = 0;
    uft_error_t err = uft_mfm_encode_track(sectors->sectors, 
                                            (int)sectors->sector_count,
                                            mfm_buf, buf_size, &out_bits);
    if (err != UFT_OK) {
        free(mfm_buf);
        return err;
    }
    
    /* Allocate flux array */
    size_t max_flux = out_bits;  /* Max one flux per bit */
    uint32_t* flux_data = malloc(max_flux * sizeof(uint32_t));
    if (!flux_data) {
        free(mfm_buf);
        return UFT_ERROR_NO_MEMORY;
    }
    
    /* Convert MFM bits to flux transitions */
    size_t flux_count = 0;
    err = uft_mfm_to_flux(mfm_buf, out_bits, bit_cell_ns,
                          flux_data, max_flux, &flux_count);
    free(mfm_buf);
    
    if (err != UFT_OK) {
        free(flux_data);
        return err;
    }
    
    /* Populate flux track data structure */
    flux->cylinder = sectors->cylinder;
    flux->head = sectors->head;
    flux->revolution_count = 1;
    
    /* Allocate revolution */
    flux->revolutions = calloc(1, sizeof(uft_flux_revolution_t));
    if (!flux->revolutions) {
        free(flux_data);
        return UFT_ERROR_NO_MEMORY;
    }
    
    /* Allocate transitions */
    flux->revolutions[0].transitions = malloc(flux_count * sizeof(uft_flux_transition_t));
    if (!flux->revolutions[0].transitions) {
        free(flux->revolutions);
        free(flux_data);
        return UFT_ERROR_NO_MEMORY;
    }
    
    /* Copy flux data */
    uint64_t total_time = 0;
    for (size_t i = 0; i < flux_count; i++) {
        flux->revolutions[0].transitions[i].delta_ns = flux_data[i];
        flux->revolutions[0].transitions[i].index = false;
        total_time += flux_data[i];
    }
    flux->revolutions[0].count = flux_count;
    flux->revolutions[0].total_time_ns = total_time;
    
    free(flux_data);
    return UFT_OK;
}

// ============================================================================
// Default Options
// ============================================================================

static void mfm_get_default_options(uft_decode_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->struct_size = sizeof(*opts);
    opts->pll_initial_period_us = 4.0;  // DD MFM
    opts->pll_period_tolerance = 0.25;
    opts->pll_phase_adjust = 0.05;
    opts->max_retries = 3;
    opts->use_multiple_revolutions = true;
    opts->include_weak_sectors = false;
    opts->preserve_errors = true;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_decoder_ops_t uft_decoder_mfm_v2 = {
    .name = "MFM",
    .description = "IBM MFM (PC, Amiga, Atari ST)",
    .version = 0x00020000,
    .encoding = UFT_ENCODING_MFM,
    .probe = mfm_probe,
    .decode_track = mfm_decode_track,
    .encode_track = mfm_encode_track,
    .get_default_options = mfm_get_default_options,
};
