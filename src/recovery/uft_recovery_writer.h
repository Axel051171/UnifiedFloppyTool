/**
 * @file uft_recovery_writer.h
 * @brief GOD MODE Writer Recovery (Rückweg!)
 * 
 * Writer-Recovery (Rückweg!):
 * - Original-Layout reproduzieren
 * - Absichtlich defekte CRCs schreiben
 * - Weak-Bit-Simulation (HW-abhängig)
 * - Track-Timing approximieren
 * - Verify-Read mit Toleranzen
 * - Delta-Analyse Original ↔ Re-Write
 * 
 * @author UFT Team / GOD MODE
 * @version 3.0.0
 */

#ifndef UFT_RECOVERY_WRITER_H
#define UFT_RECOVERY_WRITER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Writer Hardware Types
 * ============================================================================ */

typedef enum {
    UFT_WRITER_UNKNOWN = 0,
    UFT_WRITER_GREASEWEAZLE,    /**< Greaseweazle */
    UFT_WRITER_FLUXENGINE,      /**< FluxEngine */
    UFT_WRITER_KRYOFLUX,        /**< KryoFlux */
    UFT_WRITER_SUPERCARD_PRO,   /**< SuperCard Pro */
    UFT_WRITER_APPLESAUCE,      /**< Applesauce */
    UFT_WRITER_PAULINE,         /**< Pauline */
    UFT_WRITER_CATWEASEL,       /**< Catweasel */
    UFT_WRITER_DISCFERRET,      /**< DiscFerret */
    UFT_WRITER_GENERIC_FDC,     /**< Generic FDC */
} uft_writer_type_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Writer capabilities
 */
typedef struct {
    uft_writer_type_t type;
    char     name[64];
    
    /* Basic capabilities */
    bool     can_write;         /**< Can write at all */
    bool     can_write_flux;    /**< Can write raw flux */
    bool     can_write_mfm;     /**< Can write MFM */
    bool     can_write_fm;      /**< Can write FM */
    bool     can_write_gcr;     /**< Can write GCR */
    
    /* Advanced capabilities */
    bool     can_bad_crc;       /**< Can write bad CRC */
    bool     can_weak_bits;     /**< Can simulate weak bits */
    bool     can_long_track;    /**< Can write long tracks */
    bool     can_timing_control;/**< Can control timing */
    bool     can_variable_speed;/**< Can vary write speed */
    
    /* Limits */
    uint32_t max_track_bits;    /**< Max bits per track */
    uint32_t min_flux_ns;       /**< Min flux transition (ns) */
    double   timing_resolution; /**< Timing resolution (ns) */
    double   timing_accuracy;   /**< Timing accuracy (%) */
} uft_writer_caps_t;

/**
 * @brief Write instruction
 */
typedef struct {
    uint8_t  track;
    uint8_t  head;
    
    /* Data to write */
    uint8_t *data;              /**< Data bytes */
    size_t   data_len;          /**< Data length */
    uint8_t *flux_data;         /**< Raw flux (optional) */
    size_t   flux_len;          /**< Flux length */
    
    /* Special handling */
    bool     write_bad_crc;     /**< Write bad CRC */
    uint16_t bad_crc_value;     /**< CRC value to write */
    uint8_t  bad_crc_sector;    /**< Which sector */
    
    bool     write_weak_bits;   /**< Write weak bit zone */
    size_t   weak_start;        /**< Weak zone start */
    size_t   weak_length;       /**< Weak zone length */
    uint8_t  weak_method;       /**< Weak bit method */
    
    bool     long_track;        /**< Write long track */
    size_t   track_bits;        /**< Total bits to write */
    
    /* Timing */
    bool     use_timing;        /**< Use specific timing */
    double  *timing_profile;    /**< Timing profile */
    size_t   timing_points;     /**< Timing profile points */
    
    /* Verification */
    bool     verify_after;      /**< Verify after write */
    double   verify_tolerance;  /**< Verification tolerance */
} uft_write_instruction_t;

/**
 * @brief Verify result
 */
typedef struct {
    bool     passed;            /**< Verification passed */
    double   match_percent;     /**< Match percentage */
    
    /* Per-sector results */
    uint8_t *sector_status;     /**< Status per sector */
    size_t   sector_count;
    
    /* Differences */
    size_t  *diff_positions;    /**< Difference positions */
    size_t   diff_count;        /**< Number of differences */
    
    /* Timing analysis */
    double   timing_deviation;  /**< Average timing deviation */
    double   max_timing_error;  /**< Maximum timing error */
    
    /* Detailed report */
    char    *report;
} uft_verify_result_t;

/**
 * @brief Delta analysis
 */
