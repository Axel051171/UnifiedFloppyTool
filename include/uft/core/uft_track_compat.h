/**
 * @file uft_track_compat.h
 * @brief Track Structure Compatibility Layer (P2-ARCH-001)
 * 
 * Provides conversion functions between format-specific track
 * structures and the unified uft_track_base_t.
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#ifndef UFT_TRACK_COMPAT_H
#define UFT_TRACK_COMPAT_H

#include "uft_track_base.h"
#include "uft/decoder/uft_unified_decoder.h"
#include "uft/uft_ir_format.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Unified Decoder Track Conversion
 *===========================================================================*/

/**
 * @brief Convert uft_track (decoder) to uft_track_base_t
 */
static inline int uft_track_to_base(const struct uft_track *src,
                                     uft_track_base_t *dst)
{
    if (!src || !dst) return -1;
    
    dst->cylinder = (uint8_t)src->track_num;
    dst->head = src->side;
    dst->encoding = (uint8_t)src->encoding;
    dst->detection_confidence = src->detection_confidence;
    dst->sectors_found = (uint8_t)src->sector_count;
    dst->sectors_good = src->good_sectors;
    dst->sectors_bad = src->bad_sectors;
    dst->flags = UFT_TF_PRESENT;
    
    if (src->bad_sectors > 0) {
        dst->flags |= UFT_TF_CRC_ERRORS;
    }
    
    return 0;
}

/**
 * @brief Convert uft_track_base_t to uft_track (decoder)
 */
static inline int uft_base_to_track(const uft_track_base_t *src,
                                     struct uft_track *dst)
{
    if (!src || !dst) return -1;
    
    dst->track_num = src->cylinder;
    dst->side = src->head;
    dst->encoding = (uft_encoding_t)src->encoding;
    dst->detection_confidence = src->detection_confidence;
    dst->good_sectors = src->sectors_good;
    dst->bad_sectors = src->sectors_bad;
    dst->missing_sectors = src->sectors_expected - src->sectors_found;
    
    return 0;
}

/*===========================================================================
 * IR Format Track Conversion
 *===========================================================================*/

/**
 * @brief Convert uft_ir_track to uft_track_base_t
 */
static inline int uft_ir_track_to_base(const uft_ir_track_t *src,
                                        uft_track_base_t *dst)
{
    if (!src || !dst) return -1;
    
    dst->cylinder = src->cylinder;
    dst->head = src->head;
    dst->cyl_offset_q = src->cyl_offset_quarters;
    dst->flags = src->flags;
    
    /* Map IR encoding to base encoding */
    switch (src->encoding) {
        case UFT_IR_ENC_FM:           dst->encoding = UFT_ENC_FM; break;
        case UFT_IR_ENC_MFM:          dst->encoding = UFT_ENC_MFM; break;
        case UFT_IR_ENC_GCR_COMMODORE:dst->encoding = UFT_ENC_GCR_C64; break;
        case UFT_IR_ENC_GCR_APPLE:    dst->encoding = UFT_ENC_GCR_APPLE; break;
        case UFT_IR_ENC_AMIGA_MFM:    dst->encoding = UFT_ENC_AMIGA_MFM; break;
        case UFT_IR_ENC_GCR_VICTOR:   dst->encoding = UFT_ENC_GCR_VICTOR; break;
        default:                      dst->encoding = UFT_ENC_UNKNOWN; break;
    }
    
    dst->sectors_expected = src->sectors_expected;
    dst->sectors_found = src->sectors_found;
    dst->sectors_good = src->sectors_good;
    dst->sectors_bad = src->sectors_found - src->sectors_good;
    
    dst->bitcell_ns = src->bitcell_ns;
    dst->rpm_x100 = src->rpm_measured;
    dst->write_splice_ns = src->write_splice_ns;
    
    dst->revolution_count = src->revolution_count;
    dst->best_revolution = src->best_revolution;
    
    /* Map quality */
    switch (src->quality) {
        case UFT_IR_QUAL_PERFECT:  dst->quality = UFT_TQ_PERFECT; break;
        case UFT_IR_QUAL_GOOD:     dst->quality = UFT_TQ_GOOD; break;
        case UFT_IR_QUAL_MARGINAL: dst->quality = UFT_TQ_MARGINAL; break;
        case UFT_IR_QUAL_POOR:     dst->quality = UFT_TQ_POOR; break;
        case UFT_IR_QUAL_BAD:      dst->quality = UFT_TQ_UNREADABLE; break;
        default:                   dst->quality = UFT_TQ_UNKNOWN; break;
    }
    
    return 0;
}

