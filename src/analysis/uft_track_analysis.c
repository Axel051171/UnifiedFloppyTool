/**
 * @file uft_track_analysis.c
 * @brief Generic Track Analysis Framework Implementation
 * 
 * Universal algorithms derived from XCopy Pro (1989-2011).
 * Platform-independent - works with Amiga, Atari, PC, Apple, C64, etc.
 * 
 * @version 1.0.0
 */

#include "uft_track_analysis.h"
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Platform Profiles - Pre-defined
 *===========================================================================*/

/* Amiga sync patterns */
static const uint32_t AMIGA_SYNCS[] = {
    0x4489, 0x9521, 0xA245, 0xA89A, 0x448A, 0xF8BC
};

/* IBM/PC sync patterns */
static const uint32_t IBM_SYNCS[] = {
    0x4489  /* A1 with missing clock */
};

/* Atari ST sync patterns */
static const uint32_t ATARI_SYNCS[] = {
    0x4489, 0xA1A1, 0x4E4E
};

/* Apple II sync patterns (using lower 16 bits) */
static const uint32_t APPLE_SYNCS[] = {
    0xD5AA, 0x96AD  /* D5 AA 96, D5 AA AD */
};

/* C64 GCR sync */
static const uint32_t C64_SYNCS[] = {
    0x52FF, 0xFF52
};

