// gwraw.h - Greaseweazle RAW/GWF flux format (C11, no deps)
// UFT - Unified Floppy Tooling
//
// Greaseweazle RAW/GWF stores per-track flux intervals similar to SCP,
// but optimized for Greaseweazle hardware workflows.
//
// This module is flux-only and analysis-oriented.

#ifndef UFT_GWRAW_H
#define UFT_GWRAW_H

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

/* Native flux metadata */
typedef struct {
    uint32_t nominal_cell_ns;
    uint32_t jitter_ns;
    uint32_t encoding_hint; /* 0=unknown */
} FluxTimingProfile;

typedef struct {
    uint32_t track;
    uint32_t head;
    uint32_t *flux_intervals_ns;
    uint32_t flux_count;
} FluxTrack;

typedef struct {
    FluxTimingProfile timing;
    FluxTrack *tracks;
    uint32_t track_count;
} FluxMeta;

/* Unified API */
int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int floppy_analyze_protection(FloppyDevice *dev);

/* Flux access */
const FluxMeta* gwraw_get_flux(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_GWRAW_H
