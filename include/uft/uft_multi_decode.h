#ifndef UFT_MULTI_DECODE_H
#define UFT_MULTI_DECODE_H

/**
 * @file uft_multi_decode.h
 * @brief UFT Multi-Interpretation Decoder - N-Best Hypothesis Management
 * 
 * Provides infrastructure for maintaining multiple decode interpretations
 * simultaneously, enabling forensic-grade preservation where ambiguous
 * data is not prematurely resolved to a single interpretation.
 * 
 * Key Features:
 * - N-Best candidate lists per sector with confidence scoring
 * - Lazy evaluation - interpretations resolved only when needed
 * - Candidate persistence for forensic export
 * - GUI-ready data structures for visualization
 * - Integration with audit trail system
 * 
 * Architecture:
 * ```
 * ┌─────────────────────────────────────────────────────────┐
 * │                  Multi-Decode Session                   │
 * │  ┌─────────────────────────────────────────────────┐   │
 * │  │              Track Candidate Set                 │   │
 * │  │  ┌─────────────────────────────────────────┐    │   │
 * │  │  │         Sector Candidate List           │    │   │
 * │  │  │  ┌─────────────────────────────────┐   │    │   │
 * │  │  │  │    Decode Candidate (N-Best)    │   │    │   │
 * │  │  │  │  • Data bytes                   │   │    │   │
 * │  │  │  │  • Confidence score             │   │    │   │
 * │  │  │  │  • Decode method                │   │    │   │
 * │  │  │  │  • Error correction applied     │   │    │   │
 * │  │  │  │  • Source revolution(s)         │   │    │   │
 * │  │  │  └─────────────────────────────────┘   │    │   │
 * │  │  └─────────────────────────────────────────┘    │   │
 * │  └─────────────────────────────────────────────────┘   │
 * └─────────────────────────────────────────────────────────┘
 * ```
 * 
 * @version 3.2.0.003
 * @date 2026-01-04
 * @author UFT Development Team
 * 
 * SPDX-License-Identifier: MIT
 */


#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * CONFIGURATION CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

/** Maximum candidates per sector (N-Best list size) */
#define UFT_MD_MAX_CANDIDATES       16

/** Maximum sectors per track */
#define UFT_MD_MAX_SECTORS          32

/** Maximum tracks in session */
#define UFT_MD_MAX_TRACKS          168

/** Maximum sector data size */
#define UFT_MD_MAX_SECTOR_SIZE    8192

/** Maximum decode methods tracked */
#define UFT_MD_MAX_METHODS          8

/** Maximum source revolutions referenced */
#define UFT_MD_MAX_REVOLUTIONS     16

/** Maximum decode notes length */
#define UFT_MD_MAX_NOTES_LEN      256

/** Confidence threshold for "good" decode */
#define UFT_MD_CONFIDENCE_GOOD     85

/** Confidence threshold for "acceptable" decode */
#define UFT_MD_CONFIDENCE_ACCEPT   60

/** Confidence threshold for "marginal" decode */
#define UFT_MD_CONFIDENCE_MARGINAL 40

/* ═══════════════════════════════════════════════════════════════════════════
 * ENUMERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Decode method identifiers
 */
