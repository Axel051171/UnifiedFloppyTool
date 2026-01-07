/**
 * @file uft_uft_sam_algorithms.h
 * @brief SamDisk algorithms extracted for UFT integration
 * 
 * This is the ORIGINAL REFERENCE implementation for the PLL algorithm
 * 
 * Key algorithms:
 * - Original PLL (Phase-Locked Loop) flux decoder
 * - CRC16-CCITT with lookup table
 * - Format definitions (IBM, Amiga, Apple, C64, Victor 9000)
 * - Copy protection detection (Speedlock, Rainbow Arts, KBI)
 * - Bitstream processing utilities
 * - Track layout and gap calculations
 * 
 * License: MIT (SamDisk)
 */

#ifndef UFT_UFT_SAM_ALGORITHMS_H
#define UFT_UFT_SAM_ALGORITHMS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * PLL CONSTANTS - ORIGINAL SAMDISK/KEIR FRASER VALUES
 *============================================================================*/

/**
 * Default PLL parameters from SamDisk FluxDecoder.h
 * These are the authoritative reference values!
 */
#define UFT_SD_DEFAULT_PLL_ADJUST   4       /* Clock adjustment percentage */
#define UFT_SD_DEFAULT_PLL_PHASE    60      /* Phase adjustment percentage */
#define UFT_SD_MAX_PLL_ADJUST       50      /* Maximum adjust range */
#define UFT_SD_MAX_PLL_PHASE        90      /* Maximum phase range */

/**
 * Jitter compensation for motor speed variation
 */
#define UFT_SD_JITTER_PERCENT       2       /* ±2% speed variation */

/*============================================================================
 * DATA RATES AND TIMING
 *============================================================================*/

typedef enum {
    UFT_SD_DATARATE_UNKNOWN = 0,
    UFT_SD_DATARATE_250K    = 250000,   /* DD 3.5" / DD 5.25" */
    UFT_SD_DATARATE_300K    = 300000,   /* DD 5.25" at 360 RPM */
    UFT_SD_DATARATE_500K    = 500000,   /* HD 3.5" / HD 5.25" */
    UFT_SD_DATARATE_1M      = 1000000   /* ED 3.5" */
} uft_sd_datarate_t;

typedef enum {
    UFT_SD_ENCODING_UNKNOWN = 0,
    UFT_SD_ENCODING_FM,
    UFT_SD_ENCODING_MFM,
    UFT_SD_ENCODING_AMIGA,
    UFT_SD_ENCODING_GCR,        /* Commodore 64 */
    UFT_SD_ENCODING_APPLE,      /* Apple II 6&2 GCR */
    UFT_SD_ENCODING_VICTOR,     /* Victor 9000 GCR */
    UFT_SD_ENCODING_ACE,        /* Jupiter Ace */
    UFT_SD_ENCODING_MX,         /* DVK MX */
    UFT_SD_ENCODING_AGAT,       /* Soviet Apple II clone */
    UFT_SD_ENCODING_VISTA,
    UFT_SD_ENCODING_RX02        /* DEC RX02 */
} uft_sd_encoding_t;

/**
 * Calculate bitcell time in nanoseconds from data rate
 */
static inline int uft_sd_bitcell_ns(uft_sd_datarate_t datarate) {
    switch (datarate) {
        case UFT_SD_DATARATE_250K: return 4000;   /* 4µs */
        case UFT_SD_DATARATE_300K: return 3333;   /* 3.33µs */
        case UFT_SD_DATARATE_500K: return 2000;   /* 2µs */
        case UFT_SD_DATARATE_1M:   return 1000;   /* 1µs */
        default: return 4000;
    }
}

/*============================================================================
 * PLL STRUCTURE - ORIGINAL SAMDISK IMPLEMENTATION
 *============================================================================*/

/**
 * PLL state structure based on SamDisk FluxDecoder
 */
typedef struct {
    /* Clock parameters (in nanoseconds) */
    int clock;              /* Current clock period */
    int clock_centre;       /* Nominal clock period */
    int clock_min;          /* Minimum allowed clock */
    int clock_max;          /* Maximum allowed clock */
    
    /* Phase tracking */
    int flux;               /* Accumulated flux time since last bit */
    int clocked_zeros;      /* Count of consecutive zero bits */
    int goodbits;           /* Count of good bits since last sync loss */
    
    /* Configuration */
    int flux_scale_percent; /* Flux scaling (100 = nominal) */
    int pll_adjust;         /* Clock adjustment percentage */
    int pll_phase;          /* Phase adjustment percentage */
    
    /* Status flags */
    bool index;             /* Index pulse detected */
    bool sync_lost;         /* Sync loss detected */
} uft_sd_pll_t;

