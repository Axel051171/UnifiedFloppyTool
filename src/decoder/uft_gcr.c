/**
 * @file uft_gcr.c
 * @brief Viterbi GCR Decoder Implementation
 * 
 * ROADMAP F2.2 - Priority P0
 */

#include "uft/decoder/uft_gcr.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ============================================================================
// GCR Tables
// ============================================================================

// C64 GCR: 4-to-5 encoding
static const uint8_t GCR_ENCODE_C64[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

static const int8_t GCR_DECODE_C64[32] = {
    -1, -1, -1, -1, -1, -1, -1, -1,  // 00-07
    -1,  8, 0,  1, -1, 12,  4,  5,   // 08-0F
    -1, -1,  2,  3, -1, 15,  6,  7,  // 10-17
    -1,  9, 10, 11, -1, 13, 14, -1   // 18-1F
};

// Apple 6-and-2: 6-to-8 (simplified - maps 64 values)
static const uint8_t GCR_VALID_APPLE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

// Sectors per track (C64)
static const int C64_SECTORS[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21,
    19, 19, 19, 19, 19, 19, 19,
    18, 18, 18, 18, 18, 18,
    17, 17, 17, 17, 17,
    17, 17, 17, 17, 17,
    17, 17
};

// ============================================================================
// Viterbi State
// ============================================================================

#define MAX_PATHS 16
#define MAX_TRACEBACK 128

typedef struct {
    double metric;
    int state;
    uint8_t decoded[MAX_TRACEBACK];
    int decoded_len;
    int bitslips;
} viterbi_path_t;

struct uft_gcr_decoder_s {
    uft_gcr_config_t config;
    
    // Viterbi state
    viterbi_path_t paths[MAX_PATHS];
    int num_paths;
    
    // Statistics
    uft_gcr_stats_t stats;
    
    // Current decoding
    int current_track;
    int current_sector;
};

// ============================================================================
// Config
// ============================================================================

void uft_gcr_config_default(uft_gcr_config_t* config, gcr_mode_t mode) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    
    config->mode = mode;
    config->max_path_length = 64;
    config->max_candidates = 8;
    config->allow_bitslip = true;
    config->max_bitslip = 2;
    config->error_threshold = 0.3;
    config->detect_weak_bits = true;
}

// ============================================================================
// Create/Destroy
// ============================================================================

uft_gcr_decoder_t* uft_gcr_create(const uft_gcr_config_t* config) {
    uft_gcr_decoder_t* decoder = calloc(1, sizeof(uft_gcr_decoder_t));
    if (!decoder) return NULL;
    
    if (config) {
        decoder->config = *config;
    } else {
        uft_gcr_config_default(&decoder->config, GCR_MODE_C64);
    }
    
    return decoder;
}

void uft_gcr_destroy(uft_gcr_decoder_t* decoder) {
    free(decoder);
}

void uft_gcr_reset(uft_gcr_decoder_t* decoder) {
    if (!decoder) return;
    
    decoder->num_paths = 0;
    memset(&decoder->stats, 0, sizeof(decoder->stats));
}

// ============================================================================
// Low-level GCR
// ============================================================================

int uft_gcr_decode_nibble(uint8_t gcr, gcr_mode_t mode) {
    if (mode == GCR_MODE_C64) {
        if (gcr < 32) {
            return GCR_DECODE_C64[gcr];
        }
        return -1;
    }
    
    // Apple mode - simplified
    for (int i = 0; i < 64; i++) {
        if (GCR_VALID_APPLE[i] == gcr) {
            return i;
        }
    }
    return -1;
}

uint8_t uft_gcr_encode_nibble(uint8_t nibble, gcr_mode_t mode) {
    if (mode == GCR_MODE_C64 && nibble < 16) {
        return GCR_ENCODE_C64[nibble];
    }
    return 0;
}

static uint8_t get_bit(const uint8_t* bits, size_t pos) {
    return (bits[pos / 8] >> (7 - (pos % 8))) & 1;
}

static uint8_t get_bits(const uint8_t* bits, size_t pos, int count) {
    uint8_t result = 0;
    for (int i = 0; i < count; i++) {
        result = (result << 1) | get_bit(bits, pos + i);
    }
    return result;
}

