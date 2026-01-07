// f86.h - 86F (IBM PC preservation format) (C11, no deps)
// UFT - Unified Floppy Tooling
//
// 86F is a track-based preservation format used by PCE/PCem for copy-protected PC disks.
// It stores per-track data streams and flags for weak bits / special timing.
//
// This module is analysis-oriented and GUI-integratable via the unified API.

#ifndef UFT_86F_H
#define UFT_86F_H

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

/* Flux metadata */
typedef struct {
    uint32_t nominal_cell_ns;
    uint32_t jitter_ns;
    uint32_t encoding_hint; /* PC = MFM */
} FluxTimingProfile;

typedef struct {
    uint32_t track, head;
    uint32_t start_bitcell, length_bitcell;
    uint32_t prng_seed;
} WeakBitRegion;

typedef struct {
    FluxTimingProfile timing;
    WeakBitRegion *weak_regions;
    uint32_t weak_region_count;
} FluxMeta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_86F_H
