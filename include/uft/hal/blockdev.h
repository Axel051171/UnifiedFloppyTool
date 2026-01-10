#pragma once
/*
 * blockdev.h — portable block device abstraction for sector-based media.
 * UFT uses this as the stable API between loaders/writers and platform/HAL backends.
 */
#include <stddef.h>
#include <stdint.h>
#include "uft/uft_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_blockdev uft_blockdev_t;

typedef struct uft_blockdev_ops {
    /* Reads exactly `count` sectors starting at LBA into `dst` (bytes = count*sector_size). */
    uft_rc_t (*read_lba)(uft_blockdev_t *bd, uint64_t lba, uint32_t count, void *dst, uft_diag_t *diag);
    /* Writes exactly `count` sectors starting at LBA from `src`. */
    uft_rc_t (*write_lba)(uft_blockdev_t *bd, uint64_t lba, uint32_t count, const void *src, uft_diag_t *diag);
    /* Optional: flush/cache sync. */
    uft_rc_t (*flush)(uft_blockdev_t *bd, uft_diag_t *diag);
} uft_blockdev_ops_t;

struct uft_blockdev {
    uft_blockdev_ops_t ops;
    uint32_t sector_size; /* bytes, e.g. 512 */
    void *user;           /* backend state */
};

/* Helper: validate ops table. */
static inline uft_rc_t uft_blockdev_validate(const uft_blockdev_t *bd, uft_diag_t *diag)
{
    if (!bd) { uft_diag_set(diag, "blockdev: null"); return UFT_EINVAL; }
    if (!bd->ops.read_lba) { uft_diag_set(diag, "blockdev: read_lba missing"); return UFT_EINVAL; }
    if (!bd->ops.write_lba) { uft_diag_set(diag, "blockdev: write_lba missing"); return UFT_EINVAL; }
    if (bd->sector_size == 0) { uft_diag_set(diag, "blockdev: sector_size=0"); return UFT_EINVAL; }
    return UFT_OK;
}

#ifdef __cplusplus
}
#endif
