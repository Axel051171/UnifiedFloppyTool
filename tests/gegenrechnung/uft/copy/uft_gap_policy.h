/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef UFT_GAP_POLICY_H
#define UFT_GAP_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint16_t gap4a;
    uint16_t gap1;
    uint16_t gap2;
    uint16_t gap3_read_write;
    uint16_t gap3_format;
    uint16_t gap4b;
    uint8_t fill_byte;
    bool has_index_address_mark;
} uft_gap_layout_t;

typedef struct {
    size_t track_bytes;
    size_t sector_count;
    size_t encoded_bytes_per_sector; /* Includes marks/sync/CRC, excludes gaps. */
    size_t index_address_mark_bytes; /* Used only when layout says IAM is present. */
} uft_gap_budget_t;

typedef enum {
    UFT_GAP_OK = 0,
    UFT_GAP_EINVAL = -1,
    UFT_GAP_EOVERFLOW = -2,
    UFT_GAP_DOES_NOT_FIT = -3,
    UFT_GAP_BAD_ORDER = -4
} uft_gap_status_t;

/* Validates both the read/write layout and the formatting layout. */
int uft_gap_validate(const uft_gap_layout_t *layout,
                     const uft_gap_budget_t *budget,
                     size_t *format_bytes_used,
                     size_t *read_write_bytes_used);

/* Calculates only the trailing format gap; all other fields remain unchanged. */
int uft_gap_calculate_gap4b(uft_gap_layout_t *layout,
                            const uft_gap_budget_t *budget,
                            uint16_t minimum_gap4b);

/*
 * Evidence-backed CBM 1581 values. Only gap3_read_write and gap3_format are
 * claimed as measured here; zero in another gap field means "not supplied".
 */
uft_gap_layout_t uft_gap_profile_cbm1581(void);

#ifdef __cplusplus
}
#endif
#endif
