/**
 * @file uft_multi_decoder.h
 * @brief UFT Multi-Interpretations-Decoder - Parallel Hypothesis Management
 * @version 3.2.0.003
 * @date 2026-01-04
 * 
 * @copyright Copyright (c) 2026 UFT Project
 * @license MIT
 * 
 * This module implements a multi-interpretation decoder that maintains
 * multiple hypotheses for ambiguous bitstreams. Instead of making early
 * decisions, all plausible interpretations are preserved with confidence
 * scores until final resolution is required.
 * 
 * Key Features:
 * - N-Best candidate list per sector
 * - Lazy evaluation (resolve only when needed)
 * - Confidence scoring with provenance tracking
 * - Forensic export of all interpretations
 * - GUI-ready alternative display
 * 
 * Architecture:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │                    UFT Multi-Decoder Pipeline                   │
 * ├─────────────────────────────────────────────────────────────────┤
 * │  Bitstream ──► Tokenizer ──► Candidate Generator ──► Scorer    │
 * │                                      │                          │
 * │                              ┌───────▼───────┐                  │
 * │                              │  N-Best List  │                  │
 * │                              │  (per sector) │                  │
 * │                              └───────┬───────┘                  │
 * │                                      │                          │
 * │                         ┌────────────┼────────────┐             │
 * │                         ▼            ▼            ▼             │
 * │                    Best Pick   All Candidates  Forensic         │
 * │                    (Lazy)      (Export)        (Report)         │
 * └─────────────────────────────────────────────────────────────────┘
 * 
 * "Bei uns geht kein Bit verloren" - No interpretation is discarded prematurely.
 */

#ifndef UFT_MULTI_DECODER_H
#define UFT_MULTI_DECODER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * VERSION & CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_MDEC_VERSION_MAJOR       3
#define UFT_MDEC_VERSION_MINOR       2
#define UFT_MDEC_VERSION_PATCH       0

/** Maximum candidates per sector (N-Best) */
#define UFT_MDEC_MAX_CANDIDATES      16

/** Maximum sectors per track */
#define UFT_MDEC_MAX_SECTORS         32

/** Maximum tracks per disk */
#define UFT_MDEC_MAX_TRACKS          168

/** Maximum sector data size */
#define UFT_MDEC_MAX_SECTOR_SIZE     16384

/** Maximum ambiguous regions per sector */
#define UFT_MDEC_MAX_AMBIGUOUS       64

/** Maximum provenance entries */
#define UFT_MDEC_MAX_PROVENANCE      32

/** Confidence threshold for auto-resolution */
#define UFT_MDEC_CONFIDENCE_AUTO     95.0f

/** Minimum confidence delta for differentiation */
#define UFT_MDEC_CONFIDENCE_DELTA    5.0f

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_MDEC_OK                      = 0,
    UFT_MDEC_ERR_NULL                = -1,
    UFT_MDEC_ERR_MEMORY              = -2,
    UFT_MDEC_ERR_OVERFLOW            = -3,
    UFT_MDEC_ERR_INVALID_PARAM       = -4,
    UFT_MDEC_ERR_NO_CANDIDATES       = -5,
    UFT_MDEC_ERR_AMBIGUOUS           = -6,
    UFT_MDEC_ERR_RESOLUTION_FAILED   = -7,
    UFT_MDEC_ERR_IO                  = -8,
    UFT_MDEC_ERR_FORMAT              = -9,
    UFT_MDEC_ERR_CHECKSUM            = -10,
    UFT_MDEC_ERR_TIMEOUT             = -11,
    UFT_MDEC_ERR_NOT_FOUND           = -12,
    UFT_MDEC_ERR_ALREADY_RESOLVED    = -13,
    UFT_MDEC_ERR_ENCODING            = -14,
    UFT_MDEC_ERR_SYNC                = -15
} uft_mdec_error_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Candidate resolution status
 */
