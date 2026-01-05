/**
 * @file uft_track_base.c
 * @brief Unified Track Base Implementation (P2-ARCH-001)
 * 
 * @version 1.0.0
 * @date 2026-01-05
 */

#include "uft/core/uft_track_base.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*===========================================================================
 * Lifecycle
 *===========================================================================*/

uft_track_base_t* uft_track_base_create(uint8_t cylinder, uint8_t head)
{
    uft_track_base_t *track = calloc(1, sizeof(uft_track_base_t));
    if (!track) return NULL;
    
    track->cylinder = cylinder;
    track->head = head;
    track->flags = UFT_TF_NONE;
    track->quality = UFT_TQ_UNKNOWN;
    track->encoding = UFT_ENC_UNKNOWN;
    
    return track;
}

void uft_track_base_free(uft_track_base_t *track)
{
    if (!track) return;
    
    /* Free sectors */
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    /* Free raw data */
    free(track->bitstream);
    free(track->flux_data);
    free(track->weak_mask);
    
    /* Free revolutions */
    for (int i = 0; i < UFT_TRACK_MAX_REVOLUTIONS; i++) {
        if (track->revolutions[i]) {
            free(track->revolutions[i]->flux_data);
            free(track->revolutions[i]->bitstream);
            free(track->revolutions[i]);
        }
    }
    
    free(track);
}

uft_track_base_t* uft_track_base_clone(const uft_track_base_t *src)
{
    if (!src) return NULL;
    
    uft_track_base_t *dst = uft_track_base_create(src->cylinder, src->head);
    if (!dst) return NULL;
    
    /* Copy scalar fields */
    dst->cyl_offset_q = src->cyl_offset_q;
    dst->flags = src->flags;
    dst->quality = src->quality;
    dst->encoding = src->encoding;
    dst->sectors_expected = src->sectors_expected;
    dst->sectors_found = src->sectors_found;
    dst->sectors_good = src->sectors_good;
    dst->sectors_bad = src->sectors_bad;
    dst->bitcell_ns = src->bitcell_ns;
    dst->rpm_x100 = src->rpm_x100;
    dst->track_time_ns = src->track_time_ns;
    dst->write_splice_ns = src->write_splice_ns;
    dst->bit_length = src->bit_length;
    dst->byte_length = src->byte_length;
    dst->detection_confidence = src->detection_confidence;
    dst->protection_type = src->protection_type;
    
    /* Clone sectors */
    if (src->sectors && src->sector_count > 0) {
        if (uft_track_base_alloc_sectors(dst, src->sector_count) == 0) {
            for (size_t i = 0; i < src->sector_count; i++) {
                uft_track_base_add_sector(dst, &src->sectors[i]);
            }
        }
    }
    
    /* Clone bitstream */
    if (src->bitstream && src->bitstream_size > 0) {
        dst->bitstream = malloc(src->bitstream_size);
        if (dst->bitstream) {
            memcpy(dst->bitstream, src->bitstream, src->bitstream_size);
            dst->bitstream_size = src->bitstream_size;
        }
    }
    
    /* Clone flux data */
    if (src->flux_data && src->flux_count > 0) {
        size_t flux_bytes = src->flux_count * sizeof(uint32_t);
        dst->flux_data = malloc(flux_bytes);
        if (dst->flux_data) {
            memcpy(dst->flux_data, src->flux_data, flux_bytes);
            dst->flux_count = src->flux_count;
        }
    }
    
    /* Clone weak regions */
    dst->weak_region_count = src->weak_region_count;
    memcpy(dst->weak_regions, src->weak_regions, 
           sizeof(uft_weak_region_t) * src->weak_region_count);
    
    return dst;
}

void uft_track_base_reset(uft_track_base_t *track)
{
    if (!track) return;
    
    uint8_t cyl = track->cylinder;
    uint8_t head = track->head;
    
    /* Free all data */
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    free(track->bitstream);
    free(track->flux_data);
    free(track->weak_mask);
    
    for (int i = 0; i < UFT_TRACK_MAX_REVOLUTIONS; i++) {
        if (track->revolutions[i]) {
            free(track->revolutions[i]->flux_data);
            free(track->revolutions[i]->bitstream);
            free(track->revolutions[i]);
        }
    }
    
    /* Reset to clean state */
    memset(track, 0, sizeof(uft_track_base_t));
    track->cylinder = cyl;
    track->head = head;
}

/*===========================================================================
 * Sector Management
 *===========================================================================*/

