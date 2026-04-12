/**
 * @file uft_core_stubs.c
 * @brief Minimal core function stubs
 *
 * Provides essential functions from uft_core.c without pulling in
 * the full dependency chain (uft_strerror, plugin registry, etc.)
 */

/* Only include uft_disk.h for the full struct definition.
 * Do NOT include uft_format_plugin.h here — it has a conflicting
 * struct uft_disk definition that causes redefinition errors. */
#include "uft/uft_disk.h"
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

void uft_disk_close(uft_disk_t *disk) {
    if (!disk) return;

    /* Free track array */
    if (disk->tracks) {
        for (int i = 0; i < disk->track_count; i++) {
            free(disk->tracks[i]);
        }
        free(disk->tracks);
        disk->tracks = NULL;
    }

    /* Free image buffer */
    if (disk->image_data) {
        free(disk->image_data);
        disk->image_data = NULL;
    }

    disk->is_open = false;
    free(disk);
}
