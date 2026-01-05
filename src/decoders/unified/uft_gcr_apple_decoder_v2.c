/**
 * @file uft_gcr_apple_decoder_v2.c
 * @brief GCR Apple Decoder - Unified Registry Version
 * 
 * Apple II GCR (6&2 Encoding):
 * - Apple II, II+, IIe, IIc
 * - 5.25" disk drives
 * - 16-sector (DOS 3.3) and 13-sector (DOS 3.2) formats
 */

#include "uft/uft_decoder_registry.h"
#include "uft/uft_unified_image.h"
#include "uft/uft_safe.h"
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Apple GCR Constants
// ============================================================================

#define APPLE_TRACKS        35
#define APPLE_SECTORS_16    16
#define APPLE_SECTORS_13    13
#define APPLE_SECTOR_SIZE   256
#define APPLE_CELL_NS       4000    // 4µs bit cell (~250kbit/s)

// Address field prologue/epilogue
#define APPLE_ADDR_PROLOG1  0xD5
#define APPLE_ADDR_PROLOG2  0xAA
#define APPLE_ADDR_PROLOG3  0x96

#define APPLE_DATA_PROLOG1  0xD5
#define APPLE_DATA_PROLOG2  0xAA
#define APPLE_DATA_PROLOG3  0xAD

#define APPLE_EPILOG1       0xDE
#define APPLE_EPILOG2       0xAA
#define APPLE_EPILOG3       0xEB

// ============================================================================
// 6&2 Decode Tables
// ============================================================================

// 6&2 decode: Convert disk byte to 6-bit value
static const uint8_t apple_decode_62[256] = {
    // 00-0F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 10-1F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 20-2F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 30-3F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 40-4F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 50-5F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 60-6F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 70-7F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 80-8F
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // 90-9F
    0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0x01, 0x02, 0x03,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // A0-AF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x04, 0x05,
    0xFF, 0xFF, 0x06, 0x07, 0xFF, 0x08, 0x09, 0x0A,
    // B0-BF
    0xFF, 0xFF, 0x0B, 0x0C, 0xFF, 0x0D, 0x0E, 0x0F,
    0xFF, 0x10, 0x11, 0x12, 0xFF, 0x13, 0x14, 0x15,
    // C0-CF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    // D0-DF
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x16, 0x17,
    0xFF, 0xFF, 0x18, 0x19, 0xFF, 0x1A, 0x1B, 0x1C,
    // E0-EF
    0xFF, 0xFF, 0x1D, 0x1E, 0xFF, 0x1F, 0x20, 0x21,
    0xFF, 0x22, 0x23, 0x24, 0xFF, 0x25, 0x26, 0x27,
    // F0-FF
    0xFF, 0xFF, 0x28, 0x29, 0xFF, 0x2A, 0x2B, 0x2C,
    0xFF, 0x2D, 0x2E, 0x2F, 0xFF, 0x30, 0x31, 0x32,
    0xFF, 0xFF, 0x33, 0x34, 0xFF, 0x35, 0x36, 0x37,
    0xFF, 0x38, 0x39, 0x3A, 0xFF, 0x3B, 0x3C, 0x3D
};

// ============================================================================
// Apple PLL
// ============================================================================

typedef struct {
    double cell_time;
    double nominal;
    double min_cell;
    double max_cell;
    double adjust;
} apple_pll_t;

static void apple_pll_init(apple_pll_t* pll) {
    pll->cell_time = APPLE_CELL_NS;
    pll->nominal = APPLE_CELL_NS;
    pll->min_cell = APPLE_CELL_NS * 0.75;
    pll->max_cell = APPLE_CELL_NS * 1.25;
    pll->adjust = 0.05;
}

