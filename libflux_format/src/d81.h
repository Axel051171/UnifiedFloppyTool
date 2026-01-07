// d81.h - Commodore 1581 D81 disk image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// D81 is a working sector-dump format for 1581 disks (3.5").
// Geometry:
// - 80 tracks
// - 2 heads
// - 10 sectors/track
// - 512 bytes/sector
// - total size: 819,200 bytes
//
// This format preserves data only (no GCR timing, weak bits, or long tracks).

#ifndef UFT_D81_H
#define UFT_D81_H

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

#endif // UFT_D81_H