/**
 * Initialize PLL with SamDisk defaults
 * 
 * @param pll       PLL state to initialize
 * @param bitcell_ns Nominal bitcell time in nanoseconds
 */
static inline void uft_sd_pll_init(uft_sd_pll_t *pll, int bitcell_ns) {
    pll->clock = bitcell_ns;
    pll->clock_centre = bitcell_ns;
    pll->clock_min = bitcell_ns * (100 - UFT_SD_DEFAULT_PLL_ADJUST) / 100;
    pll->clock_max = bitcell_ns * (100 + UFT_SD_DEFAULT_PLL_ADJUST) / 100;
    pll->flux = 0;
    pll->clocked_zeros = 0;
    pll->goodbits = 0;
    pll->flux_scale_percent = 100;
    pll->pll_adjust = UFT_SD_DEFAULT_PLL_ADJUST;
    pll->pll_phase = UFT_SD_DEFAULT_PLL_PHASE;
    pll->index = false;
    pll->sync_lost = false;
}

/**
 * Initialize PLL with custom parameters
 */
static inline void uft_sd_pll_init_ex(uft_sd_pll_t *pll, int bitcell_ns,
                                       int flux_scale, int pll_adjust) {
    uft_sd_pll_init(pll, bitcell_ns);
    pll->flux_scale_percent = flux_scale;
    pll->pll_adjust = pll_adjust;
    pll->clock_min = bitcell_ns * (100 - pll_adjust) / 100;
    pll->clock_max = bitcell_ns * (100 + pll_adjust) / 100;
}

/**
 * Process a flux transition and return decoded bit
 * 
 * 
 * @param pll       PLL state
 * @param flux_ns   Time since last flux transition in nanoseconds
 * @return          Decoded bit (0 or 1), or -1 on error
 */
static inline int uft_sd_pll_add_flux(uft_sd_pll_t *pll, int flux_ns) {
    int new_flux;
    
    /* Apply flux scaling if configured */
    if (pll->flux_scale_percent != 100) {
        flux_ns = flux_ns * pll->flux_scale_percent / 100;
    }
    
    pll->flux += flux_ns;
    pll->clocked_zeros = 0;
    
    return 0; /* Caller should call uft_sd_pll_next_bit() */
}

/**
 * Get next decoded bit from PLL
 * 
 * @param pll       PLL state
 * @return          0, 1, or -1 if more flux data needed
 */
static inline int uft_sd_pll_next_bit(uft_sd_pll_t *pll) {
    int new_flux;
    
    /* Need more flux data? */
    if (pll->flux < pll->clock / 2) {
        return -1;
    }
    
    pll->flux -= pll->clock;
    
    /* Check if this is a zero bit (no transition in window) */
    if (pll->flux >= pll->clock / 2) {
        pll->clocked_zeros++;
        pll->goodbits++;
        return 0;
    }
    
    /* 
     * PLL Clock Adjustment - THE CORE ALGORITHM
     */
    if (pll->clocked_zeros <= 3) {
        /* In sync: adjust base clock by percentage of phase mismatch */
        pll->clock += pll->flux * pll->pll_adjust / 100;
    } else {
        /* Out of sync: adjust base clock towards centre */
        pll->clock += (pll->clock_centre - pll->clock) * pll->pll_adjust / 100;
        
        /* Require 256 good bits before reporting another loss of sync */
        if (pll->goodbits >= 256) {
            pll->sync_lost = true;
        }
        pll->goodbits = 0;
    }
    
    /* Clamp the clock's adjustment range */
    if (pll->clock < pll->clock_min) pll->clock = pll->clock_min;
    if (pll->clock > pll->clock_max) pll->clock = pll->clock_max;
    
    /* 
     * Authentic PLL: Do not snap the timing window to each flux transition.
     * This preserves phase information for better tracking.
     */
    new_flux = pll->flux * (100 - pll->pll_phase) / 100;
    pll->flux = new_flux;
    
    pll->goodbits++;
    return 1;
}

/**
 * Check and clear sync lost flag
 */
static inline bool uft_sd_pll_sync_lost(uft_sd_pll_t *pll) {
    bool ret = pll->sync_lost;
    pll->sync_lost = false;
    return ret;
}

/**
 * Check and clear index flag
 */
static inline bool uft_sd_pll_index(uft_sd_pll_t *pll) {
    bool ret = pll->index;
    pll->index = false;
    return ret;
}