typedef enum uft_decode_method {
    UFT_DM_NONE             = 0x0000,  /**< No decode attempted */
    
    /* MFM Methods */
    UFT_DM_MFM_STANDARD     = 0x0100,  /**< Standard MFM decode */
    UFT_DM_MFM_PLL_TIGHT    = 0x0101,  /**< MFM with tight PLL */
    UFT_DM_MFM_PLL_LOOSE    = 0x0102,  /**< MFM with loose PLL */
    UFT_DM_MFM_MULTI_REV    = 0x0103,  /**< MFM multi-revolution fusion */
    UFT_DM_MFM_WEAK_BIT     = 0x0104,  /**< MFM with weak bit handling */
    
    /* GCR Methods */
    UFT_DM_GCR_C64          = 0x0200,  /**< Commodore 64 GCR */
    UFT_DM_GCR_APPLE        = 0x0201,  /**< Apple II/III GCR */
    UFT_DM_GCR_APPLE_NIB    = 0x0202,  /**< Apple nibble-level */
    UFT_DM_GCR_VICTOR       = 0x0203,  /**< Victor 9000 GCR */
    
    /* FM Methods */
    UFT_DM_FM_STANDARD      = 0x0300,  /**< Standard FM decode */
    UFT_DM_FM_INTEL         = 0x0301,  /**< Intel 8271 FM */
    
    /* Special Methods */
    UFT_DM_RAW_BITSTREAM    = 0x0400,  /**< Raw bitstream (no decode) */
    UFT_DM_FLUX_DIRECT      = 0x0401,  /**< Direct flux interpretation */
    UFT_DM_PROTECTION_AWARE = 0x0402,  /**< Protection-aware decode */
    
    /* Error Correction Methods */
    UFT_DM_ECC_CRC_REPAIR   = 0x0500,  /**< CRC-based repair */
    UFT_DM_ECC_INTERLEAVE   = 0x0501,  /**< Interleave reconstruction */
    UFT_DM_ECC_REED_SOLOMON = 0x0502,  /**< Reed-Solomon ECC */
    UFT_DM_ECC_HAMMING      = 0x0503,  /**< Hamming code repair */
    
    /* Fusion Methods */
    UFT_DM_FUSION_VOTING    = 0x0600,  /**< Multi-revolution voting */
    UFT_DM_FUSION_WEIGHTED  = 0x0601,  /**< Confidence-weighted fusion */
    UFT_DM_FUSION_CONSENSUS = 0x0602,  /**< Consensus from all methods */
    
    UFT_DM_METHOD_COUNT     = 0x0700   /**< Sentinel for iteration */
} uft_decode_method_t;

/**
 * @brief Candidate status flags
 */
typedef enum uft_candidate_status {
    UFT_CS_PENDING      = 0x00,  /**< Candidate not yet evaluated */
    UFT_CS_VALID        = 0x01,  /**< Candidate passes all checks */
    UFT_CS_CRC_FAIL     = 0x02,  /**< CRC check failed */
    UFT_CS_CHECKSUM_FAIL= 0x04,  /**< Checksum failed */
    UFT_CS_REPAIRED     = 0x08,  /**< Data was repaired */
    UFT_CS_UNCERTAIN    = 0x10,  /**< Contains uncertain bits */
    UFT_CS_WEAK_BITS    = 0x20,  /**< Contains weak bit regions */
    UFT_CS_SYNTHESIZED  = 0x40,  /**< Data was synthesized/estimated */
    UFT_CS_BEST_EFFORT  = 0x80   /**< Best effort, not verified */
} uft_candidate_status_t;

/**
 * @brief Resolution strategy for selecting final candidate
 */
typedef enum uft_resolution_strategy {
    UFT_RS_HIGHEST_CONFIDENCE,  /**< Select highest confidence score */
    UFT_RS_CRC_PRIORITY,        /**< Prefer CRC-valid candidates */
    UFT_RS_MULTI_REV_FUSION,    /**< Fuse multi-revolution candidates */
    UFT_RS_CONSENSUS_VOTING,    /**< Bit-level consensus voting */
    UFT_RS_USER_SELECT,         /**< Defer to user selection */
    UFT_RS_FORENSIC_ALL         /**< Export all candidates (no resolution) */
} uft_resolution_strategy_t;

/**
 * @brief Candidate comparison result
 */
typedef enum uft_candidate_cmp {
    UFT_CMP_IDENTICAL,     /**< Candidates are byte-identical */
    UFT_CMP_EQUIVALENT,    /**< Same data, different metadata */
    UFT_CMP_DIFFERENT,     /**< Data differs */
    UFT_CMP_SUBSET,        /**< One is subset of other */
    UFT_CMP_CONFLICT       /**< Conflicting interpretations */
} uft_candidate_cmp_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * DATA STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Bit-level uncertainty map
 * 
 * Tracks which bits within a sector have uncertainty from
 * weak bit detection, multi-revolution disagreement, etc.
 */
typedef struct uft_uncertainty_map {
    uint8_t     bitmap[UFT_MD_MAX_SECTOR_SIZE];  /**< Bit uncertainty flags */
    uint32_t    uncertain_count;                  /**< Total uncertain bits */
    uint32_t    weak_regions;                     /**< Number of weak regions */
    float       overall_certainty;                /**< 0.0-1.0 overall score */
} uft_uncertainty_map_t;

