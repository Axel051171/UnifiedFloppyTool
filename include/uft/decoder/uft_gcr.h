/**
 * @file uft_gcr.h
 * @brief Viterbi GCR Decoder API
 */

#ifndef UFT_DECODER_GCR_H
#define UFT_DECODER_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum values */
#define UFT_GCR_MAX_PATHS 16
#define UFT_GCR_MAX_TRACEBACK 128

/**
 * @brief GCR encoding modes
 */
typedef enum {
    GCR_MODE_C64,        /**< Commodore 64/1541 4-to-5 GCR */
    GCR_MODE_APPLE,      /**< Apple II 6-and-2 GCR (alias) */
    GCR_MODE_APPLE_62,   /**< Apple II 6-and-2 GCR */
    GCR_MODE_APPLE_53,   /**< Apple II 5-and-3 GCR */
    GCR_MODE_APPLE53,    /**< Apple II 5-and-3 GCR (alias) */
    GCR_MODE_VICTOR,     /**< Victor 9000 GCR */
    GCR_MODE_AUTO        /**< Auto-detect */
} gcr_mode_t;

/**
 * @brief GCR decoder configuration
 */
typedef struct {
    gcr_mode_t mode;            /**< Encoding mode */
    double cell_time_ns;        /**< Nominal cell time */
    int expected_sectors;       /**< Expected sectors per track */
    bool detect_errors;         /**< Detect decode errors */
    bool viterbi_correction;    /**< Use Viterbi correction */
    
    /* Viterbi-specific */
    int max_path_length;        /**< Maximum Viterbi path length */
    int max_candidates;         /**< Maximum candidates to track */
    bool allow_bitslip;         /**< Allow bit slips */
    int max_bitslip;            /**< Maximum bit slips allowed */
    double error_threshold;     /**< Error threshold (0.0-1.0) */
    bool detect_weak_bits;      /**< Detect weak bits */
} uft_gcr_config_t;

/**
 * @brief GCR decoder statistics
 */
typedef struct {
    int sectors_found;          /**< Sectors found */
    int sectors_ok;             /**< Sectors with good checksum */
    int sectors_bad;            /**< Sectors with bad checksum */
    int sync_found;             /**< Sync patterns found */
    int decode_errors;          /**< Decode errors */
    double bit_error_rate;      /**< Estimated BER */
    /* Extended stats */
    int total_sectors;          /**< Total sectors processed */
    int valid_sectors;          /**< Valid sectors */
    int corrected_sectors;      /**< Sectors corrected */
    int failed_sectors;         /**< Failed sectors */
    int bitslip_recoveries;     /**< Bitslip recoveries */
    double average_confidence;  /**< Average confidence */
} uft_gcr_stats_t;

/**
 * @brief GCR sector result
 */
typedef struct {
    int track;                  /**< Track number */
    int sector;                 /**< Sector number */
    uint8_t data[256];          /**< Sector data */
    int data_size;              /**< Expected data size */
    int data_length;            /**< Actual data length decoded */
    uint8_t checksum;           /**< Calculated checksum */
    bool checksum_ok;           /**< Checksum status */
    bool header_valid;          /**< Header valid flag */
    bool data_valid;            /**< Data valid flag */
    size_t bit_position;        /**< Position in bit stream */
    double confidence;          /**< Decode confidence */
    int corrections;            /**< Number of corrections applied */
    int bitslips;               /**< Number of bit slips */
} uft_gcr_sector_t;

/* Forward declaration for decoder */
typedef struct uft_gcr_decoder_s uft_gcr_decoder_t;

/* Configuration */
void uft_gcr_config_default(uft_gcr_config_t* config, gcr_mode_t mode);

/* Lifecycle */
uft_gcr_decoder_t* uft_gcr_create(const uft_gcr_config_t* config);
void uft_gcr_destroy(uft_gcr_decoder_t* decoder);
void uft_gcr_reset(uft_gcr_decoder_t* decoder);

/* Low-level decode */
int uft_gcr_decode_nibble(uint8_t gcr, gcr_mode_t mode);
int uft_gcr_decode_byte(const uint8_t* bits, size_t offset, gcr_mode_t mode);

/* Sync detection */
int uft_gcr_find_sync(const uint8_t* bits, size_t bit_count,
                      gcr_mode_t mode, size_t start_pos);

/* Sector decode */
int uft_gcr_decode_c64_sector(uft_gcr_decoder_t* decoder,
                              const uint8_t* bits, size_t bit_count,
                              size_t start_pos, uft_gcr_sector_t* sector);

int uft_gcr_decode_apple_sector(uft_gcr_decoder_t* decoder,
                                const uint8_t* bits, size_t bit_count,
                                size_t start_pos, uft_gcr_sector_t* sector);

int uft_gcr_decode_apple53_sector(uft_gcr_decoder_t* decoder,
                                  const uint8_t* bits, size_t bit_count,
                                  size_t start_pos, uft_gcr_sector_t* sector);

/* Track decode */
int uft_gcr_decode_track(uft_gcr_decoder_t* decoder,
                         const uint8_t* bits, size_t bit_count,
                         uft_gcr_sector_t* sectors, int max_sectors);

/* Statistics */
void uft_gcr_get_stats(const uft_gcr_decoder_t* decoder, uft_gcr_stats_t* stats);

/* Utilities */
const char* uft_gcr_mode_name(gcr_mode_t mode);
int uft_gcr_sectors_in_track(int track, gcr_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_GCR_H */