/* --- AMIGA --- */
const uft_platform_profile_t UFT_PROFILE_AMIGA_DD = {
    .platform = PLATFORM_AMIGA,
    .encoding = ENCODING_MFM,
    .name = "Amiga DD",
    .sync_patterns = AMIGA_SYNCS,
    .sync_count = 6,
    .sync_bits = 16,
    .track_length_min = 11000,
    .track_length_max = 14000,
    .track_length_nominal = 12668,  /* 0x318C */
    .long_track_threshold = 13056,  /* 0x3300 */
    .sectors_per_track = 11,
    .sector_size = 512,
    .sector_mfm_size = 1088,        /* 0x440 */
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

const uft_platform_profile_t UFT_PROFILE_AMIGA_HD = {
    .platform = PLATFORM_AMIGA,
    .encoding = ENCODING_MFM,
    .name = "Amiga HD",
    .sync_patterns = AMIGA_SYNCS,
    .sync_count = 6,
    .sync_bits = 16,
    .track_length_min = 22000,
    .track_length_max = 28000,
    .track_length_nominal = 25336,
    .long_track_threshold = 26112,
    .sectors_per_track = 22,
    .sector_size = 512,
    .sector_mfm_size = 1088,
    .sector_tolerance = 32,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/* --- ATARI ST --- */
const uft_platform_profile_t UFT_PROFILE_ATARI_ST_DD = {
    .platform = PLATFORM_ATARI_ST,
    .encoding = ENCODING_MFM,
    .name = "Atari ST DD",
    .sync_patterns = ATARI_SYNCS,
    .sync_count = 3,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 9,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

const uft_platform_profile_t UFT_PROFILE_ATARI_ST_HD = {
    .platform = PLATFORM_ATARI_ST,
    .encoding = ENCODING_MFM,
    .name = "Atari ST HD",
    .sync_patterns = ATARI_SYNCS,
    .sync_count = 3,
    .sync_bits = 16,
    .track_length_min = 12000,
    .track_length_max = 14000,
    .track_length_nominal = 12500,
    .long_track_threshold = 13000,
    .sectors_per_track = 18,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/* --- IBM/PC --- */
const uft_platform_profile_t UFT_PROFILE_IBM_DD = {
    .platform = PLATFORM_IBM_PC,
    .encoding = ENCODING_MFM,
    .name = "IBM PC DD",
    .sync_patterns = IBM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 9,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

const uft_platform_profile_t UFT_PROFILE_IBM_HD = {
    .platform = PLATFORM_IBM_PC,
    .encoding = ENCODING_MFM,
    .name = "IBM PC HD",
    .sync_patterns = IBM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 12000,
    .track_length_max = 14000,
    .track_length_nominal = 12500,
    .long_track_threshold = 13000,
    .sectors_per_track = 18,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 500.0,
    .rpm = 300.0
};

/* --- APPLE II --- */
const uft_platform_profile_t UFT_PROFILE_APPLE_DOS33 = {
    .platform = PLATFORM_APPLE_II,
    .encoding = ENCODING_GCR_APPLE,
    .name = "Apple DOS 3.3",
    .sync_patterns = APPLE_SYNCS,
    .sync_count = 1,
    .sync_bits = 24,
    .track_length_min = 6200,
    .track_length_max = 6800,
    .track_length_nominal = 6392,
    .long_track_threshold = 6600,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 400,
    .sector_tolerance = 16,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

const uft_platform_profile_t UFT_PROFILE_APPLE_PRODOS = {
    .platform = PLATFORM_APPLE_II,
    .encoding = ENCODING_GCR_APPLE,
    .name = "Apple ProDOS",
    .sync_patterns = APPLE_SYNCS,
    .sync_count = 2,
    .sync_bits = 24,
    .track_length_min = 6200,
    .track_length_max = 6800,
    .track_length_nominal = 6392,
    .long_track_threshold = 6600,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 400,
    .sector_tolerance = 16,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/* --- C64 --- */
const uft_platform_profile_t UFT_PROFILE_C64 = {
    .platform = PLATFORM_C64,
    .encoding = ENCODING_GCR_C64,
    .name = "Commodore 64",
    .sync_patterns = C64_SYNCS,
    .sync_count = 2,
    .sync_bits = 16,
    .track_length_min = 7600,
    .track_length_max = 8400,
    .track_length_nominal = 7928,
    .long_track_threshold = 8200,
    .sectors_per_track = 21,  /* Varies by track zone */
    .sector_size = 256,
    .sector_mfm_size = 360,
    .sector_tolerance = 16,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/* --- BBC MICRO --- */
const uft_platform_profile_t UFT_PROFILE_BBC_DFS = {
    .platform = PLATFORM_BBC_MICRO,
    .encoding = ENCODING_FM,
    .name = "BBC DFS",
    .sync_patterns = IBM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 3100,
    .track_length_max = 3300,
    .track_length_nominal = 3125,
    .long_track_threshold = 3200,
    .sectors_per_track = 10,
    .sector_size = 256,
    .sector_mfm_size = 320,
    .sector_tolerance = 16,
    .data_rate_kbps = 125.0,
    .rpm = 300.0
};

const uft_platform_profile_t UFT_PROFILE_BBC_ADFS = {
    .platform = PLATFORM_BBC_MICRO,
    .encoding = ENCODING_MFM,
    .name = "BBC ADFS",
    .sync_patterns = IBM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6200,
    .track_length_max = 6400,
    .track_length_nominal = 6250,
    .long_track_threshold = 6350,
    .sectors_per_track = 16,
    .sector_size = 256,
    .sector_mfm_size = 390,
    .sector_tolerance = 16,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/* --- MSX --- */
const uft_platform_profile_t UFT_PROFILE_MSX = {
    .platform = PLATFORM_MSX,
    .encoding = ENCODING_MFM,
    .name = "MSX",
    .sync_patterns = IBM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 9,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/* --- AMSTRAD CPC --- */
const uft_platform_profile_t UFT_PROFILE_AMSTRAD = {
    .platform = PLATFORM_AMSTRAD_CPC,
    .encoding = ENCODING_MFM,
    .name = "Amstrad CPC",
    .sync_patterns = IBM_SYNCS,
    .sync_count = 1,
    .sync_bits = 16,
    .track_length_min = 6000,
    .track_length_max = 7000,
    .track_length_nominal = 6250,
    .long_track_threshold = 6500,
    .sectors_per_track = 9,
    .sector_size = 512,
    .sector_mfm_size = 640,
    .sector_tolerance = 32,
    .data_rate_kbps = 250.0,
    .rpm = 300.0
};

/*===========================================================================
 * Utility Functions
 *===========================================================================*/

static inline uint32_t rol32(uint32_t val, int bits) {
    return (val << 1) | (val >> (bits - 1));
}

static inline uint16_t read_be16(const uint8_t *p) {
    return ((uint16_t)p[0] << 8) | p[1];
}

static inline uint32_t read_be24(const uint8_t *p) {
    return ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | p[2];
}

static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | p[3];
}

/*===========================================================================
 * Name Lookup Functions
 *===========================================================================*/

const char *uft_platform_name(uft_platform_t platform) {
    switch (platform) {
        case PLATFORM_AMIGA:        return "Amiga";
        case PLATFORM_ATARI_ST:     return "Atari ST";
        case PLATFORM_IBM_PC:       return "IBM PC";
        case PLATFORM_APPLE_II:     return "Apple II";
        case PLATFORM_C64:          return "Commodore 64";
        case PLATFORM_BBC_MICRO:    return "BBC Micro";
        case PLATFORM_MSX:          return "MSX";
        case PLATFORM_AMSTRAD_CPC:  return "Amstrad CPC";
        case PLATFORM_ARCHIMEDES:   return "Archimedes";
        case PLATFORM_SAM_COUPE:    return "SAM Coupé";
        case PLATFORM_SPECTRUM_PLUS3: return "Spectrum +3";
        case PLATFORM_PC98:         return "PC-98";
        case PLATFORM_X68000:       return "X68000";
        case PLATFORM_FM_TOWNS:     return "FM Towns";
        case PLATFORM_CUSTOM:       return "Custom";
        default:                    return "Unknown";
    }
}

const char *uft_encoding_name(uft_encoding_t encoding) {
    switch (encoding) {
        case ENCODING_FM:           return "FM";
        case ENCODING_MFM:          return "MFM";
        case ENCODING_GCR_APPLE:    return "GCR (Apple)";
        case ENCODING_GCR_C64:      return "GCR (C64)";
        case ENCODING_GCR_VICTOR:   return "GCR (Victor)";
        case ENCODING_M2FM:         return "M2FM";
        case ENCODING_MMFM:         return "MMFM";
        default:                    return "Unknown";
    }
}

const char *uft_track_type_name(uft_track_type_t type) {
    switch (type) {
        case TRACK_TYPE_STANDARD:    return "Standard";
        case TRACK_TYPE_PROTECTED:   return "Protected";
        case TRACK_TYPE_LONG:        return "Long Track";
        case TRACK_TYPE_SHORT:       return "Short Track";
        case TRACK_TYPE_WEAK_BITS:   return "Weak Bits";
        case TRACK_TYPE_FUZZY:       return "Fuzzy Bits";
        case TRACK_TYPE_NO_SYNC:     return "No Sync";
        case TRACK_TYPE_UNFORMATTED: return "Unformatted";
        case TRACK_TYPE_DAMAGED:     return "Damaged";
        default:                     return "Unknown";
    }
}

const uft_platform_profile_t *uft_get_platform_profile(uft_platform_t platform) {
    switch (platform) {
        case PLATFORM_AMIGA:        return &UFT_PROFILE_AMIGA_DD;
        case PLATFORM_ATARI_ST:     return &UFT_PROFILE_ATARI_ST_DD;
        case PLATFORM_IBM_PC:       return &UFT_PROFILE_IBM_DD;
        case PLATFORM_APPLE_II:     return &UFT_PROFILE_APPLE_DOS33;
        case PLATFORM_C64:          return &UFT_PROFILE_C64;
        case PLATFORM_BBC_MICRO:    return &UFT_PROFILE_BBC_ADFS;
        case PLATFORM_MSX:          return &UFT_PROFILE_MSX;
        case PLATFORM_AMSTRAD_CPC:  return &UFT_PROFILE_AMSTRAD;
        default:                    return NULL;
    }
}

/*===========================================================================
 * Sync Pattern Detection (Generic - from XCopy Pro)
 *===========================================================================*/

int uft_find_syncs_rotated(const uint8_t *data, size_t size,
                           const uint32_t *patterns, int pattern_count,
                           int sync_bits, uft_sync_result_t *result) {
    if (!data || !patterns || !result || size < 4) {
        return 0;
    }
    
    memset(result, 0, sizeof(*result));
    
    int found = 0;
    size_t pos = 0;
    uint32_t mask = (1ULL << sync_bits) - 1;
    
    /* Read initial word */
    uint32_t d0 = (sync_bits >= 24) ? read_be32(data) : 
                  (sync_bits >= 16) ? read_be16(data) : data[0];
    pos = (sync_bits + 7) / 8;
    
    int bytes_per_step = (sync_bits <= 16) ? 2 : 4;
    
    while (pos < size - bytes_per_step && found < UFT_MAX_SYNC_POSITIONS) {
        /* Shift in next data */
        if (sync_bits <= 16) {
            d0 = ((d0 & 0xFFFF) << 16) | read_be16(data + pos);
        } else {
            d0 = read_be32(data + pos);
        }
        pos += bytes_per_step;
        
        uint32_t rotated = d0;
        
        /* Try all bit rotations */
        for (int bit = 0; bit < sync_bits; bit++) {
            uint32_t word = (rotated >> (32 - sync_bits)) & mask;
            
            /* Check against all patterns */
            for (int p = 0; p < pattern_count; p++) {
                if (word == (patterns[p] & mask)) {
                    result->positions[found].byte_position = pos - bytes_per_step;
                    result->positions[found].bit_offset = bit;
                    result->positions[found].pattern = patterns[p];
                    result->positions[found].pattern_index = p;
                    result->positions[found].confidence = (bit == 0) ? 1.0f : 0.8f;
                    
                    if (bit > 0) {
                        result->bit_shifted = true;
                    }
                    
                    found++;
                    
                    if (found >= UFT_MAX_SYNC_POSITIONS) {
                        goto done;
                    }
                    
                    /* Skip minimum sector length */
                    pos += 0x100;
                    if (pos < size - bytes_per_step) {
                        d0 = (sync_bits <= 16) ? 
                             (uint32_t)((read_be16(data + pos - 2) << 16) | read_be16(data + pos)) :
                             read_be32(data + pos);
                    }
                    goto next_pos;
                }
            }
            
            /* Rotate left */
            rotated = rol32(rotated, 32);
        }
        next_pos:;
    }
    
done:
    result->count = found;
    
    /* Find primary pattern */
    if (found > 0) {
        int counts[UFT_MAX_SYNC_PATTERNS] = {0};
        for (int i = 0; i < found; i++) {
            counts[result->positions[i].pattern_index]++;
        }
        
        int max_count = 0;
        for (int p = 0; p < pattern_count; p++) {
            if (counts[p] > max_count) {
                max_count = counts[p];
                result->primary_pattern = patterns[p];
                result->primary_count = max_count;
            }
        }
    }
    
    return found;
}

int uft_find_sync_simple(const uint8_t *data, size_t size,
                         uint32_t pattern, int sync_bits,
                         size_t *positions, int max_positions) {
    if (!data || !positions || size < 2) {
        return 0;
    }
    
    int found = 0;
    uint32_t mask = (1ULL << sync_bits) - 1;
    int bytes = (sync_bits + 7) / 8;
    
    for (size_t i = 0; i < size - bytes && found < max_positions; i++) {
        uint32_t word = 0;
        for (int b = 0; b < bytes; b++) {
            word = (word << 8) | data[i + b];
        }
        
        if ((word & mask) == (pattern & mask)) {
            positions[found++] = i;
            i += 0x100;  /* Skip minimum sector */
        }
    }
    
    return found;
}

/*===========================================================================
 * Track Length Measurement (Generic - from XCopy Pro getracklen)
 *===========================================================================*/

size_t uft_measure_track_length(const uint8_t *data, size_t size,
                                size_t *end_position) {
    if (!data || size < 2) {
        return 0;
    }
    
    /* Find last non-zero word from end */
    size_t pos = size;
    while (pos >= 2) {
        pos -= 2;
        uint16_t word = read_be16(data + pos);
        if (word != 0) {
            pos += 2;
            break;
        }
    }
    
    if (end_position) {
        *end_position = pos;
    }
    
    /* For 2-rotation read, track length is half */
    size_t track_len = pos / 2;
    track_len &= ~1;  /* Ensure even */
    
    return track_len;
}

size_t uft_measure_track_length_enc(const uint8_t *data, size_t size,
                                    uft_encoding_t encoding,
                                    size_t *end_position) {
    (void)encoding;  /* Currently same algorithm for all */
    return uft_measure_track_length(data, size, end_position);
}

/*===========================================================================
 * Sector/GAP Analysis (Generic - from XCopy Pro gapsearch)
 *===========================================================================*/

int uft_analyze_sectors(const size_t *sync_positions, int sync_count,
                        size_t tolerance, uft_sector_analysis_t *result) {
    if (!sync_positions || !result || sync_count < 2) {
        return -1;
    }
    
    memset(result, 0, sizeof(*result));
    result->sector_count = sync_count;
    
    /* Calculate sector lengths and find unique ones */
    for (int i = 0; i < sync_count - 1; i++) {
        size_t len = sync_positions[i + 1] - sync_positions[i];
        
        /* Find or add to unique lengths */
        bool found = false;
        for (int j = 0; j < result->unique_lengths; j++) {
            size_t diff = (len > result->lengths[j].length) ?
                          (len - result->lengths[j].length) :
                          (result->lengths[j].length - len);
            
            if (diff <= tolerance) {
                result->lengths[j].count++;
                found = true;
                break;
            }
        }
        
        if (!found && result->unique_lengths < UFT_MAX_SECTOR_TYPES) {
            result->lengths[result->unique_lengths].length = len;
            result->lengths[result->unique_lengths].count = 1;
            result->unique_lengths++;
        }
    }
    
    /* Calculate percentages and find nominal */
    int max_count = 0;
    for (int i = 0; i < result->unique_lengths; i++) {
        result->lengths[i].percentage = 
            (float)result->lengths[i].count / (float)(sync_count - 1);
        
        if (result->lengths[i].count > max_count) {
            max_count = result->lengths[i].count;
            result->nominal_length = result->lengths[i].length;
        }
    }
    
    /* Check uniformity */
    result->is_uniform = (result->unique_lengths == 1);
    result->uniformity = (float)max_count / (float)(sync_count - 1);
    
    /* Find GAP */
    result->gap_found = uft_find_gap_by_frequency(
        sync_positions, sync_count, tolerance,
        &result->gap_sector_index, &result->gap_length);
    
    return 0;
}

bool uft_find_gap_by_frequency(const size_t *sync_positions, int sync_count,
                               size_t tolerance, int *gap_index, size_t *gap_length) {
    if (!sync_positions || !gap_index || sync_count < 3) {
        return false;
    }
    
    /* Calculate lengths */
    size_t lengths[UFT_MAX_SYNC_POSITIONS];
    int counts[UFT_MAX_SECTOR_TYPES] = {0};
    size_t unique_lens[UFT_MAX_SECTOR_TYPES];
    int unique_count = 0;
    
    for (int i = 0; i < sync_count - 1 && i < UFT_MAX_SYNC_POSITIONS; i++) {
        lengths[i] = sync_positions[i + 1] - sync_positions[i];
        
        /* Count occurrences with tolerance */
        bool found = false;
        for (int j = 0; j < unique_count; j++) {
            size_t diff = (lengths[i] > unique_lens[j]) ?
                          (lengths[i] - unique_lens[j]) :
                          (unique_lens[j] - lengths[i]);
            
            if (diff <= tolerance) {
                counts[j]++;
                found = true;
                break;
            }
        }
        
        if (!found && unique_count < UFT_MAX_SECTOR_TYPES) {
            unique_lens[unique_count] = lengths[i];
            counts[unique_count] = 1;
            unique_count++;
        }
    }
    
    /* Find minimum occurrence (GAP) */
    int min_count = 0x7F;
    size_t gap_len = 0;
    
    for (int i = 0; i < unique_count; i++) {
        if (counts[i] < min_count) {
            min_count = counts[i];
            gap_len = unique_lens[i];
        }
    }
    
    /* Find sector index with this length */
    for (int i = 0; i < sync_count - 1; i++) {
        size_t diff = (lengths[i] > gap_len) ?
                      (lengths[i] - gap_len) : (gap_len - lengths[i]);
        
        if (diff <= tolerance) {
            *gap_index = i + 1;
            if (gap_length) {
                *gap_length = lengths[i];
            }
            return true;
        }
    }
    
    return false;
}

size_t uft_calc_write_start(const size_t *sync_positions, int sync_count,
                            int gap_sector_index, size_t pre_gap_bytes) {
    if (!sync_positions || gap_sector_index <= 0 || gap_sector_index >= sync_count) {
        return 0;
    }
    
    size_t start = sync_positions[gap_sector_index];
    
    if (start >= pre_gap_bytes) {
        start -= pre_gap_bytes;
    }
    
    return start;
}

/*===========================================================================
 * Breakpoint Detection (Generic - from XCopy Pro Neuhaus)
 *===========================================================================*/

bool uft_detect_breakpoints(const uint8_t *data, size_t size,
                            int max_breakpoints,
                            size_t *positions, int *count) {
    if (!data || !count || size < 16) {
        return false;
    }
    
    int bp_count = 0;
    size_t bytes_left = size - 8;
    const uint8_t *p = data;
    
    while (bytes_left > 0) {
        uint8_t val = *p++;
        bytes_left--;
        
        /* Count consecutive identical bytes */
        while (bytes_left > 0 && *p == val) {
            p++;
            bytes_left--;
        }
        
        if (bytes_left > 0 && *p != val) {
            bp_count++;
            
            if (positions && bp_count <= UFT_MAX_BREAKPOINTS) {
                positions[bp_count - 1] = (size_t)(p - data);
            }
            
            if (bp_count > max_breakpoints) {
                *count = bp_count;
                return false;
            }
        }
    }
    
    *count = bp_count;
    return (bp_count > 0 && bp_count <= max_breakpoints);
}

bool uft_is_long_track(size_t track_length, const uft_platform_profile_t *profile) {
    if (!profile) {
        return track_length >= 13056;  /* Amiga default */
    }
    return track_length >= profile->long_track_threshold;
}

/*===========================================================================
 * Platform Detection
 *===========================================================================*/

uft_platform_t uft_detect_platform(const uft_track_analysis_t *analysis) {
    if (!analysis || analysis->sync.count == 0) {
        return PLATFORM_UNKNOWN;
    }
    
    uint32_t sync = analysis->sync.primary_pattern;
    int sectors = analysis->sectors.sector_count;
    size_t track_len = analysis->track_length;
    
    /* Amiga: $4489 sync, 11 or 22 sectors */
    if (sync == 0x4489) {
        if (sectors == 11 && track_len >= 11000 && track_len <= 14000) {
            return PLATFORM_AMIGA;
        }
        if (sectors == 22 && track_len >= 22000 && track_len <= 28000) {
            return PLATFORM_AMIGA;
        }
        if (sectors == 9 && track_len >= 6000 && track_len <= 7000) {
            return PLATFORM_IBM_PC;  /* Could also be Atari ST */
        }
        if (sectors == 18 && track_len >= 12000 && track_len <= 14000) {
            return PLATFORM_IBM_PC;
        }
    }
    
    /* Amiga protection syncs */
    if (sync == 0x9521 || sync == 0xA245 || sync == 0xA89A) {
        return PLATFORM_AMIGA;
    }
    
    /* Apple: D5 AA sync start */
    if ((sync & 0xFFFF00) == 0xD5AA00) {
        return PLATFORM_APPLE_II;
    }
    
    /* C64: GCR sync */
    if (sync == 0x52FF || sync == 0xFF52) {
        return PLATFORM_C64;
    }
    
    return PLATFORM_UNKNOWN;
}

/*===========================================================================
 * Main Analysis Functions
 *===========================================================================*/

void uft_analysis_config_init(uft_analysis_config_t *config) {
    if (!config) return;
    
    memset(config, 0, sizeof(*config));
    config->auto_detect_platform = true;
    config->search_all_syncs = true;
    config->detect_protection = true;
    config->detect_breakpoints = true;
    config->length_tolerance = 32;
    config->max_syncs_to_find = UFT_MAX_SYNC_POSITIONS;
}

int uft_analyze_track(const uint8_t *track_data, size_t track_size,
                      uft_track_analysis_t *result) {
    uft_analysis_config_t config;
    uft_analysis_config_init(&config);
    config.track_data = track_data;
    config.track_size = track_size;
    
    return uft_analyze_track_ex(&config, result);
}

int uft_analyze_track_profile(const uint8_t *track_data, size_t track_size,
                              const uft_platform_profile_t *profile,
                              uft_track_analysis_t *result) {
    uft_analysis_config_t config;
    uft_analysis_config_init(&config);
    config.track_data = track_data;
    config.track_size = track_size;
    config.profile = profile;
    config.auto_detect_platform = false;
    
    return uft_analyze_track_ex(&config, result);
}

int uft_analyze_track_ex(const uft_analysis_config_t *config,
                         uft_track_analysis_t *result) {
    if (!config || !config->track_data || !result || config->track_size < 100) {
        return -1;
    }
    
    memset(result, 0, sizeof(*result));
    
    const uint8_t *data = config->track_data;
    size_t size = config->track_size;
    const uft_platform_profile_t *profile = config->profile;
    
    /* Step 1: Measure track length */
    size_t end_pos = 0;
    result->track_length = uft_measure_track_length(data, size, &end_pos);
    result->data_end = end_pos;
    
    /* Step 2: Find sync patterns */
    if (profile) {
        /* Use profile-specific syncs */
        uft_find_syncs_rotated(data, result->track_length,
                               profile->sync_patterns, profile->sync_count,
                               profile->sync_bits, &result->sync);
    } else if (config->search_all_syncs) {
        /* Search all known patterns - try common MFM first */
        static const uint32_t all_syncs[] = {
            0x4489, 0x9521, 0xA245, 0xA89A, 0x448A, 0xF8BC,
            0xA1A1, 0x4E4E, 0x52FF
        };
        uft_find_syncs_rotated(data, result->track_length,
                               all_syncs, 9, 16, &result->sync);
    }
    
    /* Step 3: Handle no sync case */
    if (result->sync.count == 0) {
        if (config->detect_breakpoints) {
            result->has_breakpoints = uft_detect_breakpoints(
                data, result->track_length, 5,
                result->breakpoint_positions, &result->breakpoint_count);
            
            if (result->has_breakpoints) {
                result->type = TRACK_TYPE_PROTECTED;
                result->is_protected = true;
                result->confidence = 0.6f;
                return 0;
            }
        }
        
        result->type = TRACK_TYPE_NO_SYNC;
        result->confidence = 0.0f;
        return 0;
    }
    
    /* Step 4: Analyze sector structure */
    size_t sync_pos[UFT_MAX_SYNC_POSITIONS];
    for (int i = 0; i < result->sync.count; i++) {
        sync_pos[i] = result->sync.positions[i].byte_position;
    }
    
    size_t tolerance = config->length_tolerance;
    if (profile) {
        tolerance = profile->sector_tolerance;
    }
    
    uft_analyze_sectors(sync_pos, result->sync.count, tolerance, &result->sectors);
    
    /* Step 5: Calculate write start */
    result->optimal_write_start = uft_calc_write_start(
        sync_pos, result->sync.count,
        result->sectors.gap_sector_index, 10);
    
    /* Step 6: Detect platform */
    if (config->auto_detect_platform) {
        result->detected_platform = uft_detect_platform(result);
        profile = uft_get_platform_profile(result->detected_platform);
    } else if (profile) {
        result->detected_platform = profile->platform;
    }
    
    if (profile) {
        result->detected_encoding = profile->encoding;
    }
    
    /* Step 7: Check for long track */
    result->is_long_track = uft_is_long_track(result->track_length, profile);
    
    /* Step 8: Classify track */
    if (result->is_long_track) {
        result->type = TRACK_TYPE_LONG;
        result->is_protected = true;
        result->confidence = 0.9f;
    } else if (result->has_breakpoints) {
        result->type = TRACK_TYPE_PROTECTED;
        result->is_protected = true;
        result->confidence = 0.8f;
    } else if (result->sync.bit_shifted) {
        result->type = TRACK_TYPE_PROTECTED;
        result->is_protected = true;
        result->confidence = 0.7f;
    } else if (result->sectors.is_uniform) {
        result->type = TRACK_TYPE_STANDARD;
        result->confidence = 0.95f;
    } else {
        result->type = TRACK_TYPE_PROTECTED;
        result->is_protected = true;
        result->confidence = 0.6f;
    }
    
    /* Step 9: Identify protection */
    uft_identify_protection(result, result->protection_name,
                            sizeof(result->protection_name));
    
    /* Set format name */
    if (profile) {
        snprintf(result->format_name, sizeof(result->format_name),
                 "%s", profile->name);
    } else {
        snprintf(result->format_name, sizeof(result->format_name),
                 "%s", uft_platform_name(result->detected_platform));
    }
    
    return 0;
}

/*===========================================================================
 * Protection Identification
 *===========================================================================*/

bool uft_identify_protection(const uft_track_analysis_t *analysis,
                             char *name, size_t name_size) {
    if (!analysis || !name || name_size == 0) {
        return false;
    }
    
    name[0] = '\0';
    
    uint32_t sync = analysis->sync.primary_pattern;
    
    /* Known Amiga protections by sync */
    switch (sync) {
        case 0x9521:
            snprintf(name, name_size, "Arkanoid Protection");
            return true;
        case 0xA245:
            snprintf(name, name_size, "Ocean/Imagine Protection");
            return true;
        case 0xA89A:
            snprintf(name, name_size, "Novagen Protection");
            return true;
        case 0xF8BC:
            snprintf(name, name_size, "Index Copy Protection");
            return true;
        default:
            break;
    }
    
    /* By characteristics */
    if (analysis->is_long_track) {
        snprintf(name, name_size, "Long Track Protection");
        return true;
    }
    
    if (analysis->has_breakpoints) {
        snprintf(name, name_size, "Breakpoint Protection");
        return true;
    }
    
    if (analysis->sync.bit_shifted) {
        snprintf(name, name_size, "Bit-Shifted Sync Protection");
        return true;
    }
    
    if (!analysis->sectors.is_uniform && analysis->sectors.unique_lengths > 2) {
        snprintf(name, name_size, "Variable Sector Protection");
        return true;
    }
    
    return false;
}

/*===========================================================================
 * Reporting
 *===========================================================================*/

void uft_print_track_analysis(const uft_track_analysis_t *result) {
    if (!result) return;
    
    printf("=== Track Analysis ===\n");
    printf("Type:           %s\n", uft_track_type_name(result->type));
    printf("Platform:       %s\n", uft_platform_name(result->detected_platform));
    printf("Encoding:       %s\n", uft_encoding_name(result->detected_encoding));
    printf("Format:         %s\n", result->format_name);
    printf("Track Length:   %zu bytes\n", result->track_length);
    printf("Sync Pattern:   0x%04X (%d found)\n", 
           result->sync.primary_pattern, result->sync.count);
    printf("Sectors:        %d\n", result->sectors.sector_count);
    printf("Uniform:        %s (%.0f%%)\n",
           result->sectors.is_uniform ? "Yes" : "No",
           result->sectors.uniformity * 100);
    printf("Protected:      %s\n", result->is_protected ? "Yes" : "No");
    printf("Confidence:     %.0f%%\n", result->confidence * 100);
    
    if (result->protection_name[0]) {
        printf("Protection:     %s\n", result->protection_name);
    }
}

int uft_track_analysis_to_json(const uft_track_analysis_t *result,
                               char *buffer, size_t size) {
    if (!result || !buffer || size == 0) return -1;
    
    return snprintf(buffer, size,
        "{\n"
        "  \"type\": \"%s\",\n"
        "  \"platform\": \"%s\",\n"
        "  \"encoding\": \"%s\",\n"
        "  \"format\": \"%s\",\n"
        "  \"track_length\": %zu,\n"
        "  \"sync_pattern\": \"0x%04X\",\n"
        "  \"sync_count\": %d,\n"
        "  \"sector_count\": %d,\n"
        "  \"is_uniform\": %s,\n"
        "  \"uniformity\": %.2f,\n"
        "  \"is_protected\": %s,\n"
        "  \"is_long_track\": %s,\n"
        "  \"has_breakpoints\": %s,\n"
        "  \"confidence\": %.2f,\n"
        "  \"protection\": \"%s\"\n"
        "}",
        uft_track_type_name(result->type),
        uft_platform_name(result->detected_platform),
        uft_encoding_name(result->detected_encoding),
        result->format_name,
        result->track_length,
        result->sync.primary_pattern,
        result->sync.count,
        result->sectors.sector_count,
        result->sectors.is_uniform ? "true" : "false",
        result->sectors.uniformity,
        result->is_protected ? "true" : "false",
        result->is_long_track ? "true" : "false",
        result->has_breakpoints ? "true" : "false",
        result->confidence,
        result->protection_name[0] ? result->protection_name : "none"
    );
}