typedef struct {
    /* Overall */
    double   similarity;        /**< Overall similarity */
    bool     functionally_equal;/**< Functionally equivalent */
    
    /* Byte-level */
    size_t   bytes_identical;   /**< Identical bytes */
    size_t   bytes_different;   /**< Different bytes */
    size_t   bytes_total;       /**< Total bytes */
    
    /* Flux-level */
    size_t   flux_identical;    /**< Identical transitions */
    size_t   flux_different;    /**< Different transitions */
    size_t   flux_total;        /**< Total transitions */
    double   avg_flux_deviation;/**< Avg flux deviation (ns) */
    
    /* Sector-level */
    size_t   sectors_identical; /**< Identical sectors */
    size_t   sectors_different; /**< Different sectors */
    size_t   sectors_missing;   /**< Missing sectors */
    size_t   sectors_extra;     /**< Extra sectors */
    
    /* Protection */
    bool     protection_preserved; /**< Protection preserved */
    bool     weak_bits_similar; /**< Weak bits similar */
    bool     bad_crc_preserved; /**< Bad CRCs preserved */
    
    /* Report */
    char    *detailed_report;
} uft_delta_analysis_t;

/**
 * @brief Writer context
 */
typedef struct {
    /* Hardware */
    uft_writer_type_t writer_type;
    uft_writer_caps_t capabilities;
    
    /* Source data */
    const uint8_t **original_tracks;
    size_t *original_lengths;
    uint8_t track_count;
    uint8_t head_count;
    
    /* Write instructions */
    uft_write_instruction_t *instructions;
    size_t instruction_count;
    
    /* Results */
    uft_verify_result_t *verify_results;
    uft_delta_analysis_t *delta_analysis;
    
    /* Options */
    bool     preserve_protection;
    bool     verify_all;
    double   timing_tolerance;
} uft_writer_ctx_t;

/* ============================================================================
 * Original Layout Reproduction
 * ============================================================================ */

/**
 * @brief Analyze original layout
 */
void uft_writer_analyze_layout(const uint8_t *track_data, size_t len,
                               uft_write_instruction_t *instruction);

/**
 * @brief Generate write instructions from original
 */
size_t uft_writer_gen_instructions(const uint8_t **tracks,
                                   const size_t *lengths,
                                   size_t track_count,
                                   uint8_t head_count,
                                   uft_write_instruction_t **instructions);

/**
 * @brief Preserve original gap layout
 */
void uft_writer_preserve_gaps(const uint8_t *original, size_t len,
                              uft_write_instruction_t *instruction);

/**
 * @brief Preserve original sector order
 */
void uft_writer_preserve_order(const uint8_t *original, size_t len,
                               uft_write_instruction_t *instruction);

/* ============================================================================
 * Intentional Bad CRC
 * ============================================================================ */

/**
 * @brief Set up bad CRC write
 */
void uft_writer_set_bad_crc(uft_write_instruction_t *instruction,
                            uint8_t sector,
                            uint16_t crc_value);

/**
 * @brief Generate data with bad CRC
 */
bool uft_writer_gen_bad_crc_sector(uint8_t *sector_data, size_t len,
                                   uint16_t bad_crc);

/**
 * @brief Check if writer can do bad CRC
 */
bool uft_writer_can_bad_crc(const uft_writer_caps_t *caps);

/**
 * @brief Alternative: Write with no CRC check
 */
bool uft_writer_write_raw_sector(uft_write_instruction_t *instruction,
                                 const uint8_t *data, size_t len);

/* ============================================================================
 * Weak Bit Simulation
 * ============================================================================ */

/**
 * @brief Weak bit simulation methods
 */
typedef enum {
    UFT_WEAK_METHOD_NONE = 0,
    UFT_WEAK_METHOD_NOISE,      /**< Add noise to flux */
    UFT_WEAK_METHOD_NO_FLUX,    /**< No flux transitions */
    UFT_WEAK_METHOD_RANDOM,     /**< Random flux density */
    UFT_WEAK_METHOD_HALF_FLUX,  /**< Half-strength flux */
    UFT_WEAK_METHOD_OVERWRITE,  /**< Multiple overwrites */
} uft_weak_method_t;

/**
 * @brief Set up weak bit zone
 */
void uft_writer_set_weak_bits(uft_write_instruction_t *instruction,
                              size_t start, size_t length,
                              uft_weak_method_t method);

/**
 * @brief Generate weak bit flux pattern
 */
bool uft_writer_gen_weak_flux(uint8_t *flux_data, size_t *flux_len,
                              size_t weak_start, size_t weak_length,
                              uft_weak_method_t method,
                              const uft_writer_caps_t *caps);

/**
 * @brief Check if writer can do weak bits
 */
bool uft_writer_can_weak_bits(const uft_writer_caps_t *caps,
                              uft_weak_method_t method);

/**
 * @brief Get best weak bit method for writer
 */
uft_weak_method_t uft_writer_best_weak_method(const uft_writer_caps_t *caps);

/* ============================================================================
 * Track Timing
 * ============================================================================ */

/**
 * @brief Extract timing profile from original
 */
bool uft_writer_extract_timing(const uint8_t *flux_data, size_t len,
                               double **timing_profile,
                               size_t *profile_points);

/**
 * @brief Apply timing profile to instruction
 */
