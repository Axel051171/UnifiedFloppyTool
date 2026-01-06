/**
 * @file uft_a8rawconv_algorithms.h
 * @brief Atari 8-bit algorithms from a8rawconv
 * 
 * Source: a8rawconv by Avery Lee (Phaeron)
 * https://github.com/atari800/atari800/tree/master/tools/a8rawconv
 * 
 * Key algorithms:
 * - FM/MFM sector parsing with WD1771/WD1772 compatibility
 * - Apple II GCR 6&2 encoding/decoding
 * - Write precompensation (anti-peak-shift)
 * - Interleave calculation
 * - SuperCardPro device communication
 * - Flux stream processing
 * 
 * License: GPL v2 (a8rawconv)
 */

#ifndef UFT_A8RAWCONV_ALGORITHMS_H
#define UFT_A8RAWCONV_ALGORITHMS_H

#include &lt;stdint.h&gt;
#include &lt;stdbool.h&gt;
#include &lt;stddef.h&gt;
#include &lt;math.h&gt;

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * TIMING CONSTANTS
 *============================================================================*/

/**
 * Nominal bitcell times at 5ns/tick @ 360 RPM
 * a8rawconv uses 5ns resolution for compatibility with both
 * KryoFlux (40ns) and SuperCardPro (25ns)
 */
#define UFT_A8_FM_BITCELL_TIME      640     /* 4µs @ 288 RPM = 3.2µs @ 360 RPM */
#define UFT_A8_A2GCR_BITCELL_TIME   667     /* 4µs @ 300 RPM = 3.33µs @ 360 RPM */

/**
 * Samples per revolution at 360 RPM with 5ns ticks
 * 200000000 / 6 = 33,333,333 samples
 */
#define UFT_A8_SAMPLES_PER_REV_360  (200000000.0f / 6.0f)

/*============================================================================
 * CRC CALCULATION (ATARI-COMPATIBLE)
 *============================================================================*/

/**
 * CRC-CCITT calculation (x^16 + x^12 + x^5 + 1)
 * 
 * @param buf        Input data
 * @param len        Data length
 * @param initial    Initial CRC value (usually 0xFFFF)
 * @return           Calculated CRC
 */
static inline uint16_t uft_a8_compute_crc(const uint8_t *buf, size_t len, 
                                           uint16_t initial) {
    uint16_t crc = initial;
    
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)buf[i] << 8;
        
        for (int j = 0; j < 8; j++) {
            uint16_t xorval = (crc & 0x8000) ? 0x1021 : 0;
            crc = (crc << 1) ^ xorval;
        }
    }
    
    return crc;
}

/**
 * CRC with inverted input bytes (for some protection schemes)
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
 * Simple byte sum checksum
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
 * Compute address field CRC for FM/MFM
 */
static inline uint16_t uft_a8_compute_address_crc(uint32_t track, uint32_t side,
                                                   uint32_t sector, uint32_t size,
                                                   bool mfm) {
    uint8_t data[8];
    
    /* MFM includes 3x 0xA1 sync bytes */
    data[0] = 0xA1;
    data[1] = 0xA1;
    data[2] = 0xA1;
    data[3] = 0xFE;  /* IDAM */
    data[4] = (uint8_t)track;
    data[5] = (uint8_t)side;
    data[6] = (uint8_t)sector;
    data[7] = (uint8_t)(size > 512 ? 3 : size > 256 ? 2 : size > 128 ? 1 : 0);
    
    return mfm ? uft_a8_compute_crc(data, 8, 0xFFFF) 
               : uft_a8_compute_crc(data + 3, 5, 0xFFFF);
}

/*============================================================================
 * APPLE II GCR 6&2 ENCODING
 *============================================================================*/

/**
 * GCR 6&2 encoder table (64 entries)
 * Maps 6-bit values to 8-bit disk bytes
 */
