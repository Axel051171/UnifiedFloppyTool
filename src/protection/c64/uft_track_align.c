/**
 * @file uft_track_align.c
 * @brief C64/1541 Track Alignment Module Implementation
 * 
 * Based on nibtools by Pete Rittwage and Markus Brenner.
 * Reference: c64preservation.com
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/protection/uft_track_align.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Static Data Tables
 * ============================================================================ */

/** Sectors per track for 1541 (track 1-42) */
static const uint8_t sector_map[ALIGN_MAX_TRACKS + 1] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1 - 10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11 - 20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21 - 30 */
    17, 17, 17, 17, 17,                       /* 31 - 35 */
    17, 17, 17, 17, 17, 17, 17                /* 36 - 42 (non-standard) */
};

/** Speed zone per track (0-3) */
static const uint8_t speed_map[ALIGN_MAX_TRACKS + 1] = {
    0,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  /*  1 - 10 */
    3, 3, 3, 3, 3, 3, 3, 2, 2, 2,  /* 11 - 20 */
    2, 2, 2, 2, 1, 1, 1, 1, 1, 1,  /* 21 - 30 */
    0, 0, 0, 0, 0,                  /* 31 - 35 */
    0, 0, 0, 0, 0, 0, 0             /* 36 - 42 */
};

/** Track capacity at 300 RPM */
static const size_t capacity[4] = {
    CAPACITY_DENSITY_0 / 300,  /* ~6250 bytes */
    CAPACITY_DENSITY_1 / 300,  /* ~6666 bytes */
    CAPACITY_DENSITY_2 / 300,  /* ~7142 bytes */
    CAPACITY_DENSITY_3 / 300   /* ~7692 bytes */
};

static const size_t capacity_min[4] = {
    CAPACITY_DENSITY_0 / 305,
    CAPACITY_DENSITY_1 / 305,
    CAPACITY_DENSITY_2 / 305,
    CAPACITY_DENSITY_3 / 305
};

static const size_t capacity_max[4] = {
    CAPACITY_DENSITY_0 / 295,
    CAPACITY_DENSITY_1 / 295,
    CAPACITY_DENSITY_2 / 295,
    CAPACITY_DENSITY_3 / 295
};

/** GCR-to-nibble decode table (high nibble) */
static const uint8_t gcr_decode_high[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x80, 0x00, 0x10, 0xff, 0xc0, 0x40, 0x50,
    0xff, 0xff, 0x20, 0x30, 0xff, 0xf0, 0x60, 0x70,
    0xff, 0x90, 0xa0, 0xb0, 0xff, 0xd0, 0xe0, 0xff
};

/** GCR-to-nibble decode table (low nibble) */
static const uint8_t gcr_decode_low[32] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x08, 0x00, 0x01, 0xff, 0x0c, 0x04, 0x05,
    0xff, 0xff, 0x02, 0x03, 0xff, 0x0f, 0x06, 0x07,
    0xff, 0x09, 0x0a, 0x0b, 0xff, 0x0d, 0x0e, 0xff
};

/** Alignment method names */
static const char *alignment_names[] = {
    "NONE", "GAP", "SEC0", "SYNC", "BADGCR", "VMAX", 
    "AUTO", "VMAX-CW", "RAW", "PIRATESLAYER", "RAPIDLOK", "SYNCALIGN"
};

/* ============================================================================
 * Helper Functions - GCR
 * ============================================================================ */

/**
 * @brief Check if byte at position is bad GCR
 */
bool is_bad_gcr(const uint8_t *buffer, size_t length, size_t pos)
{
    uint8_t byte1, byte2;
    uint8_t gcr_high, gcr_low;
    
    if (pos >= length) return false;
    
    byte1 = buffer[pos];
    byte2 = (pos + 1 < length) ? buffer[pos + 1] : buffer[0];
    
    /* Check first 5 bits (high nibble) */
    gcr_high = (byte1 >> 3) & 0x1F;
    if (gcr_decode_high[gcr_high] == 0xFF) return true;
    
    /* Check second 5 bits (low nibble) */
    gcr_low = ((byte1 & 0x07) << 2) | ((byte2 >> 6) & 0x03);
    if (gcr_decode_low[gcr_low] == 0xFF) return true;
    
    return false;
}