/*============================================================================
 * CRC16-CCITT IMPLEMENTATION
 *============================================================================*/

#define UFT_SD_CRC16_POLYNOMIAL  0x1021
#define UFT_SD_CRC16_INIT        0xFFFF
#define UFT_SD_CRC16_A1A1A1      0xCDB4   /* CRC of 0xA1, 0xA1, 0xA1 */

/**
 * CRC16 state with lazy-initialized lookup table
 */
typedef struct {
    uint16_t crc;
    bool table_initialized;
    uint16_t table[256];
} uft_sd_crc16_t;

/**
 * Initialize CRC16 lookup table
 */
static inline void uft_sd_crc16_init_table(uft_sd_crc16_t *ctx) {
    if (ctx->table_initialized) return;
    
    for (int i = 0; i < 256; i++) {
        uint16_t crc = (uint16_t)(i << 8);
        for (int j = 0; j < 8; j++) {
            crc = (crc << 1) ^ ((crc & 0x8000) ? UFT_SD_CRC16_POLYNOMIAL : 0);
        }
        ctx->table[i] = crc;
    }
    ctx->table_initialized = true;
}

/**
 * Initialize CRC16 calculation
 */
static inline void uft_sd_crc16_init(uft_sd_crc16_t *ctx, uint16_t init) {
    uft_sd_crc16_init_table(ctx);
    ctx->crc = init;
}

/**
 * Add a byte to CRC calculation
 */
static inline uint16_t uft_sd_crc16_add_byte(uft_sd_crc16_t *ctx, uint8_t byte) {
    ctx->crc = (ctx->crc << 8) ^ ctx->table[((ctx->crc >> 8) ^ byte) & 0xFF];
    return ctx->crc;
}

/**
 * Add multiple bytes to CRC calculation
 */
static inline uint16_t uft_sd_crc16_add(uft_sd_crc16_t *ctx, 
                                         const uint8_t *data, size_t len) {
    while (len--) {
        uft_sd_crc16_add_byte(ctx, *data++);
    }
    return ctx->crc;
}

/**
 * Calculate CRC16 for buffer (one-shot)
 */
static inline uint16_t uft_sd_crc16_calc(const uint8_t *data, size_t len, 
                                          uint16_t init) {
    uft_sd_crc16_t ctx = {0};
    uft_sd_crc16_init(&ctx, init);
    return uft_sd_crc16_add(&ctx, data, len);
}

/*============================================================================
 * IBM PC FORMAT CONSTANTS
 *============================================================================*/

/* Address marks */
#define UFT_SD_IBM_DAM_DELETED      0xF8
#define UFT_SD_IBM_DAM_DELETED_ALT  0xF9
#define UFT_SD_IBM_DAM_ALT          0xFA
#define UFT_SD_IBM_DAM              0xFB
#define UFT_SD_IBM_IAM              0xFC
#define UFT_SD_IBM_DAM_RX02         0xFD
#define UFT_SD_IBM_IDAM             0xFE

/* Gap sizes */
#define UFT_SD_GAP2_MFM_ED          41      /* Gap2 for MFM 1Mbps (ED) */
#define UFT_SD_GAP2_MFM_DDHD        22      /* Gap2 for MFM DD/HD */
#define UFT_SD_GAP2_FM              11      /* Gap2 for FM */
#define UFT_SD_MIN_GAP3             1
#define UFT_SD_MAX_GAP3             82

/* Track overhead calculations */
#define UFT_SD_TRACK_OVERHEAD_MFM   146     /* gap4a + sync + iam + gap1 */
#define UFT_SD_SECTOR_OVERHEAD_MFM  62      /* sync + idam + chrn + crc + gap2 + sync + dam + crc */
#define UFT_SD_DATA_OVERHEAD_MFM    4       /* 3xA1 + DAM */
#define UFT_SD_SYNC_OVERHEAD_MFM    12      /* 12x 0x00 */

#define UFT_SD_TRACK_OVERHEAD_FM    73      /* gap4a + sync + iam + gap1 */
#define UFT_SD_SECTOR_OVERHEAD_FM   33      /* sync + idam + chrn + crc + gap2 + sync + dam + crc */
#define UFT_SD_DATA_OVERHEAD_FM     1       /* DAM only */
#define UFT_SD_SYNC_OVERHEAD_FM     6       /* 6x 0x00 */

/* RPM timing */
#define UFT_SD_RPM_TIME_200         300000  /* µs for 200 RPM */
#define UFT_SD_RPM_TIME_300         200000  /* µs for 300 RPM */
#define UFT_SD_RPM_TIME_360         166667  /* µs for 360 RPM */

