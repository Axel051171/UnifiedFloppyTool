/**
 * @file uft_fs_amigados_driver.c
 * @brief First uft_fs_driver_t adapter — wraps uft_amiga_* for the
 *        generic FS registry. Activated by the AmigaDOS backend landing
 *        in src/fs/uft_amigados.c.
 *
 * Probe strategy: look at disk->image_data / image_size, call
 * uft_amiga_detect(), return 90 if bootblock checksum is valid else 40.
 *
 * Mount strategy: allocate a private struct holding a uft_amiga_ctx_t
 * opened via uft_amiga_open_buffer(copy=true) so the mounted FS owns
 * its own copy of the data.
 */

#include "uft/uft_integration.h"
#include "uft/uft_error.h"
#include "uft/fs/uft_amigados.h"
#include "uft/uft_format_plugin.h"   /* struct uft_disk */

#include <stdlib.h>
#include <string.h>

/* Opaque filesystem struct: the registry hands a uft_filesystem_t* back
 * to callers, but the underlying type is driver-private. */
struct uft_filesystem {
    uft_amiga_ctx_t *ctx;
};

/* ---------- probe ---------- */

static int amigados_probe(const uft_disk_t *disk)
{
    if (!disk) return 0;
    if (!disk->image_data || disk->image_size < 1024) return 0;

    uft_amiga_detect_t det;
    memset(&det, 0, sizeof(det));
    int rc = uft_amiga_detect(disk->image_data, disk->image_size, &det);
    if (rc != 0) return 0;

    /* Valid DOS bootblock: high confidence. Otherwise a medium score —
     * uft_amiga_detect() is tolerant of corrupted/zeroed bootblocks. */
    if (det.bootblock_valid) return 90;
    return 40;
}

/* ---------- mount / unmount ---------- */

static uft_error_t amigados_mount(const uft_disk_t *disk, uft_filesystem_t **fs_out)
{
    if (!disk || !fs_out) return UFT_ERROR_NULL_POINTER;
    if (!disk->image_data || disk->image_size == 0) return UFT_ERROR_INVALID_ARG;

    uft_amiga_ctx_t *ctx = uft_amiga_create();
    if (!ctx) return UFT_ERROR_NO_MEMORY;

    /* copy=true so the FS survives a uft_disk_close() on the source. */
    int rc = uft_amiga_open_buffer(ctx, disk->image_data, disk->image_size,
                                   /*copy=*/true, /*options=*/NULL);
    if (rc != 0) {
        uft_amiga_destroy(ctx);
        return UFT_ERROR_FORMAT;
    }

    uft_filesystem_t *fs = (uft_filesystem_t *)calloc(1, sizeof(*fs));
    if (!fs) { uft_amiga_close(ctx); uft_amiga_destroy(ctx); return UFT_ERROR_NO_MEMORY; }
    fs->ctx = ctx;
    *fs_out = fs;
    return UFT_OK;
}

static void amigados_unmount(uft_filesystem_t *fs)
{
    if (!fs) return;
    if (fs->ctx) {
        uft_amiga_close(fs->ctx);
        uft_amiga_destroy(fs->ctx);
    }
    free(fs);
}

/* ---------- readdir ---------- */

static uft_error_t amigados_readdir(uft_filesystem_t *fs, const char *path,
                                     uft_dirent_t **entries, size_t *count)
{
    if (!fs || !fs->ctx || !entries || !count) return UFT_ERROR_NULL_POINTER;
    *entries = NULL;
    *count = 0;

    uft_amiga_dir_t dir;
    memset(&dir, 0, sizeof(dir));
    const char *p = (path && *path) ? path : "";
    int rc = uft_amiga_load_dir_path(fs->ctx, p, &dir);
    if (rc != 0) return UFT_ERROR_NOT_FOUND;

    if (dir.count == 0) { uft_amiga_free_dir(&dir); return UFT_OK; }

    uft_dirent_t *out = (uft_dirent_t *)calloc(dir.count, sizeof(uft_dirent_t));
    if (!out) { uft_amiga_free_dir(&dir); return UFT_ERROR_NO_MEMORY; }

    for (size_t i = 0; i < dir.count; i++) {
        const uft_amiga_entry_t *e = &dir.entries[i];
        uft_dirent_t *d = &out[i];
        strncpy(d->name, e->name, sizeof(d->name) - 1);
        d->size          = e->size;
        d->attributes    = (uint8_t)(e->protection & 0xFF);
        d->start_sector  = (uint16_t)(e->first_data & 0xFFFF);
        d->start_track   = (uint16_t)((e->first_data >> 16) & 0xFFFF);
        d->is_directory  = e->is_dir;
        /* AmigaDOS protection bit 3 (H) = hidden, bit 2 (W) = writable.
         * Use inverted-write for "protected". */
        d->is_hidden     = (e->protection & (1u << 7)) != 0;
        d->is_protected  = (e->protection & (1u << 2)) == 0;
    }

    uft_amiga_free_dir(&dir);
    *entries = out;
    *count = dir.count;
    return UFT_OK;
}

/* ---------- read ---------- */

static uft_error_t amigados_read(uft_filesystem_t *fs, const char *path,
                                  uint8_t **data, size_t *size)
{
    if (!fs || !fs->ctx || !path || !data || !size) return UFT_ERROR_NULL_POINTER;
    *data = NULL; *size = 0;

    uint8_t *buf = NULL;
    size_t   sz  = 0;
    int rc = uft_amiga_extract_file_alloc(fs->ctx, path, &buf, &sz);
    if (rc != 0 || !buf) return UFT_ERROR_NOT_FOUND;
    *data = buf;
    *size = sz;
    return UFT_OK;
}

/* ---------- stat ---------- */

static uft_error_t amigados_stat(uft_filesystem_t *fs,
                                  size_t *total_blocks, size_t *free_blocks,
                                  size_t *block_size)
{
    if (!fs || !fs->ctx) return UFT_ERROR_NULL_POINTER;
    if (total_blocks) *total_blocks = fs->ctx->total_blocks;
    if (free_blocks)  *free_blocks  = 0;   /* requires bitmap walk — skip */
    if (block_size)   *block_size   = UFT_AMIGA_BLOCK_SIZE;
    return UFT_OK;
}

/* ---------- driver instance ---------- */

/* One driver handles both OFS and FFS: probe just reports generic AmigaDOS.
 * If a caller explicitly asked for UFT_FS_AMIGA_FFS we still match. */
static const uft_fs_driver_t amigados_driver = {
    .name     = "AmigaDOS",
    .type     = UFT_FS_AMIGA_OFS,  /* also handles FFS */
    .platform = UFT_PLATFORM_AMIGA,
    .probe    = amigados_probe,
    .mount    = amigados_mount,
    .unmount  = amigados_unmount,
    .readdir  = amigados_readdir,
    .read     = amigados_read,
    .stat     = amigados_stat,
};

__attribute__((constructor))
static void amigados_driver_register(void)
{
    uft_fs_driver_register(&amigados_driver);
}
