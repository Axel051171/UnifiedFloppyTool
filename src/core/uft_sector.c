/**
 * @file uft_sector.c
 * @brief Unified Sector Implementation (P2-ARCH-004)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_sector.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_sector_unified_t* uft_sector_create(void)
{
    uft_sector_unified_t *sector = calloc(1, sizeof(uft_sector_unified_t));
    if (!sector) return NULL;
    
    sector->confidence = 0.0f;
    return sector;
}

void uft_sector_free(uft_sector_unified_t *sector)
{
    if (!sector) return;
    
    free(sector->data);
    free(sector->original_timing);
    
    /* Free versions */
    for (int i = 0; i < UFT_SECTOR_MAX_ALT_DATA; i++) {
        if (sector->versions[i]) {
            free(sector->versions[i]->data);
            free(sector->versions[i]->weak_mask);
            free(sector->versions[i]);
        }
    }
    
    free(sector);
}

uft_sector_unified_t* uft_sector_clone(const uft_sector_unified_t *src)
{
    if (!src) return NULL;
    
    uft_sector_unified_t *dst = uft_sector_create();
    if (!dst) return NULL;
    
    /* Copy scalar fields */
    dst->addr = src->addr;
    dst->flags = src->flags;
    dst->data_size = src->data_size;
    dst->data_crc_stored = src->data_crc_stored;
    dst->data_crc_calc = src->data_crc_calc;
    dst->data_mark = src->data_mark;
    dst->confidence = src->confidence;
    dst->timing_variance = src->timing_variance;
    dst->error_bits = src->error_bits;
    dst->bit_start = src->bit_start;
    dst->bit_end = src->bit_end;
    dst->gap_before = src->gap_before;
    dst->version_count = src->version_count;
    dst->best_version = src->best_version;
    dst->protection_type = src->protection_type;
    
    /* Clone primary data */
    if (src->data && src->data_size > 0) {
        dst->data = malloc(src->data_size);
        if (dst->data) {
            memcpy(dst->data, src->data, src->data_size);
        }
    }
    
    /* Clone timing */
    if (src->original_timing && src->timing_size > 0) {
        dst->original_timing = malloc(src->timing_size);
        if (dst->original_timing) {
            memcpy(dst->original_timing, src->original_timing, src->timing_size);
            dst->timing_size = src->timing_size;
        }
    }
    
    /* Clone versions */
    for (int i = 0; i < UFT_SECTOR_MAX_ALT_DATA && i < src->version_count; i++) {
        if (src->versions[i]) {
            dst->versions[i] = calloc(1, sizeof(uft_sector_data_version_t));
            if (dst->versions[i]) {
                *dst->versions[i] = *src->versions[i];
                
                /* Clone version data */
                if (src->versions[i]->data && src->versions[i]->size > 0) {
                    dst->versions[i]->data = malloc(src->versions[i]->size);
                    if (dst->versions[i]->data) {
                        memcpy(dst->versions[i]->data, 
                               src->versions[i]->data, 
                               src->versions[i]->size);
                    }
                }
            }
        }
    }
    
    return dst;
}

void uft_sector_reset(uft_sector_unified_t *sector)
{
    if (!sector) return;
    
    free(sector->data);
    free(sector->original_timing);
    
    for (int i = 0; i < UFT_SECTOR_MAX_ALT_DATA; i++) {
        if (sector->versions[i]) {
            free(sector->versions[i]->data);
            free(sector->versions[i]->weak_mask);
            free(sector->versions[i]);
        }
    }
    
    memset(sector, 0, sizeof(uft_sector_unified_t));
}

/*===========================================================================
 * Data Management
 *===========================================================================*/

int uft_sector_alloc_data(uft_sector_unified_t *sector, size_t size)
{
    if (!sector || size == 0 || size > UFT_SECTOR_MAX_SIZE) return -1;
    
    free(sector->data);
    sector->data = calloc(1, size);
    if (!sector->data) return -1;
    
    sector->data_size = (uint16_t)size;
    sector->flags |= UFT_SF_DATA_PRESENT;
    
    return 0;
}

int uft_sector_set_data(uft_sector_unified_t *sector, 
                        const uint8_t *data, size_t size)
{
    if (!sector || !data || size == 0) return -1;
    
    if (uft_sector_alloc_data(sector, size) != 0) return -1;
    
    memcpy(sector->data, data, size);
    return 0;
}

int uft_sector_add_version(uft_sector_unified_t *sector,
                           const uft_sector_data_version_t *version)
{
    if (!sector || !version) return -1;
    if (sector->version_count >= UFT_SECTOR_MAX_ALT_DATA) return -1;
    
    uft_sector_data_version_t *v = calloc(1, sizeof(*v));
    if (!v) return -1;
    
    *v = *version;
    
    /* Clone data */
    if (version->data && version->size > 0) {
        v->data = malloc(version->size);
        if (!v->data) {
            free(v);
            return -1;
        }
        memcpy(v->data, version->data, version->size);
    }
    
    sector->versions[sector->version_count++] = v;
    sector->flags |= UFT_SF_MULTIPLE_COPIES;
    
    return 0;
}

/*===========================================================================
 * Address
 *===========================================================================*/

void uft_sector_set_addr(uft_sector_unified_t *sector,
                         uint8_t cyl, uint8_t head, uint8_t sec, uint8_t size_code)
{
    if (!sector) return;
    
    sector->addr.cylinder = cyl;
    sector->addr.head = head;
    sector->addr.sector = sec;
    sector->addr.size_code = size_code;
    sector->flags |= UFT_SF_PRESENT;
}