/*============================================================================
 * VICTOR 9000 VARIABLE SPEED ZONES
 *============================================================================*/

/**
 * Victor 9000 uses variable rotation speed across disk zones.
 * Each zone has a different bitcell time.
 */
static inline int uft_sd_victor_bitcell_ns(int cylinder) {
    if (cylinder < 4)  return 1789;
    if (cylinder < 16) return 1896;
    if (cylinder < 27) return 2009;
    if (cylinder < 38) return 2130;
    if (cylinder < 49) return 2272;
    if (cylinder < 60) return 2428;
    if (cylinder < 71) return 2613;
    return 2847;
}

/*============================================================================
 * GCR DECODING TABLES
 *============================================================================*/

/**
 * Commodore 64 GCR 5-bit to 4-bit decoding
 * Invalid values return 0x80 (bit 7 set)
 */
static const uint8_t UFT_SD_GCR5_DECODE[32] = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,  /* 00-07 */
    0x80, 0x08, 0x00, 0x01, 0x80, 0x0C, 0x04, 0x05,  /* 08-0F */
    0x80, 0x80, 0x02, 0x03, 0x80, 0x0F, 0x06, 0x07,  /* 10-17 */
    0x80, 0x09, 0x0A, 0x0B, 0x80, 0x0D, 0x0E, 0x80   /* 18-1F */
};

/**
 * Apple II GCR 6&2 decoding table
 * Values 0-63 are valid, 128 (0x80) indicates invalid
 */
static const uint8_t UFT_SD_GCR62_DECODE[256] = {
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,128,
    128,128,128,128,128,128,  0,  1,128,128,  2,  3,128,  4,  5,  6,
    128,128,128,128,128,128,  7,  8,128,128,  8,  9, 10, 11, 12, 13,
    128,128, 14, 15, 16, 17, 18, 19,128, 20, 21, 22, 23, 24, 25, 26,
    128,128,128,128,128,128,128,128,128,128,128, 27,128, 28, 29, 30,
    128,128,128, 31,128,128, 32, 33,128, 34, 35, 36, 37, 38, 39, 40,
    128,128,128,128,128, 41, 42, 43,128, 44, 45, 46, 47, 48, 49, 50,
    128,128, 51, 52, 53, 54, 55, 56,128, 57, 58, 59, 60, 61, 62, 63
};

/*============================================================================
 * FM/MFM ADDRESS MARK PATTERNS
 *============================================================================*/

/* FM address marks (32-bit patterns including clock) */
#define UFT_SD_FM_DDAM_PATTERN      0xAA222888UL  /* F8/C7 Deleted DAM */
#define UFT_SD_FM_DDAM_ALT_PATTERN  0xAA22288AUL  /* F9/C7 Alt Deleted DAM */
#define UFT_SD_FM_DAM_ALT_PATTERN   0xAA2228A8UL  /* FA/C7 Alt DAM */
#define UFT_SD_FM_DAM_PATTERN       0xAA2228AAUL  /* FB/C7 DAM */
#define UFT_SD_FM_IAM_PATTERN       0xAA2A2A88UL  /* FC/D7 IAM */
#define UFT_SD_FM_RX02_PATTERN      0xAA222A8AUL  /* FD/C7 RX02 DAM */
#define UFT_SD_FM_IDAM_PATTERN      0xAA222AA8UL  /* FE/C7 IDAM */

/* MFM sync pattern (16-bit, 0xA1 with missing clock bit 5) */
#define UFT_SD_MFM_SYNC_PATTERN     0x4489U

/*============================================================================
 * AMIGA MFM FORMAT
 *============================================================================*/

#define UFT_SD_AMIGA_SYNC           0x44894489UL  /* A1A1 sync pattern */
#define UFT_SD_AMIGA_MFM_MASK       0x55555555UL  /* Mask for data bits */
#define UFT_SD_AMIGA_SECTORS_DD     11            /* Sectors per track DD */
#define UFT_SD_AMIGA_SECTORS_HD     22            /* Sectors per track HD */
#define UFT_SD_AMIGA_SECTOR_SIZE    512

/**
 * Calculate Amiga sector checksum
 * XOR all 32-bit words and mask with MFM mask
 */
static inline uint32_t uft_sd_amiga_checksum(const uint8_t *data, size_t len) {
    uint32_t checksum = 0;
    const uint32_t *ptr = (const uint32_t *)data;
    size_t words = len / 4;
    
    while (words--) {
        /* Note: Amiga uses big-endian */
        uint32_t word = *ptr++;
        checksum ^= word;
    }
    
    return checksum & UFT_SD_AMIGA_MFM_MASK;
}