typedef enum {
    UFT_MDEC_STATUS_PENDING          = 0,   /**< Not yet resolved */
    UFT_MDEC_STATUS_AUTO_RESOLVED    = 1,   /**< High confidence auto-resolution */
    UFT_MDEC_STATUS_USER_RESOLVED    = 2,   /**< User manually selected */
    UFT_MDEC_STATUS_HEURISTIC        = 3,   /**< Resolved by heuristic */
    UFT_MDEC_STATUS_FORCED           = 4,   /**< Forced by timeout/export */
    UFT_MDEC_STATUS_FAILED           = 5,   /**< No valid candidate found */
    UFT_MDEC_STATUS_AMBIGUOUS        = 6    /**< Multiple equally valid */
} uft_mdec_status_t;

/**
 * @brief Encoding type for decoding
 */
typedef enum {
    UFT_MDEC_ENC_UNKNOWN             = 0,
    UFT_MDEC_ENC_MFM                 = 1,   /**< Modified Frequency Modulation */
    UFT_MDEC_ENC_GCR_CBM             = 2,   /**< Commodore GCR */
    UFT_MDEC_ENC_GCR_APPLE           = 3,   /**< Apple GCR (6&2, 5&3) */
    UFT_MDEC_ENC_FM                  = 4,   /**< Frequency Modulation */
    UFT_MDEC_ENC_M2FM                = 5,   /**< Modified M2FM */
    UFT_MDEC_ENC_AMIGA               = 6,   /**< Amiga MFM variant */
    UFT_MDEC_ENC_RAW                 = 7    /**< Raw flux/bitstream */
} uft_mdec_encoding_t;

/**
 * @brief Ambiguity type classification
 */
typedef enum {
    UFT_MDEC_AMB_NONE                = 0,
    UFT_MDEC_AMB_WEAK_BIT            = 1,   /**< Weak/unstable bit */
    UFT_MDEC_AMB_TIMING              = 2,   /**< Timing uncertainty */
    UFT_MDEC_AMB_SYNC_SLIP           = 3,   /**< Sync alignment ambiguity */
    UFT_MDEC_AMB_ENCODING            = 4,   /**< Encoding interpretation */
    UFT_MDEC_AMB_CRC_COLLISION       = 5,   /**< Multiple CRC-valid options */
    UFT_MDEC_AMB_PROTECTION          = 6,   /**< Copy protection artifact */
    UFT_MDEC_AMB_DAMAGE              = 7,   /**< Physical media damage */
    UFT_MDEC_AMB_PLL_DRIFT           = 8    /**< PLL frequency drift */
} uft_mdec_ambiguity_t;

/**
 * @brief Provenance source type
 */
typedef enum {
    UFT_MDEC_PROV_DIRECT             = 0,   /**< Direct decode from bitstream */
    UFT_MDEC_PROV_MULTI_REV          = 1,   /**< Multi-revolution consensus */
    UFT_MDEC_PROV_CRC_CORRECTED      = 2,   /**< CRC error correction */
    UFT_MDEC_PROV_INTERPOLATED       = 3,   /**< Interpolated from neighbors */
    UFT_MDEC_PROV_HEURISTIC          = 4,   /**< Heuristic guess */
    UFT_MDEC_PROV_USER_OVERRIDE      = 5,   /**< User manual override */
    UFT_MDEC_PROV_REFERENCE          = 6,   /**< Known reference image */
    UFT_MDEC_PROV_ECC                = 7    /**< ECC reconstruction */
} uft_mdec_provenance_type_t;

/**
 * @brief Resolution strategy
 */
typedef enum {
    UFT_MDEC_STRATEGY_HIGHEST_CONF   = 0,   /**< Pick highest confidence */
    UFT_MDEC_STRATEGY_MAJORITY       = 1,   /**< Majority vote across revolutions */
    UFT_MDEC_STRATEGY_CRC_PRIORITY   = 2,   /**< Prefer CRC-valid candidates */
    UFT_MDEC_STRATEGY_CONSERVATIVE   = 3,   /**< Mark ambiguous if close */
    UFT_MDEC_STRATEGY_REFERENCE      = 4,   /**< Compare to reference */
    UFT_MDEC_STRATEGY_MANUAL         = 5    /**< Wait for user decision */
} uft_mdec_strategy_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Ambiguous region within a sector
 */
