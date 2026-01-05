/**
 * @file uft_recovery_protection.h
 * @brief GOD MODE Copy Protection-Specific Recovery
 * 
 * Kopierschutz-spezifische Recovery:
 * - Absichtlich falsche CRCs erhalten
 * - Weak-Bit-Zonen konservieren
 * - Doppelte IDs behalten
 * - Non-Standard-Syncs erhalten
 * - Ungewöhnliche Track-Längen erhalten
 * - Protection-Marker setzen (nicht "reparieren")
 * 
 * WICHTIG: Kopierschutz wird NICHT umgangen, sondern ERHALTEN!
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_PROTECTION_H
#define UFT_RECOVERY_PROTECTION_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Protection Types
 * ============================================================================ */

/**
 * @brief Known protection schemes
 */
typedef enum {
    UFT_PROT_UNKNOWN = 0,
    
    /* Amiga */
    UFT_PROT_COPYLOCK,          /**< Copylock */
    UFT_PROT_ROB_NORTHEN,       /**< Rob Northen Copylock */
    UFT_PROT_RNCA,              /**< RNC Advanced */
    UFT_PROT_TIERTEX,           /**< Tiertex */
    
    /* Atari ST */
    UFT_PROT_MACRODOS,          /**< Macrodos */
    UFT_PROT_SPEEDLOCK,         /**< Speedlock ST */
    
    /* C64 */
    UFT_PROT_V_MAX,             /**< V-MAX */
    UFT_PROT_RAPIDLOK,          /**< RapidLok */
    UFT_PROT_VORPAL,            /**< Vorpal */
    UFT_PROT_GMA,               /**< GMA */
    
    /* Apple II */
    UFT_PROT_SPIRAL,            /**< Spiral boot */
    UFT_PROT_E7,                /**< E7 bitstream */
    
    /* IBM PC */
    UFT_PROT_PROLOK,            /**< ProLok */
    UFT_PROT_VAULT,             /**< Vault */
    UFT_PROT_FORMASTER,         /**< ForMaster */
    UFT_PROT_FDA,               /**< FDA long track */
    
    /* Generic */
    UFT_PROT_WEAK_BITS,         /**< Weak bit protection */
    UFT_PROT_LONG_TRACK,        /**< Long track protection */
    UFT_PROT_DUPLICATE_ID,      /**< Duplicate sector ID */
    UFT_PROT_BAD_CRC,           /**< Intentional CRC error */
    UFT_PROT_NON_STANDARD,      /**< Non-standard format */
} uft_protection_type_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Protection marker
 */
typedef struct {
    uft_protection_type_t type; /**< Protection type */
    uint8_t  track;             /**< Track number */
    uint8_t  head;              /**< Head/side */
    uint8_t  sector;            /**< Sector (0xFF for track-level) */
    size_t   bit_offset;        /**< Bit offset in track */
    size_t   bit_length;        /**< Length in bits */
    
    /* Details */
    uint8_t *signature;         /**< Protection signature */
    size_t   signature_len;     /**< Signature length */
    char     description[128];  /**< Human-readable description */
    
    /* Preservation flags */
    bool     must_preserve;     /**< Must be preserved exactly */
    bool     timing_critical;   /**< Timing is critical */
    bool     weak_bits;         /**< Contains weak bits */
} uft_protection_marker_t;

/**
 * @brief Intentional CRC error
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint16_t stored_crc;        /**< CRC stored on disk */
    uint16_t calculated_crc;    /**< CRC that should be */
    bool     is_intentional;    /**< Determined to be intentional */
    bool     preserve;          /**< Should preserve bad CRC */
    uft_protection_type_t scheme; /**< Protection scheme using this */
} uft_intentional_crc_t;

/**
 * @brief Weak bit zone (protection-specific)
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    size_t   bit_offset;        /**< Start offset */
    size_t   bit_count;         /**< Number of weak bits */
    uint8_t *baseline;          /**< Baseline pattern */
    uint8_t  variability;       /**< How much it varies */
    bool     is_protection;     /**< Part of protection */
    uft_protection_type_t scheme; /**< Protection scheme */
} uft_weak_zone_prot_t;

