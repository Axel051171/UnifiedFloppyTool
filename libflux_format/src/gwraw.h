// UFT - Unified Floppy Tooling
//
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
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* Flux access */
const FluxMeta* gwraw_get_flux(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_GWRAW_H