static const uint8_t UFT_A8_GCR6_ENCODE[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/**
 * Prenibble a 256-byte sector to 343 GCR bytes
 * This is the Apple II 6&2 encoding process
 * 
 * @param src        Source sector data (256 bytes)
 * @param dst        Destination nibble buffer (343 bytes)
 */
static inline void uft_a8_prenibble_6and2(const uint8_t *src, uint8_t *dst) {
    /* First byte is checksum seed (0) */
    dst[0] = 0;
    
    /* Prenibble whole fragment bytes (0-83) */
    for (int j = 0; j < 84; j++) {
        uint8_t a = src[j] & 3;
        uint8_t b = src[j + 86] & 3;
        uint8_t c = src[j + 172] & 3;
        uint8_t v = a + (b << 2) + (c << 4);
        
        dst[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
    }
    
    /* Prenibble partial fragment bytes (84-85) */
    for (int j = 84; j < 86; j++) {
        uint8_t a = src[j] & 3;
        uint8_t b = src[j + 86] & 3;
        uint8_t v = a + (b << 2);
        
        dst[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
    }
    
    /* Prenibble base bits 2-7 */
    for (int j = 0; j < 256; j++) {
        dst[j + 87] = src[j] >> 2;
    }
    
    dst[343] = 0; /* Final checksum byte position */
}

/**
 * Apply adjacent XOR encoding and convert to GCR
 * 
 * @param nibbles    Prenibbled data (343 bytes)
 * @param gcr        Output GCR encoded data (343 bytes)
 */
static inline void uft_a8_encode_gcr_6and2(const uint8_t *nibbles, uint8_t *gcr) {
    for (int j = 0; j < 343; j++) {
        gcr[j] = UFT_A8_GCR6_ENCODE[nibbles[j] ^ nibbles[j + 1]];
    }
}

/*============================================================================
 * SECTOR ENCODER
 *============================================================================*/

/**
 * Sector encoder state for generating flux transitions
 */
typedef struct {
    uint32_t *stream;       /* Output flux transition times */
    size_t stream_size;     /* Current stream size */
    size_t stream_capacity; /* Allocated capacity */
    
    uint32_t time;          /* Current time position */
    uint32_t bitcell_time;  /* Bitcell period in ticks */
    uint32_t mfm_shifter;   /* MFM encoding shift register */
    
    uint32_t critical_start; /* Start of critical region */
    uint32_t critical_end;   /* End of critical region */
    
    bool precomp_enabled;   /* Write precompensation enabled */
} uft_a8_encoder_t;

/**
 * Initialize encoder
 */
static inline void uft_a8_encoder_init(uft_a8_encoder_t *enc, 
                                        uint32_t *stream, size_t capacity,
                                        uint32_t bitcell_time) {
    enc->stream = stream;
    enc->stream_size = 0;
    enc->stream_capacity = capacity;
    enc->time = 0;
    enc->bitcell_time = bitcell_time;
    enc->mfm_shifter = 0;
    enc->critical_start = ~0U;
    enc->critical_end = ~0U;
    enc->precomp_enabled = false;
}

/**
 * Add flux transition at current time
 */
static inline void uft_a8_encoder_add_flux(uft_a8_encoder_t *enc, uint32_t offset) {
    if (enc->stream_size < enc->stream_capacity) {
        enc->stream[enc->stream_size++] = enc->time + offset;
    }
}

/**
 * Encode FM byte with clock
 */
static inline void uft_a8_encode_fm_byte(uft_a8_encoder_t *enc, 
                                          uint8_t clock, uint8_t data) {
    for (int i = 0; i < 8; i++) {
        if (clock & 0x80) {
            uft_a8_encoder_add_flux(enc, 0);
        }
        if (data & 0x80) {
            uft_a8_encoder_add_flux(enc, enc->bitcell_time);
        }
        
        clock <<= 1;
        data <<= 1;
        enc->time += enc->bitcell_time * 2;
    }
}

/**
 * 4-bit expansion table for MFM encoding
 */
static const uint8_t UFT_A8_EXPAND4[16] = {
    0b00000000, 0b00000001, 0b00000100, 0b00000101,
    0b00010000, 0b00010001, 0b00010100, 0b00010101,
    0b01000000, 0b01000001, 0b01000100, 0b01000101,
    0b01010000, 0b01010001, 0b01010100, 0b01010101
};

/**
 * Encode MFM byte with write precompensation
 * 
 * @param enc        Encoder state
 * @param clock_mask Clock bit mask (usually 0xFF)
 * @param data       Data byte to encode
 */
static inline void uft_a8_encode_mfm_byte(uft_a8_encoder_t *enc,
                                           uint8_t clock_mask, uint8_t data) {
    /* Expand data bits using lookup table */
    enc->mfm_shifter = (enc->mfm_shifter & 0xFF0000) + 
                       (UFT_A8_EXPAND4[data >> 4] << 8) + 
                       UFT_A8_EXPAND4[data & 0x0F];
    
    /* Compute clock bits */
    uint32_t clock32 = (UFT_A8_EXPAND4[clock_mask >> 4] << 8) + 
                       UFT_A8_EXPAND4[clock_mask & 0x0F];
    
    /* MFM rule: clock bit only if no adjacent data bits */
    enc->mfm_shifter += ~((enc->mfm_shifter << 1) | (enc->mfm_shifter >> 1)) 
                        & (clock32 << 1);
    
    /* Output with optional precompensation */
    if (enc->precomp_enabled) {
        for (int i = 0; i < 16; i++) {
            if (enc->mfm_shifter & 0x8000) {
                uint32_t adjacent = enc->mfm_shifter & 0x22000;
                uint32_t offset;
                
                if (adjacent == 0x20000) {
                    /* Close to prior transition: shift backwards 1/16 bitcell */
                    offset = 0;
                } else if (adjacent == 0x2000) {
                    /* Close to next transition: shift forwards 1/16 bitcell */
                    offset = enc->bitcell_time >> 3;
                } else {
                    /* No adjacent transitions: nominal delay */
                    offset = enc->bitcell_time >> 4;
                }
                
                uft_a8_encoder_add_flux(enc, offset);
            }
            
            enc->mfm_shifter <<= 1;
            enc->time += enc->bitcell_time;
        }
    } else {
        for (int i = 0; i < 16; i++) {
            if (enc->mfm_shifter & 0x8000) {
                uft_a8_encoder_add_flux(enc, 0);
            }
            enc->mfm_shifter <<= 1;
            enc->time += enc->bitcell_time;
        }
    }
}

/**
 * Encode GCR byte (Apple II style)
 */
static inline void uft_a8_encode_gcr_byte(uft_a8_encoder_t *enc, uint8_t data) {
    for (int i = 0; i < 8; i++) {
        if (data & 0x80) {
            uft_a8_encoder_add_flux(enc, 0);
        }
        data <<= 1;
        enc->time += enc->bitcell_time;
    }
}

/**
 * Encode GCR sync byte (Apple II)
 * Writes 0xFF followed by 2-bit slip
 */
static inline void uft_a8_encode_gcr_sync(uft_a8_encoder_t *enc) {
    uft_a8_encode_gcr_byte(enc, 0xFF);
    enc->time += enc->bitcell_time * 2;  /* Slip 2 bits */
}

/**
 * Encode weak/random bits (for copy protection)
 */
static inline void uft_a8_encode_weak_fm(uft_a8_encoder_t *enc) {
    for (int i = 0; i < 5; i++) {
        uft_a8_encoder_add_flux(enc, enc->bitcell_time);
        enc->time += (enc->bitcell_time * 3) >> 1;
        
        uft_a8_encoder_add_flux(enc, enc->bitcell_time);
        enc->time += (enc->bitcell_time * 3 + 1) >> 1;
    }
    enc->time += enc->bitcell_time;
}

/*============================================================================
 * WRITE PRECOMPENSATION (ANTI-PEAK-SHIFT)
 *============================================================================*/

/**
 * Apply post-compensation to flux transitions (Mac 800K style)
 * This combats peak shift effects on high-density recordings.
 * 
 * The algorithm pushes transitions apart when they are closer than
 * a threshold, with increasing effect as they get closer together.
 * 
 * @param transitions    Array of flux transition times
 * @param count          Number of transitions
 * @param samples_per_rev Samples per revolution
 * @param phys_track     Physical track number (for density adjustment)
 */
static inline void uft_a8_postcomp_mac800k(uint32_t *transitions, size_t count,
                                            float samples_per_rev, int phys_track) {
    if (count < 3) return;
    
    /*
     * Calculate threshold based on track position.
     * Standard 2µs MFM has minimum 4µs spacing at 300 RPM = 1/50000 rotation.
     * Mac 800K tracks 0-15 have 2µs spacing at 394 RPM = 1/76142 rotation.
     * 
     * Linear mapping with clamp for inner tracks (above track 47).
     */
    int min_track = (phys_track < 47) ? phys_track : 47;
    int thresh = (int)(0.5f + samples_per_rev / 30000.0f * 
                       (float)(160 + min_track) / 240.0f);
    
    uint32_t t0 = transitions[0];
    uint32_t t1 = transitions[1];
    
    for (size_t i = 2; i < count; i++) {
        uint32_t t2 = transitions[i];
        
        /* Compute deltas between each pair */
        int32_t t01 = (int32_t)(t1 - t0);
        int32_t t12 = (int32_t)(t2 - t1);
        
        /* Anti peak-shift delta for narrow pairs */
        int32_t delta1 = (thresh - t01 > 0) ? (thresh - t01) : 0;
        int32_t delta2 = (thresh - t12 > 0) ? (thresh - t12) : 0;
        
        /* Apply correction, limited to half the distance */
        int32_t correction = ((delta2 - delta1) * 5) / 12;
        int32_t max_back = -t01 / 2;
        int32_t max_fwd = t12 / 2;
        
        if (correction < max_back) correction = max_back;
        if (correction > max_fwd) correction = max_fwd;
        
        transitions[i - 1] = t1 + correction;
        
        t0 = t1;
        t1 = t2;
    }
}

/*============================================================================
 * INTERLEAVE CALCULATION
 *============================================================================*/

typedef enum {
    UFT_A8_INTERLEAVE_AUTO,
    UFT_A8_INTERLEAVE_FORCE_AUTO,
    UFT_A8_INTERLEAVE_NONE,
    UFT_A8_INTERLEAVE_XF551_DD_HS
} uft_a8_interleave_mode_t;

/**
 * Calculate sector timing positions based on interleave
 * 
 * @param timings      Output array of sector positions (0.0-1.0)
 * @param sector_count Number of sectors per track
 * @param sector_size  Sector size in bytes
 * @param track        Track number (for skew calculation)
 * @param mfm          True for MFM, false for FM
 * @param mode         Interleave mode
 */
static inline void uft_a8_compute_interleave(float *timings, int sector_count,
                                              int sector_size, int track,
                                              bool mfm,
                                              uft_a8_interleave_mode_t mode) {
    /* Initialize all positions to invalid */
    for (int i = 0; i < sector_count; i++) {
        timings[i] = -1.0f;
    }
    
    /* Track-to-track skew (~8% = 16.7ms rotation) */
    float t0 = 0.08f * track;
    float spacing = 0.98f / sector_count;
    
    /*
     * Interleave ratios by sector size:
     * 128-byte: 9:1 for SD (18spt), 13:1 for ED (26spt)
     * 256-byte: 15:1
     * 512-byte: 1:1 (no interleave needed)
     */
    int interleave = 1;
    
    switch (mode) {
        case UFT_A8_INTERLEAVE_AUTO:
        case UFT_A8_INTERLEAVE_FORCE_AUTO:
            if (sector_size == 128) {
                interleave = (sector_count + 1) / 2;
            } else if (sector_size == 256) {
                interleave = (sector_count * 15 + 17) / 18;
            } else {
                t0 = 0;  /* No skew for 512-byte sectors */
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
    
    /* Generate positions using interleave pattern */
    bool *occupied = (bool *)__builtin_alloca(sector_count * sizeof(bool));
    for (int i = 0; i < sector_count; i++) occupied[i] = false;
    
    int slot = 0;
    for (int i = 0; i < sector_count; i++) {
        /* Find next free slot */
        while (occupied[slot]) {
            if (++slot >= sector_count) slot = 0;
        }
        
        occupied[slot] = true;
        
        float t = t0 + spacing * (float)slot;
        timings[i] = t - floorf(t);
        
        slot += interleave;
        if (slot >= sector_count) slot -= sector_count;
    }
}

/*============================================================================
 * DISK STRUCTURE DEFINITIONS
 *============================================================================*/

#define UFT_A8_MAX_PHYS_TRACKS  84

/**
 * Sector information structure
 */
typedef struct {
    uint32_t raw_start;         /* Start position in raw stream */
    uint32_t raw_end;           /* End position in raw stream */
    float position;             /* Position in rotation (0.0-1.0) */
    float ending_position;      /* Ending position */
    
    int index;                  /* Sector number */
    int weak_offset;            /* First byte of weak data (-1 = none) */
    uint32_t sector_size;       /* Sector data size in bytes */
    
    bool mfm;                   /* True for MFM, false for FM */
    uint8_t address_mark;       /* DAM byte (F8-FB) or volume (GCR) */
    
    uint16_t recorded_addr_crc;
    uint16_t computed_addr_crc;
    uint32_t recorded_crc;
    uint32_t computed_crc;
    
    uint8_t data[1024];         /* Sector data */
} uft_a8_sector_t;

/**
 * Track information structure
 */
typedef struct {
    uft_a8_sector_t *sectors;
    size_t sector_count;
    size_t sector_capacity;
    
    uint8_t *gcr_data;          /* Raw GCR data for Apple II */
    size_t gcr_data_size;
} uft_a8_track_t;

/**
 * Raw track structure (flux data)
 */
typedef struct {
    int phys_track;             /* Physical track (96 tpi spacing) */
    int side;
    float samples_per_rev;
    
    int32_t splice_start;
    int32_t splice_end;
    
    uint32_t *transitions;
    size_t transition_count;
    
    uint32_t *index_times;
    size_t index_count;
} uft_a8_raw_track_t;

/**
 * Disk geometry structure
 */
typedef struct {
    int track_count;            /* Logical tracks (typically 40) */
    int track_step;             /* 1 for 96tpi, 2 for 48tpi */
    int side_count;             /* 1 or 2 */
    
    int primary_sector_size;
    int primary_sectors_per_track;
    
    bool synthesized;           /* True if generated from decoded data */
} uft_a8_disk_info_t;

/*============================================================================
 * SUPERCARD PRO INTERFACE
 *============================================================================*/

/* SCP command status codes */
#define UFT_A8_SCP_OK               0x4F
#define UFT_A8_SCP_BAD_COMMAND      0x01
#define UFT_A8_SCP_COMMAND_ERROR    0x02
#define UFT_A8_SCP_CHECKSUM_ERROR   0x03
#define UFT_A8_SCP_USB_TIMEOUT      0x04
#define UFT_A8_SCP_NO_TRACK0        0x05
#define UFT_A8_SCP_NO_DRIVE         0x06
#define UFT_A8_SCP_NO_MOTOR         0x07
#define UFT_A8_SCP_NOT_READY        0x08
#define UFT_A8_SCP_NO_INDEX         0x09
#define UFT_A8_SCP_ZERO_REVS        0x0A
#define UFT_A8_SCP_READ_TOO_LONG    0x0B
#define UFT_A8_SCP_INVALID_LENGTH   0x0C
#define UFT_A8_SCP_BOUNDARY_ODD     0x0E
#define UFT_A8_SCP_WRITE_PROTECTED  0x0F
#define UFT_A8_SCP_RAM_TEST_FAIL    0x10
#define UFT_A8_SCP_NO_DISK          0x11
#define UFT_A8_SCP_BAD_BAUD_RATE    0x12
#define UFT_A8_SCP_BAD_PORT_CMD     0x13

/**
 * SCP checksum calculation
 */
static inline uint8_t uft_a8_scp_checksum(const uint8_t *src, size_t len) {
    uint8_t chk = 0x4A;
    while (len--) {
        chk += *src++;
    }
    return chk;
}

/**
 * Get error string for SCP status code
 */
static inline const char *uft_a8_scp_error_string(uint8_t code) {
    switch (code) {
        case UFT_A8_SCP_BAD_COMMAND:      return "bad command";
        case UFT_A8_SCP_COMMAND_ERROR:    return "command error";
        case UFT_A8_SCP_CHECKSUM_ERROR:   return "packet checksum error";
        case UFT_A8_SCP_USB_TIMEOUT:      return "USB timeout";
        case UFT_A8_SCP_NO_TRACK0:        return "track 0 not found";
        case UFT_A8_SCP_NO_DRIVE:         return "no drive selected";
        case UFT_A8_SCP_NO_MOTOR:         return "motor not enabled";
        case UFT_A8_SCP_NOT_READY:        return "drive not ready";
        case UFT_A8_SCP_NO_INDEX:         return "no index pulse detected";
        case UFT_A8_SCP_ZERO_REVS:        return "zero revolutions chosen";
        case UFT_A8_SCP_READ_TOO_LONG:    return "read too long";
        case UFT_A8_SCP_INVALID_LENGTH:   return "invalid length";
        case UFT_A8_SCP_BOUNDARY_ODD:     return "location boundary is odd";
        case UFT_A8_SCP_WRITE_PROTECTED:  return "disk write protected";
        case UFT_A8_SCP_RAM_TEST_FAIL:    return "RAM test failed";
        case UFT_A8_SCP_NO_DISK:          return "no disk in drive";
        case UFT_A8_SCP_BAD_BAUD_RATE:    return "bad baud rate selected";
        case UFT_A8_SCP_BAD_PORT_CMD:     return "bad command for selected port";
        default: return "unknown error";
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_A8RAWCONV_ALGORITHMS_H */
