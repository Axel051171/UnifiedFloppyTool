/**
 * @file uft_mfm_flux.h
 * @brief MFM Flux Analysis for IBM PC Floppy Disks
 * 
 * Based on flux-analyze project
 * Implements MFM decoding, address mark detection, and sector parsing
 */

#ifndef UFT_MFM_FLUX_H
#define UFT_MFM_FLUX_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MFM Encoding Constants
 *===========================================================================*/

/**
 * MFM Encoding Rules:
 *   1 bit  -> NR (no reversal, then reversal)
 *   0 bit after 0 -> RN (reversal, then no reversal)
 *   0 bit after 1 -> NN (no reversal, no reversal)
 * 
 * A flux reversal pattern that doesn't match these rules indicates
 * either an error or out-of-band data (like address mark sync bytes).
 */

#define UFT_MFM_BAND_1T     0   /**< Single cell time (shortest delay) */
#define UFT_MFM_BAND_2T     1   /**< Two cell times */
#define UFT_MFM_BAND_3T     2   /**< Three cell times (longest delay) */

/** Standard data rates */
#define UFT_MFM_RATE_DD     250000   /**< DD: 250 kbit/s */
#define UFT_MFM_RATE_HD     500000   /**< HD: 500 kbit/s */
#define UFT_MFM_RATE_ED     1000000  /**< ED: 1 Mbit/s */

/*===========================================================================
 * IBM Floppy Address Mark Types
 *===========================================================================*/

/**
 * IBM floppy address marks:
 *   IAM  - Index Address Mark:    C2C2C2 FC (start of track)
 *   IDAM - ID Address Mark:       A1A1A1 FE (start of sector header)
 *   DAM  - Data Address Mark:     A1A1A1 FB (start of sector data)
 *   DDAM - Deleted Data Mark:     A1A1A1 F8 (start of deleted sector data)
 * 
 * The A1 sync byte has a deliberate MFM encoding error (missing clock)
 * to distinguish it from regular data.
 */

typedef enum {
    UFT_AM_UNKNOWN = 0,
    UFT_AM_IAM     = 1,     /**< Index Address Mark (C2C2C2 FC) */
    UFT_AM_IDAM    = 2,     /**< ID Address Mark (A1A1A1 FE) */
    UFT_AM_DAM     = 3,     /**< Data Address Mark (A1A1A1 FB) */
    UFT_AM_DDAM    = 4      /**< Deleted Data Address Mark (A1A1A1 F8) */
} uft_mfm_am_type_t;

/** Address mark byte values */
#define UFT_MFM_SYNC_A1     0xA1    /**< Sync byte with missing clock */
#define UFT_MFM_SYNC_C2     0xC2    /**< Index sync byte */
#define UFT_MFM_MARK_IAM    0xFC    /**< Index address mark */
#define UFT_MFM_MARK_IDAM   0xFE    /**< ID address mark */
#define UFT_MFM_MARK_DAM    0xFB    /**< Data address mark */
#define UFT_MFM_MARK_DDAM   0xF8    /**< Deleted data address mark */

/*===========================================================================
 * Sector Size Table (IBM Standard)
 *===========================================================================*/

/** IDAM datalen field to actual byte count */
static const uint16_t uft_mfm_sector_sizes[] = {
    128,    /**< N=0 */
    256,    /**< N=1 */
    512,    /**< N=2 (most common) */
    1024,   /**< N=3 */
    2048,   /**< N=4 */
    4096,   /**< N=5 */
    8192    /**< N=6 */
};

#define UFT_MFM_MAX_SECTOR_SIZE_IDX 6

/*===========================================================================
 * MFM Preamble Patterns
 *===========================================================================*/

/**
 * A1 sync byte MFM pattern (with missing clock):
 * Binary:  1 0 1 0 0 0 0 1
 * MFM:     01 00 01 00 10 00 10 01
 * 
 * The pattern "10 00" at position 4-5 is illegal in normal MFM
 * (0 after 0 should be "01" not "10").
 */
