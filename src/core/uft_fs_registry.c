/**
 * @file uft_fs_registry.c
 * @brief Filesystem driver registry — Phase P2a.
 *
 * uft_integration.h has declared uft_fs_driver_t + register/get/mount_auto
 * since the early integration work, but the three functions had no
 * implementation anywhere in the tree. Without them, no filesystem can
 * be added uniformly: each FS lives in its own ad-hoc API (uft_amiga_*,
 * uft_fat_*, uft_cbm_*, uft_apple_*, …).
 *
 * This file lights up the registry so drivers can register themselves
 * via __attribute__((constructor)) or an explicit call. Mirrors the
 * HAL pattern in uft_hw_batch.c.
 *
 * No drivers are wrapped here — each filesystem adapter lands in its
 * own follow-up commit.
 */

#include "uft/uft_integration.h"
#include "uft/uft_error.h"

#include <stddef.h>

#define UFT_FS_MAX_DRIVERS 32

static const uft_fs_driver_t *g_drivers[UFT_FS_MAX_DRIVERS];
static size_t g_driver_count = 0;

uft_error_t uft_fs_driver_register(const uft_fs_driver_t *driver)
{
    if (!driver) return UFT_ERROR_NULL_POINTER;
    if (g_driver_count >= UFT_FS_MAX_DRIVERS) return UFT_ERROR_NO_MEMORY;
    for (size_t i = 0; i < g_driver_count; i++)
        if (g_drivers[i] == driver) return UFT_OK;   /* duplicate */
    g_drivers[g_driver_count++] = driver;
    return UFT_OK;
}

const uft_fs_driver_t *uft_fs_driver_get(uft_fs_type_t type)
{
    for (size_t i = 0; i < g_driver_count; i++)
        if (g_drivers[i] && g_drivers[i]->type == type)
            return g_drivers[i];
    return NULL;
}

uft_error_t uft_fs_mount_auto(const uft_disk_t *disk,
                               uft_filesystem_t **fs,
                               const uft_fs_driver_t **used_driver)
{
    if (!disk || !fs) return UFT_ERROR_NULL_POINTER;
    *fs = NULL;
    if (used_driver) *used_driver = NULL;
    if (g_driver_count == 0) return UFT_ERROR_NOT_FOUND;

    const uft_fs_driver_t *best = NULL;
    int best_score = 0;
    for (size_t i = 0; i < g_driver_count; i++) {
        const uft_fs_driver_t *d = g_drivers[i];
        if (!d || !d->probe) continue;
        int score = d->probe(disk);
        if (score > best_score) { best_score = score; best = d; }
    }
    if (!best || best_score <= 0) return UFT_ERROR_NOT_FOUND;
    if (!best->mount) return UFT_ERROR_NOT_SUPPORTED;

    uft_error_t err = best->mount(disk, fs);
    if (err == UFT_OK && used_driver) *used_driver = best;
    return err;
}

/* Diagnostics: how many drivers are currently registered. */
size_t uft_fs_driver_count(void)
{
    return g_driver_count;
}