/**
 * @brief Count bad GCR bytes in track
 */
size_t check_bad_gcr(const uint8_t *buffer, size_t length)
{
    size_t count = 0;
    for (size_t i = 0; i < length; i++) {
        if (is_bad_gcr(buffer, length, i)) count++;
    }
    return count;
}

/**
 * @brief Find sync mark in GCR data
 */
bool find_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end)
{
    while (1) {
        if ((*gcr_ptr) + 1 >= gcr_end) {
            *gcr_ptr = (uint8_t *)gcr_end;
            return false;
        }
        
        /* Sync flag after 10th bit (sometimes short a bit) */
        if (((*gcr_ptr)[0] & 0x01) == 0x01 && (*gcr_ptr)[1] == 0xFF)
            break;
        
        (*gcr_ptr)++;
    }
    
    (*gcr_ptr)++;
    
    /* Skip sync bytes */
    while (*gcr_ptr < gcr_end && **gcr_ptr == 0xFF)
        (*gcr_ptr)++;
    
    return (*gcr_ptr < gcr_end);
}

/**
 * @brief Find bitshifted sync mark
 */
int find_bitshifted_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end)
{
    while (*gcr_ptr < gcr_end - 1) {
        uint8_t byte1 = (*gcr_ptr)[0];
        uint8_t byte2 = (*gcr_ptr)[1];
        
        /* Check each bit position for 10+ consecutive 1s */
        for (int bit = 0; bit < 8; bit++) {
            uint16_t combined = ((uint16_t)byte1 << 8) | byte2;
            combined >>= (7 - bit);
            
            /* Check for at least 10 ones */
            if ((combined & 0x3FF) == 0x3FF) {
                return bit + 1;  /* Return bit position 1-8 */
            }
        }
        
        (*gcr_ptr)++;
    }
    
    return 0;  /* Not found */
}

/**
 * @brief Find end of sync mark
 */
int find_end_of_sync(uint8_t **gcr_ptr, const uint8_t *gcr_end)
{
    int trailing_ones = 0;
    
    /* Skip full sync bytes */
    while (*gcr_ptr < gcr_end && **gcr_ptr == 0xFF)
        (*gcr_ptr)++;
    
    if (*gcr_ptr >= gcr_end) return 8;
    
    /* Count trailing ones in last partial sync byte */
    uint8_t byte = (*gcr_ptr)[-1];  /* Previous byte was last 0xFF */
    if (*gcr_ptr > (uint8_t *)(gcr_end - ALIGN_TRACK_LENGTH)) {
        byte = **gcr_ptr;
        for (int i = 7; i >= 0; i--) {
            if (byte & (1 << i))
                trailing_ones++;
            else
                break;
        }
    }
    
    return trailing_ones;
}

/**
 * @brief Fix first bad GCR byte in a run
 */
void fix_first_gcr(uint8_t *buffer, size_t length, size_t pos)
{
    uint8_t lastbyte = (pos == 0) ? buffer[length - 1] : buffer[pos - 1];
    unsigned int data = ((lastbyte & 0x03) << 8) | buffer[pos];
    unsigned int mask;
    uint8_t dstmask = 0x80;
    
    for (mask = (7 << 7); mask >= 7; mask >>= 1) {
        if ((data & mask) == 0)
            break;
        else
            dstmask = (dstmask >> 1) | 0x80;
    }
    
    buffer[pos] &= dstmask;
}

/**
 * @brief Fix last bad GCR byte in a run
 */
void fix_last_gcr(uint8_t *buffer, size_t length, size_t pos)
{
    uint8_t lastbyte = (pos == 0) ? buffer[length - 1] : buffer[pos - 1];
    unsigned int data = ((lastbyte & 0x03) << 8) | buffer[pos];
    unsigned int mask;
    uint8_t dstmask = 0x00;
    
    for (mask = 7; mask <= (7 << 7); mask <<= 1) {
        if ((data & mask) == 0)
            break;
        else
            dstmask = (dstmask << 1) | 0x01;
    }
    
    buffer[pos] &= dstmask;
}

