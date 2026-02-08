/**
 * @file uft_a8rawconv_atari.h
 * @brief Atari 8-bit Disk Algorithms standard
 * 
 * raw disk conversion utility with:
 * - FM/MFM decoding for Atari
 * - Apple II GCR support
 * - Mac GCR support
 * - Sector interleaving calculation
 * - Write precompensation
 * - ATX/ATR/XFD format support
 * 
 * @copyright Adaptation for UFT: 2025
 */

#ifndef UFT_A8RAWCONV_ATARI_H
#define UFT_A8RAWCONV_ATARI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * ATARI DISK TIMING CONSTANTS
 *============================================================================*/

/**
 * @defgroup Atari_Timing Atari Disk Timing Constants
 * @{
 */

/** Nominal FM bitcell time at 360 RPM in 5ns ticks (4µs @ 288 RPM = 3.2µs @ 360 RPM) */
#define UFT_A8_NOMINAL_FM_BITCELL       640

/** Nominal Apple II GCR bitcell time at 360 RPM in 5ns ticks */
#define UFT_A8_NOMINAL_A2GCR_BITCELL    667

/** Atari disk clocks per second at 288 RPM */
#define UFT_A8_FM_CLOCKS_PER_SEC        250000

/** MFM clocks per second */
#define UFT_A8_MFM_CLOCKS_PER_SEC       500000

/** Atari nominal RPM */
#define UFT_A8_ATARI_RPM                288

/** PC nominal RPM */
#define UFT_A8_PC_RPM                   300

/** Samples per revolution at 360 RPM (5ns/tick) */
#define UFT_A8_SAMPLES_PER_REV          (200000000.0f / 6.0f)

/** @} */

/*============================================================================
 * CRC CALCULATION (Atari/WD1771/WD1772)
 *============================================================================*/

/**
 * @brief Calculate CRC-16-CCITT (x^16 + x^12 + x^5 + 1)
 * 
 * This is the standard floppy disk CRC used by WD1771/WD1772 controllers.
 * 
 * @param buf Pointer to data buffer
 * @param len Length of data
 * @param initial Initial CRC value (0xFFFF for start of IDAM/DAM)
 * @return Calculated CRC
 */
static inline uint16_t uft_a8_compute_crc(const uint8_t *buf, size_t len, 
                                           uint16_t initial) {
    uint16_t crc = initial;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t c = buf[i];
        crc ^= (uint16_t)c << 8;
        
        for (int j = 0; j < 8; j++) {
            uint16_t xorval = (crc & 0x8000) ? 0x1021 : 0;
            crc = (crc << 1) ^ xorval;
        }
    }
    
    return crc;
}

/**
 * @brief Calculate inverted CRC (for some protection schemes)
 * @param buf Pointer to data buffer
 * @param len Length of data
 * @param initial Initial CRC value
 * @return Calculated inverted CRC
 */
static inline uint16_t uft_a8_compute_inverted_crc(const uint8_t *buf, size_t len,
                                                    uint16_t initial) {
    uint16_t crc = initial;
    
    for (size_t i = 0; i < len; i++) {
        uint8_t c = ~buf[i];
        crc ^= (uint16_t)c << 8;
        
        for (int j = 0; j < 8; j++) {
            uint16_t xorval = (crc & 0x8000) ? 0x1021 : 0;
            crc = (crc << 1) ^ xorval;
        }
    }
    
    return crc;
}

/**
 * @brief Calculate simple byte sum checksum
 * @param buf Pointer to data buffer
 * @param len Length of data
 * @return Sum of all bytes
 */
static inline uint32_t uft_a8_compute_bytesum(const void *buf, size_t len) {
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t chk = 0;
    
    while (len--) {
        chk += *src++;
    }
    
    return chk;
}

/**
 * @brief Calculate address field CRC
 * @param track Track number
 * @param side Side number (0 or 1)
 * @param sector Sector number
 * @param sector_size Sector size in bytes
 * @param mfm true for MFM, false for FM
 * @return Calculated address CRC
 */