/**
 * @brief Convert uft_track_base_t to uft_ir_track
 */
static inline int uft_base_to_ir_track(const uft_track_base_t *src,
                                        uft_ir_track_t *dst)
{
    if (!src || !dst) return -1;
    
    dst->cylinder = src->cylinder;
    dst->head = src->head;
    dst->cyl_offset_quarters = src->cyl_offset_q;
    dst->flags = src->flags;
    
    /* Map base encoding to IR encoding */
    switch (src->encoding) {
        case UFT_ENC_FM:        dst->encoding = UFT_IR_ENC_FM; break;
        case UFT_ENC_MFM:       dst->encoding = UFT_IR_ENC_MFM; break;
        case UFT_ENC_GCR_C64:   dst->encoding = UFT_IR_ENC_GCR_COMMODORE; break;
        case UFT_ENC_GCR_APPLE: dst->encoding = UFT_IR_ENC_GCR_APPLE; break;
        case UFT_ENC_AMIGA_MFM: dst->encoding = UFT_IR_ENC_AMIGA_MFM; break;
        case UFT_ENC_GCR_VICTOR:dst->encoding = UFT_IR_ENC_GCR_VICTOR; break;
        default:                dst->encoding = UFT_IR_ENC_UNKNOWN; break;
    }
    
    dst->sectors_expected = src->sectors_expected;
    dst->sectors_found = src->sectors_found;
    dst->sectors_good = src->sectors_good;
    
    dst->bitcell_ns = src->bitcell_ns;
    dst->rpm_measured = src->rpm_x100;
    dst->write_splice_ns = src->write_splice_ns;
    
    dst->revolution_count = src->revolution_count;
    dst->best_revolution = src->best_revolution;
    
    /* Map quality */
    switch (src->quality) {
        case UFT_TQ_PERFECT:    dst->quality = UFT_IR_QUAL_PERFECT; break;
        case UFT_TQ_GOOD:       dst->quality = UFT_IR_QUAL_GOOD; break;
        case UFT_TQ_MARGINAL:   dst->quality = UFT_IR_QUAL_MARGINAL; break;
        case UFT_TQ_POOR:       dst->quality = UFT_IR_QUAL_POOR; break;
        case UFT_TQ_UNREADABLE: dst->quality = UFT_IR_QUAL_BAD; break;
        default:                dst->quality = UFT_IR_QUAL_UNKNOWN; break;
    }
    
    return 0;
}

/*===========================================================================
 * IPF Track Conversion (Forward Declarations)
 *===========================================================================*/

struct ipf_track;

/**
 * @brief Convert ipf_track to uft_track_base_t
 */
int uft_ipf_track_to_base(const struct ipf_track *src, uft_track_base_t *dst);

/**
 * @brief Convert uft_track_base_t to ipf_track
 */
int uft_base_to_ipf_track(const uft_track_base_t *src, struct ipf_track *dst);

/*===========================================================================
 * SCP Track Conversion (Forward Declarations)
 *===========================================================================*/

struct uft_scp_track_info;

/**
 * @brief Convert SCP track info to uft_track_base_t
 */
int uft_scp_track_to_base(const struct uft_scp_track_info *src, 
                          uft_track_base_t *dst);

/*===========================================================================
 * Generic Track Copy
 *===========================================================================*/

/**
 * @brief Copy track data from any source to base
 * 
 * This function attempts to detect the source type and perform
 * the appropriate conversion.
 * 
 * @param src       Source track (any type)
 * @param src_type  Source type identifier
 * @param dst       Destination base track
 * @return 0 on success, -1 on error
 */
typedef enum {
    UFT_TRACK_TYPE_BASE = 0,
    UFT_TRACK_TYPE_DECODER,
    UFT_TRACK_TYPE_IR,
    UFT_TRACK_TYPE_IPF,
    UFT_TRACK_TYPE_SCP,
    UFT_TRACK_TYPE_TD0,
    UFT_TRACK_TYPE_HFE
} uft_track_src_type_t;

int uft_track_convert_to_base(const void *src, uft_track_src_type_t src_type,
                               uft_track_base_t *dst);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TRACK_COMPAT_H */
