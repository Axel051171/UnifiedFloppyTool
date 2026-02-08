#include "uft/compat/uft_platform.h"
#include "uft_error.h"
#include "uft_error_compat.h"
#include "uft_track.h"
/**
 * @file uft_gcr_cbm_decoder_v2.c
 * @brief GCR CBM Decoder - Unified Registry Version
 * 
 * Commodore GCR (Group Coded Recording):
 * - 1541/1571 floppy drives
 * - Commodore 64/128
 * - Zone-based timing (4 speed zones)
 */

#include "uft/uft_decoder_registry.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// CBM GCR Constants
// ============================================================================

// Sync mark: 10 consecutive 1-bits
#define GCR_SYNC_PATTERN    0x3FF   // 10 ones

// GCR block markers
#define GCR_HEADER_MARK     0x08
#define GCR_DATA_MARK       0x07
#define GCR_ERROR_MARK      0x09

// Zone timing (1541 uses 4 speed zones)
// Zone 1: Tracks 1-17,  21 sectors, 3.25µs bit cell
// Zone 2: Tracks 18-24, 19 sectors, 3.50µs bit cell
// Zone 3: Tracks 25-30, 18 sectors, 3.75µs bit cell
// Zone 4: Tracks 31-35, 17 sectors, 4.00µs bit cell

static const struct {
    int start_track;
    int sectors;
    int cell_ns;
} gcr_zones[4] = {
    { 1,  21, 3250 },   // Zone 1
    { 18, 19, 3500 },   // Zone 2
    { 25, 18, 3750 },   // Zone 3
    { 31, 17, 4000 },   // Zone 4
};

// GCR 4-to-5 decode table
static const uint8_t gcr_decode[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // 00-07 invalid
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,  // 08-0F
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,  // 10-17
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF   // 18-1F
};

