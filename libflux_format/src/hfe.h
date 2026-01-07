// hfe.h - HxC HFE floppy image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// HFE is a track-based image used by the UFT HFE Format.
// It stores per-track bitcells with timing information.
// Sector-based access is NOT supported by design.

#ifndef UFT_HFE_H
#define UFT_HFE_H

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
    uint16_t track;
    uint32_t bitcells;
    uint8_t *data;
} HfeTrack;

typedef struct {
    uint8_t version;
    uint16_t track_count;
    uint8_t sides;
    HfeTrack *tracks;
} HfeMeta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* Metadata */
const HfeMeta* hfe_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_HFE_H
