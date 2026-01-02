// hdm.h - HDM raw sector image (77 tracks, 2 heads, 8 sectors, 1024 bytes) (C11, no deps)
// UFT - Unified Floppy Tooling
//
// HDM in practice is a headerless raw dump with 1024-byte sectors and 77 tracks,
// common in some non-PC ecosystems / 8-inch style geometries.
// Geometry example: 77 tracks, 2 heads, 8 sectors/track, 1024 bytes/sector. citeturn0search11
//
// Notes:
// - Like IMG, HDM cannot represent weak-bits or bad CRC sectors.
// - This module is still FLUX-READY (exposes metadata structs) so higher layers can preserve intent.

#ifndef UFT_HDM_H
#define UFT_HDM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "uft/uft_error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    uint32_t tracks, heads, sectors, sectorSize;
    bool flux_supported;
    void (*log_callback)(const char* msg);
    void *internal_ctx;
} FloppyDevice;

/* Flux-ready metadata */
typedef struct {
    uint32_t nominal_cell_ns;
    uint32_t jitter_ns;
    uint32_t encoding_hint; /* 0=unknown, 1=MFM, 2=FM, 3=GCR */
} FluxTimingProfile;

typedef struct {
    uint32_t track;
    uint32_t head;
    uint32_t start_bitcell;
    uint32_t length_bitcell;
    uint32_t prng_seed;
} WeakBitRegion;

typedef struct {
    FluxTimingProfile timing;
    WeakBitRegion *weak_regions;
    uint32_t weak_region_count;
} FluxMeta;

/* Unified API */
int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int floppy_analyze_protection(FloppyDevice *dev);

/* Helpers */
uint64_t hdm_expected_size(void);
int hdm_create_new(const char *out_path); /* creates 77x2x8x1024, zero-filled */

int generate_flux_pattern(uint8_t *out_bits, size_t out_bits_len,
                          uint32_t seed, uint32_t nominal_cell_ns, uint32_t jitter_ns);

#ifdef __cplusplus
}
#endif

#endif // UFT_HDM_H