// GCR 4-to-5 encode table
static const uint8_t gcr_encode[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

// ============================================================================
// Zone Helpers
// ============================================================================

static int get_zone(int track) {
    if (track <= 17) return 0;
    if (track <= 24) return 1;
    if (track <= 30) return 2;
    return 3;
}

static int get_sectors_for_track(int track) {
    return gcr_zones[get_zone(track)].sectors;
}

static int get_cell_ns(int track) {
    return gcr_zones[get_zone(track)].cell_ns;
}

// ============================================================================
// GCR PLL
// ============================================================================

typedef struct {
    double cell_time;
    double nominal;
    double min_cell;
    double max_cell;
    double adjust;
    uint32_t bit_buffer;
    int bits_count;
} gcr_pll_t;

static void gcr_pll_init(gcr_pll_t* pll, int track) {
    double nominal = get_cell_ns(track);
    pll->cell_time = nominal;
    pll->nominal = nominal;
    pll->min_cell = nominal * 0.70;
    pll->max_cell = nominal * 1.30;
    pll->adjust = 0.05;
    pll->bit_buffer = 0;
    pll->bits_count = 0;
}

static int gcr_pll_process(gcr_pll_t* pll, uint32_t delta_ns) {
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

// ============================================================================
// GCR Decoding
// ============================================================================

/**
 * @brief Decode 5 GCR bits to 4 data bits
 */
static int gcr_decode_nybble(uint8_t gcr) {
    if (gcr >= 32) return -1;
    int val = gcr_decode[gcr];
    return (val == 0xFF) ? -1 : val;
}

/**
 * @brief Decode 10 GCR bits to 1 byte
 */
static int gcr_decode_byte(uint16_t gcr10) {
    int hi = gcr_decode_nybble((gcr10 >> 5) & 0x1F);
    int lo = gcr_decode_nybble(gcr10 & 0x1F);
    if (hi < 0 || lo < 0) return -1;
    return (hi << 4) | lo;
}

/**
 * @brief Convert flux to raw bitstream
 */
static size_t gcr_flux_to_bits(const uft_flux_revolution_t* rev,
                                gcr_pll_t* pll,
                                uint8_t* bits,
                                size_t max_bits) {
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < rev->count && bit_pos < max_bits; i++) {
        int cells = gcr_pll_process(pll, rev->transitions[i].delta_ns);
        
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
 * @brief Find sync (10+ consecutive 1-bits)
 */
static ssize_t gcr_find_sync(const uint8_t* bits, size_t bit_count, size_t start) {
    int ones = 0;
    
    for (size_t pos = start; pos < bit_count; pos++) {
        int bit = (bits[pos / 8] >> (7 - (pos % 8))) & 1;
        if (bit) {
            ones++;
            if (ones >= 10) {
                // Found sync, skip remaining ones
                while (pos + 1 < bit_count) {
                    int next = (bits[(pos + 1) / 8] >> (7 - ((pos + 1) % 8))) & 1;
                    if (!next) break;
                    pos++;
                }
                return (ssize_t)(pos + 1);
            }
        } else {
            ones = 0;
        }
    }
    
    return -1;
}

/**
 * @brief Read GCR block (header or data)
 */
static int gcr_read_block(const uint8_t* bits, size_t bit_count,
                          size_t start, uint8_t* data, size_t byte_count) {
    size_t bits_needed = byte_count * 10;  // 10 bits per byte in GCR
    if (start + bits_needed > bit_count) return -1;
    
    size_t bit_pos = start;
    
    for (size_t i = 0; i < byte_count; i++) {
        uint16_t gcr10 = 0;
        for (int b = 0; b < 10; b++) {
            gcr10 <<= 1;
            if (bit_pos < bit_count) {
                gcr10 |= (bits[bit_pos / 8] >> (7 - (bit_pos % 8))) & 1;
            }
            bit_pos++;
        }
        
        int byte = gcr_decode_byte(gcr10);
        if (byte < 0) return -1;
        data[i] = (uint8_t)byte;
    }
    
    return (int)(bit_pos - start);
}

// ============================================================================
// CBM Checksum
// ============================================================================

static uint8_t gcr_checksum(const uint8_t* data, size_t len) {
    uint8_t xor = 0;
    for (size_t i = 0; i < len; i++) {
        xor ^= data[i];
    }
    return xor;
}

// ============================================================================
// Probe Function
// ============================================================================

static int gcr_cbm_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0 || !confidence) {
        return 0;
    }
    
    *confidence = 0;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count < 1000) return 0;
    
    // CBM GCR: ~3.25-4.0µs cell time
    // Average transition 6-10µs
    uint64_t total = 0;
    size_t count = rev->count < 5000 ? rev->count : 5000;
    for (size_t i = 0; i < count; i++) {
        total += rev->transitions[i].delta_ns;
    }
    double avg = (double)total / count;
    
    if (avg < 5000 || avg > 15000) {
        return 0;
    }
    
    *confidence = 40;
    
    // Try to find sync patterns
    gcr_pll_t pll;
    gcr_pll_init(&pll, 18);  // Middle zone
    
    size_t max_bits = rev->count * 4;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return 0;
    
    size_t bit_count = gcr_flux_to_bits(rev, &pll, bits, max_bits);
    
    int sync_count = 0;
    ssize_t pos = 0;
    for (size_t i = 0; i < 40 && pos >= 0 && pos < (ssize_t)bit_count; i++) {
        pos = gcr_find_sync(bits, bit_count, (size_t)pos);
        if (pos > 0) {
            sync_count++;
            pos += 50;
        }
    }
    
    free(bits);
    
    if (sync_count >= 20) {
        *confidence = 90;
    } else if (sync_count >= 10) {
        *confidence = 75;
    } else if (sync_count >= 5) {
        *confidence = 60;
    }
    
    return (*confidence >= 50) ? 1 : 0;
}

// ============================================================================
// Decode Function
// ============================================================================