static inline uint16_t uft_a8_compute_address_crc(uint32_t track, uint32_t side,
                                                   uint32_t sector, 
                                                   uint32_t sector_size,
                                                   bool mfm) {
    uint8_t data[] = {
        0xA1, 0xA1, 0xA1, 0xFE,
        (uint8_t)track,
        (uint8_t)side,
        (uint8_t)sector,
        (uint8_t)(sector_size > 512 ? 3 : sector_size > 256 ? 2 : 
                  sector_size > 128 ? 1 : 0)
    };
    
    return mfm ? uft_a8_compute_crc(data, 8, 0xFFFF) : 
                 uft_a8_compute_crc(data + 3, 5, 0xFFFF);
}

/*============================================================================
 * INTERLEAVE CALCULATION
 *============================================================================*/

/**
 * @defgroup Interleave Sector Interleave Calculation
 * @{
 */

/** Interleave calculation modes */
typedef enum {
    UFT_A8_INTERLEAVE_AUTO,         /**< Automatic based on sector size */
    UFT_A8_INTERLEAVE_FORCE_AUTO,   /**< Force automatic calculation */
    UFT_A8_INTERLEAVE_NONE,         /**< No interleave (1:1) */
    UFT_A8_INTERLEAVE_XF551_DD_HS   /**< XF551 DD high-speed mode */
} uft_a8_interleave_mode_t;

/**
 * @brief Calculate sector timing positions with interleaving
 * 
 * This calculates optimal sector positions based on sector size and count.
 * - 128-byte formats: 9:1 interleave (SD) or 13:1 (ED)
 * - 256-byte formats: 15:1 interleave
 * - 512-byte formats: 1:1 interleave
 * 
 * Includes 8% track-to-track skew (~16.7ms).
 * 
 * @param timings Output array of sector timing positions (0.0-1.0)
 * @param sector_count Number of sectors
 * @param mfm true for MFM, false for FM
 * @param sector_size Sector size in bytes
 * @param track Track number (for skew calculation)
 * @param mode Interleave calculation mode
 */
static inline void uft_a8_compute_interleave(float *timings, int sector_count,
                                              bool mfm, int sector_size,
                                              int track,
                                              uft_a8_interleave_mode_t mode) {
    /* Initialize all timings to -1 (unset) */
    for (int i = 0; i < sector_count; i++) {
        timings[i] = -1.0f;
    }
    
    /* Calculate track-to-track skew (8%) */
    float t0 = 0.08f * track;
    float spacing = 0.98f / sector_count;
    
    /* Calculate interleave based on sector size */
    int interleave = 1;
    
    switch (mode) {
        case UFT_A8_INTERLEAVE_AUTO:
        case UFT_A8_INTERLEAVE_FORCE_AUTO:
            if (sector_size == 128) {
                /* 9:1 for 18spt (SD), 13:1 for 26spt (ED) */
                interleave = (sector_count + 1) / 2;
            } else if (sector_size == 256) {
                /* 15:1 interleave */
                interleave = (sector_count * 15 + 17) / 18;
            } else {
                /* No skew for 512-byte sectors */
                t0 = 0;
            }
            break;
            
        case UFT_A8_INTERLEAVE_NONE:
            interleave = 1;
            t0 = 0;
            break;
            
        case UFT_A8_INTERLEAVE_XF551_DD_HS:
            /* 9:1 interleave with 18 DD sectors */
            interleave = (sector_count + 1) / 2;
            break;
    }
    
    /* Mark occupied slots */
    bool *occupied = (bool *)__builtin_alloca(sector_count * sizeof(bool));
    for (int i = 0; i < sector_count; i++) {
        occupied[i] = false;
    }
    
    /* Assign timing positions */
    int slot_idx = 0;
    for (int i = 0; i < sector_count; i++) {
        /* Find next unoccupied slot */
        while (occupied[slot_idx]) {
            if (++slot_idx >= sector_count) {
                slot_idx = 0;
            }
        }
        
        occupied[slot_idx] = true;
        
        float t = t0 + spacing * (float)slot_idx;
        timings[i] = t - floorf(t);
        
        slot_idx += interleave;
        if (slot_idx >= sector_count) {
            slot_idx -= sector_count;
        }
    }
}

/** @} */

/*============================================================================
 * WRITE PRECOMPENSATION
 *============================================================================*/

/**
 * @defgroup Precompensation Write Precompensation
 * @{
 */

/** Post-compensation modes */
typedef enum {
    UFT_A8_POSTCOMP_NONE,       /**< No compensation */
    UFT_A8_POSTCOMP_AUTO,       /**< Automatic detection */
    UFT_A8_POSTCOMP_MAC800K     /**< Mac 800K disk compensation */
} uft_a8_postcomp_mode_t;