/**
 * @brief Source information for a decode candidate
 */
typedef struct uft_decode_source {
    uint8_t     revolution_mask;                 /**< Which revolutions used */
    uint32_t    flux_offset_start;               /**< Start in flux stream */
    uint32_t    flux_offset_end;                 /**< End in flux stream */
    uint32_t    bitstream_offset;                /**< Offset in bitstream */
    float       pll_phase_error_avg;             /**< Average PLL phase error */
    float       pll_frequency_deviation;         /**< PLL frequency deviation */
} uft_decode_source_t;

/**
 * @brief Error correction details
 */
typedef struct uft_ecc_details {
    bool        ecc_applied;                     /**< Was ECC applied? */
    uint16_t    bits_corrected;                  /**< Bits corrected */
    uint16_t    bytes_affected;                  /**< Bytes modified */
    uint8_t     correction_method;               /**< Method used */
    float       correction_confidence;           /**< Confidence in correction */
    uint32_t    syndrome;                        /**< CRC/ECC syndrome value */
} uft_ecc_details_t;

/**
 * @brief Single decode candidate for a sector
 * 
 * Represents one possible interpretation of sector data,
 * complete with confidence scoring and provenance.
 */
typedef struct uft_decode_candidate {
    /* Identity */
    uint32_t            candidate_id;            /**< Unique candidate ID */
    uint32_t            sector_id;               /**< Logical sector number */
    
    /* Decoded Data */
    uint8_t             data[UFT_MD_MAX_SECTOR_SIZE]; /**< Decoded bytes */
    uint32_t            data_size;               /**< Actual data size */
    uint32_t            data_crc32;              /**< CRC32 of data */
    
    /* Confidence & Status */
    float               confidence;              /**< 0.0-100.0 confidence */
    uft_candidate_status_t status;               /**< Status flags */
    
    /* Decode Provenance */
    uft_decode_method_t primary_method;          /**< Primary decode method */
    uft_decode_method_t secondary_methods[UFT_MD_MAX_METHODS]; /**< Additional methods */
    uint8_t             method_count;            /**< Number of methods used */
    
    /* Source Information */
    uft_decode_source_t source;                  /**< Flux/bitstream source info */
    
    /* Error Correction */
    uft_ecc_details_t   ecc;                     /**< Error correction details */
    
    /* Uncertainty */
    uft_uncertainty_map_t *uncertainty;          /**< Optional uncertainty map */
    
    /* Metadata */
    char                notes[UFT_MD_MAX_NOTES_LEN]; /**< Human-readable notes */
    uint64_t            timestamp_ns;            /**< Decode timestamp */
    
    /* Linkage */
    struct uft_decode_candidate *alternative;    /**< Next alternative (linked list) */
} uft_decode_candidate_t;

/**
 * @brief N-Best list for a single sector
 * 
 * Contains all candidate interpretations for one sector,
 * sorted by confidence score.
 */
typedef struct uft_sector_candidates {
    /* Identification */
    uint8_t             track;                   /**< Physical track */
    uint8_t             head;                    /**< Physical head/side */
    uint8_t             sector;                  /**< Logical sector */
    
    /* Candidate List */
    uft_decode_candidate_t *candidates[UFT_MD_MAX_CANDIDATES];
    uint8_t             candidate_count;         /**< Number of candidates */
    
    /* Best Candidate (lazy evaluated) */
    uft_decode_candidate_t *resolved;            /**< Resolved best candidate */
    bool                is_resolved;             /**< Has been resolved? */
    int                 selected_index;          /**< Index of selected candidate */
    uint8_t             sector_num;              /**< Sector number alias */
    uft_resolution_strategy_t resolution_used;   /**< Strategy that was used */
    
    /* Statistics */
    float               confidence_spread;       /**< Max - min confidence */
    float               data_agreement;          /**< % bytes all agree on */
    bool                has_conflict;            /**< Conflicting interpretations? */
    
    /* Sector Format Info */
    uint16_t            expected_size;           /**< Expected sector size */
    bool                size_verified;           /**< Size matches expected? */
} uft_sector_candidates_t;

