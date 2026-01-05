/**
 * @file uft_recovery_sector.h
 * @brief GOD MODE Sector-Level Recovery (Extended)
 * 
 * Sektor-Recovery:
 * - CRC-Fail akzeptieren (markiert)
 * - Datenfeld ohne Header retten
 * - Header ohne Datenfeld retten
 * - Mehrere Sektor-Kandidaten verwalten
 * - Best-of-N-Sektor-Rekonstruktion
 * - Größen-Heuristik (N-Code-Validierung)
 * - Duplikat-ID-Analyse
 * - Ghost-/Phantom-Sektor-Erkennung
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_SECTOR_H
#define UFT_RECOVERY_SECTOR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_MAX_SECTOR_CANDIDATES   8       /**< Max candidates per sector ID */
#define UFT_MAX_SECTORS_PER_TRACK   64      /**< Max sectors on one track */

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Sector header
 */
typedef struct {
    uint8_t  track;             /**< Track number */
    uint8_t  head;              /**< Head/side */
    uint8_t  sector;            /**< Sector number */
    uint8_t  size_code;         /**< Size code (N) */
    uint16_t crc;               /**< Header CRC */
    bool     crc_valid;         /**< CRC is valid */
    bool     is_recovered;      /**< Was recovered */
    uint8_t  confidence;        /**< Confidence 0-100 */
    size_t   bit_offset;        /**< Offset in track */
} uft_sector_header_t;

/**
 * @brief Sector data field
 */
typedef struct {
    uint8_t *data;              /**< Sector data */
    size_t   data_len;          /**< Data length */
    uint16_t crc;               /**< Data CRC */
    bool     crc_valid;         /**< CRC is valid */
    bool     crc_accepted;      /**< CRC fail but data accepted */
    bool     is_recovered;      /**< Was recovered */
    uint8_t  confidence;        /**< Confidence 0-100 */
    uint8_t *confidence_map;    /**< Per-byte confidence */
    size_t   bit_offset;        /**< Offset in track */
} uft_sector_data_t;

/**
 * @brief Sector candidate
 */
typedef struct {
    uft_sector_header_t header;
    uft_sector_data_t   data;
    bool has_header;            /**< Has valid header */
    bool has_data;              /**< Has valid data */
    uint8_t source_rev;         /**< Source revolution */
    double score;               /**< Candidate score */
    bool is_best;               /**< Is best candidate */
} uft_sector_candidate_t;

/**
 * @brief Sector ID (for multi-candidate management)
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    uft_sector_candidate_t *candidates;
    size_t candidate_count;
    uft_sector_candidate_t *best;
} uft_sector_id_t;

/**
 * @brief Ghost/Phantom sector
 */
typedef struct {
    size_t bit_offset;          /**< Where detected */
    uint8_t type;               /**< Type of ghost */
    bool has_partial_header;    /**< Has partial header */
    bool has_partial_data;      /**< Has partial data */
    uint8_t confidence;         /**< Detection confidence */
    bool is_protection;         /**< Likely copy protection */
} uft_ghost_sector_t;

#define UFT_GHOST_TYPE_PARTIAL  1   /**< Partial sector (cut off) */
#define UFT_GHOST_TYPE_OVERWRITTEN 2 /**< Overwritten by splice */
#define UFT_GHOST_TYPE_WEAK     3   /**< Weak/unstable */
#define UFT_GHOST_TYPE_DUPLICATE 4  /**< Duplicate ID */

/**
 * @brief Duplicate ID info
 */
typedef struct {
    uint8_t track;
    uint8_t head;
    uint8_t sector;
    size_t  occurrence_count;   /**< How many times this ID appears */
    size_t *bit_offsets;        /**< Offset of each occurrence */
    bool    is_protection;      /**< Intentional duplicate (protection) */
    uint8_t best_occurrence;    /**< Index of best occurrence */
} uft_duplicate_id_t;

/**
 * @brief Sector recovery options
 */
