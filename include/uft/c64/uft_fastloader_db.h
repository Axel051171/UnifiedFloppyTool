/**
 * @file uft_fastloader_db.h
 * @brief C64/CBM Fastloader & Copy Protection Signature Database
 * 
 * Known signatures for:
 * - Commercial fastloaders (Epyx, Novaload, etc.)
 * - Copy programs (Turbonibbler, FCopy, etc.)
 * - Protection schemes (V-Max, Rapidlok, etc.)
 * - Cracker group intros
 * - Boot routines
 * 
 * @author UFT Team
 * @version 1.0.0
 * @date 2025
 */

#ifndef UFT_FASTLOADER_DB_H
#define UFT_FASTLOADER_DB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define UFT_SIG_MAX_NAME        32
#define UFT_SIG_MAX_VERSION     16
#define UFT_SIG_MAX_BYTES       64
#define UFT_SIG_MAX_PATTERNS    4

/** Signature categories */
typedef enum uft_sig_category {
    UFT_SIG_CAT_UNKNOWN = 0,
    UFT_SIG_CAT_FASTLOADER,     /**< Commercial/shareware fastloaders */
    UFT_SIG_CAT_COPIER,         /**< Disk copy programs */
    UFT_SIG_CAT_NIBBLER,        /**< Bit-level copiers */
    UFT_SIG_CAT_PROTECTION,     /**< Copy protection schemes */
    UFT_SIG_CAT_CRACKER,        /**< Cracker group signatures */
    UFT_SIG_CAT_DOS,            /**< DOS extensions/replacements */
    UFT_SIG_CAT_BOOT,           /**< Boot sector routines */
    UFT_SIG_CAT_SPEEDER         /**< Drive speeders */
} uft_sig_category_t;

/** Detection confidence */
typedef enum uft_sig_confidence {
    UFT_SIG_CONF_LOW = 1,       /**< Possible match */
    UFT_SIG_CONF_MEDIUM = 2,    /**< Likely match */
    UFT_SIG_CONF_HIGH = 3,      /**< Definite match */
    UFT_SIG_CONF_EXACT = 4      /**< Exact byte match */
} uft_sig_confidence_t;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * @brief Byte pattern for signature matching
 */
typedef struct uft_sig_pattern {
    uint16_t offset;            /**< Offset from load address (0xFFFF = any) */
    uint8_t bytes[UFT_SIG_MAX_BYTES];
    uint8_t mask[UFT_SIG_MAX_BYTES];  /**< 0xFF = must match, 0x00 = wildcard */
    uint8_t length;
} uft_sig_pattern_t;

/**
 * @brief Complete signature entry
 */
typedef struct uft_sig_entry {
    char name[UFT_SIG_MAX_NAME];
    char version[UFT_SIG_MAX_VERSION];
    uft_sig_category_t category;
    
    /* Patterns to match */
    uft_sig_pattern_t patterns[UFT_SIG_MAX_PATTERNS];
    uint8_t pattern_count;
    uint8_t patterns_required;  /**< How many must match */
    
    /* Load address hint (0 = any) */
    uint16_t load_addr_min;
    uint16_t load_addr_max;
    
    /* Size hint (0 = any) */
    uint16_t size_min;
    uint16_t size_max;
    
    /* Additional info */
    const char *description;
    const char *publisher;
    uint16_t year;
    
    /* Scoring weight */
    uint8_t score_weight;
} uft_sig_entry_t;

/**
 * @brief Match result
 */
typedef struct uft_sig_match {
    const uft_sig_entry_t *entry;
    uft_sig_confidence_t confidence;
    int score;
    uint32_t match_offset;      /**< Where pattern was found */
    uint8_t patterns_matched;
} uft_sig_match_t;

/**
 * @brief Multiple match results
 */
typedef struct uft_sig_result {
    uft_sig_match_t matches[16];
    int match_count;
    int total_score;
    uft_sig_category_t primary_category;
} uft_sig_result_t;

/* ============================================================================
 * Database Access
 * ============================================================================ */

/**
 * @brief Get signature database size
 */
size_t uft_sig_db_size(void);

/**
 * @brief Get signature entry by index
 */
const uft_sig_entry_t *uft_sig_db_get(size_t index);

/**
 * @brief Find signatures by category
 */
size_t uft_sig_db_find_category(uft_sig_category_t cat, 
                                 const uft_sig_entry_t **entries,
                                 size_t max_entries);

/**
 * @brief Find signature by name
 */
const uft_sig_entry_t *uft_sig_db_find_name(const char *name);

/* ============================================================================
 * Pattern Matching
 * ============================================================================ */

/**
 * @brief Scan data for all signatures
 * 
 * @param data          Data to scan (typically PRG payload)
 * @param size          Data size
 * @param load_addr     Load address (for offset calculation)
 * @param result        Output results
 * @return              Number of matches found
 */
int uft_sig_scan(const uint8_t *data, size_t size, uint16_t load_addr,
                 uft_sig_result_t *result);

/**
 * @brief Scan for specific category only
 */
int uft_sig_scan_category(const uint8_t *data, size_t size, uint16_t load_addr,
                          uft_sig_category_t category, uft_sig_result_t *result);

/**
 * @brief Check if data matches specific signature
 */
int uft_sig_match_entry(const uint8_t *data, size_t size, uint16_t load_addr,
                        const uft_sig_entry_t *entry, uft_sig_match_t *match);

/**
 * @brief Quick check for any fastloader
 */
int uft_sig_has_fastloader(const uint8_t *data, size_t size, uint16_t load_addr);

/**
 * @brief Quick check for copy protection
 */
int uft_sig_has_protection(const uint8_t *data, size_t size, uint16_t load_addr);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * @brief Get category name
 */
const char *uft_sig_category_name(uft_sig_category_t cat);

/**
 * @brief Get confidence name
 */
const char *uft_sig_confidence_name(uft_sig_confidence_t conf);

/**
 * @brief Format match result as text
 */
size_t uft_sig_format_result(const uft_sig_result_t *result, 
                              char *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FASTLOADER_DB_H */