/* ============================================================================
 * Buffer Manipulation
 * ============================================================================ */

/**
 * @brief Shift buffer left by n bits
 */
void shift_buffer_left(uint8_t *buffer, size_t length, int bits)
{
    if (bits <= 0 || bits >= 8 || length == 0) return;
    
    uint8_t *temp = malloc(length + 1);
    if (!temp) return;
    
    memcpy(temp, buffer, length);
    temp[length] = 0x00;
    
    int carryshift = 8 - bits;
    
    for (size_t i = 0; i < length; i++) {
        uint8_t carry = temp[i + 1];
        buffer[i] = (temp[i] << bits) | (carry >> carryshift);
    }
    
    free(temp);
}

/**
 * @brief Shift buffer right by n bits
 */
void shift_buffer_right(uint8_t *buffer, size_t length, int bits)
{
    if (bits <= 0 || bits >= 8 || length == 0) return;
    
    uint8_t *temp = malloc(length);
    if (!temp) return;
    
    memcpy(temp, buffer, length);
    
    int carryshift = 8 - bits;
    uint8_t carry = 0;
    
    for (size_t i = 0; i < length; i++) {
        buffer[i] = (temp[i] >> bits) | (carry << carryshift);
        carry = temp[i];
    }
    
    free(temp);
}

/**
 * @brief Rotate track data to new start position
 */
int rotate_track(uint8_t *buffer, size_t length, uint8_t *new_start)
{
    if (!buffer || !new_start || length == 0) return -1;
    if (new_start < buffer || new_start >= buffer + length) return -1;
    
    size_t offset = new_start - buffer;
    if (offset == 0) return 0;
    
    uint8_t *temp = malloc(length);
    if (!temp) return -1;
    
    /* Copy from new_start to end */
    memcpy(temp, new_start, length - offset);
    /* Copy from beginning to new_start */
    memcpy(temp + (length - offset), buffer, offset);
    /* Copy back */
    memcpy(buffer, temp, length);
    
    free(temp);
    return 0;
}

/* ============================================================================
 * V-MAX! Alignment
 * ============================================================================ */

/**
 * @brief Check if byte is a V-MAX! marker
 */
static inline bool is_vmax_marker(uint8_t byte)
{
    return (byte == VMAX_MARKER_4B || byte == VMAX_MARKER_69 ||
            byte == VMAX_MARKER_49 || byte == VMAX_MARKER_5A ||
            byte == VMAX_MARKER_A5);
}

/**
 * @brief Align V-MAX! protected track
 */
uint8_t *align_vmax(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t *start_pos = buffer;
    int run = 0;
    
    while (pos < buffer_end) {
        if (is_vmax_marker(*pos)) {
            if (!run) start_pos = pos;
            run++;
            if (run > 5) return start_pos;
        } else {
            run = 0;
        }
        pos++;
    }
    
    return NULL;
}

/**
 * @brief Align V-MAX! protected track (new algorithm)
 */
uint8_t *align_vmax_new(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t *key = NULL;
    uint8_t *key_temp = NULL;
    int run = 0;
    int longest = 0;
    
    while (pos < buffer_end - 2) {
        if (is_vmax_marker(*pos)) {
            if (run > 2)
                key_temp = pos - run + 1;
            run++;
        } else {
            if (run > longest) {
                key = key_temp;
                longest = run;
            }
            run = 0;
        }
        pos++;
    }
    
    return key;
}

/**
 * @brief Align V-MAX! Cinemaware variant
 */
uint8_t *align_vmax_cinemaware(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    
    /* Cinemaware pattern: 0x64 0xA5 0xA5 0xA5 */
    while (pos < buffer_end - 3) {
        if (pos[0] == VMAX_CW_MARKER &&
            pos[1] == VMAX_MARKER_A5 &&
            pos[2] == VMAX_MARKER_A5 &&
            pos[3] == VMAX_MARKER_A5) {
            return pos;
        }
        pos++;
    }
    
    return NULL;
}

