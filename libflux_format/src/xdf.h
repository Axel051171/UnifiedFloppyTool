// xdf.h - IBM XDF (Extended Density Format) floppy image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// XDF is IBM's high-capacity 3.5" floppy format using variable sectors/track.
// Working/analysis format: sector addressing is non-uniform per track.
//
// Typical capacities:
// - ~1.86 MB (80 tracks, 2 heads, variable SPT, 512 bytes)
//
// NOTE: Precise layouts vary; this module provides safe read-only analysis hooks.

#ifndef UFT_XDF_H
#define UFT_XDF_H

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

/* Metadata */
typedef struct {
    uint16_t tracks;
    uint16_t heads;
    uint32_t approx_bytes;
    bool variable_spt;
} XdfMeta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* Metadata access */
const XdfMeta* xdf_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_XDF_H