int uft_track_base_alloc_sectors(uft_track_base_t *track, size_t count)
{
    if (!track || count == 0) return -1;
    if (count > UFT_TRACK_MAX_SECTORS) count = UFT_TRACK_MAX_SECTORS;
    
    uft_sector_base_t *sectors = calloc(count, sizeof(uft_sector_base_t));
    if (!sectors) return -1;
    
    /* Free old sectors if any */
    if (track->sectors) {
        for (size_t i = 0; i < track->sector_count; i++) {
            free(track->sectors[i].data);
        }
        free(track->sectors);
    }
    
    track->sectors = sectors;
    track->sector_count = 0;
    track->sector_capacity = count;
    
    return 0;
}

int uft_track_base_add_sector(uft_track_base_t *track, 
                               const uft_sector_base_t *sector)
{
    if (!track || !sector) return -1;
    
    /* Allocate if needed */
    if (!track->sectors) {
        if (uft_track_base_alloc_sectors(track, UFT_TRACK_MAX_SECTORS) != 0) {
            return -1;
        }
    }
    
    if (track->sector_count >= track->sector_capacity) {
        return -1;  /* Full */
    }
    
    /* Copy sector */
    uft_sector_base_t *dst = &track->sectors[track->sector_count];
    *dst = *sector;
    
    /* Deep copy sector data if present */
    if (sector->data && sector->data_size > 0) {
        dst->data = malloc(sector->data_size);
        if (dst->data) {
            memcpy(dst->data, sector->data, sector->data_size);
        }
    } else {
        dst->data = NULL;
    }
    
    track->sector_count++;
    return 0;
}

uft_sector_base_t* uft_track_base_find_sector(uft_track_base_t *track,
                                               uint8_t sector_id)
{
    if (!track || !track->sectors) return NULL;
    
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].sector_id == sector_id) {
            return &track->sectors[i];
        }
    }
    
    return NULL;
}

static int sector_compare(const void *a, const void *b)
{
    const uft_sector_base_t *sa = (const uft_sector_base_t *)a;
    const uft_sector_base_t *sb = (const uft_sector_base_t *)b;
    
    if (sa->bit_offset < sb->bit_offset) return -1;
    if (sa->bit_offset > sb->bit_offset) return 1;
    return 0;
}

void uft_track_base_sort_sectors(uft_track_base_t *track)
{
    if (!track || !track->sectors || track->sector_count < 2) return;
    
    qsort(track->sectors, track->sector_count, 
          sizeof(uft_sector_base_t), sector_compare);
}

/*===========================================================================
 * Data Management
 *===========================================================================*/

int uft_track_base_alloc_bitstream(uft_track_base_t *track, size_t bits)
{
    if (!track || bits == 0) return -1;
    
    size_t bytes = (bits + 7) / 8;
    
    free(track->bitstream);
    track->bitstream = calloc(1, bytes);
    if (!track->bitstream) return -1;
    
    track->bitstream_size = bytes;
    track->bit_length = (uint32_t)bits;
    
    return 0;
}

int uft_track_base_alloc_flux(uft_track_base_t *track, size_t count)
{
    if (!track || count == 0) return -1;
    
    free(track->flux_data);
    track->flux_data = calloc(count, sizeof(uint32_t));
    if (!track->flux_data) return -1;
    
    track->flux_count = count;
    
    return 0;
}

int uft_track_base_alloc_weak_mask(uft_track_base_t *track)
{
    if (!track || track->bitstream_size == 0) return -1;
    
    free(track->weak_mask);
    track->weak_mask = calloc(1, track->bitstream_size);
    
    return track->weak_mask ? 0 : -1;
}

int uft_track_base_add_revolution(uft_track_base_t *track,
                                   uft_revolution_base_t *rev)
{
    if (!track || !rev) return -1;
    if (track->revolution_count >= UFT_TRACK_MAX_REVOLUTIONS) return -1;
    
    track->revolutions[track->revolution_count++] = rev;
    track->flags |= UFT_TF_MULTI_REV;
    
    return 0;
}

/*===========================================================================
 * Analysis
 *===========================================================================*/

uft_track_quality_t uft_track_base_calc_quality(const uft_track_base_t *track)
{
    if (!track) return UFT_TQ_UNKNOWN;
    if (track->sectors_expected == 0) return UFT_TQ_UNKNOWN;
    
    float good_ratio = (float)track->sectors_good / (float)track->sectors_expected;
    
    if (good_ratio >= 1.0f && track->sectors_bad == 0) {
        return UFT_TQ_PERFECT;
    } else if (good_ratio >= 0.9f) {
        return UFT_TQ_GOOD;
    } else if (good_ratio >= 0.5f) {
        return UFT_TQ_MARGINAL;
    } else if (good_ratio > 0.0f) {
        return UFT_TQ_POOR;
    }
    
    return UFT_TQ_UNREADABLE;
}

