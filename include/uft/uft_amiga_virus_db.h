/**
 * @file uft_amiga_virus_db.h
 * @brief Amiga bootblock / link / file virus signature catalog (SSOT).
 *
 * SSOT source: data/amiga_bootblock_viruses.tsv
 * Generator:   scripts/generators/gen_virus_db.py
 * Generated:   src/fs/uft_amiga_virus_db.c  (do not edit by hand)
 *
 * Forensic rule: every entry's disinfect_strategy is REPORT_ONLY.
 * UFT does not auto-kill viruses — see docs/XCOPY_INTEGRATION_TODO.md
 * § Anti-Features.
 *
 * Part of MASTER_PLAN.md M2 T1.
 */
#ifndef UFT_AMIGA_VIRUS_DB_H
#define UFT_AMIGA_VIRUS_DB_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Virus category — matches the TSV `category` column.
 */
typedef enum {
    UFT_AMIGA_VIRUS_CAT_BOOT       = 1, /**< Amiga bootblock self-replicating */
    UFT_AMIGA_VIRUS_CAT_LINK       = 2, /**< Attaches to executables */
    UFT_AMIGA_VIRUS_CAT_FILE       = 3, /**< Overwrites files directly */
    UFT_AMIGA_VIRUS_CAT_BOOTBLOCK  = 4, /**< Non-replicating malicious bootblock */
    UFT_AMIGA_VIRUS_CAT_TROJAN     = 5  /**< Payload disguised as utility */
} uft_amiga_virus_category_t;

/**
 * One virus catalog entry. When @c pattern_len == 0 the entry is
 * NAME-ONLY (pattern not yet extracted) — scanners must not attempt
 * to match; entry is still valid for UI catalog display.
 */
typedef struct {
    uint32_t                     id;             /**< Stable numeric ID */
    const char                  *name;
    uft_amiga_virus_category_t   category;
    const uint8_t               *pattern;        /**< NULL iff pattern_len==0 */
    const uint8_t               *pattern_mask;   /**< NULL iff pattern_len==0 */
    size_t                       pattern_len;    /**< 0 = PENDING */
    const char                  *first_seen;
    const char                  *source;
} uft_amiga_virus_sig_t;

/**
 * Total number of entries in the compiled-in virus database.
 */
extern const size_t UFT_AMIGA_VIRUS_DB_COUNT;

/**
 * Returns a pointer to the immutable virus catalog. @p count, if
 * non-NULL, is set to the number of entries (same as
 * UFT_AMIGA_VIRUS_DB_COUNT).
 */
const uft_amiga_virus_sig_t *uft_amiga_get_virus_db(size_t *count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_VIRUS_DB_H */