typedef struct {
    bool accept_crc_fail;       /**< Accept sectors with CRC errors */
    bool recover_header_only;   /**< Try to recover header without data */
    bool recover_data_only;     /**< Try to recover data without header */
    bool use_multiple_revs;     /**< Use multiple revolutions */
    uint8_t min_confidence;     /**< Minimum required confidence */
    bool preserve_duplicates;   /**< Keep duplicate IDs */
    bool detect_ghosts;         /**< Detect ghost sectors */
    bool validate_size_code;    /**< Validate N-code against data */
} uft_sector_recovery_opts_t;

/**
 * @brief Sector recovery context
 */
typedef struct {
    /* Track info */
    uint8_t track;
    uint8_t head;
    
    /* Detected sectors */
    uft_sector_id_t *sectors;
    size_t sector_count;
    
    /* Ghost sectors */
    uft_ghost_sector_t *ghosts;
    size_t ghost_count;
    
    /* Duplicate IDs */
    uft_duplicate_id_t *duplicates;
    size_t duplicate_count;
    
    /* Statistics */
    uint32_t headers_found;
    uint32_t headers_valid;
    uint32_t data_found;
    uint32_t data_valid;
    uint32_t recovered;
    uint32_t crc_accepted;
    
    /* Options */
    uft_sector_recovery_opts_t opts;
} uft_sector_recovery_ctx_t;

/* ============================================================================
 * CRC Handling
 * ============================================================================ */

/**
 * @brief Accept CRC fail but mark it
 * 
 * Akzeptiert Daten trotz CRC-Fehler, markiert sie aber.
 */
bool uft_sector_accept_crc_fail(uft_sector_data_t *data,
                                const char *reason);

/**
 * @brief Try to fix CRC error
 */
bool uft_sector_try_fix_crc(uft_sector_data_t *data,
                            int *bit_corrected);

/**
 * @brief Calculate CRC confidence
 */
uint8_t uft_sector_crc_confidence(const uft_sector_data_t *data,
                                  uint16_t expected_crc);

/* ============================================================================
 * Header/Data Separation
 * ============================================================================ */

/**
 * @brief Recover header without data field
 * 
 * Rettet Header auch wenn Datenfeld fehlt/beschädigt.
 */
bool uft_sector_recover_header_only(const uint8_t *track_data, size_t len,
                                    size_t search_start,
                                    uft_sector_header_t *header);

/**
 * @brief Recover data without header
 * 
 * Rettet Datenfeld auch wenn Header fehlt.
 * Verwendet Heuristiken zur Identifikation.
 */
bool uft_sector_recover_data_only(const uint8_t *track_data, size_t len,
                                  size_t search_start,
                                  uint8_t expected_size_code,
                                  uft_sector_data_t *data);

/**
 * @brief Match orphan headers with orphan data
 * 
 * Versucht Header ohne Daten mit Daten ohne Header zu matchen.
 */
size_t uft_sector_match_orphans(uft_sector_header_t *headers, size_t header_count,
                                uft_sector_data_t *datas, size_t data_count,
                                uft_sector_candidate_t **matched);

/* ============================================================================
 * Multi-Candidate Management
 * ============================================================================ */

/**
 * @brief Create sector ID tracker
 */
uft_sector_id_t* uft_sector_id_create(uint8_t track, uint8_t head, uint8_t sector);

/**
 * @brief Add candidate to sector ID
 */
bool uft_sector_id_add_candidate(uft_sector_id_t *id,
                                 const uft_sector_candidate_t *candidate);

/**
 * @brief Score all candidates
 */
void uft_sector_score_candidates(uft_sector_id_t *id);

/**
 * @brief Select best candidate
 */
uft_sector_candidate_t* uft_sector_select_best(uft_sector_id_t *id);

/**
 * @brief Free sector ID
 */
void uft_sector_id_free(uft_sector_id_t *id);

/* ============================================================================
 * Best-of-N Reconstruction
 * ============================================================================ */

/**
 * @brief Reconstruct sector from N candidates
 * 
 * Kombiniert beste Teile aus mehreren Kandidaten.
 */
bool uft_sector_reconstruct_best_of_n(const uft_sector_candidate_t *candidates,
                                      size_t count,
                                      uft_sector_candidate_t *result);

/**
 * @brief Merge header from one, data from another
 */
