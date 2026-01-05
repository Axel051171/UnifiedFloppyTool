// nib.h - Commodore 64 NIB (nibtools raw GCR tracks) (C11, no deps)
// UFT - Unified Floppy Tooling
//
// NIB stores raw per-track GCR bitstreams as used by nibtools.
// It is a preservation/analysis format (between G64 and flux).
// Sector-based access is NOT supported.

#ifndef UFT_NIB_H
#define UFT_NIB_H

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

typedef struct {
    uint32_t track;
    uint32_t bitlen;
    uint8_t *bits;
} NibTrack;

typedef struct {
    uint32_t track_count;
    NibTrack *tracks;
} NibMeta;

/* Unified API */
int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int floppy_analyze_protection(FloppyDevice *dev);

/* Metadata */
const NibMeta* nib_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_NIB_H