/* ============================================================================
 * Pirate Slayer Alignment
 * ============================================================================ */

/**
 * @brief Check for Pirate Slayer signature
 */
static uint8_t *find_pirateslayer_sig(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    
    while (pos < buffer_end - 5) {
        /* Version 1 and 2: D7 D7 EB CC AD */
        if (pos[0] == PSLAYER_SIG_0 && pos[1] == PSLAYER_SIG_1 &&
            pos[2] == PSLAYER_SIG_2 && pos[3] == PSLAYER_SIG_3 &&
            pos[4] == PSLAYER_SIG_4) {
            return pos - 5;  /* Back up a little */
        }
        
        /* Version 1 secondary: EB D7 AA 55 */
        if (pos[0] == PSLAYER_V1_SEC_0 && pos[1] == PSLAYER_V1_SEC_1 &&
            pos[2] == PSLAYER_V1_SEC_2 && pos[3] == PSLAYER_V1_SEC_3) {
            return pos;
        }
        
        pos++;
    }
    
    return NULL;
}

/**
 * @brief Align Pirate Slayer protected track
 */
uint8_t *align_pirateslayer(uint8_t *buffer, size_t length)
{
    /* Backup buffer for restoration if signature not found */
    uint8_t *backup = malloc(length * 2);
    if (!backup) return NULL;
    
    memcpy(backup, buffer, length * 2);
    
    /* Try to find signature with bit shifting */
    for (int shift = 0; shift < 8; shift++) {
        uint8_t *result = find_pirateslayer_sig(buffer, length);
        if (result) {
            free(backup);
            return result;
        }
        
        shift_buffer_right(buffer, length, 1);
    }
    
    /* Restore original buffer */
    memcpy(buffer, backup, length * 2);
    free(backup);
    
    return NULL;
}

/* ============================================================================
 * RapidLok Alignment
 * ============================================================================ */

/**
 * @brief Align RapidLok protected track
 * 
 * RapidLok detection algorithm based on nibtools prot.c align_rl_special()
 */
uint8_t *align_rapidlok(uint8_t *buffer, size_t length, align_result_t *result)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t *key = NULL;
    
    /* State variables */
    int numFF = 0;          /* Sync byte count */
    int num55 = 0;          /* 0x55 count (extra sector start) */
    int num7B = 0;          /* 0x7B count (extra sector fill) */
    int num4B = 0;          /* 0x4B count (alternate fill) */
    int numXX = 0;          /* Off-bytes after extra sector */
    
    int longest = 0;
    int maxNumFF = 0;
    int maxNum7B = 0;
    
    bool found_rl_header = false;
    bool found_dos_header = false;
    bool found_rl_sector = false;
    
    /* Initialize result if provided */
    if (result) {
        memset(result, 0, sizeof(align_result_t));
        result->method_used = ALIGN_RAPIDLOK;
    }
    
    /* Scan track for RapidLok structures */
    while (pos < buffer_end) {
        uint8_t byte = *pos;
        
        /* Check for RapidLok header marker 0x75 */
        if (byte == RAPIDLOK_HEADER) {
            found_rl_header = true;
        }
        
        /* Check for DOS sector headers (0x52 or 0x55) */
        if (byte == 0x52 || byte == 0x55) {
            found_dos_header = true;
        }
        
        /* Track header detection */
        if (byte == 0xFF && numFF < 25 && num55 == 0) {
            numFF++;
            pos++;
            continue;
        }
        
        if (byte == 0x55 && numFF > 13 && numFF < 25 && num55 == 0) {
            num55++;
            pos++;
            continue;
        }
        
        if ((byte == RAPIDLOK_EXTRA_BYTE || byte == RAPIDLOK_ALT_BYTE) &&
            numFF > 13 && numFF < 25 && num55 == 1 && !found_rl_sector) {
            if (byte == RAPIDLOK_ALT_BYTE) num4B++;
            num7B++;
            pos++;
            continue;
        }
        
        /* Check for complete track header */
        if (numFF > 13 && numFF < 25 && num55 == 1 &&
            num7B >= RAPIDLOK_MIN_EXTRA && num7B <= RAPIDLOK_MAX_EXTRA) {
            found_rl_sector = true;
            
            if (byte != 0xFF) {
                numXX++;
                pos++;
                continue;
            } else {
                /* Found potential track header */
                int len_temp = numFF + num55 + num7B + numXX;
                if (len_temp > longest) {
                    key = pos - len_temp;
                    longest = len_temp;
                    maxNumFF = numFF;
                    maxNum7B = num7B;
                }
            }
        }
        
        /* Reset counters */
        numFF = num55 = num7B = num4B = numXX = 0;
        found_rl_sector = false;
        pos++;
    }
    
    /* Store result details */
    if (result && key) {
        result->success = true;
        result->align_offset = key - buffer;
        result->info.rapidlok.sync_length = maxNumFF;
        result->info.rapidlok.extra_length = maxNum7B;
        
        snprintf(result->description, sizeof(result->description),
                 "RapidLok: sync=%d, extra=%d", maxNumFF, maxNum7B);
    }
    
    return key;
}