/**
 * @brief Apply post-write compensation for Mac 800K disks
 * 
 * Macintosh 800K disks have higher density on inner tracks and are
 * more susceptible to peak shift. This function pushes transitions
 * apart when they are too close together.
 * 
 * @param transitions Array of flux transition times
 * @param count Number of transitions
 * @param samples_per_rev Samples per revolution
 * @param phys_track Physical track number
 */
static inline void uft_a8_postcomp_mac800k(uint32_t *transitions, size_t count,
                                            float samples_per_rev, 
                                            int phys_track) {
    if (count < 3) return;
    
    uint32_t t0 = transitions[0];
    uint32_t t1 = transitions[1];
    
    /* Threshold calculation:
     * - Begin at ~1/45000th of a rotation
     * - Adjust for track circumference (inner tracks need less correction)
     */
    int thresh = (int)(0.5 + samples_per_rev / 30000.0 * 
                       (float)(160 + (phys_track < 47 ? phys_track : 47)) / 240.0f);
    
    for (size_t i = 2; i < count; i++) {
        uint32_t t2 = transitions[i];
        
        /* Compute deltas between adjacent pairs */
        int32_t t01 = (int32_t)(t1 - t0);
        int32_t t12 = (int32_t)(t2 - t1);
        
        /* Compute anti-peak-shift delta for narrow pairs */
        int32_t delta1 = (thresh - t01 > 0) ? thresh - t01 : 0;
        int32_t delta2 = (thresh - t12 > 0) ? thresh - t12 : 0;
        
        /* Apply correction, limited to half the distance */
        int32_t correction = ((delta2 - delta1) * 5) / 12;
        int32_t max_neg = -t01 / 2;
        int32_t max_pos = t12 / 2;
        
        if (correction < max_neg) correction = max_neg;
        if (correction > max_pos) correction = max_pos;
        
        transitions[i - 1] = t1 + correction;
        
        t0 = t1;
        t1 = t2;
    }
}

/**
 * @brief MFM write precompensation during encoding
 * 
 * Shifts flux transitions by 1/16th of a bit cell when adjacent
 * to other transitions:
 * - Close to prior transition: shift backwards
 * - Close to next transition: shift forwards
 * 
 * @param shifter 24-bit MFM shifter value
 * @param bitcell_time Bitcell time in ticks
 * @return Precompensation offset (signed, in ticks)
 */
static inline int32_t uft_a8_mfm_precomp(uint32_t shifter, uint32_t bitcell_time) {
    if (!(shifter & 0x8000)) {
        return 0;  /* No transition at current position */
    }
    
    uint32_t adjacent_bits = shifter & 0x22000;
    
    if (adjacent_bits == 0x20000) {
        /* Close to prior transition - shift backwards */
        return 0;
    } else if (adjacent_bits == 0x2000) {
        /* Close to next transition - shift forwards */
        return (int32_t)(bitcell_time >> 3);  /* 1/8 of bit cell */
    } else {
        /* No adjacent transitions - nominal position */
        return (int32_t)(bitcell_time >> 4);  /* 1/16 of bit cell */
    }
}

/** @} */

/*============================================================================
 * APPLE II 6&2 GCR ENCODING/DECODING
 *============================================================================*/

/**
 * @defgroup Apple2_GCR Apple II 6&2 GCR Encoding
 * @{
 */