/**
 * @brief Track-level candidate set
 */
typedef struct uft_track_candidates {
    uint8_t             track;                   /**< Physical track number */
    uint8_t             head;                    /**< Physical head/side */
    
    /* Sector Candidates */
    uft_sector_candidates_t *sectors[UFT_MD_MAX_SECTORS];
    uint8_t             sector_count;            /**< Number of sectors */
    
    /* Track-Level Statistics */
    float               avg_confidence;          /**< Average across sectors */
    float               min_confidence;          /**< Minimum confidence */
    uint8_t             unresolved_count;        /**< Sectors not yet resolved */
    uint8_t             conflict_count;          /**< Sectors with conflicts */
    
    /* Track Format Info */
    uint16_t            expected_sectors;        /**< Expected sector count */
    bool                sector_count_verified;   /**< Count matches expected? */
} uft_track_candidates_t;

/**
 * @brief Multi-decode session configuration
 */
typedef struct uft_md_config {
    /* Candidate Generation */
    uint8_t             max_candidates;          /**< Max per sector (1-16) */
    float               min_confidence;          /**< Min confidence to keep */
    bool                generate_all_methods;    /**< Try all decode methods */
    
    /* Resolution Settings */
    uft_resolution_strategy_t default_strategy;  /**< Default resolution */
    float               auto_resolve_threshold;  /**< Auto-resolve if >= this */
    bool                preserve_all;            /**< Keep all even after resolve */
    
    /* Memory Management */
    bool                lazy_alloc;              /**< Allocate uncertainty maps lazily */
    bool                stream_mode;             /**< Stream-process (low memory) */
    uint32_t            memory_limit_mb;         /**< Memory limit (0=unlimited) */
    
    /* Forensic Options */
    bool                track_provenance;        /**< Full source tracking */
    bool                record_timing;           /**< Timing information */
    bool                enable_audit;            /**< Audit trail integration */
} uft_md_config_t;

/**
 * @brief Multi-decode session state
 */
typedef struct uft_md_session {
    /* Session Identity */
    uint8_t             session_uuid[16];        /**< Session UUID */
    uint64_t            created_timestamp;       /**< Creation time (ns) */
    
    /* Configuration */
    uft_md_config_t     config;                  /**< Session configuration */
    
    /* Track Data */
    uft_track_candidates_t *tracks[UFT_MD_MAX_TRACKS];
    uint16_t            track_count;             /**< Number of tracks */
    
    /* Statistics Struct */
    struct {
        uint32_t total_sectors;
        uint32_t resolved_count;
        uint32_t conflict_count;
        float avg_confidence;
    } stats;

    /* Session Statistics */
    uint32_t            total_candidates;        /**< Total candidates generated */
    uint32_t            resolved_sectors;        /**< Sectors resolved */
    uint32_t            pending_sectors;         /**< Sectors pending resolution */
    uint32_t            conflict_sectors;        /**< Sectors with conflicts */
    
    /* Memory Tracking */
    size_t              memory_used;             /**< Bytes allocated */
    size_t              peak_memory;             /**< Peak memory usage */
    
    /* State */
    bool                finalized;               /**< Session finalized? */
} uft_md_session_t;

/**
 * @brief Decode candidate iterator
 */
typedef struct uft_candidate_iter {
    uft_md_session_t    *session;                /**< Parent session */
    uint16_t            track_idx;               /**< Current track index */
    uint8_t             sector_idx;              /**< Current sector index */
    uint8_t             candidate_idx;           /**< Current candidate index */
    bool                include_resolved;        /**< Include resolved sectors */
    float               min_confidence;          /**< Min confidence filter */
} uft_candidate_iter_t;

/**
 * @brief Resolution result
 */
typedef struct uft_resolution_result {
    uft_decode_candidate_t *selected;            /**< Selected candidate */
    float               confidence_margin;       /**< Margin over runner-up */
    uint8_t             alternatives_count;      /**< How many alternatives */
    bool                unanimous;               /**< All methods agreed */
    char                rationale[UFT_MD_MAX_NOTES_LEN]; /**< Why this was selected */
} uft_resolution_result_t;