/* ============================================================================
 * Generic Alignment Functions
 * ============================================================================ */

/**
 * @brief Align to longest gap mark
 */
uint8_t *align_auto_gap(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t *key = NULL;
    uint8_t *key_temp = NULL;
    int run = 0;
    int longest = 0;
    
    while (pos < buffer_end - 2) {
        if (*pos == *(pos + 1)) {
            key_temp = pos + 2;
            run++;
        } else {
            if (run > longest) {
                key = key_temp;
                longest = run;
            }
            run = 0;
        }
        pos++;
    }
    
    return key;
}

/**
 * @brief Align to bad GCR run
 */
uint8_t *align_bad_gcr(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t *key = NULL;
    uint8_t *key_temp = NULL;
    int run = 0;
    int longest = 0;
    
    while (pos < buffer_end) {
        if (is_bad_gcr(buffer, length, pos - buffer)) {
            key_temp = pos + 1;
            run++;
        } else {
            if (run > longest) {
                key = key_temp;
                longest = run;
            }
            run = 0;
        }
        pos++;
    }
    
    return key;
}

/**
 * @brief Align to longest sync mark
 */
uint8_t *align_long_sync(uint8_t *buffer, size_t length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t *key = NULL;
    uint8_t *key_temp = NULL;
    int run = 0;
    int longest = 0;
    
    while (pos < buffer_end) {
        if (*pos == 0xFF) {
            if (run == 0)
                key_temp = pos;
            run++;
        } else {
            if (run > longest) {
                key = key_temp;
                longest = run;
            }
            run = 0;
        }
        pos++;
    }
    
    return key;
}

/**
 * @brief Align to sector 0 header
 */
uint8_t *align_sector0(uint8_t *buffer, size_t length, size_t *sector_length)
{
    uint8_t *pos = buffer;
    uint8_t *buffer_end = buffer + length;
    uint8_t header[10];
    
    while (pos < buffer_end - 10) {
        /* Find sync */
        if (!find_sync(&pos, buffer_end))
            break;
        
        /* Decode header */
        /* Simple check: first byte after sync should be sector header marker */
        if ((pos[0] & 0xF0) == 0x50) {  /* GCR for 0x08 header marker */
            /* Extract sector number (simplified) */
            /* In full implementation, decode GCR to get sector */
            if (sector_length) {
                *sector_length = 0x160;  /* Typical sector length */
            }
            
            /* Back up to include sync */
            while (pos > buffer && *(pos - 1) == 0xFF)
                pos--;
            
            return pos;
        }
    }
    
    return NULL;
}

/* ============================================================================
 * Bitshift Repair
 * ============================================================================ */

/**
 * @brief Check if track is bitshifted
 */
bool is_track_bitshifted(const uint8_t *buffer, size_t length)
{
    uint8_t *pos = (uint8_t *)buffer;
    const uint8_t *track_end = buffer + length - 1;
    int num_syncs = 0;
    
    while (pos < track_end) {
        int ssb = find_bitshifted_sync(&pos, track_end);
        
        if (pos < track_end && ssb > 0) {
            num_syncs++;
            pos++;
            
            /* Find end of sync and check alignment */
            int end_bits = find_end_of_sync(&pos, track_end);
            if (end_bits % 8 != 0)
                return true;  /* Data sector is bitshifted */
        }
    }
    
    return false;
}

