// woz.h - Apple II WOZ (v1/v2) track image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// WOZ is a preservation-grade track image for Apple II (5.25" / 3.5").
// Track-based, exact bitstream with timing hints.
// Sector-based access is NOT supported by design.

#ifndef UFT_WOZ_H
#define UFT_WOZ_H

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

/* Track metadata */
typedef struct {
    uint16_t track_no;
    uint32_t bit_count;
    uint8_t *bits; /* packed bits */
} WozTrack;

typedef struct {
    uint8_t version; /* 1 or 2 */
    uint16_t track_count;
    WozTrack *tracks;
} WozMeta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* Metadata access */
const WozMeta* woz_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_WOZ_H
