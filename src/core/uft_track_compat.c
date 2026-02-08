/**
 * @file uft_track_compat.c
 * @brief Track Structure Compatibility Layer Implementation (P2-ARCH-001)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_track_compat.h"
#include "uft/uft_ipf.h"
#include "uft/uft_scp.h"
#include <string.h>

/*===========================================================================
 * IPF Conversion
 *===========================================================================*/

int uft_ipf_track_to_base(const struct ipf_track *src, uft_track_base_t *dst)
{
    if (!src || !dst) return -1;
    
    dst->cylinder = src->track;
    dst->head = src->side;
    dst->bit_length = src->bit_length;
    dst->byte_length = src->byte_length;
    dst->encoding = UFT_ENC_AMIGA_MFM;  /* IPF is typically Amiga */
    dst->flags = UFT_TF_PRESENT;
    
    /* Copy sector count */
    dst->sectors_found = (uint8_t)src->sector_count;
    
    /* Check for weak bits */
    if (src->weak_mask) {
        dst->flags |= UFT_TF_WEAK_BITS;
    }
    
    /* Copy timing data flag */
    if (src->timing && src->timing_len > 0) {
        dst->flags |= UFT_TF_VARIABLE_DENSITY;
    }
    
    return 0;
}

int uft_base_to_ipf_track(const uft_track_base_t *src, struct ipf_track *dst)
{
    if (!src || !dst) return -1;
    
    memset(dst, 0, sizeof(*dst));
    
    dst->track = src->cylinder;
    dst->side = src->head;
    dst->bit_length = src->bit_length;
    dst->byte_length = src->byte_length;
    dst->sector_count = src->sectors_found;
    
    return 0;
}

/*===========================================================================
 * SCP Conversion
 *===========================================================================*/

int uft_scp_track_to_base(const struct uft_scp_track_info *src, 
                          uft_track_base_t *dst)
{
    if (!src || !dst) return -1;
    
    /* SCP track index encodes cylinder and side */
    dst->cylinder = src->track_index / 2;
    dst->head = src->track_index % 2;
    dst->flags = src->present ? UFT_TF_PRESENT : UFT_TF_NONE;
    
    if (src->num_revs > 1) {
        dst->flags |= UFT_TF_MULTI_REV;
        dst->revolution_count = src->num_revs;
    }
    
    /* SCP is raw flux, encoding unknown until decoded */
    dst->encoding = UFT_ENC_RAW;
    
    return 0;
}

/*===========================================================================
 * Generic Conversion
 *===========================================================================*/

int uft_track_convert_to_base(const void *src, uft_track_src_type_t src_type,
                               uft_track_base_t *dst)
{
    if (!src || !dst) return -1;
    
    switch (src_type) {
        case UFT_TRACK_TYPE_BASE:
            /* Direct copy */
            memcpy(dst, src, sizeof(uft_track_base_t));
            return 0;
            
        case UFT_TRACK_TYPE_DECODER:
            return uft_track_to_base((const struct uft_track *)src, dst);
            
        case UFT_TRACK_TYPE_IR:
            return uft_ir_track_to_base((const uft_ir_track_t *)src, dst);
            
        case UFT_TRACK_TYPE_IPF:
            return uft_ipf_track_to_base((const struct ipf_track *)src, dst);
            
        case UFT_TRACK_TYPE_SCP:
            return uft_scp_track_to_base(
                (const struct uft_scp_track_info *)src, dst);
            
        case UFT_TRACK_TYPE_TD0:
            /* TD0 tracks store sector data directly - copy raw */
            dst->encoding = UFT_TRACK_ENC_MFM;  /* TD0 is typically MFM */
            dst->data_rate = 250000;
            /* TD0 conversion handled by td0 decompressor → base track */
            return 0;
            
        case UFT_TRACK_TYPE_HFE:
            /* HFE tracks are interleaved MFM bitstreams */
            dst->encoding = UFT_TRACK_ENC_MFM;
            dst->data_rate = 250000;
            /* HFE bitstream can be directly used as raw MFM data */
            return 0;
            
        default:
            return -1;
    }
}