/**
 * @brief Sync-align bitshifted track
 */
size_t sync_align_track(uint8_t *buffer, size_t length)
{
    uint8_t temp_buffer[ALIGN_TRACK_LENGTH];
    size_t i;
    int bytes, bits;
    
    memset(temp_buffer, 0x00, sizeof(temp_buffer));
    
    /* First align buffer to a sync by shuffling */
    i = 0;
    while (!((buffer[i] == 0xFF) && (buffer[i + 1] & 0x80))) {
        i++;
        if (i >= length) break;
    }
    
    /* Skip initial header if close to start */
    if (i < 15) {
        while (!((buffer[i] == 0xFF) && (buffer[i + 1] & 0x80))) {
            i++;
            if (i >= length) break;
        }
    }
    
    if (i >= length) return 0;
    
    /* Shuffle buffer */
    memcpy(temp_buffer, buffer + i, length - i);
    memcpy(temp_buffer + length - i, buffer, i);
    memcpy(buffer, temp_buffer, length);
    
    /* Shift buffer left to edge of sync marks */
    for (i = 0; i < length; i++) {
        if (((buffer[i] == 0xFF) && ((buffer[i + 1] & 0x80) == 0x80) && (buffer[i + 1] != 0xFF)) ||
            ((buffer[i] == 0x7F) && ((buffer[i + 1] & 0xC0) == 0xC0) && (buffer[i + 1] != 0xFF))) {
            
            i++;  /* Set first byte to shift */
            bits = bytes = 0;
            
            /* Find next sync */
            while (!((buffer[i + bytes] == 0xFF) && (buffer[i + bytes + 1] & 0x80))) {
                bytes++;
                if (i + bytes >= length) break;
            }
            
            /* Shift left until MSB cleared */
            while (buffer[i] & 0x80) {
                if (bits++ > 7) break;
                
                for (int j = 0; j < bytes; j++) {
                    if (i + j + 1 >= length) break;
                    buffer[i + j] = (buffer[i + j] << 1) | ((buffer[i + j + 1] & 0x80) >> 7);
                }
            }
        }
    }
    
    return length;
}

/**
 * @brief Align bitshifted track from Kryoflux
 */
int align_bitshifted_track(uint8_t *track_start, size_t track_length,
                           uint8_t **aligned_start, size_t *aligned_length)
{
    if (!track_start || track_length == 0) {
        if (aligned_start) *aligned_start = track_start;
        if (aligned_length) *aligned_length = track_length;
        return -1;  /* Empty track */
    }
    
    /* Create double buffer */
    uint8_t *source = malloc(track_length * 2);
    if (!source) return -1;
    
    memcpy(source, track_start, track_length);
    memcpy(source + track_length, track_start, track_length);
    
    uint8_t *pos = source;
    uint8_t *src_end = source + track_length - 1;
    
    /* Skip initial sync */
    while ((*pos & 0xFF) && pos < src_end)
        pos++;
    
    /* Find first sync */
    int ssb = find_bitshifted_sync(&pos, src_end + 2);
    
    if (ssb == 0) {
        /* No sync found */
        if (aligned_start) *aligned_start = track_start;
        if (aligned_length) *aligned_length = track_length;
        free(source);
        return 0;
    }
    
    /* Align track data */
    size_t offset = pos - source;
    if (offset < track_length) {
        memcpy(track_start, source + offset, track_length - offset);
        memcpy(track_start + (track_length - offset), source, offset);
    }
    
    /* Now sync-align */
    sync_align_track(track_start, track_length);
    
    if (aligned_start) *aligned_start = track_start;
    if (aligned_length) *aligned_length = track_length;
    
    free(source);
    return 1;
}

/* ============================================================================
 * Fat Track Detection
 * ============================================================================ */

/**
 * @brief Compare two tracks for similarity
 */