void uft_writer_apply_timing(uft_write_instruction_t *instruction,
                             const double *timing_profile,
                             size_t profile_points);

/**
 * @brief Generate flux with timing
 */
bool uft_writer_gen_timed_flux(const uint8_t *data, size_t data_len,
                               const double *timing_profile,
                               uint8_t **flux_data, size_t *flux_len);

/**
 * @brief Approximate timing for format
 */
void uft_writer_approx_timing(uft_write_instruction_t *instruction,
                              double nominal_rpm,
                              double bit_cell_ns);

/* ============================================================================
 * Verify Read
 * ============================================================================ */

/**
 * @brief Verify written track
 */
void uft_writer_verify(const uint8_t *written, size_t written_len,
                       const uint8_t *expected, size_t expected_len,
                       double tolerance,
                       uft_verify_result_t *result);

/**
 * @brief Verify with protection awareness
 */
void uft_writer_verify_protected(const uint8_t *written, size_t written_len,
                                 const uint8_t *expected, size_t expected_len,
                                 const uft_write_instruction_t *instruction,
                                 uft_verify_result_t *result);

/**
 * @brief Verify weak bit zone
 */
bool uft_writer_verify_weak_bits(const uint8_t **reads, size_t read_count,
                                 size_t len,
                                 size_t weak_start, size_t weak_length,
                                 double *variability);

/**
 * @brief Free verify result
 */
void uft_writer_verify_free(uft_verify_result_t *result);

/* ============================================================================
 * Delta Analysis
 * ============================================================================ */

/**
 * @brief Full delta analysis
 */
void uft_writer_delta_analysis(const uint8_t *original, size_t orig_len,
                               const uint8_t *rewritten, size_t rewrite_len,
                               uft_delta_analysis_t *result);

/**
 * @brief Flux-level delta
 */
void uft_writer_delta_flux(const uint32_t *orig_flux, size_t orig_count,
                           const uint32_t *new_flux, size_t new_count,
                           uft_delta_analysis_t *result);

/**
 * @brief Sector-level delta
 */
void uft_writer_delta_sectors(const uint8_t *original, size_t orig_len,
                              const uint8_t *rewritten, size_t rewrite_len,
                              uft_delta_analysis_t *result);

/**
 * @brief Check if functionally equivalent
 */
bool uft_writer_is_equivalent(const uft_delta_analysis_t *delta);

/**
 * @brief Free delta analysis
 */
void uft_writer_delta_free(uft_delta_analysis_t *delta);

/* ============================================================================
 * Writer Hardware Abstraction
 * ============================================================================ */

/**
 * @brief Get writer capabilities
 */
void uft_writer_get_caps(uft_writer_type_t type, uft_writer_caps_t *caps);

/**
 * @brief Check if writer can handle instruction
 */
bool uft_writer_can_execute(const uft_writer_caps_t *caps,
                            const uft_write_instruction_t *instruction);

/**
 * @brief Adapt instruction for writer
 * 
 * Passt Instruktion an Writer-Capabilities an.
 */
bool uft_writer_adapt_instruction(uft_write_instruction_t *instruction,
                                  const uft_writer_caps_t *caps);

/**
 * @brief Generate writer-specific output
 */
bool uft_writer_gen_output(const uft_write_instruction_t *instruction,
                           uft_writer_type_t writer,
                           uint8_t **output, size_t *output_len);

/* ============================================================================
 * Full Writer Recovery
 * ============================================================================ */

/**
 * @brief Create writer context
 */
uft_writer_ctx_t* uft_writer_create(uft_writer_type_t writer);

/**
 * @brief Set original data
 */
void uft_writer_set_original(uft_writer_ctx_t *ctx,
                             const uint8_t **tracks,
                             const size_t *lengths,
                             uint8_t track_count,
                             uint8_t head_count);

/**
 * @brief Generate all write instructions
 */
void uft_writer_generate_all(uft_writer_ctx_t *ctx);

/**
 * @brief Get instructions for track
 */
const uft_write_instruction_t* uft_writer_get_instruction(
    const uft_writer_ctx_t *ctx,
    uint8_t track, uint8_t head);

/**
 * @brief Export for writer
 */
bool uft_writer_export(const uft_writer_ctx_t *ctx,
                       const char *filename);

/**
 * @brief Generate report
 */
char* uft_writer_report(const uft_writer_ctx_t *ctx);

/**
 * @brief Free context
 */
void uft_writer_free(uft_writer_ctx_t *ctx);

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/**
 * @brief Quick clone for writer
 */
bool uft_writer_quick_clone(const uint8_t **original_tracks,
                            const size_t *lengths,
                            uint8_t track_count, uint8_t head_count,
                            uft_writer_type_t writer,
                            const char *output_file);

/**
 * @brief Clone with protection preservation
 */
bool uft_writer_clone_protected(const uint8_t **original_tracks,
                                const size_t *lengths,
                                uint8_t track_count, uint8_t head_count,
                                uft_writer_type_t writer,
                                const char *output_file);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_WRITER_H */
