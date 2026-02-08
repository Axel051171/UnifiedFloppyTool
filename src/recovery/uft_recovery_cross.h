/**
 * @file uft_recovery_cross.h
 * @brief GOD MODE Cross-Track Recovery
 * 
 * Cross-Track-Recovery:
 * - Vergleich identischer Sektoren über Tracks
 * - Interleave-Rekonstruktion
 * - Pattern-Wiederholung erkennen
 * - Bootsektor-Redundanz nutzen
 * - Directory-Struktur-Konsistenz prüfen
 * - Side-to-Side-Vergleich (Head 0 ↔ Head 1)
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_CROSS_H
#define UFT_RECOVERY_CROSS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Cross-track sector comparison
 */
typedef struct {
    uint8_t track_a;
    uint8_t track_b;
    uint8_t sector;
    double  similarity;         /**< 0-1 similarity */
    bool    are_identical;      /**< Byte-for-byte identical */
    size_t  diff_count;         /**< Number of differing bytes */
    size_t *diff_positions;     /**< Positions of differences */
} uft_cross_sector_cmp_t;

/**
 * @brief Interleave analysis
 */
typedef struct {
    uint8_t interleave;         /**< Detected interleave value */
    uint8_t *sector_order;      /**< Physical sector order */
    size_t  sector_count;       /**< Number of sectors */
    double  confidence;         /**< Confidence in detection */
    bool    is_standard;        /**< Standard interleave for format */
} uft_interleave_info_t;

/**
 * @brief Pattern repetition
 */
typedef struct {
    uint8_t *pattern;           /**< Repeated pattern */
    size_t   pattern_len;       /**< Pattern length */
    size_t   repeat_count;      /**< How many times repeated */
    size_t  *positions;         /**< Where it occurs */
    bool     is_fill;           /**< Fill pattern (e.g., 0xE5) */
    bool     is_format_marker;  /**< Format marker pattern */
} uft_pattern_repeat_t;

/**
 * @brief Boot sector info
 */
typedef struct {
    bool     found;             /**< Boot sector found */
    uint8_t  track;             /**< Track number */
    uint8_t  sector;            /**< Sector number */
    uint8_t *data;              /**< Boot sector data */
    size_t   data_len;          /**< Data length */
    bool     has_backup;        /**< Has backup copy */
    uint8_t  backup_track;      /**< Backup track */
    uint8_t  backup_sector;     /**< Backup sector */
    double   backup_similarity; /**< Similarity to primary */
} uft_boot_sector_t;

/**
 * @brief Directory consistency check
 */
typedef struct {
    bool     consistent;        /**< Directory is consistent */
    uint32_t errors_found;      /**< Number of errors */
    char   **error_messages;    /**< Error descriptions */
    size_t   error_count;       /**< Number of messages */
    bool     can_use_for_recovery; /**< Can use dir for recovery hints */
} uft_dir_consistency_t;

/**
 * @brief Side-to-side comparison
 */
typedef struct {
    uint8_t  track;             /**< Track number */
    double   similarity;        /**< Overall similarity */
    bool     head0_better;      /**< Head 0 has better data */
    bool     head1_better;      /**< Head 1 has better data */
    size_t   sectors_recoverable; /**< Sectors recoverable from other side */
    uint8_t *sector_source;     /**< Which head for each sector (0, 1, or 2=both) */
} uft_side_comparison_t;

/**
 * @brief Cross-track recovery context
 */
typedef struct {
    /* Disk data */
    uint8_t  **track_data;      /**< All tracks [track][head] */
    size_t   *track_lengths;    /**< Track lengths */
    uint8_t   track_count;      /**< Number of tracks */
    uint8_t   head_count;       /**< Number of heads */
    
    /* Analysis results */
    uft_cross_sector_cmp_t **sector_cmps;
    size_t   cmp_count;
    
    uft_interleave_info_t interleave;
    
    uft_pattern_repeat_t *patterns;
    size_t   pattern_count;
    
    uft_boot_sector_t boot;
    
    uft_dir_consistency_t dir_check;
    
    uft_side_comparison_t *side_cmps;
    size_t   side_cmp_count;
} uft_cross_recovery_ctx_t;

/* ============================================================================
 * Cross-Track Sector Comparison
 * ============================================================================ */

/**
 * @brief Compare same sector across tracks
 * 
 * Vergleicht identische Sektor-IDs auf verschiedenen Tracks.
 */
void uft_cross_compare_sectors(const uint8_t *sector_a, size_t len_a,
                               const uint8_t *sector_b, size_t len_b,
                               uft_cross_sector_cmp_t *result);

/**
 * @brief Find matching sectors across disk
 */
size_t uft_cross_find_matching(const uft_cross_recovery_ctx_t *ctx,
                               const uint8_t *target_sector, size_t len,
                               uft_cross_sector_cmp_t **matches);

/**
 * @brief Use matching sector to recover damaged one
 */
bool uft_cross_recover_from_match(const uft_cross_sector_cmp_t *match,
                                  const uint8_t *good_sector,
                                  uint8_t *damaged_sector,
                                  size_t len);

/* ============================================================================
 * Interleave Analysis
 * ============================================================================ */

/**
 * @brief Detect interleave from track
 */
void uft_cross_detect_interleave(const uint8_t *track_data, size_t len,
                                 uint8_t expected_sectors,
                                 uft_interleave_info_t *result);

/**
 * @brief Reconstruct interleave from partial data
 */