typedef struct {
    uint32_t                bit_offset;         /**< Bit offset in decoded data */
    uint32_t                bit_length;         /**< Number of ambiguous bits */
    uft_mdec_ambiguity_t    type;               /**< Type of ambiguity */
    uint8_t                 alternatives;       /**< Number of alternatives */
    float                   confidence[4];      /**< Confidence per alternative */
    uint8_t                 values[4];          /**< Possible byte values */
    uint32_t                flux_sample;        /**< Source flux sample index */
} uft_mdec_ambiguous_region_t;

/**
 * @brief Provenance entry for tracking data origin
 */
typedef struct {
    uft_mdec_provenance_type_t  type;           /**< Source type */
    uint32_t                    revolution;     /**< Source revolution (if multi-rev) */
    uint32_t                    bit_offset;     /**< Start bit in original */
    uint32_t                    bit_length;     /**< Length in bits */
    float                       confidence;     /**< Confidence at this point */
    uint32_t                    timestamp_us;   /**< Processing timestamp */
    char                        note[64];       /**< Human-readable note */
} uft_mdec_provenance_t;

/**
 * @brief Single decode candidate for a sector
 */
typedef struct {
    /* Identification */
    uint32_t                id;                 /**< Unique candidate ID */
    uint32_t                sector_index;       /**< Logical sector number */
    
    /* Decoded data */
    uint8_t                 data[UFT_MDEC_MAX_SECTOR_SIZE];  /**< Decoded bytes */
    uint32_t                data_size;          /**< Actual data size */
    uint32_t                data_crc;           /**< CRC32 of data */
    
    /* Confidence metrics */
    float                   confidence;         /**< Overall confidence 0-100% */
    float                   checksum_confidence;/**< Checksum match confidence */
    float                   timing_confidence;  /**< Timing consistency confidence */
    float                   encoding_confidence;/**< Encoding validity confidence */
    
    /* Validation status */
    bool                    crc_valid;          /**< Sector CRC matches */
    bool                    header_valid;       /**< Header checksum valid */
    bool                    complete;           /**< All bytes decoded */
    uint32_t                errors_corrected;   /**< Number of bits corrected */
    
    /* Ambiguity tracking */
    uint32_t                ambiguous_count;    /**< Number of ambiguous regions */
    uft_mdec_ambiguous_region_t ambiguous[UFT_MDEC_MAX_AMBIGUOUS];
    
    /* Provenance chain */
    uint32_t                provenance_count;   /**< Number of provenance entries */
    uft_mdec_provenance_t   provenance[UFT_MDEC_MAX_PROVENANCE];
    
    /* Source information */
    uft_mdec_encoding_t     encoding;           /**< Encoding used for decode */
    uint32_t                revolution;         /**< Source revolution */
    uint32_t                flux_offset;        /**< Start position in flux data */
    uint32_t                flux_length;        /**< Flux span consumed */
    
    /* Timestamps */
    uint64_t                created_us;         /**< Creation timestamp */
    uint64_t                modified_us;        /**< Last modification timestamp */
} uft_mdec_candidate_t;

/**
 * @brief N-Best candidate list for a sector
 */
typedef struct {
    /* Sector identification */
    uint8_t                 track;              /**< Physical track */
    uint8_t                 head;               /**< Physical head/side */
    uint8_t                 sector;             /**< Logical sector number */
    
    /* Candidates (sorted by confidence) */
    uint32_t                count;              /**< Number of candidates */
    uft_mdec_candidate_t    candidates[UFT_MDEC_MAX_CANDIDATES];
    
    /* Resolution status */
    uft_mdec_status_t       status;             /**< Current resolution status */
    int32_t                 selected_index;     /**< Index of selected candidate (-1 = none) */
    
    /* Statistics */
    float                   max_confidence;     /**< Highest confidence */
    float                   confidence_spread;  /**< Delta between best and 2nd best */
    uint32_t                total_generated;    /**< Total candidates generated */
    
    /* Lazy evaluation state */
    bool                    resolved;           /**< Has been resolved */
    bool                    resolution_deferred;/**< Resolution explicitly deferred */
    uft_mdec_strategy_t     resolution_strategy;/**< Strategy used for resolution */
} uft_mdec_sector_t;