static const uint8_t uft_mfm_a1_pattern[] = {
    0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1
};

/**
 * C2 sync byte MFM pattern (for IAM):
 * Binary:  1 1 0 0 0 0 1 0
 * MFM:     01 01 00 10 00 10 01 00
 */
static const uint8_t uft_mfm_c2_pattern[] = {
    0, 1, 0, 1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0
};

/** A1A1A1 preamble (3 sync bytes) */
#define UFT_MFM_A1_PREAMBLE_BITS    48  /* 16 * 3 */

/** Minimum gap bytes before preamble (0x4E or 0x00) */
#define UFT_MFM_MIN_GAP_BYTES       8

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief MFM flux train data
 */
typedef struct {
    uint8_t *data;          /**< MFM bit stream (0 or 1 per entry) */
    size_t *flux_indices;   /**< Maps MFM bits to original flux positions */
    size_t length;          /**< Number of MFM bits */
    size_t capacity;        /**< Allocated capacity */
} uft_mfm_train_t;

/**
 * @brief ID Address Mark (sector header)
 */
typedef struct {
    uint8_t track;          /**< Cylinder number */
    uint8_t head;           /**< Head number (0 or 1) */
    uint8_t sector;         /**< Sector number (1-based typically) */
    uint8_t size_code;      /**< Size code (index into sector_sizes) */
    uint16_t crc;           /**< Header CRC */
    bool crc_ok;            /**< CRC validation result */
    size_t flux_pos;        /**< Position in flux stream */
} uft_mfm_idam_t;

/**
 * @brief Data Address Mark (sector data)
 */
typedef struct {
    uint8_t *data;          /**< Sector data bytes */
    size_t data_len;        /**< Data length in bytes */
    uint16_t crc;           /**< Data CRC */
    bool crc_ok;            /**< CRC validation result */
    bool deleted;           /**< true if DDAM (deleted data) */
    size_t flux_pos;        /**< Position in flux stream */
} uft_mfm_dam_t;

/**
 * @brief Complete sector (IDAM + DAM)
 */
typedef struct {
    uft_mfm_idam_t idam;    /**< Sector header */
    uft_mfm_dam_t dam;      /**< Sector data */
    bool complete;          /**< Both IDAM and DAM found and valid */
} uft_mfm_sector_t;

/**
 * @brief Track analysis result
 */
typedef struct {
    uft_mfm_sector_t *sectors;  /**< Array of sectors */
    size_t sector_count;        /**< Number of sectors found */
    size_t capacity;            /**< Allocated capacity */
    uint8_t track_num;          /**< Track number from IDAMs */
    uint8_t head_num;           /**< Head number from IDAMs */
    size_t error_count;         /**< Total MFM decode errors */
} uft_mfm_track_t;

/**
 * @brief Band interval for clustering
 */
typedef struct {
    int32_t min;            /**< Minimum flux time in band */
    int32_t max;            /**< Maximum flux time in band */
    int32_t center;         /**< Band center (median) */
} uft_mfm_band_t;

/**
 * @brief Flux clustering result
 */
typedef struct {
    uft_mfm_band_t bands[3];    /**< Three bands for 1T, 2T, 3T */
    double clock_period;        /**< Estimated clock period */
    double error_metric;        /**< Clustering error (lower is better) */
} uft_mfm_clustering_t;

/*===========================================================================
 * CRC-16-CCITT (IBM Floppy)
 *===========================================================================*/

/** CRC-16-CCITT polynomial: x^16 + x^12 + x^5 + 1 */
#define UFT_CRC16_POLY      0x1021
#define UFT_CRC16_INIT      0xFFFF

/**
 * @brief CRC-16-CCITT lookup table
 */
extern const uint16_t uft_mfm_crc16_table[256];

/**
 * @brief Compute CRC-16-CCITT
 */
static inline uint16_t uft_mfm_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = UFT_CRC16_INIT;
    for (size_t i = 0; i < len; i++) {
        crc = (crc << 8) ^ uft_mfm_crc16_table[((crc >> 8) ^ data[i]) & 0xFF];
    }
    return crc;
}