/** Apple II GCR 6-bit to 8-bit encoder table */
static const uint8_t uft_a8_gcr6_encode[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/** Apple II GCR 8-bit to 6-bit decoder table (255 = invalid) */
static const uint8_t uft_a8_gcr6_decode[256] = {
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,
    255,255,255,255,255,255,  0,  1,255,255,  2,  3,255,  4,  5,  6,
    255,255,255,255,255,255,  7,  8,255,255,  8,  9, 10, 11, 12, 13,
    255,255, 14, 15, 16, 17, 18, 19,255, 20, 21, 22, 23, 24, 25, 26,
    255,255,255,255,255,255,255,255,255,255,255, 27,255, 28, 29, 30,
    255,255,255, 31,255,255, 32, 33,255, 34, 35, 36, 37, 38, 39, 40,
    255,255,255,255,255, 41, 42, 43,255, 44, 45, 46, 47, 48, 49, 50,
    255,255, 51, 52, 53, 54, 55, 56,255, 57, 58, 59, 60, 61, 62, 63
};

/** Apple II sector sync patterns */
#define UFT_A8_APPLE2_SYNC_ADDR     0xD5AA96    /**< Address field prologue */
#define UFT_A8_APPLE2_SYNC_DATA     0xD5AAAD    /**< Data field prologue */
#define UFT_A8_APPLE2_EPILOGUE      0xDEAAEB    /**< Standard epilogue */

/**
 * @brief Prenibble Apple II sector data using 6&2 encoding
 * 
 * Converts 256 bytes of sector data to 343 bytes of GCR-encoded data.
 * The encoding splits each byte into 6 high bits and 2 low bits,
 * then recombines them with checksums.
 * 
 * @param output Output buffer (343 bytes)
 * @param input Input sector data (256 bytes)
 */
static inline void uft_a8_apple2_prenibble(uint8_t *output, const uint8_t *input) {
    output[0] = 0;  /* Initial accumulator */
    
    /* Prenibble whole fragment bytes (0-83) */
    for (int j = 0; j < 84; j++) {
        const uint8_t a = input[j] & 3;
        const uint8_t b = input[j + 86] & 3;
        const uint8_t c = input[j + 172] & 3;
        const uint8_t v = a + (b << 2) + (c << 4);
        
        output[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
    }
    
    /* Prenibble partial fragment bytes (84-85) */
    for (int j = 84; j < 86; j++) {
        const uint8_t a = input[j] & 3;
        const uint8_t b = input[j + 86] & 3;
        const uint8_t v = a + (b << 2);
        
        output[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
    }
    
    /* Prenibble base bits 2-7 */
    for (int j = 0; j < 256; j++) {
        output[j + 87] = input[j] >> 2;
    }
    
    output[343] = 0;  /* Final checksum placeholder */
}

/**
 * @brief Encode prenibbled data with adjacent XOR and GCR
 * @param output Output buffer (343 GCR bytes)
 * @param nibbles Input prenibbled data (344 bytes)
 */
static inline void uft_a8_apple2_encode_gcr(uint8_t *output, const uint8_t *nibbles) {
    for (int j = 0; j < 343; j++) {
        output[j] = uft_a8_gcr6_encode[nibbles[j] ^ nibbles[j + 1]];
    }
}

/** @} */

/*============================================================================
 * ATARI FORMAT STRUCTURES
 *============================================================================*/

/**
 * @defgroup Atari_Formats Atari Disk Format Definitions
 * @{
 */

/** Atari sector sizes */
typedef enum {
    UFT_A8_SECTOR_128 = 128,    /**< Single density */
    UFT_A8_SECTOR_256 = 256,    /**< Enhanced/double density */
    UFT_A8_SECTOR_512 = 512     /**< High speed */
} uft_a8_sector_size_t;

/** Atari disk densities */
typedef enum {
    UFT_A8_DENSITY_SD,          /**< Single density (FM, 128 bytes) */
    UFT_A8_DENSITY_ED,          /**< Enhanced density (FM, 128 bytes, 26 sectors) */
    UFT_A8_DENSITY_DD,          /**< Double density (MFM, 256 bytes) */
    UFT_A8_DENSITY_QD,          /**< Quad density (MFM, 256 bytes, 77 tracks) */
    UFT_A8_DENSITY_HD           /**< High density (MFM, 512 bytes) */
} uft_a8_density_t;

/** ATR file header structure */
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;             /**< 0x0296 (NICKATARI) */
    uint16_t paragraphs;        /**< Size in 16-byte paragraphs (low) */
    uint16_t sector_size;       /**< Sector size (128, 256, 512) */
    uint8_t paragraphs_high;    /**< Size in 16-byte paragraphs (high) */
    uint8_t crc;                /**< CRC (optional) */
    uint32_t flags;             /**< Flags */
    uint8_t reserved[4];        /**< Reserved */
} uft_a8_atr_header_t;

/** ATR magic number */
#define UFT_A8_ATR_MAGIC    0x0296

/** ATX file header structure */
#pragma pack(push, 1)
typedef struct {
    uint8_t signature[4];       /**< "AT8X" */
    uint16_t version;           /**< Format version */
    uint16_t min_version;       /**< Minimum reader version */
    uint16_t creator;           /**< Creator ID */
    uint16_t creator_version;   /**< Creator version */
    uint32_t flags;             /**< Flags */
    uint16_t image_type;        /**< Image type */
    uint8_t density;            /**< Density */
    uint8_t reserved1;          /**< Reserved */
    uint32_t image_id;          /**< Image ID */
    uint16_t image_version;     /**< Image version */
    uint16_t reserved2;         /**< Reserved */
    uint32_t start_data;        /**< Offset to first track record */
    uint32_t end_data;          /**< Offset to end of data */
    uint8_t reserved3[12];      /**< Reserved */
} uft_a8_atx_header_t;

/** ATX signature */
#define UFT_A8_ATX_SIGNATURE    "AT8X"

/** @} */

/*============================================================================
 * FM/MFM BIT EXPANSION FOR ENCODING
 *============================================================================*/

/**
 * @brief 4-bit to 8-bit expansion table for MFM encoding
 * Each 4-bit value expands to 8 bits with data bits at even positions
 */
static const uint8_t uft_a8_expand4[16] = {
    0b00000000,  /* 0x0 */
    0b00000001,  /* 0x1 */
    0b00000100,  /* 0x2 */
    0b00000101,  /* 0x3 */
    0b00010000,  /* 0x4 */
    0b00010001,  /* 0x5 */
    0b00010100,  /* 0x6 */
    0b00010101,  /* 0x7 */
    0b01000000,  /* 0x8 */
    0b01000001,  /* 0x9 */
    0b01000100,  /* 0xA */
    0b01000101,  /* 0xB */
    0b01010000,  /* 0xC */
    0b01010001,  /* 0xD */
    0b01010100,  /* 0xE */
    0b01010101   /* 0xF */
};

/**
 * @brief Expand byte to MFM data bits (no clocks)
 * @param byte Input byte
 * @return 16-bit value with data bits at odd positions
 */
static inline uint16_t uft_a8_mfm_expand_data(uint8_t byte) {
    return ((uint16_t)uft_a8_expand4[byte >> 4] << 8) | 
           uft_a8_expand4[byte & 0x0F];
}

/**
 * @brief Add MFM clock bits to expanded data
 * @param data Expanded data bits
 * @param prev_bit Previous data bit (for clock calculation)
 * @return 16-bit MFM encoded value with clocks
 */
static inline uint16_t uft_a8_mfm_add_clocks(uint16_t data, bool prev_bit) {
    /* Clock bit is 1 only between two 0 data bits */
    uint16_t with_prev = data | (prev_bit ? 0x8000 : 0);
    uint16_t clocks = ~(with_prev | (data >> 1)) & 0x5555;
    return data | (clocks << 1);
}

/*============================================================================
 * WEAK SECTOR GENERATION
 *============================================================================*/

/**
 * @brief Generate weak/fuzzy byte for FM encoding
 * 
 * Creates a deliberately unstable flux pattern that reads differently
 * each time. Used for copy protection.
 * 
 * @param stream Output flux transition times array
 * @param time Current time position (updated)
 * @param bitcell_time Nominal bitcell time
 * @return Number of transitions written
 */
static inline int uft_a8_encode_weak_fm(uint32_t *stream, uint32_t *time,
                                         uint32_t bitcell_time) {
    int count = 0;
    
    for (int i = 0; i < 5; i++) {
        stream[count++] = *time;
        *time += (bitcell_time * 3) >> 1;
        
        stream[count++] = *time;
        *time += (bitcell_time * 3 + 1) >> 1;
    }
    
    *time += bitcell_time;
    return count;
}

/**
 * @brief Generate weak/fuzzy byte for MFM encoding
 * 
 * @param stream Output flux transition times array
 * @param time Current time position (updated)
 * @param bitcell_time Nominal bitcell time
 * @return Number of transitions written
 */
static inline int uft_a8_encode_weak_mfm(uint32_t *stream, uint32_t *time,
                                          uint32_t bitcell_time) {
    int count = 0;
    
    for (int i = 0; i < 5; i++) {
        stream[count++] = *time;
        *time += (bitcell_time * 3) >> 1;
        
        stream[count++] = *time;
        *time += (bitcell_time * 3 + 1) >> 1;
    }
    
    *time += bitcell_time;
    return count;
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_A8RAWCONV_ATARI_H */
