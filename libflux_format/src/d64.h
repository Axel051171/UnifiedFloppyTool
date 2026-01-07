// d64.h - Commodore 64 D64 disk image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// D64 is a sector dump of a 1541 disk with variable sectors per track (GCR on disk).
// Working format only: no weak bits or true GCR timing preserved.

#ifndef UFT_D64_H
#define UFT_D64_H

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
#endif