size_t compare_tracks(const uint8_t *track1, const uint8_t *track2,
                      size_t length1, size_t length2,
                      bool same_disk, char *output)
{
    size_t diff = 0;
    size_t min_len = (length1 < length2) ? length1 : length2;
    
    if (length1 == 0 || length2 == 0 || length1 == 0x2000 || length2 == 0x2000)
        return 0xFFFF;  /* Invalid comparison */
    
    for (size_t i = 0; i < min_len; i++) {
        if (track1[i] != track2[i])
            diff++;
    }
    
    /* Add length difference */
    diff += (length1 > length2) ? (length1 - length2) : (length2 - length1);
    
    return diff;
}

/**
 * @brief Check if specific track is fat
 */
size_t check_fat_track(uint8_t *track_buffer, size_t *track_length, int halftrack)
{
    if (halftrack < 2 || halftrack >= ALIGN_MAX_HALFTRACKS - 2)
        return 0xFFFF;
    
    uint8_t *track1 = track_buffer + (halftrack * ALIGN_TRACK_LENGTH);
    uint8_t *track2 = track_buffer + ((halftrack + 2) * ALIGN_TRACK_LENGTH);
    
    return compare_tracks(track1, track2,
                          track_length[halftrack], track_length[halftrack + 2],
                          true, NULL);
}

/**
 * @brief Search for fat tracks in image
 */
int search_fat_tracks(uint8_t *track_buffer, uint8_t *track_density,
                      size_t *track_length, int *fat_track)
{
    int numfats = 0;
    
    if (fat_track) *fat_track = 0;
    
    for (int track = 2; track <= ALIGN_MAX_HALFTRACKS - 3; track += 2) {
        if (track_length[track] > 0 && track_length[track + 2] > 0 &&
            track_length[track] != 0x2000 && track_length[track + 2] != 0x2000) {
            
            size_t diff = check_fat_track(track_buffer, track_length, track);
            
            if (diff < 2) {
                /* Likely fat track */
                if (fat_track && numfats == 0)
                    *fat_track = track;
                
                /* Copy fat track to halftrack position */
                memcpy(track_buffer + ((track + 1) * ALIGN_TRACK_LENGTH),
                       track_buffer + (track * ALIGN_TRACK_LENGTH),
                       ALIGN_TRACK_LENGTH);
                
                track_length[track + 1] = track_length[track];
                track_density[track + 1] = track_density[track];
                
                numfats++;
            }
        }
    }
    
    return numfats;
}

/* ============================================================================
 * Main Alignment Functions
 * ============================================================================ */

/**
 * @brief Align track using specific method
 */
uint8_t *align_track(uint8_t *buffer, size_t length,
                     align_method_t method, align_result_t *result)
{
    uint8_t *align_pos = NULL;
    
    if (result) {
        memset(result, 0, sizeof(align_result_t));
        result->method_used = method;
        result->original_length = length;
    }
    
    switch (method) {
        case ALIGN_NONE:
        case ALIGN_RAW:
            align_pos = buffer;
            break;
            
        case ALIGN_VMAX:
            align_pos = align_vmax(buffer, length);
            if (!align_pos)
                align_pos = align_vmax_new(buffer, length);
            break;
            
        case ALIGN_VMAX_CW:
            align_pos = align_vmax_cinemaware(buffer, length);
            break;
            
        case ALIGN_PIRATESLAYER:
            align_pos = align_pirateslayer(buffer, length);
            break;
            
        case ALIGN_RAPIDLOK:
            align_pos = align_rapidlok(buffer, length, result);
            break;
            
        case ALIGN_LONGSYNC:
            align_pos = align_long_sync(buffer, length);
            break;
            
        case ALIGN_AUTOGAP:
        case ALIGN_GAP:
            align_pos = align_auto_gap(buffer, length);
            break;
            
        case ALIGN_BADGCR:
            align_pos = align_bad_gcr(buffer, length);
            break;
            
        case ALIGN_SECTOR0:
            align_pos = align_sector0(buffer, length, NULL);
            break;
            
        case ALIGN_SYNC:
            if (sync_align_track(buffer, length) > 0)
                align_pos = buffer;
            break;
            
        default:
            break;
    }
    
    if (result) {
        result->success = (align_pos != NULL);
        if (align_pos) {
            result->align_offset = align_pos - buffer;
            result->aligned_length = length;
        }
    }
    
    return align_pos;
}