bool uft_cross_reconstruct_interleave(const uint8_t *sector_order,
                                      size_t known_count,
                                      size_t total_sectors,
                                      uft_interleave_info_t *result);

/**
 * @brief Apply interleave knowledge to find missing sectors
 */
size_t uft_cross_find_by_interleave(const uint8_t *track_data, size_t len,
                                    const uft_interleave_info_t *interleave,
                                    uint8_t missing_sector,
                                    size_t *expected_offset);

/* ============================================================================
 * Pattern Recognition
 * ============================================================================ */

/**
 * @brief Detect repeated patterns
 */
size_t uft_cross_detect_patterns(const uint8_t *data, size_t len,
                                 size_t min_pattern_len,
                                 size_t min_repeats,
                                 uft_pattern_repeat_t **patterns);

/**
 * @brief Identify known fill patterns
 */
bool uft_cross_identify_fill(const uft_pattern_repeat_t *pattern);

/**
 * @brief Use pattern to fill gaps
 * 
 * Wenn bekanntes Pattern, verwende es um Lücken zu füllen.
 * Markiert als "inferiert".
 */
bool uft_cross_fill_gaps_with_pattern(uint8_t *data, size_t len,
                                      const uint8_t *gap_map,
                                      const uft_pattern_repeat_t *pattern);

/* ============================================================================
 * Boot Sector Redundancy
 * ============================================================================ */

/**
 * @brief Locate boot sector(s)
 */
void uft_cross_find_boot_sector(const uft_cross_recovery_ctx_t *ctx,
                                uft_boot_sector_t *result);

/**
 * @brief Find boot sector backup
 */
bool uft_cross_find_boot_backup(const uft_cross_recovery_ctx_t *ctx,
                                const uft_boot_sector_t *primary,
                                uft_boot_sector_t *backup);

/**
 * @brief Recover boot sector from backup
 */
bool uft_cross_recover_boot_from_backup(const uft_boot_sector_t *backup,
                                        uft_boot_sector_t *primary);

/**
 * @brief Compare boot sector with backup
 */
double uft_cross_compare_boot_backup(const uft_boot_sector_t *primary,
                                     const uft_boot_sector_t *backup);

/* ============================================================================
 * Directory Structure
 * ============================================================================ */

/**
 * @brief Check directory consistency
 * 
 * Prüft Directory-Struktur auf Konsistenz.
 * Read-Only - ändert nichts!
 */
void uft_cross_check_directory(const uft_cross_recovery_ctx_t *ctx,
                               uint8_t dir_track, uint8_t dir_sector,
                               uft_dir_consistency_t *result);

/**
 * @brief Extract hints from directory for recovery
 */
size_t uft_cross_dir_recovery_hints(const uft_dir_consistency_t *dir,
                                    uint8_t **expected_sectors,
                                    size_t *expected_count);

/**
 * @brief Validate sector against directory entry
 */
bool uft_cross_validate_vs_directory(const uint8_t *sector_data, size_t len,
                                     const uft_dir_consistency_t *dir,
                                     uint8_t sector_num);

/* ============================================================================
 * Side-to-Side Recovery
 * ============================================================================ */

/**
 * @brief Compare head 0 with head 1
 */
void uft_cross_compare_sides(const uint8_t *head0_data, size_t len0,
                             const uint8_t *head1_data, size_t len1,
                             uint8_t track,
                             uft_side_comparison_t *result);

/**
 * @brief Find sectors recoverable from other side
 */
size_t uft_cross_find_side_recoverable(const uft_side_comparison_t *cmp,
                                       uint8_t *recoverable_map);

/**
 * @brief Recover sector from other side
 * 
 * Für manche Formate sind bestimmte Sektoren auf beiden Seiten.
 */
bool uft_cross_recover_from_side(const uft_side_comparison_t *cmp,
                                 uint8_t source_head,
                                 uint8_t sector_num,
                                 const uint8_t *source_data,
                                 uint8_t *target_data,
                                 size_t len);

/**
 * @brief Merge best sectors from both sides
 */
bool uft_cross_merge_sides(const uft_side_comparison_t *cmp,
                           const uint8_t *head0_sectors[], size_t *lens0,
                           const uint8_t *head1_sectors[], size_t *lens1,
                           size_t sector_count,
                           uint8_t **merged[], size_t **merged_lens);

/* ============================================================================
 * Full Cross-Track Recovery
 * ============================================================================ */

/**
 * @brief Create cross-track recovery context
 */
uft_cross_recovery_ctx_t* uft_cross_recovery_create(uint8_t track_count,
                                                    uint8_t head_count);

/**
 * @brief Add track data to context
 */
bool uft_cross_recovery_add_track(uft_cross_recovery_ctx_t *ctx,
                                  uint8_t track, uint8_t head,
                                  const uint8_t *data, size_t len);

/**
 * @brief Run full cross-track analysis
 */
void uft_cross_recovery_analyze(uft_cross_recovery_ctx_t *ctx);

/**
 * @brief Get sectors recoverable through cross-track
 */
size_t uft_cross_recovery_get_recoverable(const uft_cross_recovery_ctx_t *ctx,
                                          uint8_t track, uint8_t head,
                                          uint8_t *sector_map);

/**
 * @brief Perform cross-track recovery
 */
bool uft_cross_recovery_execute(uft_cross_recovery_ctx_t *ctx,
                                uint8_t track, uint8_t head,
                                uint8_t sector);

/**
 * @brief Generate report
 */
char* uft_cross_recovery_report(const uft_cross_recovery_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_cross_recovery_free(uft_cross_recovery_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_CROSS_H */
