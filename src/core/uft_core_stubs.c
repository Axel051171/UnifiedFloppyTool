/**
 * @file uft_core_stubs.c
 * @brief Minimal core function stubs
 *
 * Provides essential functions from uft_core.c without pulling in
 * the full dependency chain (uft_strerror, plugin registry, etc.)
 */

/**
 * @file uft_core_stubs.c
 * @brief Minimal stubs — no heavy headers, no conflicts.
 *
 * These stub functions avoid including uft_format_plugin.h or uft_disk.h
 * because those headers have enum/struct redefinition conflicts with
 * each other and with other UFT headers. Instead we use opaque void*
 * pointers and minimal logic.
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

/* Opaque stubs — the real implementations are in the full core module.
 * These exist only so the linker doesn't fail when GUI code references
 * these symbols before the full core is linked. */

const void* uft_track_find_sector(const void* track, int sector) {
    (void)track; (void)sector;
    return NULL;  /* Stub: full impl in core module */
}

uint32_t uft_track_get_status(const void* track) {
    (void)track;
    return 0;  /* Stub: full impl in core module */
}

void uft_disk_close(void *disk) {
    free(disk);  /* Basic cleanup — full impl frees tracks/image_data */
}
