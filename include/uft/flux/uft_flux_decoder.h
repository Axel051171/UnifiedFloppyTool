/**
 * @file uft_flux_decoder.h
 * @brief Universal Flux-to-Sector Decoder
 * @version 3.9.0
 * 
 * Decodes raw flux timing data into sector data for various encodings:
 * - MFM (Modified Frequency Modulation) - PC, Amiga, Atari ST
 * - FM (Frequency Modulation) - older 8" drives, Apple II
 * - GCR (Group Coded Recording) - C64, Apple II
 * 
 * Supports flux data from:
 * - SuperCard Pro (.scp)
 * - KryoFlux (.raw)
 * - DiscFerret (.dfi)
 * - Greaseweazle (.gw)
 * 
 * Reference: Various ROMs, ROL analysis
 */

#ifndef UFT_FLUX_DECODER_H
#define UFT_FLUX_DECODER_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/* Standard bit cell times in nanoseconds */
#define FLUX_MFM_HD_BITCELL_NS      1000    /* 1µs for HD MFM (500 kbps) */
#define FLUX_MFM_DD_BITCELL_NS      2000    /* 2µs for DD MFM (250 kbps) */
#define FLUX_MFM_ED_BITCELL_NS       500    /* 0.5µs for ED MFM (1 Mbps) */
#define FLUX_FM_BITCELL_NS          4000    /* 4µs for FM (125 kbps) */
#define FLUX_GCR_C64_BITCELL_NS     3200    /* ~3.2µs for C64 GCR */
#define FLUX_GCR_APPLE_BITCELL_NS   4000    /* 4µs for Apple II GCR */

/* Sync patterns */
#define MFM_SYNC_PATTERN            0x4489  /* MFM sync (A1 with missing clock) */
#define FM_SYNC_PATTERN             0xF57E  /* FM sync (FE with clock) */
#define FM_IAM_PATTERN              0xF77A  /* FM Index Address Mark */

/* Address marks */
#define MFM_IDAM                    0xFE    /* ID Address Mark */
#define MFM_DAM                     0xFB    /* Data Address Mark */
#define MFM_DDAM                    0xF8    /* Deleted Data Address Mark */

/* Tolerances */
#define FLUX_TIMING_TOLERANCE       0.15    /* 15% timing tolerance */
#define FLUX_PLL_GAIN               0.05    /* PLL adjustment gain */

/* Maximum values */
#define FLUX_MAX_SECTORS            64
#define FLUX_MAX_TRACK_SIZE         65536
#define FLUX_MAX_REVOLUTIONS        16

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/**
 * @brief Flux encoding types
 */
typedef enum {
    FLUX_ENC_AUTO = 0,      /* Auto-detect encoding */
    FLUX_ENC_MFM,           /* MFM (PC, Amiga, Atari ST) */
    FLUX_ENC_FM,            /* FM (8", early systems) */
    FLUX_ENC_GCR_C64,       /* GCR Commodore 64 */
    FLUX_ENC_GCR_APPLE,     /* GCR Apple II */
    FLUX_ENC_AMIGA,         /* Amiga-specific MFM */
    FLUX_ENC_RAW            /* Raw bits, no decoding */
} flux_encoding_t;

/**
 * @brief Decoder status codes
 */
typedef enum {
    FLUX_OK = 0,
    FLUX_ERR_NO_SYNC,       /* No sync pattern found */
    FLUX_ERR_BAD_CRC,       /* CRC mismatch */
    FLUX_ERR_NO_DATA,       /* No data after ID */
    FLUX_ERR_WEAK_BITS,     /* Unreliable flux timing */
    FLUX_ERR_OVERFLOW,      /* Buffer overflow */
    FLUX_ERR_UNDERFLOW,     /* Not enough data */
    FLUX_ERR_INVALID        /* Invalid parameters */
} flux_status_t;

/* ============================================================================
 * Structures
 * ============================================================================ */

/**
 * @brief Raw flux data input
 */
typedef struct {
    uint32_t *transitions;      /* Flux transition times (in sample ticks) */
    size_t    transition_count;
    uint32_t  sample_rate;      /* Sample rate in Hz */
    uint32_t *index_times;      /* Index pulse positions */
    size_t    index_count;
} flux_raw_data_t;

/**
 * @brief Decoded sector information
 */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;         /* 0=128, 1=256, 2=512, 3=1024 */
    
    uint8_t *data;
    size_t   data_size;
    
    uint16_t id_crc;            /* CRC from ID field */
    uint16_t data_crc;          /* CRC from data field */
    bool     id_crc_ok;
    bool     data_crc_ok;
    bool     deleted;           /* Deleted data mark */
    
    /* Timing info */
    uint32_t id_position;       /* Position in flux stream */
    uint32_t data_position;
    double   bitrate;           /* Measured bitrate */
    
} flux_decoded_sector_t;

/**
 * @brief Decoded track result
 */
typedef struct {
    flux_decoded_sector_t sectors[FLUX_MAX_SECTORS];
    size_t sector_count;
    
    flux_encoding_t detected_encoding;
    double avg_bitrate;
    uint32_t track_length_bits;
    
    /* Statistics */
    size_t good_sectors;
    size_t bad_id_crc;
    size_t bad_data_crc;
    size_t missing_data;
    
    /* Raw decoded bits (optional) */
    uint8_t *raw_bits;
    size_t   raw_bit_count;
    
} flux_decoded_track_t;

