/**
 * @file uft_forensic_flux_decoder.h
 * @brief Forensic-Grade Multi-Stage Flux Decoder API
 */

#ifndef UFT_FORENSIC_FLUX_DECODER_H
#define UFT_FORENSIC_FLUX_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "uft_fusion.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum values */
#define UFT_FFD_MAX_SECTORS 64
#define UFT_FFD_MAX_REVOLUTIONS 20

/**
 * @brief Forensic decoder configuration
 */
typedef struct {
    /* Pre-analysis */
    double min_cell_ratio;     /**< Minimum cell ratio for anomaly detection */
    double max_cell_ratio;     /**< Maximum cell ratio for anomaly detection */
    
    /* PLL */
    double pll_bandwidth;      /**< PLL bandwidth (0.0-1.0) */
    double pll_damping;        /**< PLL damping factor */
    double weak_threshold;     /**< Threshold for weak bit detection */
    
    /* Multi-rev fusion */
    bool enable_fusion;        /**< Enable multi-revolution fusion */
    double fusion_min_consensus; /**< Minimum consensus for fusion */
    int max_revolutions;       /**< Maximum revolutions to process */
    
    /* Sector recovery */
    int sync_hamming_tolerance; /**< Hamming distance tolerance for sync */
    bool enable_correction;    /**< Enable error correction */
    int max_correction_bits;   /**< Maximum bits to correct */
    
    /* Output */
    bool keep_raw_bits;        /**< Keep raw bit stream */
    bool keep_confidence;      /**< Keep per-bit confidence */
} uft_forensic_decoder_config_t;

/**
 * @brief Pre-analysis result
 */
typedef struct {
    double cell_time_ns;       /**< Estimated cell time in ns */
    double rpm;                /**< Estimated RPM */
    int anomaly_count;         /**< Number of anomalies detected */
    double quality_score;      /**< Quality score (0.0-1.0) */
} uft_preanalysis_result_t;

/**
 * @brief PLL decode result for one revolution
 */
typedef struct {
    uint8_t* bits;             /**< Decoded bits */
    size_t bit_count;          /**< Number of bits */
    double* confidence;        /**< Per-bit confidence */
    uint8_t* weak_flags;       /**< Weak bit flags */
    double average_confidence; /**< Average confidence */
    int weak_count;            /**< Number of weak bits */
} uft_pll_decode_result_t;

/**
 * @brief Sector decode result
 */
typedef struct {
    int cylinder;              /**< Cylinder number */
    int head;                  /**< Head number */
    int sector;                /**< Sector number */
    int size_code;             /**< Size code */
    uint8_t* data;             /**< Sector data */
    size_t data_size;          /**< Data size */
    uint16_t crc_stored;       /**< Stored CRC */
    uint16_t crc_calculated;   /**< Calculated CRC */
    bool crc_ok;               /**< CRC status */
    bool corrected;            /**< Was error correction applied */
    int corrections_count;     /**< Number of corrections */
    double confidence;         /**< Sector confidence */
} uft_sector_decode_result_t;

/**
 * @brief Track decode result
 */
typedef struct {
    int cylinder;              /**< Cylinder number */
    int head;                  /**< Head number */
    uft_sector_decode_result_t sectors[UFT_FFD_MAX_SECTORS];
    int sector_count;          /**< Number of sectors decoded */
    int crc_ok_count;          /**< Sectors with good CRC */
    int crc_error_count;       /**< Sectors with CRC errors */
    int corrected_count;       /**< Sectors that were corrected */
    double average_confidence; /**< Average confidence */
} uft_track_decode_result_t;

/* Configuration presets */
uft_forensic_decoder_config_t uft_forensic_decoder_config_default(void);
uft_forensic_decoder_config_t uft_forensic_decoder_config_paranoid(void);
uft_forensic_decoder_config_t uft_forensic_decoder_config_fast(void);

/* Pre-analysis */
int uft_forensic_preanalyze(const uint32_t* flux, size_t count,
                            const uft_forensic_decoder_config_t* config,
                            uft_preanalysis_result_t* result);

/* PLL decode */
int uft_forensic_pll_decode(const uint32_t* flux, size_t count,
                            double cell_time_ns,
                            const uft_forensic_decoder_config_t* config,
                            uft_pll_decode_result_t* result);

/* Multi-rev fusion */
int uft_forensic_fuse_revolutions(const uft_pll_decode_result_t* revs,
                                   int rev_count,
                                   const uft_forensic_decoder_config_t* config,
                                   uft_pll_decode_result_t* result);

/* Sector recovery */
int uft_forensic_recover_sectors(const uft_pll_decode_result_t* bits,
                                  const uft_forensic_decoder_config_t* config,
                                  uft_track_decode_result_t* result);

/* Error correction */
int uft_forensic_correct_sector(uft_sector_decode_result_t* sector,
                                 const uft_forensic_decoder_config_t* config);

/* High-level decode */
int uft_forensic_decode_track(const uint32_t* flux, size_t count,
                               int cylinder, int head,
                               const uft_forensic_decoder_config_t* config,
                               uft_track_decode_result_t* result);

/* Cleanup */
void uft_forensic_free_pll_result(uft_pll_decode_result_t* result);
void uft_forensic_free_track_result(uft_track_decode_result_t* result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORENSIC_FLUX_DECODER_H */