/**
 * @brief Export options for forensic reports
 */
typedef struct uft_md_export_opts {
    bool                include_all_candidates;  /**< All or just resolved */
    bool                include_hex_dump;        /**< Hex dump of data */
    bool                include_uncertainty;     /**< Uncertainty maps */
    bool                include_diff;            /**< Diff between candidates */
    bool                include_source_info;     /**< Flux/bitstream sources */
    bool                include_timing;          /**< Timing information */
    const char          *output_path;            /**< Output file path */
} uft_md_export_opts_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * SESSION MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new multi-decode session
 * 
 * @param config Session configuration (NULL for defaults)
 * @return New session or NULL on error
 */
uft_md_session_t *uft_md_session_create(const uft_md_config_t *config);

/**
 * @brief Initialize session with default configuration
 * 
 * @param session Session to initialize
 * @return UFT_OK on success
 */
int uft_md_session_init_defaults(uft_md_session_t *session);

/**
 * @brief Destroy a multi-decode session
 * 
 * @param session Session to destroy
 */
void uft_md_session_destroy(uft_md_session_t *session);

/**
 * @brief Reset session for reuse
 * 
 * @param session Session to reset
 */
void uft_md_session_reset(uft_md_session_t *session);

/**
 * @brief Finalize session (no more candidates can be added)
 * 
 * @param session Session to finalize
 * @return UFT_OK on success
 */
int uft_md_session_finalize(uft_md_session_t *session);

/* ═══════════════════════════════════════════════════════════════════════════
 * CANDIDATE MANAGEMENT
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create a new decode candidate
 * 
 * @param sector_id Logical sector number
 * @param data Decoded data bytes
 * @param data_size Size of decoded data
 * @param method Decode method used
 * @param confidence Confidence score (0-100)
 * @return New candidate or NULL on error
 */
uft_decode_candidate_t *uft_md_candidate_create(
    uint32_t sector_id,
    const uint8_t *data,
    uint32_t data_size,
    uft_decode_method_t method,
    float confidence
);

/**
 * @brief Clone a decode candidate
 * 
 * @param src Source candidate
 * @return Cloned candidate or NULL on error
 */
uft_decode_candidate_t *uft_md_candidate_clone(const uft_decode_candidate_t *src);

/**
 * @brief Destroy a decode candidate
 * 
 * @param candidate Candidate to destroy
 */
void uft_md_candidate_destroy(uft_decode_candidate_t *candidate);

/**
 * @brief Add a candidate to a session
 * 
 * @param session Target session
 * @param track Physical track
 * @param head Physical head/side
 * @param sector Logical sector
 * @param candidate Candidate to add (ownership transferred)
 * @return UFT_OK on success
 */
int uft_md_add_candidate(
    uft_md_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector,
    uft_decode_candidate_t *candidate
);

/**
 * @brief Get sector candidates
 * 
 * @param session Session to query
 * @param track Physical track
 * @param head Physical head/side
 * @param sector Logical sector
 * @return Sector candidates or NULL if not found
 */
uft_sector_candidates_t *uft_md_get_sector(
    uft_md_session_t *session,
    uint8_t track,
    uint8_t head,
    uint8_t sector
);

/**
 * @brief Get track candidates
 * 
 * @param session Session to query
 * @param track Physical track
 * @param head Physical head/side
 * @return Track candidates or NULL if not found
 */
uft_track_candidates_t *uft_md_get_track(
    uft_md_session_t *session,
    uint8_t track,
    uint8_t head
);

/* ═══════════════════════════════════════════════════════════════════════════
 * CANDIDATE COMPARISON
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Compare two candidates
 * 
 * @param a First candidate
 * @param b Second candidate
 * @return Comparison result
 */
uft_candidate_cmp_t uft_md_compare_candidates(
    const uft_decode_candidate_t *a,
    const uft_decode_candidate_t *b
);

/**
 * @brief Calculate data agreement between candidates
 * 
 * @param candidates Array of candidates
 * @param count Number of candidates
 * @param agreement_out Output: per-byte agreement count
 * @return Percentage of bytes where all agree (0.0-100.0)
 */