static uft_error_t gcr_cbm_decode_track(const uft_flux_track_data_t* flux,
                                         uft_track_t* sectors,
                                         const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    if (flux->revolution_count == 0) return UFT_ERROR_NO_DATA;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    int track = flux->cylinder + 1;  // 1-based track number
    
    gcr_pll_t pll;
    gcr_pll_init(&pll, track);
    
    if (opts && opts->pll_initial_period_us > 0) {
        pll.nominal = opts->pll_initial_period_us * 1000.0;
        pll.cell_time = pll.nominal;
        pll.min_cell = pll.nominal * 0.70;
        pll.max_cell = pll.nominal * 1.30;
    }
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    
    size_t max_bits = rev->count * 4;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return UFT_ERROR_NO_MEMORY;
    
    size_t bit_count = gcr_flux_to_bits(rev, &pll, bits, max_bits);
    
    int max_sectors = get_sectors_for_track(track);
    sectors->sectors = calloc(max_sectors + 5, sizeof(uft_sector_t));
    if (!sectors->sectors) {
        free(bits);
        return UFT_ERROR_NO_MEMORY;
    }
    
    ssize_t bit_pos = 0;
    
    while (bit_pos >= 0 && sectors->sector_count < (size_t)(max_sectors + 5)) {
        // Find sync
        bit_pos = gcr_find_sync(bits, bit_count, (size_t)bit_pos);
        if (bit_pos < 0) break;
        
        // Read header block (8 bytes in GCR = 80 bits)
        uint8_t header[8];
        int bits_read = gcr_read_block(bits, bit_count, (size_t)bit_pos, header, 8);
        if (bits_read < 0) {
            bit_pos += 10;
            continue;
        }
        
        // Check for header mark
        if (header[0] != GCR_HEADER_MARK) {
            bit_pos += 10;
            continue;
        }
        
        // Parse header: [mark, checksum, sector, track, id2, id1, 0x0F, 0x0F]
        uint8_t hdr_checksum = header[1];
        uint8_t sector_id = header[2];
        uint8_t track_id = header[3];
        uint8_t disk_id2 = header[4];
        uint8_t disk_id1 = header[5];
        
        // Verify header checksum
        uint8_t calc_check = sector_id ^ track_id ^ disk_id2 ^ disk_id1;
        bool hdr_ok = (calc_check == hdr_checksum);
        
        bit_pos += bits_read;
        
        // Find data sync
        ssize_t data_sync = gcr_find_sync(bits, bit_count, (size_t)bit_pos);
        if (data_sync < 0 || (data_sync - bit_pos) > 2000) {
            continue;
        }
        
        // Read data block (325 bytes in GCR: mark + 256 data + 2 checksum + off bytes)
        uint8_t data_buf[325];
        bits_read = gcr_read_block(bits, bit_count, (size_t)data_sync, data_buf, 325);
        if (bits_read < 0 || data_buf[0] != GCR_DATA_MARK) {
            bit_pos = data_sync + 10;
            continue;
        }
        
        // Data checksum (XOR of all 256 data bytes)
        uint8_t data_check = gcr_checksum(&data_buf[1], 256);
        bool data_ok = (data_check == data_buf[257]);
        
        // Store sector
        uft_sector_t* s = &sectors->sectors[sectors->sector_count];
        memset(s, 0, sizeof(*s));
        
        s->id.cylinder = track_id - 1;  // Convert to 0-based
        s->id.head = 0;
        s->id.sector = sector_id;
        s->id.size_code = 1;  // 256 bytes
        s->id.crc_ok = hdr_ok;
        
        s->data = malloc(256);
        if (s->data) {
            memcpy(s->data, &data_buf[1], 256);
            s->data_size = 256;
        }
        
        s->status = UFT_SECTOR_OK;
        if (!hdr_ok) s->status |= UFT_SECTOR_ID_CRC_ERROR;
        if (!data_ok) s->status |= UFT_SECTOR_CRC_ERROR;
        
        sectors->sector_count++;
        
        bit_pos = data_sync + bits_read;
    }
    
    free(bits);
    return UFT_OK;
}

// ============================================================================
// Default Options
// ============================================================================

static void gcr_cbm_get_default_options(uft_decode_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->struct_size = sizeof(*opts);
    opts->pll_initial_period_us = 3.5;  // Middle zone
    opts->pll_period_tolerance = 0.30;
    opts->pll_phase_adjust = 0.05;
    opts->max_retries = 5;
    opts->use_multiple_revolutions = true;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_decoder_ops_t uft_decoder_gcr_cbm_v2 = {
    .name = "GCR-CBM",
    .description = "Commodore GCR (C64, 1541, 1571)",
    .version = 0x00020000,
    .encoding = UFT_ENCODING_GCR_CBM,
    .probe = gcr_cbm_probe,
    .decode_track = gcr_cbm_decode_track,
    .encode_track = NULL,
    .get_default_options = gcr_cbm_get_default_options,
};