/**
 * @brief Track-level multi-decode context
 */
typedef struct {
    /* Track identification */
    uint8_t                 track;              /**< Physical track number */
    uint8_t                 head;               /**< Physical head/side */
    
    /* Sectors */
    uint32_t                sector_count;       /**< Number of sectors */
    uft_mdec_sector_t       sectors[UFT_MDEC_MAX_SECTORS];
    
    /* Track-level metrics */
    float                   avg_confidence;     /**< Average sector confidence */
    uint32_t                resolved_count;     /**< Sectors resolved */
    uint32_t                ambiguous_count;    /**< Sectors still ambiguous */
    uint32_t                failed_count;       /**< Sectors with no valid candidate */
    
    /* Encoding detected */
    uft_mdec_encoding_t     encoding;           /**< Primary encoding type */
    uint32_t                sector_size;        /**< Expected sector size */
} uft_mdec_track_t;

/**
 * @brief Disk-level multi-decode session
 */
typedef struct {
    /* Session identification */
    uint64_t                session_id;         /**< Unique session ID */
    char                    source_file[256];   /**< Source flux file path */
    
    /* Tracks */
    uint32_t                track_count;        /**< Number of tracks */
    uft_mdec_track_t       *tracks;             /**< Dynamically allocated tracks */
    
    /* Global configuration */
    uft_mdec_strategy_t     default_strategy;   /**< Default resolution strategy */
    float                   auto_resolve_threshold; /**< Auto-resolve confidence threshold */
    bool                    lazy_evaluation;    /**< Enable lazy evaluation */
    bool                    preserve_all;       /**< Keep all candidates (forensic mode) */
    
    /* Global statistics */
    uint64_t                total_sectors;      /**< Total sectors processed */
    uint64_t                total_candidates;   /**< Total candidates generated */
    uint64_t                resolved_sectors;   /**< Sectors resolved */
    uint64_t                ambiguous_sectors;  /**< Sectors with multiple valid options */
    float                   overall_confidence; /**< Weighted average confidence */
    
    /* Memory management */
    size_t                  memory_used;        /**< Current memory usage */
    size_t                  memory_limit;       /**< Maximum memory allowed */
    
    /* Timestamps */
    uint64_t                created_us;         /**< Session creation time */
    uint64_t                modified_us;        /**< Last modification time */
} uft_mdec_session_t;

/**
 * @brief Configuration for multi-decoder
 */
typedef struct {
    /* Resolution settings */
    uft_mdec_strategy_t     strategy;           /**< Default resolution strategy */
    float                   auto_threshold;     /**< Auto-resolve threshold (0-100) */
    float                   ambiguity_delta;    /**< Min delta for clear winner */
    
    /* Candidate generation */
    uint32_t                max_candidates;     /**< Max candidates per sector */
    bool                    generate_all;       /**< Generate all possible candidates */
    bool                    include_invalid;    /**< Include CRC-invalid candidates */
    
    /* Memory limits */
    size_t                  memory_limit;       /**< Maximum memory usage */
    bool                    stream_mode;        /**< Streaming mode (lower memory) */
    
    /* Forensic options */
    bool                    forensic_mode;      /**< Full forensic logging */
    bool                    preserve_all;       /**< Never discard candidates */
    bool                    track_provenance;   /**< Track full provenance chain */
    
    /* Multi-revolution */
    uint32_t                min_revolutions;    /**< Minimum revolutions for consensus */
    float                   revolution_weight;  /**< Weight for revolution agreement */
} uft_mdec_config_t;

/**
 * @brief Statistics for forensic export
 */