float uft_md_calculate_agreement(
    uft_decode_candidate_t * const *candidates,
    uint8_t count,
    uint8_t *agreement_out
);

/**
 * @brief Find differing bytes between candidates
 * 
 * @param a First candidate
 * @param b Second candidate
 * @param diff_offsets Output: array of differing offsets
 * @param max_diffs Maximum diffs to report
 * @return Number of differences found
 */
uint32_t uft_md_find_differences(
    const uft_decode_candidate_t *a,
    const uft_decode_candidate_t *b,
    uint32_t *diff_offsets,
    uint32_t max_diffs
);

/* ═══════════════════════════════════════════════════════════════════════════
 * RESOLUTION (LAZY EVALUATION)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Resolve best candidate for a sector
 * 
 * Uses the specified strategy to select the best candidate.
 * Result is cached for subsequent calls.
 * 
 * @param sector Sector candidates to resolve
 * @param strategy Resolution strategy
 * @param result Output: resolution details
 * @return Best candidate or NULL if no valid candidate
 */
uft_decode_candidate_t *uft_md_resolve_sector(
    uft_sector_candidates_t *sector,
    uft_resolution_strategy_t strategy,
    uft_resolution_result_t *result
);

/**
 * @brief Resolve all sectors in a track
 * 
 * @param track Track to resolve
 * @param strategy Resolution strategy
 * @return Number of sectors successfully resolved
 */
uint8_t uft_md_resolve_track(
    uft_track_candidates_t *track,
    uft_resolution_strategy_t strategy
);

/**
 * @brief Resolve all sectors in a session
 * 
 * @param session Session to resolve
 * @param strategy Resolution strategy
 * @return Number of sectors successfully resolved
 */
uint32_t uft_md_resolve_all(
    uft_md_session_t *session,
    uft_resolution_strategy_t strategy
);

/**
 * @brief Force re-resolution of a sector
 * 
 * @param sector Sector to re-resolve
 * @param strategy New strategy to use
 * @return New best candidate
 */
uft_decode_candidate_t *uft_md_re_resolve(
    uft_sector_candidates_t *sector,
    uft_resolution_strategy_t strategy
);

/**
 * @brief Manually select a candidate as resolved
 * 
 * @param sector Sector candidates
 * @param candidate_idx Index of candidate to select
 * @param rationale User's rationale for selection
 * @return UFT_OK on success
 */
int uft_md_user_select(
    uft_sector_candidates_t *sector,
    uint8_t candidate_idx,
    const char *rationale
);

/* ═══════════════════════════════════════════════════════════════════════════
 * MULTI-REVOLUTION FUSION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Fuse candidates from multiple revolutions
 * 
 * Creates a new consensus candidate by combining information
 * from multiple revolution reads.
 * 
 * @param candidates Array of candidates (one per revolution)
 * @param count Number of candidates
 * @param voting_threshold Minimum agreement ratio (0.0-1.0)
 * @return Fused candidate or NULL on failure
 */
uft_decode_candidate_t *uft_md_fuse_revolutions(
    uft_decode_candidate_t * const *candidates,
    uint8_t count,
    float voting_threshold
);

/**
 * @brief Create weighted fusion of candidates
 * 
 * @param candidates Array of candidates
 * @param weights Confidence weights for each
 * @param count Number of candidates
 * @return Fused candidate or NULL on failure
 */
uft_decode_candidate_t *uft_md_fuse_weighted(
    uft_decode_candidate_t * const *candidates,
    const float *weights,
    uint8_t count
);

/* ═══════════════════════════════════════════════════════════════════════════
 * ITERATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Initialize a candidate iterator
 * 
 * @param session Session to iterate
 * @param iter Iterator to initialize
 * @param include_resolved Include resolved sectors
 * @param min_confidence Minimum confidence filter
 */
void uft_md_iter_init(
    uft_md_session_t *session,
    uft_candidate_iter_t *iter,
    bool include_resolved,
    float min_confidence
);

/**
 * @brief Get next candidate from iterator
 * 
 * @param iter Iterator
 * @return Next candidate or NULL if exhausted
 */
uft_decode_candidate_t *uft_md_iter_next(uft_candidate_iter_t *iter);