/**
 * @brief Non-standard sync pattern
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    size_t   bit_offset;
    uint32_t pattern;           /**< Sync pattern found */
    uint8_t  pattern_bits;      /**< Pattern length */
    uint32_t expected;          /**< Standard pattern */
    bool     is_protection;     /**< Part of protection */
    uft_protection_type_t scheme;
} uft_nonstandard_sync_t;

/**
 * @brief Long track info
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    size_t   nominal_bits;      /**< Expected length */
    size_t   actual_bits;       /**< Actual length */
    size_t   extra_bits;        /**< Extra bits */
    uint8_t *extra_data;        /**< Extra data content */
    bool     is_protection;     /**< Part of protection */
    uft_protection_type_t scheme;
} uft_long_track_t;

/**
 * @brief Protection analysis result
 */
typedef struct {
    /* Detected protection */
    uft_protection_type_t primary_scheme;
    uft_protection_type_t *additional_schemes;
    size_t   scheme_count;
    
    /* Markers */
    uft_protection_marker_t *markers;
    size_t   marker_count;
    
    /* Specifics */
    uft_intentional_crc_t *bad_crcs;
    size_t   bad_crc_count;
    
    uft_weak_zone_prot_t *weak_zones;
    size_t   weak_zone_count;
    
    uft_nonstandard_sync_t *nonstandard_syncs;
    size_t   nonstandard_sync_count;
    
    uft_long_track_t *long_tracks;
    size_t   long_track_count;
    
    /* Analysis */
    uint8_t  confidence;        /**< Confidence in detection */
    char    *analysis_report;   /**< Detailed analysis */
    
    /* Warnings */
    bool     has_weak_bits;
    bool     has_timing_critical;
    bool     needs_special_writer;
} uft_protection_analysis_t;

/* ============================================================================
 * Detection Functions
 * ============================================================================ */

/**
 * @brief Detect copy protection
 */
uft_protection_analysis_t* uft_prot_detect(const uint8_t **tracks,
                                           const size_t *track_lengths,
                                           size_t track_count,
                                           uint8_t head_count);

/**
 * @brief Detect specific protection scheme
 */
bool uft_prot_detect_scheme(const uint8_t *track_data, size_t len,
                            uft_protection_type_t scheme,
                            uft_protection_marker_t *marker);

/**
 * @brief Identify protection from signature
 */
uft_protection_type_t uft_prot_identify(const uint8_t *signature,
                                        size_t sig_len);

/**
 * @brief Get protection scheme name
 */
const char* uft_prot_scheme_name(uft_protection_type_t scheme);

/* ============================================================================
 * Intentional CRC Preservation
 * ============================================================================ */

/**
 * @brief Detect intentional CRC errors
 */
size_t uft_prot_detect_intentional_crc(const uint8_t *track_data, size_t len,
                                       uint8_t track, uint8_t head,
                                       uft_intentional_crc_t **bad_crcs);

/**
 * @brief Verify CRC is intentionally bad
 */
bool uft_prot_verify_intentional_crc(const uft_intentional_crc_t *crc,
                                     const uint8_t *sector_data,
                                     size_t sector_len);

/**
 * @brief Mark CRC for preservation
 */
void uft_prot_preserve_crc(uft_intentional_crc_t *crc);

/**
 * @brief DO NOT fix this CRC!
 */
#define uft_prot_crc_do_not_fix(crc) ((crc)->preserve = true)

/* ============================================================================
 * Weak Bit Zone Preservation
 * ============================================================================ */

/**
 * @brief Detect weak bit zones (protection)
 */
size_t uft_prot_detect_weak_zones(const uint8_t **rev_data,
                                  size_t rev_count, size_t len,
                                  uint8_t track, uint8_t head,
                                  uft_weak_zone_prot_t **zones);

/**
 * @brief Analyze weak zone pattern
 */
void uft_prot_analyze_weak_zone(uft_weak_zone_prot_t *zone,
                                const uint8_t **rev_data,
                                size_t rev_count);

/**
 * @brief Preserve weak bits (don't "fix" them!)
 */
void uft_prot_preserve_weak_bits(uft_weak_zone_prot_t *zone);

/* ============================================================================
 * Duplicate ID Handling
 * ============================================================================ */

