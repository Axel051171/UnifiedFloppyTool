// dmk.h - TRS-80 DMK track image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// DMK is a preservation-oriented track image used for TRS-80 systems.
// Track-based with sector offset tables; supports weak bits and timing nuances.
// Sector-based access is generally NOT reliable; analysis/conversion preferred.

#ifndef UFT_DMK_H
#define UFT_DMK_H

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
    uint16_t track_no;
    uint16_t track_len;
    uint8_t *raw;
} DmkTrack;

typedef struct {
    uint16_t track_count;
    uint16_t track_len;
    bool single_density;
    bool double_sided;
    DmkTrack *tracks;
} DmkMeta;

/* Unified API */
int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int floppy_analyze_protection(FloppyDevice *dev);

/* Metadata */
const DmkMeta* dmk_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_DMK_H
