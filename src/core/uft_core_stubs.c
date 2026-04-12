/**
 * @file uft_core_stubs.c
 * @brief Minimal core function stubs
 *
 * Provides essential functions from uft_core.c without pulling in
 * the full dependency chain (uft_strerror, plugin registry, etc.)
 */

/* Minimal stubs for core functions.
 *
 * NOTE: uft_track_t and uft_sector_t are defined in uft_format_plugin.h
 * which conflicts with uft_disk.h (both define struct uft_disk).
 * We use uft_format_plugin.h for track/sector access, and cast
 * the disk pointer to void* for uft_disk_close. */
#include "uft/uft_format_plugin.h"
#include <stddef.h>
#include <stdlib.h>

const uft_sector_t* uft_track_find_sector(const uft_track_t* track, int sector) {
    if (!track) return NULL;
    for (size_t i = 0; i < track->sector_count; i++) {
        if (track->sectors[i].id.sector == sector)
            return &track->sectors[i];
    }
    return NULL;
}

uint32_t uft_track_get_status(const uft_track_t* track) {
    return track ? track->status : 0;
}

void uft_disk_close(void *disk_ptr) {
    /* Cast to the struct uft_disk defined in uft_format_plugin.h.
     * This avoids the redefinition conflict with uft_disk.h. */
    struct uft_disk *disk = (struct uft_disk *)disk_ptr;
    if (!disk) return;
    free(disk);
}