void uft_track_base_update_stats(uft_track_base_t *track)
{
    if (!track || !track->sectors) return;
    
    track->sectors_found = 0;
    track->sectors_good = 0;
    track->sectors_bad = 0;
    
    for (size_t i = 0; i < track->sector_count; i++) {
        track->sectors_found++;
        if (track->sectors[i].data_ok) {
            track->sectors_good++;
        } else {
            track->sectors_bad++;
        }
    }
    
    track->quality = uft_track_base_calc_quality(track);
    
    if (track->sectors_bad > 0) {
        track->flags |= UFT_TF_CRC_ERRORS;
    }
}

bool uft_track_base_has_errors(const uft_track_base_t *track)
{
    if (!track) return false;
    return (track->flags & UFT_TF_CRC_ERRORS) != 0 || track->sectors_bad > 0;
}

int uft_track_base_error_summary(const uft_track_base_t *track,
                                  char *buffer, size_t size)
{
    if (!track || !buffer || size == 0) return -1;
    
    return snprintf(buffer, size,
                    "Track %u.%u: %u/%u sectors good, %u bad, quality=%s",
                    track->cylinder, track->head,
                    track->sectors_good, track->sectors_expected,
                    track->sectors_bad,
                    uft_track_quality_name(track->quality));
}

/*===========================================================================
 * Conversion Helpers
 *===========================================================================*/

const char* uft_track_encoding_name(uft_track_encoding_t enc)
{
    switch (enc) {
        case UFT_ENC_UNKNOWN:   return "Unknown";
        case UFT_ENC_FM:        return "FM";
        case UFT_ENC_MFM:       return "MFM";
        case UFT_ENC_GCR_C64:   return "GCR (C64)";
        case UFT_ENC_GCR_APPLE: return "GCR (Apple)";
        case UFT_ENC_AMIGA_MFM: return "Amiga MFM";
        case UFT_ENC_GCR_VICTOR:return "GCR (Victor)";
        case UFT_ENC_M2FM:      return "M2FM";
        case UFT_ENC_RAW:       return "Raw";
        default:                return "Unknown";
    }
}

const char* uft_track_quality_name(uft_track_quality_t qual)
{
    switch (qual) {
        case UFT_TQ_UNKNOWN:    return "Unknown";
        case UFT_TQ_PERFECT:    return "Perfect";
        case UFT_TQ_GOOD:       return "Good";
        case UFT_TQ_MARGINAL:   return "Marginal";
        case UFT_TQ_POOR:       return "Poor";
        case UFT_TQ_UNREADABLE: return "Unreadable";
        default:                return "Unknown";
    }
}

int uft_track_flags_describe(uint16_t flags, char *buffer, size_t size)
{
    if (!buffer || size == 0) return -1;
    
    buffer[0] = '\0';
    size_t pos = 0;
    
    const struct { uint16_t flag; const char *name; } flag_names[] = {
        { UFT_TF_PRESENT,   "present" },
        { UFT_TF_INDEXED,   "indexed" },
        { UFT_TF_WEAK_BITS, "weak" },
        { UFT_TF_PROTECTED, "protected" },
        { UFT_TF_LONG,      "long" },
        { UFT_TF_SHORT,     "short" },
        { UFT_TF_MODIFIED,  "modified" },
        { UFT_TF_VARIABLE_DENSITY, "var-density" },
        { UFT_TF_HALF_TRACK,"half-track" },
        { UFT_TF_CRC_ERRORS,"crc-errors" },
        { UFT_TF_MULTI_REV, "multi-rev" },
        { 0, NULL }
    };
    
    for (int i = 0; flag_names[i].name; i++) {
        if (flags & flag_names[i].flag) {
            if (pos > 0 && pos < size - 1) {
                buffer[pos++] = ',';
            }
            int written = snprintf(buffer + pos, size - pos, "%s", 
                                   flag_names[i].name);
            if (written > 0) pos += (size_t)written;
        }
    }
    
    return (int)pos;
}

uft_track_encoding_t uft_track_encoding_from_legacy(int legacy_enc)
{
    /* Map common legacy encoding values */
    switch (legacy_enc) {
        case 0:  return UFT_ENC_UNKNOWN;
        case 1:  return UFT_ENC_FM;
        case 2:  return UFT_ENC_MFM;
        case 3:  return UFT_ENC_GCR_C64;
        case 4:  return UFT_ENC_GCR_APPLE;
        case 5:  return UFT_ENC_AMIGA_MFM;
        default: return UFT_ENC_UNKNOWN;
    }
}

int uft_track_encoding_to_legacy(uft_track_encoding_t enc)
{
    return (int)enc;
}
