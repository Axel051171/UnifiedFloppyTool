/**
 * @file uft_sector_recovery.h
 * @brief Private contract of the sector-level recovery module
 *        (src/recovery/uft_sector_recovery.c).
 *
 * Extracted from the .c file so that tests can exercise the public
 * functions against the module's real types instead of re-declaring
 * them (single source of truth).
 *
 * DELIBERATELY PRIVATE — kept in src/recovery/, not include/uft/:
 * this module predates v4.x and its `uft_sector_t` is NOT the global
 * `uft_sector_t` from include/uft/uft_types.h (different layout, same
 * name). Never include this header in a translation unit that also
 * includes uft/uft_types.h, or the two definitions collide.
 */
#ifndef UFT_RECOVERY_SECTOR_RECOVERY_H
#define UFT_RECOVERY_SECTOR_RECOVERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_SECTOR_OK,
    UFT_SECTOR_CRC_ERROR,
    UFT_SECTOR_HEADER_ERROR,
    UFT_SECTOR_MISSING,
    UFT_SECTOR_WEAK,
    UFT_SECTOR_RECOVERED
} uft_sector_status_t;

typedef struct {
    uint8_t  track;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;
    uint8_t *data;
    size_t   data_len;
    uint16_t header_crc;
    uint16_t data_crc;
    uft_sector_status_t status;
    uint8_t  confidence;
    uint8_t  read_count;        /* Number of successful reads */
} uft_sector_t;

typedef struct {
    uft_sector_t *sectors;
    size_t sector_count;
    size_t good_sectors;
    size_t bad_sectors;
    size_t recovered_sectors;
    size_t missing_sectors;
} uft_sector_map_t;

typedef struct {
    size_t max_retries;
    bool   use_averaging;
    bool   attempt_reconstruction;
    uint8_t recovery_level;
} uft_sector_recovery_config_t;

void uft_sector_recovery_config_init(uft_sector_recovery_config_t *config);

/**
 * Recover a CRC-failed sector by majority-voting the original read
 * against `read_count` additional reads.
 *
 * Forensic contract: sector->data is ONLY overwritten (and the sector
 * ONLY marked UFT_SECTOR_RECOVERED) if the averaged data actually
 * matches sector->data_crc. A vote that does not repair the CRC leaves
 * the raw read and the sector status untouched and returns -1.
 *
 * Returns 0 if the sector was recovered, -1 otherwise.
 */
int uft_sector_recover_average(uft_sector_t *sector,
                               const uint8_t **additional_reads,
                               size_t read_count);

int uft_sector_recover_reconstruct(uft_sector_t *sector,
                                   const uint8_t *partial_data,
                                   size_t partial_len,
                                   const uint8_t *valid_mask,
                                   uint8_t *out_validity);

int uft_sector_recover_track(uft_sector_t *sectors, size_t sector_count,
                             const uft_sector_recovery_config_t *config,
                             size_t *recovered);

int uft_sector_get_stats(const uft_sector_t *sectors, size_t sector_count,
                         size_t *good, size_t *bad, size_t *recovered,
                         size_t *missing);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RECOVERY_SECTOR_RECOVERY_H */
