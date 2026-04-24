/**
 * @file uft_bootblock_scanner.h
 * @brief Amiga bootblock scanner: DOS-type check, checksum, virus detection.
 *
 * Input: 1024 bytes (sector 0 + sector 1 of an Amiga disk).
 * Output: structured scan result with DOS-type classification,
 *         checksum validity, and virus-catalog matches.
 *
 * The scanner is READ-ONLY — it reports findings but never modifies
 * the bootblock or attempts disinfection (per
 * docs/XCOPY_INTEGRATION_TODO.md § Anti-Features).
 *
 * Depends on uft/uft_amiga_virus_db.h (M2 T1). Part of M2 T2.
 */
#ifndef UFT_BOOTBLOCK_SCANNER_H
#define UFT_BOOTBLOCK_SCANNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "uft/uft_amiga_virus_db.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Size of an Amiga bootblock: 2 × 512-byte sectors. */
#define UFT_BOOTBLOCK_SIZE  1024u

/** Maximum number of virus matches reported per bootblock. */
#define UFT_BB_MAX_MATCHES  4u

/** Amiga DOS-type low-byte codes (upper 24 bits must be 0x444F5300). */
typedef enum {
    UFT_BB_DOS_UNKNOWN   = -1,
    UFT_BB_DOS_OFS       =  0, /**< OldFileSystem */
    UFT_BB_DOS_FFS       =  1, /**< FastFileSystem */
    UFT_BB_DOS_OFS_INTL  =  2, /**< OFS International */
    UFT_BB_DOS_FFS_INTL  =  3, /**< FFS International */
    UFT_BB_DOS_OFS_DC    =  4, /**< OFS DirCache */
    UFT_BB_DOS_FFS_DC    =  5  /**< FFS DirCache */
} uft_bb_dos_type_t;

/** One matched virus signature in a bootblock. */
typedef struct {
    uint32_t                     virus_id;
    const char                  *virus_name;
    uft_amiga_virus_category_t   category;
    uint32_t                     match_offset;  /**< where pattern matched */
} uft_bb_match_t;

/** Scan result. */
typedef struct {
    /* DOS-type classification (bytes 0..3). */
    uint32_t               dos_type_raw;      /**< raw 4 bytes as BE32 */
    uft_bb_dos_type_t      dos_type;          /**< parsed enum */

    /* Checksum (stored at bytes 4..7, BE32). */
    uint32_t               stored_checksum;
    uint32_t               computed_checksum; /**< what it should be */
    bool                   checksum_ok;       /**< stored == computed */

    /* Root-block pointer (bytes 8..11, BE32, typically 880 for DD). */
    uint32_t               root_block;

    /* Virus matches. */
    size_t                 match_count;
    uft_bb_match_t         matches[UFT_BB_MAX_MATCHES];

    /* Heuristic signals for UI / triage. */
    bool                   all_zeros;         /**< entire bootblock is 0 */
    bool                   looks_like_code;   /**< has m68k opcodes */
} uft_bb_scan_result_t;


/**
 * Compute the Amiga bootblock checksum.
 *
 * Algorithm: sum all 256 big-endian uint32 words with carry-round,
 * then invert. The checksum field at offset 4..7 is treated as zero
 * during computation — pass this function a buffer OR use
 * uft_bb_validate_checksum() which masks the field for you.
 *
 * @param bootblock  Pointer to 1024 bytes.
 * @return The inverted sum. For a valid bootblock, this equals the
 *         value stored at offset 4..7.
 */
uint32_t uft_bb_compute_checksum(const uint8_t *bootblock);

/**
 * Run the full scan pipeline on a 1024-byte Amiga bootblock.
 *
 * Populates @p result with DOS-type, checksum, root-block pointer,
 * and any virus-catalog matches (up to UFT_BB_MAX_MATCHES). The
 * catalog is consulted via uft_amiga_get_virus_db(); entries with
 * pattern_len == 0 (PENDING) are skipped for matching but may still
 * appear in UI catalog listings (handled by caller).
 *
 * @return UFT_OK on success,
 *         UFT_ERR_INVALID_ARG if @p bootblock or @p result is NULL,
 *                             or if @p len != UFT_BOOTBLOCK_SIZE.
 */
uft_error_t uft_bb_scan(const uint8_t *bootblock,
                         size_t len,
                         uft_bb_scan_result_t *result);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BOOTBLOCK_SCANNER_H */