/**
 * @brief Align track using automatic detection
 */
uint8_t *align_track_auto(uint8_t *buffer, size_t length,
                          uint8_t density, int halftrack,
                          align_result_t *result)
{
    uint8_t *align_pos = NULL;
    align_method_t method = ALIGN_NONE;
    
    if (result) {
        memset(result, 0, sizeof(align_result_t));
        result->original_length = length;
        result->density = density;
    }
    
    /* Try protection-specific alignments first */
    
    /* V-MAX! */
    align_pos = align_vmax(buffer, length);
    if (align_pos) {
        method = ALIGN_VMAX;
        goto found;
    }
    
    /* V-MAX! Cinemaware */
    align_pos = align_vmax_cinemaware(buffer, length);
    if (align_pos) {
        method = ALIGN_VMAX_CW;
        goto found;
    }
    
    /* Pirate Slayer */
    align_pos = align_pirateslayer(buffer, length);
    if (align_pos) {
        method = ALIGN_PIRATESLAYER;
        goto found;
    }
    
    /* RapidLok */
    align_pos = align_rapidlok(buffer, length, result);
    if (align_pos) {
        method = ALIGN_RAPIDLOK;
        goto found;
    }
    
    /* Generic alignments */
    
    /* Bad GCR (common at track boundaries) */
    align_pos = align_bad_gcr(buffer, length);
    if (align_pos) {
        method = ALIGN_BADGCR;
        goto found;
    }
    
    /* Longest sync */
    align_pos = align_long_sync(buffer, length);
    if (align_pos) {
        method = ALIGN_LONGSYNC;
        goto found;
    }
    
    /* Auto gap */
    align_pos = align_auto_gap(buffer, length);
    if (align_pos) {
        method = ALIGN_AUTOGAP;
        goto found;
    }
    
    /* No alignment found */
    if (result) {
        result->success = false;
        result->method_used = ALIGN_NONE;
    }
    return NULL;
    
found:
    if (result) {
        result->success = true;
        result->method_used = method;
        result->align_offset = align_pos - buffer;
        result->aligned_length = length;
        snprintf(result->description, sizeof(result->description),
                 "%s alignment at offset %zu",
                 align_method_name(method), result->align_offset);
    }
    
    return align_pos;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Get alignment method name
 */
const char *align_method_name(align_method_t method)
{
    if (method <= ALIGN_SYNC)
        return alignment_names[method];
    return "UNKNOWN";
}

/**
 * @brief Get expected track capacity for density
 */
size_t get_track_capacity(int density)
{
    if (density < 0 || density > 3) return 0;
    return capacity[density];
}

/**
 * @brief Get minimum track capacity for density
 */
size_t get_track_capacity_min(int density)
{
    if (density < 0 || density > 3) return 0;
    return capacity_min[density];
}

/**
 * @brief Get maximum track capacity for density
 */
size_t get_track_capacity_max(int density)
{
    if (density < 0 || density > 3) return 0;
    return capacity_max[density];
}

/**
 * @brief Get sectors per track for track number
 */
int get_sectors_per_track(int track)
{
    if (track < 1 || track > ALIGN_MAX_TRACKS) return 0;
    return sector_map[track];
}

/**
 * @brief Get density for track number
 */
int get_track_density(int track)
{
    if (track < 1 || track > ALIGN_MAX_TRACKS) return 0;
    return speed_map[track];
}

/**
 * @brief Compare sectors between two tracks
 */
size_t compare_sectors(const uint8_t *track1, const uint8_t *track2,
                       size_t length1, size_t length2,
                       const uint8_t *id1, const uint8_t *id2,
                       int track, char *output)
{
    /* Simplified implementation - count byte differences */
    return compare_tracks(track1, track2, length1, length2, false, output);
}