typedef struct {
    /* Per-confidence-band counts */
    uint32_t                band_0_50;          /**< Candidates 0-50% confidence */
    uint32_t                band_50_70;         /**< Candidates 50-70% confidence */
    uint32_t                band_70_85;         /**< Candidates 70-85% confidence */
    uint32_t                band_85_95;         /**< Candidates 85-95% confidence */
    uint32_t                band_95_100;        /**< Candidates 95-100% confidence */
    
    /* Ambiguity distribution */
    uint32_t                amb_weak_bits;      /**< Weak bit ambiguities */
    uint32_t                amb_timing;         /**< Timing ambiguities */
    uint32_t                amb_sync;           /**< Sync ambiguities */
    uint32_t                amb_encoding;       /**< Encoding ambiguities */
    uint32_t                amb_protection;     /**< Protection-related */
    
    /* Resolution statistics */
    uint32_t                auto_resolved;      /**< Auto-resolved sectors */
    uint32_t                user_resolved;      /**< User-resolved sectors */
    uint32_t                heuristic_resolved; /**< Heuristic-resolved */
    uint32_t                forced_resolved;    /**< Force-resolved */
    uint32_t                unresolved;         /**< Still unresolved */
    
    /* Data integrity */
    uint32_t                crc_valid_total;    /**< Total CRC-valid candidates */
    uint32_t                crc_corrected;      /**< CRC errors corrected */
    uint32_t                unique_data;        /**< Unique data patterns found */
} uft_mdec_statistics_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SESSION MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new multi-decoder session
 * 
 * @param[out] session      Pointer to session pointer
 * @param[in]  config       Configuration (NULL for defaults)
 * @param[in]  source_file  Source file path (for logging)
 * @return UFT_MDEC_OK on success, error code otherwise
 */
int uft_mdec_session_create(uft_mdec_session_t **session,
                            const uft_mdec_config_t *config,
                            const char *source_file);

/**
 * @brief Destroy a multi-decoder session
 * 
 * @param[in,out] session   Session to destroy (set to NULL)
 */
void uft_mdec_session_destroy(uft_mdec_session_t **session);

/**
 * @brief Reset session for reuse
 * 
 * @param[in,out] session   Session to reset
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_session_reset(uft_mdec_session_t *session);

/**
 * @brief Get default configuration
 * 
 * @param[out] config       Configuration to fill
 */
void uft_mdec_config_default(uft_mdec_config_t *config);

/* ═══════════════════════════════════════════════════════════════════════════
 * CANDIDATE MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Add a decode candidate for a sector
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     track     Physical track number
 * @param[in]     head      Physical head/side
 * @param[in]     sector    Logical sector number
 * @param[in]     candidate Candidate to add (copied)
 * @return UFT_MDEC_OK on success, error code otherwise
 */
int uft_mdec_add_candidate(uft_mdec_session_t *session,
                           uint8_t track,
                           uint8_t head,
                           uint8_t sector,
                           const uft_mdec_candidate_t *candidate);

/**
 * @brief Generate candidates from bitstream
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     track     Physical track number
 * @param[in]     head      Physical head/side
 * @param[in]     bitstream Raw bitstream data
 * @param[in]     bit_count Number of bits in bitstream
 * @param[in]     encoding  Encoding type to use
 * @return Number of candidates generated, negative on error
 */
int uft_mdec_generate_candidates(uft_mdec_session_t *session,
                                 uint8_t track,
                                 uint8_t head,
                                 const uint8_t *bitstream,
                                 size_t bit_count,
                                 uft_mdec_encoding_t encoding);

/**
 * @brief Get N-Best candidates for a sector
 * 
 * @param[in]  session      Multi-decoder session
 * @param[in]  track        Physical track number
 * @param[in]  head         Physical head/side
 * @param[in]  sector       Logical sector number
 * @param[out] sector_info  Pointer to sector info (not copied)
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_get_sector(const uft_mdec_session_t *session,
                        uint8_t track,
                        uint8_t head,
                        uint8_t sector,
                        const uft_mdec_sector_t **sector_info);

/**
 * @brief Get best candidate for a sector (lazy resolution)
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     track     Physical track number
 * @param[in]     head      Physical head/side
 * @param[in]     sector    Logical sector number
 * @param[out]    candidate Best candidate (not copied)
 * @return UFT_MDEC_OK on success, UFT_MDEC_ERR_AMBIGUOUS if cannot resolve
 */
int uft_mdec_get_best(uft_mdec_session_t *session,
                      uint8_t track,
                      uint8_t head,
                      uint8_t sector,
                      const uft_mdec_candidate_t **candidate);

