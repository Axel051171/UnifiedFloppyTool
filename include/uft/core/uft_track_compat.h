/**
 * @file uft_track_compat.h
 * @brief Track Structure Compatibility Layer (P2-ARCH-001)
 * 
 * Provides conversion functions between format-specific track
 * structures and the unified uft_track_base_t.
 * 
 * Note: The inline conversion functions are implemented in 
 * uft_track_compat.c. This header only provides prototypes.
 * 
 * @version 1.1.0
 * @date 2026-01-07
 */

#ifndef UFT_TRACK_COMPAT_H
#define UFT_TRACK_COMPAT_H

#include "uft_track_base.h"

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Track Type Identifiers
 *===========================================================================*/

typedef enum {
    UFT_TRACK_TYPE_BASE = 0,
    UFT_TRACK_TYPE_DECODER,
    UFT_TRACK_TYPE_IR,
    UFT_TRACK_TYPE_IPF,
    UFT_TRACK_TYPE_SCP,
    UFT_TRACK_TYPE_TD0,
    UFT_TRACK_TYPE_HFE
} uft_track_src_type_t;

/*===========================================================================
 * Forward Declarations
 *===========================================================================*/

struct uft_track;
struct uft_ir_track;
struct ipf_track;
struct uft_scp_track_info;

/*===========================================================================
 * Function Prototypes
 * 
 * These functions are implemented in uft_track_compat.c
 *===========================================================================*/

/**
 * @brief Convert uft_track (decoder) to uft_track_base_t
 */
int uft_track_to_base(const struct uft_track *src, uft_track_base_t *dst);

/**
 * @brief Convert uft_track_base_t to uft_track (decoder)
 */
int uft_base_to_track(const uft_track_base_t *src, struct uft_track *dst);

/**
 * @brief Convert uft_ir_track to uft_track_base_t
 */
int uft_ir_track_to_base(const struct uft_ir_track *src, uft_track_base_t *dst);

/**
 * @brief Convert uft_track_base_t to uft_ir_track
 */
int uft_base_to_ir_track(const uft_track_base_t *src, struct uft_ir_track *dst);

/**
 * @brief Convert ipf_track to uft_track_base_t
 */
int uft_ipf_track_to_base(const struct ipf_track *src, uft_track_base_t *dst);

/**
 * @brief Convert uft_track_base_t to ipf_track
 */
int uft_base_to_ipf_track(const uft_track_base_t *src, struct ipf_track *dst);

/**
 * @brief Convert SCP track info to uft_track_base_t
 */
int uft_scp_track_to_base(const struct uft_scp_track_info *src, 
                          uft_track_base_t *dst);

/**
 * @brief Generic track conversion to base
 * 
 * @param src       Source track (any type)
 * @param src_type  Source type identifier
 * @param dst       Destination base track
 * @return 0 on success, -1 on error
 */
int uft_track_convert_to_base(const void *src, uft_track_src_type_t src_type,
                               uft_track_base_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_COMPAT_H */