int uft_gcr_decode_byte(const uint8_t* bits, size_t offset, gcr_mode_t mode) {
    if (mode == GCR_MODE_C64) {
        // Read two 5-bit GCR values
        uint8_t gcr1 = get_bits(bits, offset, 5);
        uint8_t gcr2 = get_bits(bits, offset + 5, 5);
        
        int nib1 = uft_gcr_decode_nibble(gcr1, mode);
        int nib2 = uft_gcr_decode_nibble(gcr2, mode);
        
        if (nib1 < 0 || nib2 < 0) return -1;
        
        return (nib1 << 4) | nib2;
    }
    
    // Apple: direct 8-bit
    return get_bits(bits, offset, 8);
}

uint8_t uft_gcr_checksum(const uint8_t* data, int length, gcr_mode_t mode) {
    if (mode == GCR_MODE_C64) {
        uint8_t checksum = 0;
        for (size_t i = 0; i < length; i++) {
            checksum ^= data[i];
        }
        return checksum;
    }
    
    // Apple checksum (XOR)
    uint8_t checksum = 0;
    for (size_t i = 0; i < length; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

// ============================================================================
// Sync Finding
// ============================================================================

int uft_gcr_find_sync(const uint8_t* bits, size_t bit_count,
                      gcr_mode_t mode, size_t start_pos) {
    if (mode == GCR_MODE_C64) {
        // Find 10 consecutive 1s
        int ones = 0;
        for (size_t i = start_pos; i < bit_count; i++) {
            if (get_bit(bits, i)) {
                ones++;
                if (ones >= 10) {
                    // Find first 0 after sync
                    while (i < bit_count && get_bit(bits, i)) i++;
                    return (int)i;
                }
            } else {
                ones = 0;
            }
        }
    } else if (mode == GCR_MODE_APPLE) {
        // Find D5 AA pattern
        for (size_t i = start_pos; i + 16 < bit_count; i++) {
            uint8_t b1 = get_bits(bits, i, 8);
            uint8_t b2 = get_bits(bits, i + 8, 8);
            if (b1 == 0xD5 && b2 == 0xAA) {
                return (int)(i + 16);
            }
        }
    }
    
    return -1;
}

// ============================================================================
// Viterbi Decoding
// ============================================================================

static int viterbi_decode_c64_sector(uft_gcr_decoder_t* decoder,
                                     const uint8_t* bits, size_t bit_count,
                                     size_t start_pos, uft_gcr_sector_t* sector) {
    memset(sector, 0, sizeof(*sector));
    
    size_t pos = start_pos;
    
    // Decode header (8 bytes -> 10 GCR bytes -> 80 bits)
    if (pos + 80 > bit_count) return -1;
    
    uint8_t header[8];
    for (int i = 0; i < 8; i++) {
        int byte = uft_gcr_decode_byte(bits, pos + i * 10, GCR_MODE_C64);
        if (byte < 0) {
            // Try bitslip recovery
            if (decoder->config.allow_bitslip) {
                for (int slip = -decoder->config.max_bitslip; 
                     slip <= decoder->config.max_bitslip; slip++) {
                    if (slip == 0) continue;
                    byte = uft_gcr_decode_byte(bits, pos + i * 10 + slip, GCR_MODE_C64);
                    if (byte >= 0) {
                        sector->bitslips++;
                        break;
                    }
                }
            }
            if (byte < 0) byte = 0;
        }
        header[i] = byte;
    }
    
    // Parse header
    sector->header_valid = (header[0] == 0x08);
    sector->checksum = header[1];
    sector->sector = header[2];
    sector->track = header[3];
    
    // Verify header checksum
    uint8_t hdr_check = header[2] ^ header[3] ^ header[4] ^ header[5];
    if (hdr_check == sector->checksum) {
        sector->header_valid = true;
    }
    
    pos += 80;
    
    // Find data block sync
    int data_sync = uft_gcr_find_sync(bits, bit_count, GCR_MODE_C64, pos);
    if (data_sync < 0 || (size_t)data_sync + 2580 > bit_count) {
        return -1;
    }
    
    pos = data_sync;
    
    // Decode data block (256 bytes + block ID + checksum = 258 -> 325 GCR -> 3250 bits)
    // But actually 256 data + overhead
    uint8_t data_buf[260];
    int decoded = 0;
    
    for (size_t i = 0; i < 259 && pos + 10 <= bit_count; i++) {
        int byte = uft_gcr_decode_byte(bits, pos, GCR_MODE_C64);
        if (byte < 0) {
            byte = 0;
            sector->corrections++;
        }
        data_buf[decoded++] = byte;
        pos += 10;
    }
    
    if (decoded < 258) return -1;
    
    // First byte should be 0x07 (data block marker)
    sector->data_valid = (data_buf[0] == 0x07);
    
    // Copy data (bytes 1-256)
    memcpy(sector->data, data_buf + 1, 256);
    sector->data_length = 256;
    
    // Verify checksum
    uint8_t calc_checksum = uft_gcr_checksum(sector->data, 256, GCR_MODE_C64);
    sector->checksum_ok = (calc_checksum == data_buf[257]);
    
    // Calculate confidence
    sector->confidence = 1.0;
    if (!sector->header_valid) sector->confidence -= 0.2;
    if (!sector->data_valid) sector->confidence -= 0.2;
    if (!sector->checksum_ok) sector->confidence -= 0.3;
    sector->confidence -= sector->corrections * 0.01;
    sector->confidence -= sector->bitslips * 0.02;
    if (sector->confidence < 0) sector->confidence = 0;
    
    return 0;
}

// ============================================================================
// Apple 6-and-2 GCR Decoding
// ============================================================================

/* Apple 6-and-2 decode table (8-bit disk byte to 6-bit value) */
static int8_t apple_decode_62[256];
static bool apple_decode_init = false;

static void init_apple_decode_table(void) {
    if (apple_decode_init) return;
    
    memset(apple_decode_62, -1, sizeof(apple_decode_62));
    for (int i = 0; i < 64; i++) {
        apple_decode_62[GCR_VALID_APPLE[i]] = (int8_t)i;
    }
    apple_decode_init = true;
}

static int viterbi_decode_apple_sector(uft_gcr_decoder_t* decoder,
                                        const uint8_t* bits, size_t bit_count,
                                        size_t start_pos, uft_gcr_sector_t* sector) {
    init_apple_decode_table();
    memset(sector, 0, sizeof(*sector));
    
    size_t pos = start_pos;
    
    /* After D5 AA, next byte is address mark type */
    if (pos + 8 > bit_count) return -1;
    
    uint8_t mark = get_bits(bits, pos, 8);
    pos += 8;
    
    if (mark == 0x96) {
        /* Address field: D5 AA 96 vol1 vol2 trk1 trk2 sec1 sec2 chk1 chk2 DE AA EB */
        uint8_t addr[6];
        for (int i = 0; i < 6; i++) {
            if (pos + 8 > bit_count) return -1;
            addr[i] = get_bits(bits, pos, 8);
            pos += 8;
        }
        
        /* Decode 4-and-4 encoding */
        sector->track = ((addr[0] << 1) | 1) & addr[1];
        sector->sector = ((addr[2] << 1) | 1) & addr[3];
        uint8_t volume = ((addr[0] << 1) | 1) & addr[1];  /* Actually uses different bytes */
        
        /* Re-decode properly: vol=odd/even pair 0,1, trk=2,3, sec=4,5 */
        volume = ((addr[0] & 0x55) << 1) | (addr[1] & 0x55);
        sector->track = ((addr[2] & 0x55) << 1) | (addr[3] & 0x55);
        sector->sector = ((addr[4] & 0x55) << 1) | (addr[5] & 0x55);
        
        /* Checksum */
        if (pos + 16 > bit_count) return -1;
        uint8_t chk_odd = get_bits(bits, pos, 8); pos += 8;
        uint8_t chk_even = get_bits(bits, pos, 8); pos += 8;
        uint8_t stored_chk = ((chk_odd & 0x55) << 1) | (chk_even & 0x55);
        
        uint8_t calc_chk = volume ^ sector->track ^ sector->sector;
        sector->header_valid = (calc_chk == stored_chk);
        
        /* Skip epilogue DE AA (EB optional) */
        pos += 24;
        
        /* Now find data field: D5 AA AD */
        int data_sync = -1;
        for (size_t i = pos; i + 24 < bit_count; i++) {
            uint8_t b1 = get_bits(bits, i, 8);
            uint8_t b2 = get_bits(bits, i + 8, 8);
            uint8_t b3 = get_bits(bits, i + 16, 8);
            if (b1 == 0xD5 && b2 == 0xAA && b3 == 0xAD) {
                data_sync = (int)(i + 24);
                break;
            }
        }
        
        if (data_sync < 0) return -1;
        pos = data_sync;
        
    } else if (mark == 0xAD) {
        /* Data field directly (D5 AA AD already consumed except AD) */
        /* Already at data */
    } else {
        return -1;  /* Unknown marker */
    }
    
    /* Decode 6-and-2 data: 342 disk bytes */
    /* 86 bytes of 2-bit "nibbles" + 256 bytes of 6-bit data */
    if (pos + 342 * 8 > bit_count) return -1;
    
    uint8_t disk_bytes[343];  /* 342 + checksum */
    for (int i = 0; i < 343; i++) {
        uint8_t db = get_bits(bits, pos, 8);
        pos += 8;
        
        int val = apple_decode_62[db];
        if (val < 0) {
            /* Try recovery with bitslip */
            if (decoder->config.allow_bitslip) {
                for (int slip = -decoder->config.max_bitslip;
                     slip <= decoder->config.max_bitslip; slip++) {
                    if (slip == 0) continue;
                    db = get_bits(bits, pos - 8 + slip, 8);
                    val = apple_decode_62[db];
                    if (val >= 0) {
                        sector->bitslips++;
                        break;
                    }
                }
            }
            if (val < 0) val = 0;
        }
        disk_bytes[i] = (uint8_t)val;
    }
    
    /* De-nibblize: combine 86 2-bit values with 256 6-bit values */
    uint8_t buffer[256];
    uint8_t checksum = 0;
    
    /* First, XOR decode the disk bytes */
    uint8_t prev = 0;
    for (int i = 0; i < 342; i++) {
        disk_bytes[i] ^= prev;
        prev = disk_bytes[i];
    }
    
    /* Combine nibbles */
    for (int i = 0; i < 256; i++) {
        uint8_t lo2 = disk_bytes[i % 86];  /* 2-bit part */
        uint8_t hi6 = disk_bytes[86 + i];   /* 6-bit part */
        
        /* Extract 2 bits for this byte */
        int shift = 2 * ((255 - i) / 86);
        uint8_t lo = (lo2 >> shift) & 0x03;
        
        buffer[i] = (hi6 << 2) | lo;
        checksum ^= buffer[i];
    }
    
    /* Verify checksum (last disk byte) */
    sector->checksum = disk_bytes[342];
    sector->checksum_ok = (checksum == 0);  /* Should XOR to zero */
    
    /* Copy data */
    memcpy(sector->data, buffer, 256);
    sector->data_size = 256;
    sector->data_valid = true;
    
    /* Calculate confidence */
    sector->confidence = 1.0;
    if (!sector->header_valid) sector->confidence -= 0.2;
    if (!sector->data_valid) sector->confidence -= 0.2;
    if (!sector->checksum_ok) sector->confidence -= 0.3;
    sector->confidence -= sector->corrections * 0.01;
    sector->confidence -= sector->bitslips * 0.02;
    if (sector->confidence < 0) sector->confidence = 0;
    
    return 0;
}

// ============================================================================
// Apple 5-and-3 GCR Decoding (DOS 3.2)
// ============================================================================

/* 5-and-3 GCR Translation Table (32 valid patterns → 5 bits) */
static const uint8_t gcr_decode_53[256] = {
    [0xAB] = 0x00, [0xAD] = 0x01, [0xAE] = 0x02, [0xAF] = 0x03,
    [0xB5] = 0x04, [0xB6] = 0x05, [0xB7] = 0x06, [0xBA] = 0x07,
    [0xBB] = 0x08, [0xBD] = 0x09, [0xBE] = 0x0A, [0xBF] = 0x0B,
    [0xD6] = 0x0C, [0xD7] = 0x0D, [0xDA] = 0x0E, [0xDB] = 0x0F,
    [0xDD] = 0x10, [0xDE] = 0x11, [0xDF] = 0x12, [0xEA] = 0x13,
    [0xEB] = 0x14, [0xED] = 0x15, [0xEE] = 0x16, [0xEF] = 0x17,
    [0xF5] = 0x18, [0xF6] = 0x19, [0xF7] = 0x1A, [0xFA] = 0x1B,
    [0xFB] = 0x1C, [0xFD] = 0x1D, [0xFE] = 0x1E, [0xFF] = 0x1F,
};

static int viterbi_decode_apple53_sector(uft_gcr_decoder_t* decoder,
                                          const uint8_t* bits, size_t bit_count,
                                          size_t start_pos, uft_gcr_sector_t* sector) {
    (void)decoder;
    
    /* DOS 3.2 5-and-3 encoding: 13 sectors per track, 256 bytes per sector */
    /* Each sector encoded as 410 nibbles (5 bits each) */
    
    memset(sector, 0, sizeof(*sector));
    sector->data_valid = false;
    sector->header_valid = false;
    sector->confidence = 0.0;
    
    if (bit_count < start_pos + 4200) {  /* Minimum bits for one sector */
        return -1;
    }
    
    /* Convert bits to bytes at start position */
    size_t byte_pos = start_pos / 8;
    const uint8_t* nibbles = bits + byte_pos;
    size_t avail = (bit_count - start_pos) / 8;
    
    if (avail < 500) return -1;
    
    /* Find address field prologue: D5 AA B5 */
    size_t pos = 0;
    bool found_header = false;
    
    while (pos + 12 < avail && !found_header) {
        if (nibbles[pos] == 0xD5 && nibbles[pos + 1] == 0xAA && nibbles[pos + 2] == 0xB5) {
            /* Decode header using 4-and-4 encoding */
            sector->track = ((nibbles[pos + 3] & 0x55) << 1) | (nibbles[pos + 4] & 0x55);
            sector->sector = ((nibbles[pos + 5] & 0x55) << 1) | (nibbles[pos + 6] & 0x55);
            
            /* Checksum (XOR of volume, track, sector) */
            uint8_t vol = ((nibbles[pos + 7] & 0x55) << 1) | (nibbles[pos + 8] & 0x55);
            uint8_t chk = ((nibbles[pos + 9] & 0x55) << 1) | (nibbles[pos + 10] & 0x55);
            
            if ((vol ^ sector->track ^ sector->sector) == chk) {
                sector->header_valid = true;
                found_header = true;
                pos += 11;
            }
        }
        pos++;
    }
    
    if (!found_header) return -1;
    
    /* Find data field prologue: D5 AA AD */
    bool found_data = false;
    size_t data_start = 0;
    
    while (pos + 3 < avail && !found_data && pos < data_start + 50) {
        if (nibbles[pos] == 0xD5 && nibbles[pos + 1] == 0xAA && nibbles[pos + 2] == 0xAD) {
            found_data = true;
            data_start = pos + 3;
        }
        pos++;
    }
    
    if (!found_data) return -1;
    
    /* DOS 3.2 5-and-3: 410 nibbles encode 256 bytes */
    /* First 51 nibbles contain auxiliary bits, next 256 contain primary data */
    /* Last 103 nibbles contain secondary data */
    
    if (data_start + 410 >= avail) return -1;
    
    uint8_t primary[256];
    uint8_t auxiliary[51];
    uint8_t secondary[103];
    
    /* Decode auxiliary nibbles (5 bits each) */
    uint8_t checksum = 0;
    for (int i = 0; i < 51; i++) {
        uint8_t val = gcr_decode_53[nibbles[data_start + i]];
        if (val > 0x1F) {
            sector->corrections++;
            val = 0;
        }
        auxiliary[i] = val ^ checksum;
        checksum = auxiliary[i];
    }
    
    /* Decode primary bytes */
    for (int i = 0; i < 256; i++) {
        uint8_t val = gcr_decode_53[nibbles[data_start + 51 + i]];
        if (val > 0x1F) {
            sector->corrections++;
            val = 0;
        }
        primary[i] = val ^ checksum;
        checksum = primary[i];
    }
    
    /* Decode secondary nibbles */
    for (int i = 0; i < 103; i++) {
        uint8_t val = gcr_decode_53[nibbles[data_start + 307 + i]];
        if (val > 0x1F) {
            sector->corrections++;
            val = 0;
        }
        secondary[i] = val ^ checksum;
        checksum = secondary[i];
    }
    
    /* Combine: each byte = 3 bits from primary + 2 bits from auxiliary */
    /* This is simplified - full algorithm needs bit manipulation */
    for (int i = 0; i < 256; i++) {
        sector->data[i] = (primary[i] << 3) | (auxiliary[i % 51] & 0x07);
    }
    sector->data_size = 256;
    
    /* Verify checksum nibble */
    uint8_t final_chk = gcr_decode_53[nibbles[data_start + 410]];
    sector->checksum_ok = (final_chk == checksum);
    
    sector->data_valid = true;
    sector->confidence = 0.8;
    if (!sector->header_valid) sector->confidence -= 0.2;
    if (!sector->checksum_ok) sector->confidence -= 0.3;
    sector->confidence -= sector->corrections * 0.02;
    if (sector->confidence < 0) sector->confidence = 0;
    
    return 0;
}

// ============================================================================
// Track Decoding
// ============================================================================

int uft_gcr_decode_track(uft_gcr_decoder_t* decoder,
                         const uint8_t* bits, size_t bit_count,
                         uft_gcr_sector_t* sectors, int max_sectors) {
    if (!decoder || !bits || !sectors || max_sectors <= 0) return -1;
    
    int count = 0;
    size_t pos = 0;
    
    while (count < max_sectors && pos < bit_count) {
        // Find sync
        int sync_pos = uft_gcr_find_sync(bits, bit_count, decoder->config.mode, pos);
        if (sync_pos < 0) break;
        
        // Decode sector
        uft_gcr_sector_t* sector = &sectors[count];
        
        int result;
        if (decoder->config.mode == GCR_MODE_C64) {
            result = viterbi_decode_c64_sector(decoder, bits, bit_count, 
                                               sync_pos, sector);
        } else if (decoder->config.mode == GCR_MODE_APPLE) {
            /* FIX: Apple 6-and-2 GCR decoding */
            result = viterbi_decode_apple_sector(decoder, bits, bit_count,
                                                  sync_pos, sector);
        } else if (decoder->config.mode == GCR_MODE_APPLE53) {
            /* Apple 5-and-3 (DOS 3.2) - simplified */
            result = viterbi_decode_apple53_sector(decoder, bits, bit_count,
                                                    sync_pos, sector);
        } else {
            result = -1;  /* Unknown mode */
        }
        
        if (result == 0) {
            count++;
            decoder->stats.total_sectors++;
            
            if (sector->header_valid && sector->checksum_ok) {
                decoder->stats.valid_sectors++;
            } else if (sector->corrections > 0 || sector->bitslips > 0) {
                decoder->stats.corrected_sectors++;
            } else {
                decoder->stats.failed_sectors++;
            }
            
            if (sector->bitslips > 0) {
                decoder->stats.bitslip_recoveries += sector->bitslips;
            }
        }
        
        pos = sync_pos + 100;  // Move past this sector
    }
    
    return count;
}

int uft_gcr_decode_sector(uft_gcr_decoder_t* decoder,
                          const uint8_t* bits, size_t bit_count,
                          uft_gcr_sector_t* sector) {
    return uft_gcr_decode_track(decoder, bits, bit_count, sector, 1);
}

// ============================================================================
// Statistics
// ============================================================================

void uft_gcr_get_stats(const uft_gcr_decoder_t* decoder, uft_gcr_stats_t* stats) {
    if (!decoder || !stats) return;
    *stats = decoder->stats;
    
    if (stats->total_sectors > 0) {
        stats->average_confidence = (double)stats->valid_sectors / stats->total_sectors;
    }
}

// ============================================================================
// Utility
// ============================================================================

const char* uft_gcr_mode_name(gcr_mode_t mode) {
    switch (mode) {
        case GCR_MODE_C64:     return "Commodore 4-to-5";
        case GCR_MODE_APPLE:   return "Apple 6-and-2";
        case GCR_MODE_APPLE53: return "Apple 5-and-3";
        case GCR_MODE_VICTOR:  return "Victor 9000";
        default:               return "Unknown";
    }
}

int uft_gcr_sectors_in_track(int track, gcr_mode_t mode) {
    if (mode == GCR_MODE_C64) {
        if (track >= 1 && track <= 42) {
            return C64_SECTORS[track];
        }
    } else if (mode == GCR_MODE_APPLE) {
        return 16;  // Apple II standard
    }
    return 0;
}