/**
 * @brief Update CRC with single byte
 */
static inline uint16_t uft_mfm_crc16_update(uint16_t crc, uint8_t byte)
{
    return (crc << 8) ^ uft_mfm_crc16_table[((crc >> 8) ^ byte) & 0xFF];
}

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Convert flux timing array to MFM bit stream
 * @param flux_times Array of flux transition times (nanoseconds)
 * @param count Number of flux transitions
 * @param clock_period Nominal clock period in ns (e.g., 2000 for DD)
 * @param train Output MFM train
 * @param error_out Output: RMS error metric
 * @return 0 on success
 */
int uft_mfm_flux_to_train(const int32_t *flux_times, size_t count,
                          double clock_period, uft_mfm_train_t *train,
                          double *error_out);

/**
 * @brief Decode MFM bit stream to bytes
 * @param train MFM train data
 * @param start Start position in train
 * @param len Number of MFM bits to decode
 * @param output Output byte buffer (must be len/16 bytes)
 * @param errors Output error count array (optional, same size as output)
 * @return Number of bytes decoded
 */
size_t uft_mfm_decode_bytes(const uft_mfm_train_t *train, size_t start,
                            size_t len, uint8_t *output, uint8_t *errors);

/**
 * @brief Find A1A1A1 preamble positions in MFM train
 * @param train MFM train data
 * @param positions Output array of positions
 * @param max_positions Maximum positions to return
 * @return Number of preambles found
 */
size_t uft_mfm_find_preambles(const uft_mfm_train_t *train,
                              size_t *positions, size_t max_positions);

/**
 * @brief Parse IDAM from MFM train
 * @param train MFM train data
 * @param pos Position after A1A1A1FE sequence
 * @param idam Output IDAM structure
 * @return 0 on success
 */
int uft_mfm_parse_idam(const uft_mfm_train_t *train, size_t pos,
                       uft_mfm_idam_t *idam);

/**
 * @brief Parse DAM from MFM train
 * @param train MFM train data
 * @param pos Position after A1A1A1FB/F8 sequence
 * @param data_len Expected data length
 * @param dam Output DAM structure
 * @return 0 on success
 */
int uft_mfm_parse_dam(const uft_mfm_train_t *train, size_t pos,
                      size_t data_len, uft_mfm_dam_t *dam);

/**
 * @brief Analyze complete track
 * @param flux_times Flux timing array
 * @param count Number of flux transitions
 * @param clock_period Nominal clock period
 * @param track Output track analysis
 * @return 0 on success
 */
int uft_mfm_analyze_track(const int32_t *flux_times, size_t count,
                          double clock_period, uft_mfm_track_t *track);

/**
 * @brief Cluster flux times into 3 bands
 * @param flux_times Flux timing array
 * @param count Number of transitions
 * @param clustering Output clustering result
 * @return 0 on success
 */
int uft_mfm_cluster_bands(const int32_t *flux_times, size_t count,
                          uft_mfm_clustering_t *clustering);

/**
 * @brief Estimate clock period from flux data
 * @param flux_times Flux timing array
 * @param count Number of transitions
 * @return Estimated clock period in same units as flux_times
 */
double uft_mfm_estimate_clock(const int32_t *flux_times, size_t count);

/**
 * @brief Free MFM train resources
 */
void uft_mfm_train_free(uft_mfm_train_t *train);

/**
 * @brief Free track analysis resources
 */
void uft_mfm_track_free(uft_mfm_track_t *track);

/**
 * @brief Get sector size from size code
 */
static inline uint16_t uft_mfm_sector_size(uint8_t size_code)
{
    if (size_code > UFT_MFM_MAX_SECTOR_SIZE_IDX) {
        return 0;
    }
    return uft_mfm_sector_sizes[size_code];
}

/**
 * @brief Get address mark type name
 */
const char *uft_mfm_am_name(uft_mfm_am_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_FLUX_H */