/**
 * @brief Decoder options
 */
typedef struct {
    flux_encoding_t encoding;   /* Encoding to use (AUTO = detect) */
    uint32_t bitcell_ns;        /* Expected bit cell time (0 = auto) */
    double   tolerance;         /* Timing tolerance (0.15 = 15%) */
    bool     use_pll;           /* Use PLL for timing recovery */
    double   pll_gain;          /* PLL adjustment gain */
    uint8_t  revolution;        /* Which revolution to use (0 = best) */
    bool     decode_all_revs;   /* Decode all revolutions and merge */
    bool     keep_raw_bits;     /* Keep raw decoded bits */
} flux_decoder_options_t;

/**
 * @brief PLL state for timing recovery
 */
typedef struct {
    double   period;            /* Current bit cell period */
    double   phase;             /* Current phase */
    double   freq_gain;         /* Frequency adjustment gain */
    double   phase_gain;        /* Phase adjustment gain */
    uint32_t last_transition;   /* Last transition time */
} flux_pll_t;

/* ============================================================================
 * Initialization Functions
 * ============================================================================ */

/**
 * @brief Initialize decoder options with defaults
 */
void flux_decoder_options_init(flux_decoder_options_t *opts);

/**
 * @brief Initialize PLL state
 */
void flux_pll_init(flux_pll_t *pll, double initial_period);

/**
 * @brief Initialize decoded track structure
 */
void flux_decoded_track_init(flux_decoded_track_t *track);

/**
 * @brief Free decoded track resources
 */
void flux_decoded_track_free(flux_decoded_track_t *track);

/* ============================================================================
 * Main Decoding Functions
 * ============================================================================ */

/**
 * @brief Decode flux data to sectors
 * 
 * @param flux Raw flux data
 * @param track Output decoded track
 * @param opts Decoder options (NULL for defaults)
 * @return Status code
 */
flux_status_t flux_decode_track(const flux_raw_data_t *flux,
                                flux_decoded_track_t *track,
                                const flux_decoder_options_t *opts);

/**
 * @brief Decode MFM flux data
 */
flux_status_t flux_decode_mfm(const flux_raw_data_t *flux,
                              flux_decoded_track_t *track,
                              const flux_decoder_options_t *opts);

/**
 * @brief Decode FM flux data
 */
flux_status_t flux_decode_fm(const flux_raw_data_t *flux,
                             flux_decoded_track_t *track,
                             const flux_decoder_options_t *opts);

/**
 * @brief Decode C64 GCR flux data
 */
flux_status_t flux_decode_gcr_c64(const flux_raw_data_t *flux,
                                  flux_decoded_track_t *track,
                                  const flux_decoder_options_t *opts);

/**
 * @brief Decode Apple II GCR flux data
 */
flux_status_t flux_decode_gcr_apple(const flux_raw_data_t *flux,
                                    flux_decoded_track_t *track,
                                    const flux_decoder_options_t *opts);

/* ============================================================================
 * Format-Specific Decoders
 * ============================================================================ */

/**
 * @brief Decode SCP file to disk image
 */
flux_status_t flux_decode_scp_file(const char *path,
                                   uft_disk_image_t **out_disk,
                                   const flux_decoder_options_t *opts);

/**
 * @brief Decode KryoFlux stream files to disk image
 */
flux_status_t flux_decode_kryoflux_files(const char *base_path,
                                         uft_disk_image_t **out_disk,
                                         const flux_decoder_options_t *opts);

/**
 * @brief Decode DFI file to disk image
 */
flux_status_t flux_decode_dfi_file(const char *path,
                                   uft_disk_image_t **out_disk,
                                   const flux_decoder_options_t *opts);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * @brief Detect encoding from flux data
 */
flux_encoding_t flux_detect_encoding(const flux_raw_data_t *flux);

/**
 * @brief Calculate CRC-16 CCITT
 */
uint16_t flux_crc16_ccitt(const uint8_t *data, size_t len);

/**
 * @brief Calculate CRC-16 for MFM (init=0xFFFF, poly=0x1021)
 */
uint16_t flux_crc16_mfm(const uint8_t *data, size_t len);

/**
 * @brief MFM decode byte pair to data byte
 */
uint8_t flux_mfm_decode_byte(uint16_t mfm_word);

/**
 * @brief MFM encode data byte to byte pair
 */
uint16_t flux_mfm_encode_byte(uint8_t data, bool prev_bit);

/**
 * @brief FM decode byte
 */
uint8_t flux_fm_decode_byte(uint16_t fm_word);

/**
 * @brief Convert flux times to bit stream
 */
flux_status_t flux_to_bitstream(const flux_raw_data_t *flux,
                                uint8_t *bits, size_t *bit_count,
                                double bitcell_ns, flux_pll_t *pll);

/**
 * @brief Find sync pattern in bitstream
 */
int flux_find_sync(const uint8_t *bits, size_t bit_count,
                   uint16_t pattern, size_t start_pos);

/**
 * @brief Get sector size from size code
 */
static inline size_t flux_sector_size(uint8_t size_code) {
    return 128u << (size_code & 3);
}

/**
 * @brief Get encoding name
 */
const char* flux_encoding_name(flux_encoding_t enc);

/**
 * @brief Get status name
 */
const char* flux_status_name(flux_status_t status);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLUX_DECODER_H */