static int apple_pll_process(apple_pll_t* pll, uint32_t delta_ns) {
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
// Bit Stream Processing
// ============================================================================

static size_t apple_flux_to_bits(const uft_flux_revolution_t* rev,
                                  apple_pll_t* pll,
                                  uint8_t* bits,
                                  size_t max_bits) {
    size_t bit_pos = 0;
    
    for (size_t i = 0; i < rev->count && bit_pos < max_bits; i++) {
        int cells = apple_pll_process(pll, rev->transitions[i].delta_ns);
        
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
 * @brief Read 8-bit disk byte from bitstream
 */
static int apple_read_byte(const uint8_t* bits, size_t bit_count,
                           size_t* bit_pos) {
    uint8_t byte = 0;
    
    // Skip leading zeros, wait for 1-bit (start of byte)
    while (*bit_pos < bit_count) {
        int bit = (bits[*bit_pos / 8] >> (7 - (*bit_pos % 8))) & 1;
        (*bit_pos)++;
        if (bit) break;
    }
    
    if (*bit_pos >= bit_count) return -1;
    
    byte = 0x80;  // First bit was the 1 we found
    
    for (int i = 1; i < 8 && *bit_pos < bit_count; i++) {
        byte <<= 1;
        byte |= (bits[*bit_pos / 8] >> (7 - (*bit_pos % 8))) & 1;
        (*bit_pos)++;
    }
    
    return byte;
}

/**
 * @brief Find address field prologue (D5 AA 96)
 */
static ssize_t apple_find_addr_field(const uint8_t* bits, size_t bit_count,
                                      size_t start) {
    size_t pos = start;
    int state = 0;
    
    while (pos < bit_count - 100) {
        int byte = apple_read_byte(bits, bit_count, &pos);
        if (byte < 0) return -1;
        
        switch (state) {
            case 0: if (byte == APPLE_ADDR_PROLOG1) state = 1; break;
            case 1: if (byte == APPLE_ADDR_PROLOG2) state = 2; else state = 0; break;
            case 2: if (byte == APPLE_ADDR_PROLOG3) return (ssize_t)pos; else state = 0; break;
        }
    }
    
    return -1;
}

/**
 * @brief Find data field prologue (D5 AA AD)
 */
static ssize_t apple_find_data_field(const uint8_t* bits, size_t bit_count,
                                      size_t start) {
    size_t pos = start;
    int state = 0;
    
    while (pos < bit_count - 500) {
        int byte = apple_read_byte(bits, bit_count, &pos);
        if (byte < 0) return -1;
        
        switch (state) {
            case 0: if (byte == APPLE_DATA_PROLOG1) state = 1; break;
            case 1: if (byte == APPLE_DATA_PROLOG2) state = 2; else state = 0; break;
            case 2: if (byte == APPLE_DATA_PROLOG3) return (ssize_t)pos; else state = 0; break;
        }
    }
    
    return -1;
}

/**
 * @brief Decode 4-4 encoded byte (used in address field)
 */
static int apple_decode_44(int b1, int b2) {
    return ((b1 & 0x55) << 1) | (b2 & 0x55);
}

/**
 * @brief Decode 6&2 sector data (342 disk bytes → 256 data bytes)
 */
static bool apple_decode_62_sector(const uint8_t* disk_bytes, uint8_t* data) {
    // First 86 bytes contain 2-bit values for bottom part
    // Next 256 bytes contain 6-bit values for top part
    // Last byte is checksum
    
    uint8_t aux[86];
    uint8_t checksum = 0;
    
    // Decode auxiliary buffer (86 bytes of 2-bit data)
    for (int i = 0; i < 86; i++) {
        uint8_t val = apple_decode_62[disk_bytes[i]];
        if (val == 0xFF) return false;
        aux[i] = val;
        checksum ^= val;
    }
    
    // Decode main data (256 6-bit values)
    for (int i = 0; i < 256; i++) {
        uint8_t val = apple_decode_62[disk_bytes[86 + i]];
        if (val == 0xFF) return false;
        checksum ^= val;
        
        // Combine with auxiliary bits
        int aux_idx = i % 86;
        int aux_shift = (i / 86) * 2;
        uint8_t low2 = (aux[aux_idx] >> aux_shift) & 0x03;
        
        data[i] = (val << 2) | low2;
    }
    
    // Verify checksum
    uint8_t disk_check = apple_decode_62[disk_bytes[342]];
    return (checksum == disk_check);
}

// ============================================================================
// Probe Function
// ============================================================================

static int gcr_apple_probe(const uft_flux_track_data_t* flux, int* confidence) {
    if (!flux || flux->revolution_count == 0 || !confidence) {
        return 0;
    }
    
    *confidence = 0;
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    if (rev->count < 1000) return 0;
    
    // Apple II: ~4µs cell, average transition ~8-10µs
    uint64_t total = 0;
    size_t count = rev->count < 5000 ? rev->count : 5000;
    for (size_t i = 0; i < count; i++) {
        total += rev->transitions[i].delta_ns;
    }
    double avg = (double)total / count;
    
    if (avg < 6000 || avg > 15000) return 0;
    
    *confidence = 40;
    
    apple_pll_t pll;
    apple_pll_init(&pll);
    
    size_t max_bits = rev->count * 4;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return 0;
    
    size_t bit_count = apple_flux_to_bits(rev, &pll, bits, max_bits);
    
    // Count address fields
    int addr_count = 0;
    ssize_t pos = 0;
    for (int i = 0; i < 20 && pos >= 0; i++) {
        pos = apple_find_addr_field(bits, bit_count, (size_t)pos);
        if (pos > 0) {
            addr_count++;
            pos += 100;
        }
    }
    
    free(bits);
    
    if (addr_count >= 10) {
        *confidence = 90;
    } else if (addr_count >= 5) {
        *confidence = 75;
    } else if (addr_count >= 2) {
        *confidence = 55;
    }
    
    return (*confidence >= 50) ? 1 : 0;
}

// ============================================================================
// Decode Function
// ============================================================================

static uft_error_t gcr_apple_decode_track(const uft_flux_track_data_t* flux,
                                           uft_track_t* sectors,
                                           const uft_decode_options_t* opts) {
    if (!flux || !sectors) return UFT_ERROR_NULL_POINTER;
    if (flux->revolution_count == 0) return UFT_ERROR_NO_DATA;
    
    memset(sectors, 0, sizeof(*sectors));
    sectors->cylinder = flux->cylinder;
    sectors->head = flux->head;
    
    apple_pll_t pll;
    apple_pll_init(&pll);
    
    if (opts && opts->pll_initial_period_us > 0) {
        pll.nominal = opts->pll_initial_period_us * 1000.0;
        pll.cell_time = pll.nominal;
    }
    
    const uft_flux_revolution_t* rev = &flux->revolutions[0];
    
    size_t max_bits = rev->count * 4;
    uint8_t* bits = calloc((max_bits + 7) / 8, 1);
    if (!bits) return UFT_ERROR_NO_MEMORY;
    
    size_t bit_count = apple_flux_to_bits(rev, &pll, bits, max_bits);
    
    sectors->sectors = calloc(20, sizeof(uft_sector_t));
    if (!sectors->sectors) {
        free(bits);
        return UFT_ERROR_NO_MEMORY;
    }
    
    ssize_t bit_pos = 0;
    
    while (bit_pos >= 0 && sectors->sector_count < 18) {
        // Find address field
        bit_pos = apple_find_addr_field(bits, bit_count, (size_t)bit_pos);
        if (bit_pos < 0) break;
        
        size_t addr_pos = (size_t)bit_pos;
        
        // Read address field: volume, track, sector, checksum (4-4 encoded)
        int vol1 = apple_read_byte(bits, bit_count, &addr_pos);
        int vol2 = apple_read_byte(bits, bit_count, &addr_pos);
        int trk1 = apple_read_byte(bits, bit_count, &addr_pos);
        int trk2 = apple_read_byte(bits, bit_count, &addr_pos);
        int sec1 = apple_read_byte(bits, bit_count, &addr_pos);
        int sec2 = apple_read_byte(bits, bit_count, &addr_pos);
        int chk1 = apple_read_byte(bits, bit_count, &addr_pos);
        int chk2 = apple_read_byte(bits, bit_count, &addr_pos);
        
        if (vol1 < 0 || vol2 < 0 || trk1 < 0 || trk2 < 0 ||
            sec1 < 0 || sec2 < 0 || chk1 < 0 || chk2 < 0) {
            bit_pos = (ssize_t)addr_pos;
            continue;
        }
        
        int volume = apple_decode_44(vol1, vol2);
        int track = apple_decode_44(trk1, trk2);
        int sector = apple_decode_44(sec1, sec2);
        int checksum = apple_decode_44(chk1, chk2);
        
        bool addr_ok = ((volume ^ track ^ sector) == checksum);
        
        bit_pos = (ssize_t)addr_pos;
        
        // Find data field
        ssize_t data_pos = apple_find_data_field(bits, bit_count, (size_t)bit_pos);
        if (data_pos < 0 || (data_pos - bit_pos) > 500) {
            continue;
        }
        
        size_t dpos = (size_t)data_pos;
        
        // Read 343 disk bytes (342 data + 1 checksum)
        uint8_t disk_bytes[343];
        bool read_ok = true;
        for (int i = 0; i < 343 && read_ok; i++) {
            int b = apple_read_byte(bits, bit_count, &dpos);
            if (b < 0) read_ok = false;
            else disk_bytes[i] = (uint8_t)b;
        }
        
        if (!read_ok) {
            bit_pos = data_pos + 100;
            continue;
        }
        
        // Decode sector data
        uint8_t sector_data[256];
        bool data_ok = apple_decode_62_sector(disk_bytes, sector_data);
        
        // Store sector
        uft_sector_t* s = &sectors->sectors[sectors->sector_count];
        memset(s, 0, sizeof(*s));
        
        s->id.cylinder = (uint8_t)track;
        s->id.head = 0;
        s->id.sector = (uint8_t)sector;
        s->id.size_code = 1;  // 256 bytes
        s->id.crc_ok = addr_ok;
        
        s->data = malloc(256);
        if (s->data) {
            memcpy(s->data, sector_data, 256);
            s->data_size = 256;
        }
        
        s->status = UFT_SECTOR_OK;
        if (!addr_ok) s->status |= UFT_SECTOR_ID_CRC_ERROR;
        if (!data_ok) s->status |= UFT_SECTOR_CRC_ERROR;
        
        sectors->sector_count++;
        
        bit_pos = (ssize_t)dpos;
    }
    
    free(bits);
    return UFT_OK;
}

// ============================================================================
// Default Options
// ============================================================================

static void gcr_apple_get_default_options(uft_decode_options_t* opts) {
    if (!opts) return;
    
    memset(opts, 0, sizeof(*opts));
    opts->struct_size = sizeof(*opts);
    opts->pll_initial_period_us = 4.0;
    opts->pll_period_tolerance = 0.25;
    opts->pll_phase_adjust = 0.05;
    opts->max_retries = 3;
    opts->use_multiple_revolutions = true;
}

// ============================================================================
// Plugin Registration
// ============================================================================

const uft_decoder_ops_t uft_decoder_gcr_apple_v2 = {
    .name = "GCR-Apple",
    .description = "Apple II GCR (6&2 encoding)",
    .version = 0x00020000,
    .encoding = UFT_ENCODING_GCR_APPLE,
    .probe = gcr_apple_probe,
    .decode_track = gcr_apple_decode_track,
    .encode_track = NULL,
    .get_default_options = gcr_apple_get_default_options,
};