/* ═══════════════════════════════════════════════════════════════════════════
 * RESOLUTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resolve a sector using specified strategy
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     track     Physical track number
 * @param[in]     head      Physical head/side
 * @param[in]     sector    Logical sector number
 * @param[in]     strategy  Resolution strategy
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_resolve_sector(uft_mdec_session_t *session,
                            uint8_t track,
                            uint8_t head,
                            uint8_t sector,
                            uft_mdec_strategy_t strategy);

/**
 * @brief Manually select a candidate
 * 
 * @param[in,out] session       Multi-decoder session
 * @param[in]     track         Physical track number
 * @param[in]     head          Physical head/side
 * @param[in]     sector        Logical sector number
 * @param[in]     candidate_idx Index of candidate to select
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_select_candidate(uft_mdec_session_t *session,
                              uint8_t track,
                              uint8_t head,
                              uint8_t sector,
                              uint32_t candidate_idx);

/**
 * @brief Resolve all sectors in session
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     strategy  Resolution strategy
 * @param[out]    stats     Resolution statistics (optional)
 * @return Number of sectors resolved, negative on error
 */
int uft_mdec_resolve_all(uft_mdec_session_t *session,
                         uft_mdec_strategy_t strategy,
                         uft_mdec_statistics_t *stats);

/**
 * @brief Defer resolution for a sector
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     track     Physical track number
 * @param[in]     head      Physical head/side
 * @param[in]     sector    Logical sector number
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_defer_resolution(uft_mdec_session_t *session,
                              uint8_t track,
                              uint8_t head,
                              uint8_t sector);

/* ═══════════════════════════════════════════════════════════════════════════
 * SCORING & CONFIDENCE
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Calculate confidence score for a candidate
 * 
 * @param[in,out] candidate Candidate to score
 * @return Confidence score 0-100
 */
float uft_mdec_calculate_confidence(uft_mdec_candidate_t *candidate);

/**
 * @brief Update candidate scores after adding ambiguity
 * 
 * @param[in,out] candidate Candidate to update
 * @param[in]     ambiguity Ambiguity information
 * @return Updated confidence score
 */
float uft_mdec_update_ambiguity(uft_mdec_candidate_t *candidate,
                                const uft_mdec_ambiguous_region_t *ambiguity);

/**
 * @brief Compare two candidates
 * 
 * @param[in] a     First candidate
 * @param[in] b     Second candidate
 * @return <0 if a better, >0 if b better, 0 if equal
 */
int uft_mdec_compare_candidates(const uft_mdec_candidate_t *a,
                                const uft_mdec_candidate_t *b);

/**
 * @brief Calculate sector confidence spread
 * 
 * @param[in] sector    Sector with candidates
 * @return Confidence delta between best and second-best
 */
float uft_mdec_confidence_spread(const uft_mdec_sector_t *sector);

/* ═══════════════════════════════════════════════════════════════════════════
 * PROVENANCE TRACKING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Add provenance entry to candidate
 * 
 * @param[in,out] candidate Candidate to update
 * @param[in]     type      Provenance type
 * @param[in]     offset    Bit offset in original
 * @param[in]     length    Bit length
 * @param[in]     conf      Confidence at this point
 * @param[in]     note      Human-readable note (max 63 chars)
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_add_provenance(uft_mdec_candidate_t *candidate,
                            uft_mdec_provenance_type_t type,
                            uint32_t offset,
                            uint32_t length,
                            float conf,
                            const char *note);

/**
 * @brief Export provenance chain as string
 * 
 * @param[in]  candidate    Candidate with provenance
 * @param[out] buffer       Output buffer
 * @param[in]  buffer_size  Buffer size
 * @return Number of bytes written, negative on error
 */
int uft_mdec_export_provenance(const uft_mdec_candidate_t *candidate,
                               char *buffer,
                               size_t buffer_size);

/* ═══════════════════════════════════════════════════════════════════════════
 * MULTI-REVOLUTION SUPPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Merge candidates from multiple revolutions
 * 
 * @param[in,out] session   Multi-decoder session
 * @param[in]     track     Physical track number
 * @param[in]     head      Physical head/side
 * @return Number of candidates after merge, negative on error
 */
