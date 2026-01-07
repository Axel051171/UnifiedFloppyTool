// d71.h - Commodore 64/128 D71 disk image (1571 double-sided) (C11, no deps)
// UFT - Unified Floppy Tooling
//
// D71 is a working sector-dump format for 1571 disks: essentially D64 * 2 sides.
// It preserves *data sectors only*; it does not preserve GCR timing/weak-bits/long-tracks.
//
// Geometry:
// - 35 tracks
// - 2 sides/heads
// - variable sectors/track (same as 1541 zones)
// - 256 bytes/sector
// - total size: 349,696 bytes

#ifndef UFT_D71_H
#define UFT_D71_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t tracks, heads, sectors, sectorSize;
    bool flux_supported;
    void (*log_callback)(const char* msg);
    void *internal_ctx;
} FloppyDevice;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_D71_H
