/**
 * @file uft_fat12_legacy.c
 * @brief Legacy uft_fat12_* API wrapping the canonical fs/ FAT backend.
 *
 * include/uft/formats/uft_fat12.h declares a `uft_fat12_t`-based API that
 * precedes the canonical fs/uft_fat12.h (uft_fat_ctx_t) port in commit
 * db6897e. It still has one live caller — src/explorertab.cpp — which
 * uses only init/free as a pair around a no-op rename placeholder.
 *
 * This file replaces the four ABI-broken stubs in uft_core_stubs.c
 * (caller passed 4 args, stubs read 2 — silent spec §1.3 violation)
 * with signature-correct implementations that:
 *   - parse the BPB directly (no dependency on fs/uft_fat12.h to avoid
 *     include-guard collision: both headers use #define UFT_FAT12_H)
 *   - return honest UFT_FAT_ERR_READONLY / -1 for the write paths
 *
 * Once the explorertab FAT12 branch is migrated to fs/uft_fat12.h, this
 * file + include/uft/formats/uft_fat12.h can be deleted.
 */

#include "uft/formats/uft_fat12.h"
#include <stdlib.h>
#include <string.h>

/* Little-endian helpers — BPB is always LE. */
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int uft_fat12_init(uft_fat12_t *fs, uint8_t *data, size_t size, bool copy)
{
    if (!fs || !data || size < 512) return -1;
    memset(fs, 0, sizeof(*fs));

    if (copy) {
        fs->data = (uint8_t *)malloc(size);
        if (!fs->data) return -1;
        memcpy(fs->data, data, size);
        fs->data_owned = true;
    } else {
        fs->data = data;
        fs->data_owned = false;
    }
    fs->data_size = size;

    /* Parse BPB into fs->boot if the layout exists — we don't assume the
     * boot struct shape, just fill the calculated geometry fields that
     * explorertab/other callers might peek at. */
    const uint8_t *d = fs->data;
    uint16_t bps      = le16(d + 0x0B);
    uint8_t  spc      = d[0x0D];
    uint16_t reserved = le16(d + 0x0E);
    uint8_t  num_fats = d[0x10];
    uint16_t root_cnt = le16(d + 0x11);
    uint16_t fatsz16  = le16(d + 0x16);
    uint16_t tot16    = le16(d + 0x13);
    uint32_t tot32    = le32(d + 0x20);
    uint32_t total    = tot16 ? tot16 : tot32;

    if (bps != 512 || spc == 0 || num_fats == 0) {
        if (fs->data_owned) free(fs->data);
        memset(fs, 0, sizeof(*fs));
        return -1;
    }

    fs->bytes_per_cluster  = (uint16_t)(spc * bps);
    fs->root_dir_sectors   = (uint16_t)((root_cnt * 32u + bps - 1) / bps);
    fs->first_fat_sector   = reserved;
    fs->first_root_sector  = reserved + num_fats * fatsz16;
    fs->first_data_sector  = fs->first_root_sector + fs->root_dir_sectors;
    if (total >= fs->first_data_sector)
        fs->total_clusters = (total - fs->first_data_sector) / spc;
    fs->modified = false;
    return 0;
}

void uft_fat12_free(uft_fat12_t *fs)
{
    if (!fs) return;
    if (fs->data_owned && fs->data) free(fs->data);
    memset(fs, 0, sizeof(*fs));
}

int uft_fat12_create_entry(uft_fat12_t *fs, const char *path, uint8_t attributes)
{
    (void)fs; (void)path; (void)attributes;
    /* Write path — the canonical fs/ backend is read-only too, so this
     * returns the documented "negative on error". */
    return -1;
}

int uft_fat12_delete(uft_fat12_t *fs, const char *path)
{
    (void)fs; (void)path;
    return -1;
}