/**
 * @brief Detect protection duplicate IDs
 */
size_t uft_prot_detect_dup_ids(const uint8_t *track_data, size_t len,
                               uint8_t track, uint8_t head,
                               uft_protection_marker_t **markers);

/**
 * @brief Analyze if duplicate is intentional
 */
bool uft_prot_is_intentional_dup(const uint8_t *sector1_data, size_t len1,
                                 const uint8_t *sector2_data, size_t len2);

/**
 * @brief Preserve duplicate IDs
 */
void uft_prot_preserve_dup_ids(uft_protection_marker_t *marker);

/* ============================================================================
 * Non-Standard Sync Preservation
 * ============================================================================ */

/**
 * @brief Detect non-standard syncs
 */
size_t uft_prot_detect_nonstandard_syncs(const uint8_t *track_data, size_t len,
                                         uint8_t track, uint8_t head,
                                         uint32_t expected_sync,
                                         uft_nonstandard_sync_t **syncs);

/**
 * @brief Analyze non-standard sync purpose
 */
void uft_prot_analyze_sync(uft_nonstandard_sync_t *sync);

/**
 * @brief Preserve non-standard sync
 */
void uft_prot_preserve_sync(uft_nonstandard_sync_t *sync);

/* ============================================================================
 * Long Track Handling
 * ============================================================================ */

/**
 * @brief Detect protection long tracks
 */
size_t uft_prot_detect_long_tracks(const size_t *track_lengths,
                                   size_t track_count,
                                   size_t expected_length,
                                   uft_long_track_t **long_tracks);

/**
 * @brief Analyze extra data in long track
 */
void uft_prot_analyze_long_track(uft_long_track_t *lt,
                                 const uint8_t *track_data);

/**
 * @brief Preserve long track (don't truncate!)
 */
void uft_prot_preserve_long_track(uft_long_track_t *lt);

/* ============================================================================
 * Protection Marking
 * ============================================================================ */

/**
 * @brief Add protection marker
 */
void uft_prot_add_marker(uft_protection_analysis_t *analysis,
                         const uft_protection_marker_t *marker);

/**
 * @brief Get markers for track
 */
size_t uft_prot_get_track_markers(const uft_protection_analysis_t *analysis,
                                  uint8_t track, uint8_t head,
                                  uft_protection_marker_t **markers);

/**
 * @brief Check if location is protected
 */
bool uft_prot_is_protected(const uft_protection_analysis_t *analysis,
                           uint8_t track, uint8_t head,
                           size_t bit_offset);

/**
 * @brief Get protection at location
 */
const uft_protection_marker_t* uft_prot_get_at(
    const uft_protection_analysis_t *analysis,
    uint8_t track, uint8_t head, size_t bit_offset);

/* ============================================================================
 * Writer Integration
 * ============================================================================ */

/**
 * @brief Generate writer instructions
 * 
 * Erzeugt Anweisungen für Writer wie Protection reproduziert werden soll.
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    
    /* CRC */
    bool     write_bad_crc;
    uint16_t crc_to_write;
    
    /* Weak bits */
    bool     write_weak_bits;
    size_t   weak_offset;
    size_t   weak_length;
    
    /* Timing */
    bool     use_special_timing;
    double   timing_offset;
    
    /* Track length */
    bool     write_long;
    size_t   total_bits;
    
    /* Notes */
    char     notes[256];
} uft_writer_instruction_t;

/**
 * @brief Generate writer instructions for track
 */
size_t uft_prot_gen_writer_instructions(const uft_protection_analysis_t *analysis,
                                        uint8_t track, uint8_t head,
                                        uft_writer_instruction_t **instructions);

/**
 * @brief Check if protection can be reproduced
 */
bool uft_prot_can_reproduce(const uft_protection_analysis_t *analysis,
                            const char *writer_type);

/* ============================================================================
 * Full Analysis
 * ============================================================================ */

/**
 * @brief Free protection analysis
 */
void uft_prot_free(uft_protection_analysis_t *analysis);

/**
 * @brief Generate protection report
 */
char* uft_prot_report(const uft_protection_analysis_t *analysis);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_PROTECTION_H */
