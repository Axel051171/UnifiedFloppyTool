// g64.h - Commodore 64 G64 (GCR track image) (C11, no deps)
// UFT - Unified Floppy Tooling
//
// G64 preserves per-track GCR bitstreams (variable length per track).
// This is a preservation-oriented format for C64 disks (better than D64).
//
// Notes:
// - Sector addressing is not native; CHS read/write is intentionally ENOTSUP.
// - Flux/GCR decoding is handled by higher layers (FluxDec/GCRDec).

#ifndef UFT_G64_H
#define UFT_G64_H

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

/* GCR / Flux-ready metadata */
typedef struct {
    uint32_t nominal_cell_ns;
    uint32_t jitter_ns;
    uint32_t encoding_hint; /* 3=GCR */
} FluxTimingProfile;

typedef struct {
    uint32_t track;
    uint32_t bitlen;
    uint8_t *gcr_bits;
} GcrTrack;

typedef struct {
    FluxTimingProfile timing;
    GcrTrack *tracks;
    uint32_t track_count;
} GcrMeta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* GCR access */
const GcrMeta* g64_get_gcr(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_G64_H
