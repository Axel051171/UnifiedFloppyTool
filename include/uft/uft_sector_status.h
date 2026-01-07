/**
 * @file uft_sector_status.h
 * @brief Sector status tracking for recovery-friendly pipelines.
 *
 * This module is designed to be GUI-friendly (per-sector status + confidence)
 * and recovery-friendly (best-effort, multi-pass, voting/merge).
 */
#ifndef UFT_SECTOR_STATUS_H
#define UFT_SECTOR_STATUS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_SECTOR_OK = 0,
    UFT_SECTOR_BAD_CRC,
    UFT_SECTOR_MISSING,
    UFT_SECTOR_RECOVERED,
    UFT_SECTOR_PARTIAL
} uft_sector_state_t;

typedef enum {
    UFT_SECTOR_FLAG_NONE        = 0u,
    UFT_SECTOR_FLAG_WEAK        = 1u << 0, /**< Weak/unstable signal */
    UFT_SECTOR_FLAG_JITTER      = 1u << 1, /**< High jitter observed */
    UFT_SECTOR_FLAG_SPEED_DRIFT = 1u << 2, /**< PLL had to compensate heavily */
    UFT_SECTOR_FLAG_VOTED       = 1u << 3, /**< Sector content is a vote/merge result */

    /* --------------------------------------------------------------------- */
    /* --------------------------------------------------------------------- */
    UFT_SECTOR_FLAG_PROTECTION       = 1u << 4, /**< Copy-protection suspected/present */
    UFT_SECTOR_FLAG_NOT_IN_IMAGE     = 1u << 5, /**< Sector seen, but not included in image */
    UFT_SECTOR_FLAG_INCOMPLETE_DATA  = 1u << 6, /**< Sector decoded but data incomplete */
    UFT_SECTOR_FLAG_HIDDEN_DATA      = 1u << 7, /**< Extra/hidden data present in header/gap */
    UFT_SECTOR_FLAG_NONSTANDARD_ID   = 1u << 8, /**< Non-standard format type or block id */
    UFT_SECTOR_FLAG_TRACK_MISMATCH   = 1u << 9, /**< Track number mismatch (header vs expected) */
    UFT_SECTOR_FLAG_SIDE_MISMATCH    = 1u << 10,/**< Side number mismatch (header vs expected) */
    UFT_SECTOR_FLAG_ID_OUT_OF_RANGE  = 1u << 11,/**< Sector id out of allowed range */
    UFT_SECTOR_FLAG_LEN_NONSTANDARD  = 1u << 12,/**< Sector length non-standard */
    UFT_SECTOR_FLAG_ILLEGAL_OFFSET   = 1u << 13,/**< Illegal offset detected (container/format specific) */
    UFT_SECTOR_FLAG_EXTRA_CHECKSUM   = 1u << 14 /**< Extra checksum present but not verified */
} uft_sector_flags_t;

/**
 * @brief GUI/analytics friendly status for one decoded sector.
 */
typedef struct {
    uint16_t track;      /**< 0..n */
    uint8_t  head;       /**< 0/1 */
    uint16_t sector;     /**< logical ID */
    uint16_t size;       /**< bytes */
    uft_sector_state_t state;
    uint8_t  confidence; /**< 0..100 */
    uint8_t  retries;    /**< how many passes touched this sector */
    uint32_t flags;      /**< uft_sector_flags_t */
    uint32_t crc;        /**< computed CRC if available */
} uft_sector_status_t;

void uft_sector_status_init(uft_sector_status_t *s, uint16_t track, uint8_t head, uint16_t sector, uint16_t size);
void uft_sector_status_mark(uft_sector_status_t *s, uft_sector_state_t state, uint8_t confidence, uint32_t flags, uint32_t crc);

/**
 * @brief Merge a newer observation into an existing status.
 *
 * Policy: Prefer OK over everything. Prefer RECOVERED over BAD_CRC/MISSING.
 * Confidence becomes max(confidence). retries increments.
 */
void uft_sector_status_merge(uft_sector_status_t *dst, const uft_sector_status_t *src);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_STATUS_H */