/**
 * @brief Get next sector from iterator
 * 
 * @param iter Iterator
 * @return Next sector candidates or NULL if exhausted
 */
uft_sector_candidates_t *uft_md_iter_next_sector(uft_candidate_iter_t *iter);

/* ═══════════════════════════════════════════════════════════════════════════
 * EXPORT & REPORTING
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Export session to JSON
 * 
 * @param session Session to export
 * @param opts Export options
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error code
 */
int uft_md_export_json(
    const uft_md_session_t *session,
    const uft_md_export_opts_t *opts,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Export session to Markdown report
 * 
 * @param session Session to export
 * @param opts Export options
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error code
 */
int uft_md_export_markdown(
    const uft_md_session_t *session,
    const uft_md_export_opts_t *opts,
    char *buffer,
    size_t buffer_size
);

/**
 * @brief Export to forensic report file
 * 
 * @param session Session to export
 * @param opts Export options
 * @return UFT_OK on success
 */
int uft_md_export_forensic_report(
    const uft_md_session_t *session,
    const uft_md_export_opts_t *opts
);

/**
 * @brief Generate diff report between two candidates
 * 
 * @param a First candidate
 * @param b Second candidate
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Bytes written or negative error code
 */
int uft_md_generate_diff(
    const uft_decode_candidate_t *a,
    const uft_decode_candidate_t *b,
    char *buffer,
    size_t buffer_size
);

/* ═══════════════════════════════════════════════════════════════════════════
 * STATISTICS & QUERIES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get session statistics
 * 
 * @param session Session to query
 * @param total_sectors Output: total sectors
 * @param resolved_count Output: resolved count
 * @param conflict_count Output: conflict count
 * @param avg_confidence Output: average confidence
 */
void uft_md_get_stats(
    const uft_md_session_t *session,
    uint32_t *total_sectors,
    uint32_t *resolved_count,
    uint32_t *conflict_count,
    float *avg_confidence
);

/**
 * @brief Find sectors with conflicts
 * 
 * @param session Session to query
 * @param sectors Output: array of sector pointers
 * @param max_sectors Maximum to return
 * @return Number of conflict sectors found
 */
uint32_t uft_md_find_conflicts(
    const uft_md_session_t *session,
    uft_sector_candidates_t **sectors,
    uint32_t max_sectors
);

/**
 * @brief Find sectors below confidence threshold
 * 
 * @param session Session to query
 * @param threshold Confidence threshold
 * @param sectors Output: array of sector pointers
 * @param max_sectors Maximum to return
 * @return Number of low-confidence sectors found
 */
uint32_t uft_md_find_low_confidence(
    const uft_md_session_t *session,
    float threshold,
    uft_sector_candidates_t **sectors,
    uint32_t max_sectors
);

/* ═══════════════════════════════════════════════════════════════════════════
 * UTILITY FUNCTIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get method name string
 * 
 * @param method Decode method
 * @return Human-readable method name
 */
const char *uft_md_method_name(uft_decode_method_t method);

/**
 * @brief Get status string
 * 
 * @param status Candidate status
 * @return Human-readable status string
 */
const char *uft_md_status_name(uft_candidate_status_t status);

/**
 * @brief Get strategy name string
 * 
 * @param strategy Resolution strategy
 * @return Human-readable strategy name
 */
const char *uft_md_strategy_name(uft_resolution_strategy_t strategy);

/**
 * @brief Calculate candidate fingerprint (for deduplication)
 * 
 * @param candidate Candidate to fingerprint
 * @param fingerprint Output: 32-byte fingerprint
 */
void uft_md_calculate_fingerprint(
    const uft_decode_candidate_t *candidate,
    uint8_t fingerprint[32]
);

/**
 * @brief Check if candidate data matches CRC
 * 
 * @param candidate Candidate to check
 * @param expected_crc Expected CRC value
 * @return true if CRC matches
 */
bool uft_md_verify_crc(
    const uft_decode_candidate_t *candidate,
    uint32_t expected_crc
);

/**
 * @brief Default configuration values
 * 
 * @param config Configuration to fill with defaults
 */
void uft_md_config_defaults(uft_md_config_t *config);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MULTI_DECODE_H */