/*============================================================================
 * COPY PROTECTION DETECTION
 *============================================================================*/

/**
 * Speedlock signature detection
 * Returns offset of "SPEEDLOCK" string or -1 if not found
 */
static inline int uft_sd_find_speedlock(const uint8_t *data, size_t len) {
    static const uint8_t sig[] = "SPEEDLOCK";
    static const size_t sig_len = 9;
    
    if (len < sig_len) return -1;
    
    for (size_t i = 0; i <= len - sig_len; i++) {
        if (memcmp(data + i, sig, sig_len) == 0) {
            return (int)i;
        }
    }
    return -1;
}

/**
 * KBI-19 sector ID sequence
 */
static const uint8_t UFT_SD_KBI19_IDS[] = {
    0, 1, 4, 7, 10, 13, 16, 2, 5, 8, 11, 14, 17, 3, 6, 9, 12, 15, 18, 19
};

/*============================================================================
 * FORMAT DEFINITIONS
 *============================================================================*/

typedef enum {
    UFT_SD_FDC_UNKNOWN = 0,
    UFT_SD_FDC_WD,          /* Western Digital 177x/179x */
    UFT_SD_FDC_PC,          /* IBM PC uPD765 compatible */
    UFT_SD_FDC_AMIGA        /* Amiga Paula */
} uft_sd_fdc_type_t;

typedef struct {
    const char *name;
    uft_sd_fdc_type_t fdc;
    uft_sd_datarate_t datarate;
    uft_sd_encoding_t encoding;
    uint8_t cyls;
    uint8_t heads;
    uint8_t sectors;
    uint8_t size;           /* Size code: 0=128, 1=256, 2=512, 3=1024 */
    uint8_t base;           /* First sector number */
    uint8_t interleave;
    uint8_t skew;
    uint8_t gap3;
    uint8_t fill;
    bool cyls_first;        /* Cylinder-first sector ordering */
} uft_sd_format_t;

/* Standard format definitions */
static const uft_sd_format_t UFT_SD_FORMAT_PC720 = {
    "PC 720K", UFT_SD_FDC_PC, UFT_SD_DATARATE_250K, UFT_SD_ENCODING_MFM,
    80, 2, 9, 2, 1, 1, 1, 0x50, 0xF6, false
};

static const uft_sd_format_t UFT_SD_FORMAT_PC1440 = {
    "PC 1.44M", UFT_SD_FDC_PC, UFT_SD_DATARATE_500K, UFT_SD_ENCODING_MFM,
    80, 2, 18, 2, 1, 1, 1, 0x65, 0xF6, false
};

static const uft_sd_format_t UFT_SD_FORMAT_AMIGA_DD = {
    "AmigaDOS DD", UFT_SD_FDC_AMIGA, UFT_SD_DATARATE_250K, UFT_SD_ENCODING_AMIGA,
    80, 2, 11, 2, 0, 1, 0, 0, 0, false
};

static const uft_sd_format_t UFT_SD_FORMAT_ATARI_ST = {
    "Atari ST", UFT_SD_FDC_WD, UFT_SD_DATARATE_250K, UFT_SD_ENCODING_MFM,
    80, 2, 9, 2, 1, 1, 0, 40, 0x00, false
};

/*============================================================================
 * UTILITY FUNCTIONS
 *============================================================================*/

/**
 * Convert sector size code to bytes
 */
static inline int uft_sd_size_code_to_bytes(int code) {
    return 128 << (code & 7);
}

/**
 * Convert bytes to sector size code
 */
static inline int uft_sd_bytes_to_size_code(int bytes) {
    int code = 0;
    while ((128 << code) < bytes && code < 7) code++;
    return code;
}

/**
 * Calculate track capacity in bytes
 */
static inline int uft_sd_track_capacity(uft_sd_datarate_t datarate, 
                                         uft_sd_encoding_t encoding,
                                         int rpm) {
    int bits_per_second = (int)datarate;
    if (encoding == UFT_SD_ENCODING_FM) {
        bits_per_second /= 2;
    }
    
    int rpm_time_us = (rpm == 360) ? UFT_SD_RPM_TIME_360 :
                      (rpm == 200) ? UFT_SD_RPM_TIME_200 :
                                     UFT_SD_RPM_TIME_300;
    
    return (bits_per_second * rpm_time_us) / (8 * 1000000);
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_UFT_SAM_ALGORITHMS_H */