bool uft_sector_merge_header_data(const uft_sector_header_t *header,
                                  const uft_sector_data_t *data,
                                  uft_sector_candidate_t *result);

/**
 * @brief Vote on per-byte basis across candidates
 */
bool uft_sector_vote_bytes(const uft_sector_data_t *datas, size_t count,
                           uint8_t *output, uint8_t *confidence);

/* ============================================================================
 * Size Code (N) Validation
 * ============================================================================ */

/**
 * @brief Validate size code against data
 */
bool uft_sector_validate_size_code(const uft_sector_header_t *header,
                                   const uft_sector_data_t *data);

/**
 * @brief Infer size code from data length
 */
uint8_t uft_sector_infer_size_code(size_t data_len);

/**
 * @brief Check for common N-code values
 */
bool uft_sector_is_standard_size(uint8_t size_code);

/**
 * @brief Get expected data length from size code
 */
size_t uft_sector_size_code_to_bytes(uint8_t size_code);

/* ============================================================================
 * Duplicate ID Analysis
 * ============================================================================ */

/**
 * @brief Detect duplicate sector IDs
 */
size_t uft_sector_detect_duplicates(const uft_sector_header_t *headers,
                                    size_t count,
                                    uft_duplicate_id_t **duplicates);

/**
 * @brief Analyze duplicate (protection vs. error)
 */
void uft_sector_analyze_duplicate(uft_duplicate_id_t *dup,
                                  const uft_sector_candidate_t *candidates);

/**
 * @brief Handle intentional duplicates (preserve!)
 */
bool uft_sector_preserve_duplicates(const uft_duplicate_id_t *dups,
                                    size_t count,
                                    uft_sector_candidate_t **all_candidates,
                                    size_t *total_count);

/* ============================================================================
 * Ghost/Phantom Sector Detection
 * ============================================================================ */

/**
 * @brief Detect ghost sectors
 * 
 * Ghost = Sektor der teilweise existiert oder überschrieben wurde.
 */
size_t uft_sector_detect_ghosts(const uint8_t *track_data, size_t len,
                                const uft_sector_header_t *valid_headers,
                                size_t valid_count,
                                uft_ghost_sector_t **ghosts);

/**
 * @brief Classify ghost sector type
 */
void uft_sector_classify_ghost(uft_ghost_sector_t *ghost,
                               const uint8_t *track_data, size_t len);

/**
 * @brief Try to recover ghost sector
 */
bool uft_sector_recover_ghost(const uft_ghost_sector_t *ghost,
                              const uint8_t *track_data, size_t len,
                              uft_sector_candidate_t *result);

/**
 * @brief Check if ghost is copy protection
 */
bool uft_sector_ghost_is_protection(const uft_ghost_sector_t *ghost);

/* ============================================================================
 * Full Sector Recovery
 * ============================================================================ */

/**
 * @brief Create sector recovery context
 */
uft_sector_recovery_ctx_t* uft_sector_recovery_create(uint8_t track, uint8_t head);

/**
 * @brief Set recovery options
 */
void uft_sector_recovery_set_opts(uft_sector_recovery_ctx_t *ctx,
                                  const uft_sector_recovery_opts_t *opts);

/**
 * @brief Add track data (from one revolution)
 */
bool uft_sector_recovery_add_data(uft_sector_recovery_ctx_t *ctx,
                                  const uint8_t *track_data, size_t len,
                                  uint8_t rev_index);

/**
 * @brief Run full sector analysis
 */
void uft_sector_recovery_analyze(uft_sector_recovery_ctx_t *ctx);

/**
 * @brief Get recovered sectors
 */
size_t uft_sector_recovery_get_sectors(const uft_sector_recovery_ctx_t *ctx,
                                       uft_sector_candidate_t **sectors);

/**
 * @brief Get specific sector
 */
uft_sector_candidate_t* uft_sector_recovery_get(const uft_sector_recovery_ctx_t *ctx,
                                                uint8_t sector_num);

/**
 * @brief Generate report
 */
char* uft_sector_recovery_report(const uft_sector_recovery_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_sector_recovery_free(uft_sector_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_SECTOR_H */
