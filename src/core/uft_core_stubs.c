/**
 * @file uft_core_stubs.c
 * @brief Minimal core function stubs
 *
 * Provides essential functions from uft_core.c without pulling in
 * the full dependency chain (uft_strerror, plugin registry, etc.)
 */

#include "uft/uft_format_plugin.h"
#include <stddef.h>

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
