/**
 * @file uft_disk_image_compat.h
 * @brief Provides uft_disk_image_t for format files using old API types
 *
 * Cat A format files (apridisk, cfi, nanowasp, qrst, hardsector) need
 * uft_disk_image_t but use the old API type system (uft_types.h).
 * This header provides a compatible uft_disk_image_t struct and
 * helper functions using old API types.
 *
 * Include AFTER uft_format_common.h / format-specific header.
 */

#ifndef UFT_DISK_IMAGE_COMPAT_H
#define UFT_DISK_IMAGE_COMPAT_H

#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * uft_disk_image_t using old API types (uft_track_t from uft_format_plugin.h)
 * ============================================================================ */

#ifndef UFT_DISK_IMAGE_COMPAT_DEFINED
#define UFT_DISK_IMAGE_COMPAT_DEFINED

typedef struct uft_disk_image_compat {
    /* Format info */
    uft_format_t format;
    char format_name[32];

    /* Geometry */
    uint16_t tracks;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t bytes_per_sector;

    /* Track data [track * heads + head] */
    uft_track_t **track_data;
    size_t track_count;

    /* Optional extras */
    char *comment;
    size_t comment_len;
    char *source_path;
    uint64_t file_size;
    bool owns_data;
} uft_disk_image_t;

/* Allocate disk image with tracks*heads track slots */
static inline uft_disk_image_t* uft_disk_alloc(uint16_t ntracks, uint8_t nheads) {
    uft_disk_image_t *d = (uft_disk_image_t*)calloc(1, sizeof(uft_disk_image_t));
    if (!d) return NULL;
    d->tracks = ntracks;
    d->heads = nheads;
    d->track_count = (size_t)ntracks * nheads;
    d->track_data = (uft_track_t**)calloc(d->track_count, sizeof(uft_track_t*));
    if (!d->track_data) { free(d); return NULL; }
    for (size_t i = 0; i < d->track_count; i++) {
        d->track_data[i] = (uft_track_t*)calloc(1, sizeof(uft_track_t));
    }
    d->owns_data = true;
    return d;
}

/* Free disk image and all track/sector data */
static inline void uft_disk_free(uft_disk_image_t *d) {
    if (!d) return;
    if (d->track_data) {
        for (size_t i = 0; i < d->track_count; i++) {
            if (d->track_data[i]) {
                uft_track_t *t = d->track_data[i];
                if (t->sectors) {
                    for (size_t s = 0; s < t->sector_count; s++) {
                        free(t->sectors[s].data);
                    }
                    free(t->sectors);
                }
                free(t->raw_data);
                free(t);
            }
        }
        free(d->track_data);
    }
    free(d->comment);
    free(d->source_path);
    free(d);
}

#endif /* UFT_DISK_IMAGE_COMPAT_DEFINED */

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_IMAGE_COMPAT_H */