int uft_mdec_merge_revolutions(uft_mdec_session_t *session,
                               uint8_t track,
                               uint8_t head);

/**
 * @brief Calculate multi-revolution consensus
 * 
 * @param[in]  session      Multi-decoder session
 * @param[in]  track        Physical track number
 * @param[in]  head         Physical head/side
 * @param[in]  sector       Logical sector number
 * @param[out] consensus    Output buffer for consensus data
 * @param[in]  size         Buffer size
 * @param[out] confidence   Consensus confidence
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_calculate_consensus(const uft_mdec_session_t *session,
                                 uint8_t track,
                                 uint8_t head,
                                 uint8_t sector,
                                 uint8_t *consensus,
                                 size_t size,
                                 float *confidence);

/* ═══════════════════════════════════════════════════════════════════════════
 * FORENSIC EXPORT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Export all candidates to JSON
 * 
 * @param[in]  session  Multi-decoder session
 * @param[in]  fp       Output file pointer
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_export_json(const uft_mdec_session_t *session, FILE *fp);

/**
 * @brief Export all candidates to Markdown report
 * 
 * @param[in]  session  Multi-decoder session
 * @param[in]  fp       Output file pointer
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_export_markdown(const uft_mdec_session_t *session, FILE *fp);

/**
 * @brief Export sector alternatives to JSON
 * 
 * @param[in]  sector   Sector with candidates
 * @param[in]  fp       Output file pointer
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_export_sector_json(const uft_mdec_sector_t *sector, FILE *fp);

/**
 * @brief Get statistics summary
 * 
 * @param[in]  session  Multi-decoder session
 * @param[out] stats    Statistics output
 * @return UFT_MDEC_OK on success
 */
int uft_mdec_get_statistics(const uft_mdec_session_t *session,
                            uft_mdec_statistics_t *stats);

/**
 * @brief Print session summary to stdout
 * 
 * @param[in] session   Multi-decoder session
 */
void uft_mdec_print_summary(const uft_mdec_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════
 * GUI INTEGRATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get display info for sector alternatives (GUI)
 * 
 * @param[in]  sector       Sector with candidates
 * @param[out] buffer       Output buffer (formatted string)
 * @param[in]  buffer_size  Buffer size
 * @return Number of bytes written
 */
int uft_mdec_format_alternatives(const uft_mdec_sector_t *sector,
                                 char *buffer,
                                 size_t buffer_size);

/**
 * @brief Get color code for confidence (GUI)
 * 
 * @param[in] confidence    Confidence value 0-100
 * @return RGBA color value (0xRRGGBBAA)
 */
uint32_t uft_mdec_confidence_color(float confidence);

/**
 * @brief Get icon/status string for resolution status (GUI)
 * 
 * @param[in] status        Resolution status
 * @return Status string (static)
 */
const char *uft_mdec_status_icon(uft_mdec_status_t status);

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get error message for error code
 * 
 * @param[in] error     Error code
 * @return Error message (static)
 */
const char *uft_mdec_error_string(uft_mdec_error_t error);

/**
 * @brief Get encoding name
 * 
 * @param[in] encoding  Encoding type
 * @return Encoding name (static)
 */
const char *uft_mdec_encoding_name(uft_mdec_encoding_t encoding);

/**
 * @brief Get ambiguity type name
 * 
 * @param[in] type      Ambiguity type
 * @return Type name (static)
 */
const char *uft_mdec_ambiguity_name(uft_mdec_ambiguity_t type);

/**
 * @brief Get strategy name
 * 
 * @param[in] strategy  Resolution strategy
 * @return Strategy name (static)
 */
const char *uft_mdec_strategy_name(uft_mdec_strategy_t strategy);

/**
 * @brief Get status name
 * 
 * @param[in] status    Resolution status
 * @return Status name (static)
 */
const char *uft_mdec_status_name(uft_mdec_status_t status);

/**
 * @brief Get provenance type name
 * 
 * @param[in] type      Provenance type
 * @return Type name (static)
 */
const char *uft_mdec_provenance_name(uft_mdec_provenance_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MULTI_DECODER_H */