uint16_t uft_sector_get_size(const uft_sector_unified_t *sector)
{
    if (!sector) return 0;
    
    if (sector->data_size > 0) {
        return sector->data_size;
    }
    
    /* Calculate from size code */
    return (uint16_t)(128 << sector->addr.size_code);
}

/*===========================================================================
 * Quality
 *===========================================================================*/

float uft_sector_calc_confidence(const uft_sector_unified_t *sector)
{
    if (!sector) return 0.0f;
    if (!(sector->flags & UFT_SF_PRESENT)) return 0.0f;
    
    float conf = 1.0f;
    
    /* Header CRC */
    if (sector->flags & UFT_SF_HEADER_CRC_OK) {
        conf *= 1.0f;
    } else {
        conf *= 0.3f;
    }
    
    /* Data CRC */
    if (sector->flags & UFT_SF_DATA_CRC_OK) {
        conf *= 1.0f;
    } else if (sector->flags & UFT_SF_CRC_CORRECTED) {
        conf *= 0.8f;
    } else {
        conf *= 0.2f;
    }
    
    /* Weak bits penalty */
    if (sector->flags & UFT_SF_WEAK_BITS) {
        conf *= 0.7f;
    }
    
    /* Timing variance penalty */
    if (sector->flags & UFT_SF_TIMING_VARIANCE) {
        conf *= 0.9f;
    }
    
    return conf;
}

void uft_sector_select_best_version(uft_sector_unified_t *sector)
{
    if (!sector || sector->version_count == 0) return;
    
    float best_conf = -1.0f;
    int best_idx = 0;
    
    for (int i = 0; i < sector->version_count; i++) {
        if (sector->versions[i] && sector->versions[i]->confidence > best_conf) {
            best_conf = sector->versions[i]->confidence;
            best_idx = i;
        }
    }
    
    sector->best_version = (uint8_t)best_idx;
    
    /* Copy best version to primary data */
    if (sector->versions[best_idx] && sector->versions[best_idx]->data) {
        uft_sector_set_data(sector, 
                            sector->versions[best_idx]->data,
                            sector->versions[best_idx]->size);
        sector->data_crc_stored = sector->versions[best_idx]->data_crc_stored;
        sector->data_crc_calc = sector->versions[best_idx]->data_crc_calc;
        sector->data_mark = sector->versions[best_idx]->data_mark;
        sector->confidence = sector->versions[best_idx]->confidence;
    }
}

/*===========================================================================
 * Comparison
 *===========================================================================*/

bool uft_sector_data_equals(const uft_sector_unified_t *a,
                            const uft_sector_unified_t *b)
{
    if (!a || !b) return false;
    if (!a->data || !b->data) return false;
    if (a->data_size != b->data_size) return false;
    
    return memcmp(a->data, b->data, a->data_size) == 0;
}

int uft_sector_compare_addr(const uft_sector_unified_t *a,
                            const uft_sector_unified_t *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    /* Compare by CHS */
    if (a->addr.cylinder != b->addr.cylinder) {
        return (int)a->addr.cylinder - (int)b->addr.cylinder;
    }
    if (a->addr.head != b->addr.head) {
        return (int)a->addr.head - (int)b->addr.head;
    }
    return (int)a->addr.sector - (int)b->addr.sector;
}

/*===========================================================================
 * Debug
 *===========================================================================*/

const char* uft_sector_flags_str(uint16_t flags)
{
    static char buf[256];
    buf[0] = '\0';
    size_t pos = 0;
    
    if (flags & UFT_SF_PRESENT)      pos += snprintf(buf+pos, sizeof(buf)-pos, "present,");
    if (flags & UFT_SF_DATA_CRC_OK)  pos += snprintf(buf+pos, sizeof(buf)-pos, "crc-ok,");
    if (flags & UFT_SF_DELETED_DATA) pos += snprintf(buf+pos, sizeof(buf)-pos, "deleted,");
    if (flags & UFT_SF_WEAK_BITS)    pos += snprintf(buf+pos, sizeof(buf)-pos, "weak,");
    if (flags & UFT_SF_PROTECTED)    pos += snprintf(buf+pos, sizeof(buf)-pos, "protected,");
    
    if (pos > 0 && buf[pos-1] == ',') buf[pos-1] = '\0';
    
    return buf;
}

int uft_sector_dump(const uft_sector_unified_t *sector, 
                    char *buffer, size_t size)
{
    if (!sector || !buffer || size == 0) return -1;
    
    return snprintf(buffer, size,
        "Sector C=%u H=%u S=%u N=%u: size=%u flags=[%s] conf=%.2f",
        sector->addr.cylinder, sector->addr.head,
        sector->addr.sector, sector->addr.size_code,
        sector->data_size,
        uft_sector_flags_str(sector->flags),
        sector->confidence);
}

/*===========================================================================
 * Legacy Conversion (Stubs - implement when needed)
 *===========================================================================*/

int uft_sector_from_decoder(const struct uft_sector *src,
                            uft_sector_unified_t *dst)
{
    (void)src;
    (void)dst;
    /* TODO: Implement when integrating with decoder */
    return -1;
}

int uft_sector_to_decoder(const uft_sector_unified_t *src,
                          struct uft_sector *dst)
{
    (void)src;
    (void)dst;
    /* TODO: Implement when integrating with decoder */
    return -1;
}

int uft_sector_from_ipf(const struct ipf_sector *src,
                        uft_sector_unified_t *dst)
{
    (void)src;
    (void)dst;
    /* TODO: Implement when integrating with IPF */
    return -1;
}

int uft_sector_to_ipf(const uft_sector_unified_t *src,
                      struct ipf_sector *dst)
{
    (void)src;
    (void)dst;
    /* TODO: Implement when integrating with IPF */
    return -1;
}
