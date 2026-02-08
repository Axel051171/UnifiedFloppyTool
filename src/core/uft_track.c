/**
 * @file uft_track.c
 * @brief Unified Track Implementation (P1-4)
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_track.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ═══════════════════════════════════════════════════════════════════════════
 * Internal Helpers
 * ═══════════════════════════════════════════════════════════════════════════ */

static void* safe_malloc(size_t size)
{
    if (size == 0) return NULL;
    void *ptr = malloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

static void safe_free(void **ptr)
{
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Lifecycle Functions
 * ═══════════════════════════════════════════════════════════════════════════ */

void uft_track_init(uft_track_t *track)
{
    if (!track) return;
    
    memset(track, 0, sizeof(uft_track_t));
    track->_magic = UFT_TRACK_MAGIC;
    track->_version = UFT_TRACK_VERSION;
    track->encoding = UFT_ENC_UNKNOWN;
    track->nominal_rpm = 300.0;
    track->quality = 1.0f;
}

uft_track_t* uft_track_alloc(uint32_t layers, size_t bit_count)
{
    uft_track_t *track = (uft_track_t*)safe_malloc(sizeof(uft_track_t));
    if (!track) return NULL;
    
    uft_track_init(track);
    track->available_layers = layers;
    
    /* Allocate flux layer */
    if (layers & UFT_LAYER_FLUX) {
        track->flux = (uft_flux_layer_t*)safe_malloc(sizeof(uft_flux_layer_t));
        if (!track->flux) goto error;
    }
    
    /* Allocate bitstream layer */
    if (layers & UFT_LAYER_BITSTREAM) {
        track->bitstream = (uft_bitstream_layer_t*)safe_malloc(sizeof(uft_bitstream_layer_t));
        if (!track->bitstream) goto error;
        
        if (bit_count > 0) {
            size_t byte_count = (bit_count + 7) / 8;
            track->bitstream->bits = (uint8_t*)safe_malloc(byte_count);
            if (!track->bitstream->bits) goto error;
            track->bitstream->capacity = byte_count;
            track->bitstream->byte_count = byte_count;
        }
    }
    
    /* Allocate sector layer */
    if (layers & UFT_LAYER_SECTORS) {
        track->sector_layer = (uft_sector_layer_t*)safe_malloc(sizeof(uft_sector_layer_t));
        if (!track->sector_layer) goto error;
        
        /* Pre-allocate for common case */
        track->sector_layer->sectors = (uft_sector_t*)safe_malloc(32 * sizeof(uft_sector_t));
        if (!track->sector_layer->sectors) goto error;
        track->sector_layer->capacity = 32;
    }
    
    return track;

error:
    uft_track_free(track);
    return NULL;
}

void uft_track_free(uft_track_t *track)
{
    if (!track) return;
    
    /* Free flux layer */
    if (track->flux) {
        safe_free((void**)&track->flux->samples);
        safe_free((void**)&track->flux);
    }
    
    /* Free bitstream layer */
    if (track->bitstream) {
        safe_free((void**)&track->bitstream->bits);
        safe_free((void**)&track->bitstream->timing);
        safe_free((void**)&track->bitstream->weak_mask);
        safe_free((void**)&track->bitstream->index_positions);
        safe_free((void**)&track->bitstream);
    }
    
    /* Free sector layer */
    if (track->sector_layer) {
        if (track->sector_layer->sectors) {
            for (size_t i = 0; i < track->sector_layer->count; i++) {
                safe_free((void**)&track->sector_layer->sectors[i].data);
            }
            safe_free((void**)&track->sector_layer->sectors);
        }
        safe_free((void**)&track->sector_layer);
    }
    
    /* Free legacy data */
    safe_free((void**)&track->raw_data);
    safe_free((void**)&track->flux_data);
    
    /* Free sector data in legacy array */
    for (int i = 0; i < track->sector_count && i < UFT_MAX_SECTORS; i++) {
        safe_free((void**)&track->sectors[i].data);
    }
    
    /* Clear and free */
    track->_magic = 0;
    free(track);
}

void uft_track_clear(uft_track_t *track)
{
    if (!track) return;
    
    /* Save magic and version */
    uint32_t magic = track->_magic;
    uint32_t version = track->_version;
    
    /* Free internal data but not track itself */
    if (track->flux) {
        safe_free((void**)&track->flux->samples);
        track->flux->sample_count = 0;
    }
    
    if (track->bitstream) {
        /* Keep allocated buffer, just reset count */
        track->bitstream->bit_count = 0;
        track->bitstream->byte_count = 0;
        if (track->bitstream->timing) {
            safe_free((void**)&track->bitstream->timing);
        }
        if (track->bitstream->weak_mask) {
            safe_free((void**)&track->bitstream->weak_mask);
        }
    }
    
    if (track->sector_layer) {
        for (size_t i = 0; i < track->sector_layer->count; i++) {
            safe_free((void**)&track->sector_layer->sectors[i].data);
        }
        track->sector_layer->count = 0;
    }
    
    /* Clear legacy */
    track->sector_count = 0;
    track->status = UFT_TRACK_OK;
    track->decoded = false;
    track->errors = 0;
    
    /* Restore magic */
    track->_magic = magic;
    track->_version = version;
}

uft_track_t* uft_track_clone(const uft_track_t *src)
{
    if (!src || !UFT_TRACK_VALID(src)) return NULL;
    
    uft_track_t *dst = uft_track_alloc(src->available_layers, 
                                        UFT_TRACK_BIT_COUNT(src));
    if (!dst) return NULL;
    
    /* Copy identity */
    dst->cylinder = src->cylinder;
    dst->head = src->head;
    dst->quarter_offset = src->quarter_offset;
    dst->is_half_track = src->is_half_track;
    
    /* Copy encoding */
    dst->encoding = src->encoding;
    dst->bitrate = src->bitrate;
    dst->rpm = src->rpm;
    dst->nominal_bit_rate_kbps = src->nominal_bit_rate_kbps;
    dst->nominal_rpm = src->nominal_rpm;
    
    /* Copy status */
    dst->status = src->status;
    dst->decoded = src->decoded;
    dst->errors = src->errors;
    dst->quality = src->quality;
    dst->quality_ext = src->quality_ext;
    
    /* Copy bitstream */
    if (src->bitstream && src->bitstream->bits && src->bitstream->bit_count > 0) {
        uft_track_set_bits(dst, src->bitstream->bits, src->bitstream->bit_count);
        
        if (src->bitstream->timing) {
            uft_track_set_timing(dst, src->bitstream->timing, src->bitstream->timing_count);
        }
        if (src->bitstream->weak_mask) {
            uft_track_set_weak_mask(dst, src->bitstream->weak_mask, src->bitstream->byte_count);
        }
    }
    
    /* Copy sectors */
    if (src->sector_layer && src->sector_layer->sectors) {
        for (size_t i = 0; i < src->sector_layer->count; i++) {
            uft_track_add_sector(dst, &src->sector_layer->sectors[i]);
        }
    }
    
    return dst;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Layer Management
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_add_layer(uft_track_t *track, uft_layer_flags_t layer, size_t capacity)
{
    if (!track) return -1;
    
    if ((layer == UFT_LAYER_FLUX) && !track->flux) {
        track->flux = (uft_flux_layer_t*)safe_malloc(sizeof(uft_flux_layer_t));
        if (!track->flux) return -1;
        if (capacity > 0) {
            track->flux->samples = (uint32_t*)safe_malloc(capacity * sizeof(uint32_t));
            if (!track->flux->samples) return -1;
            track->flux->sample_capacity = capacity;
        }
        track->available_layers |= UFT_LAYER_FLUX;
    }
    
    if ((layer == UFT_LAYER_BITSTREAM) && !track->bitstream) {
        track->bitstream = (uft_bitstream_layer_t*)safe_malloc(sizeof(uft_bitstream_layer_t));
        if (!track->bitstream) return -1;
        if (capacity > 0) {
            size_t bytes = (capacity + 7) / 8;
            track->bitstream->bits = (uint8_t*)safe_malloc(bytes);
            if (!track->bitstream->bits) return -1;
            track->bitstream->capacity = bytes;
        }
        track->available_layers |= UFT_LAYER_BITSTREAM;
    }
    
    if ((layer == UFT_LAYER_SECTORS) && !track->sector_layer) {
        track->sector_layer = (uft_sector_layer_t*)safe_malloc(sizeof(uft_sector_layer_t));
        if (!track->sector_layer) return -1;
        size_t cap = capacity > 0 ? capacity : 32;
        track->sector_layer->sectors = (uft_sector_t*)safe_malloc(cap * sizeof(uft_sector_t));
        if (!track->sector_layer->sectors) return -1;
        track->sector_layer->capacity = cap;
        track->available_layers |= UFT_LAYER_SECTORS;
    }
    
    return 0;
}

void uft_track_remove_layer(uft_track_t *track, uft_layer_flags_t layer)
{
    if (!track) return;
    
    if ((layer == UFT_LAYER_FLUX) && track->flux) {
        safe_free((void**)&track->flux->samples);
        safe_free((void**)&track->flux);
        track->available_layers &= ~UFT_LAYER_FLUX;
    }
    
    if ((layer == UFT_LAYER_BITSTREAM) && track->bitstream) {
        safe_free((void**)&track->bitstream->bits);
        safe_free((void**)&track->bitstream->timing);
        safe_free((void**)&track->bitstream->weak_mask);
        safe_free((void**)&track->bitstream->index_positions);
        safe_free((void**)&track->bitstream);
        track->available_layers &= ~UFT_LAYER_BITSTREAM;
    }
    
    if ((layer == UFT_LAYER_SECTORS) && track->sector_layer) {
        if (track->sector_layer->sectors) {
            for (size_t i = 0; i < track->sector_layer->count; i++) {
                safe_free((void**)&track->sector_layer->sectors[i].data);
            }
            safe_free((void**)&track->sector_layer->sectors);
        }
        safe_free((void**)&track->sector_layer);
        track->available_layers &= ~UFT_LAYER_SECTORS;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Bitstream Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_set_bits(uft_track_t *track, const uint8_t *bits, size_t bit_count)
{
    if (!track || !bits || bit_count == 0) return -1;
    
    /* Ensure bitstream layer exists */
    if (!track->bitstream) {
        if (uft_track_add_layer(track, UFT_LAYER_BITSTREAM, bit_count) != 0) {
            return -1;
        }
    }
    
    size_t byte_count = (bit_count + 7) / 8;
    
    /* Reallocate if needed */
    if (byte_count > track->bitstream->capacity) {
        uint8_t *new_bits = (uint8_t*)realloc(track->bitstream->bits, byte_count);
        if (!new_bits) return -1;
        track->bitstream->bits = new_bits;
        track->bitstream->capacity = byte_count;
    }
    
    memcpy(track->bitstream->bits, bits, byte_count);
    track->bitstream->bit_count = bit_count;
    track->bitstream->byte_count = byte_count;
    
    return 0;
}

int uft_track_get_bits(const uft_track_t *track, uint8_t *bits, size_t *bit_count)
{
    if (!track || !bits || !bit_count) return -1;
    
    if (!track->bitstream || !track->bitstream->bits) {
        *bit_count = 0;
        return -1;
    }
    
    memcpy(bits, track->bitstream->bits, track->bitstream->byte_count);
    *bit_count = track->bitstream->bit_count;
    
    return 0;
}

int uft_track_set_timing(uft_track_t *track, const uint16_t *timing, size_t count)
{
    if (!track || !timing || count == 0) return -1;
    if (!track->bitstream) return -1;
    
    /* Reallocate timing array */
    uint16_t *new_timing = (uint16_t*)realloc(track->bitstream->timing, 
                                               count * sizeof(uint16_t));
    if (!new_timing) return -1;
    
    memcpy(new_timing, timing, count * sizeof(uint16_t));
    track->bitstream->timing = new_timing;
    track->bitstream->timing_count = count;
    track->available_layers |= UFT_LAYER_TIMING;
    
    return 0;
}

int uft_track_set_weak_mask(uft_track_t *track, const uint8_t *mask, size_t byte_count)
{
    if (!track || !mask || byte_count == 0) return -1;
    if (!track->bitstream) return -1;
    
    uint8_t *new_mask = (uint8_t*)realloc(track->bitstream->weak_mask, byte_count);
    if (!new_mask) return -1;
    
    memcpy(new_mask, mask, byte_count);
    track->bitstream->weak_mask = new_mask;
    track->available_layers |= UFT_LAYER_WEAK;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Sector Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_add_sector(uft_track_t *track, const uft_sector_t *sector)
{
    if (!track || !sector) return -1;
    
    /* Ensure sector layer exists */
    if (!track->sector_layer) {
        if (uft_track_add_layer(track, UFT_LAYER_SECTORS, 32) != 0) {
            return -1;
        }
    }
    
    /* Grow if needed */
    if (track->sector_layer->count >= track->sector_layer->capacity) {
        size_t new_cap = track->sector_layer->capacity * 2;
        uft_sector_t *new_sectors = (uft_sector_t*)realloc(
            track->sector_layer->sectors, new_cap * sizeof(uft_sector_t));
        if (!new_sectors) return -1;
        track->sector_layer->sectors = new_sectors;
        track->sector_layer->capacity = new_cap;
    }
    
    /* Copy sector */
    uft_sector_t *dst = &track->sector_layer->sectors[track->sector_layer->count];
    *dst = *sector;
    
    /* Deep copy data */
    if (sector->data && sector->data_len > 0) {
        dst->data = (uint8_t*)safe_malloc(sector->data_len);
        if (!dst->data) return -1;
        memcpy(dst->data, sector->data, sector->data_len);
    }
    
    track->sector_layer->count++;
    
    /* Update statistics */
    track->sector_layer->found++;
    if (sector->crc_ok) {
        track->sector_layer->good++;
    } else {
        track->sector_layer->bad++;
    }
    
    /* Update legacy array too */
    if (track->sector_count < UFT_MAX_SECTORS) {
        track->sectors[track->sector_count] = *sector;
        if (sector->data && sector->data_len > 0) {
            track->sectors[track->sector_count].data = (uint8_t*)safe_malloc(sector->data_len);
            if (track->sectors[track->sector_count].data) {
                memcpy(track->sectors[track->sector_count].data, sector->data, sector->data_len);
            }
        }
        track->sector_count++;
    }
    
    return 0;
}

const uft_sector_t* uft_track_get_sector(const uft_track_t *track, int record)
{
    if (!track) return NULL;
    
    /* Try new layer first */
    if (track->sector_layer && track->sector_layer->sectors) {
        for (size_t i = 0; i < track->sector_layer->count; i++) {
            if (track->sector_layer->sectors[i].sector_id == record) {
                return &track->sector_layer->sectors[i];
            }
        }
    }
    
    /* Fall back to legacy array */
    for (int i = 0; i < track->sector_count && i < UFT_MAX_SECTORS; i++) {
        if (track->sectors[i].sector_id == record) {
            return &track->sectors[i];
        }
    }
    
    return NULL;
}

const uft_sector_t* uft_track_get_sectors(const uft_track_t *track, size_t *count)
{
    if (!track || !count) return NULL;
    
    if (track->sector_layer && track->sector_layer->sectors) {
        *count = track->sector_layer->count;
        return track->sector_layer->sectors;
    }
    
    *count = (size_t)track->sector_count;
    return track->sectors;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Flux Operations
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_set_flux(uft_track_t *track, const uint32_t *samples, 
                       size_t count, double sample_rate_mhz)
{
    if (!track || !samples || count == 0) return -1;
    
    if (!track->flux) {
        if (uft_track_add_layer(track, UFT_LAYER_FLUX, count) != 0) {
            return -1;
        }
    }
    
    /* Reallocate if needed */
    if (count > track->flux->sample_capacity) {
        uint32_t *new_samples = (uint32_t*)realloc(track->flux->samples, 
                                                    count * sizeof(uint32_t));
        if (!new_samples) return -1;
        track->flux->samples = new_samples;
        track->flux->sample_capacity = count;
    }
    
    memcpy(track->flux->samples, samples, count * sizeof(uint32_t));
    track->flux->sample_count = count;
    track->flux->sample_rate_mhz = sample_rate_mhz;
    
    return 0;
}

int uft_track_add_revolution(uft_track_t *track, const uint32_t *samples, size_t count)
{
    if (!track || !samples || count == 0) return -1;
    if (!track->flux) return -1;
    
    /* For now, just append (a more sophisticated implementation would keep revolutions separate) */
    size_t new_count = track->flux->sample_count + count;
    if (new_count > track->flux->sample_capacity) {
        size_t new_cap = new_count * 2;
        uint32_t *new_samples = (uint32_t*)realloc(track->flux->samples,
                                                    new_cap * sizeof(uint32_t));
        if (!new_samples) return -1;
        track->flux->samples = new_samples;
        track->flux->sample_capacity = new_cap;
    }
    
    memcpy(&track->flux->samples[track->flux->sample_count], samples, 
           count * sizeof(uint32_t));
    track->flux->sample_count = new_count;
    track->flux->total_revolutions++;
    track->available_layers |= UFT_LAYER_MULTIREV;
    
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Validation
 * ═══════════════════════════════════════════════════════════════════════════ */

int uft_track_compare(const uft_track_t *a, const uft_track_t *b)
{
    if (!a && !b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    /* Compare identity */
    if (a->cylinder != b->cylinder) return a->cylinder - b->cylinder;
    if (a->head != b->head) return a->head - b->head;
    
    /* Compare bitstream */
    size_t a_bits = UFT_TRACK_BIT_COUNT(a);
    size_t b_bits = UFT_TRACK_BIT_COUNT(b);
    if (a_bits != b_bits) return (int)(a_bits - b_bits);
    
    if (a->bitstream && b->bitstream && a->bitstream->bits && b->bitstream->bits) {
        return memcmp(a->bitstream->bits, b->bitstream->bits, a->bitstream->byte_count);
    }
    
    return 0;
}

int uft_track_validate(const uft_track_t *track)
{
    if (!track) return -1;
    if (track->_magic != UFT_TRACK_MAGIC) return -2;
    if (track->cylinder > 83) return -3;
    if (track->head > 1) return -4;
    
    return 0;
}

const char* uft_track_status_str(const uft_track_t *track, char *buf, size_t buf_size)
{
    if (!track || !buf || buf_size < 32) {
        if (buf && buf_size > 0) buf[0] = '\0';
        return buf;
    }
    
    static const char* enc_names[] = {
        "Unknown", "FM", "MFM", "GCR-CBM", "GCR-Apple", "GCR-Victor", "Amiga", "Raw"
    };
    
    const char *enc = (track->encoding < UFT_ENC_COUNT) ? 
                      enc_names[track->encoding] : "?";
    
    snprintf(buf, buf_size, "C%02d.H%d %s %zu bits %d sectors",
             track->cylinder, track->head, enc,
             UFT_TRACK_BIT_COUNT(track),
             (int)UFT_TRACK_SECTOR_COUNT(track));
    
    return buf;
}
