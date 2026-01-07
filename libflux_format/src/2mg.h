// 2mg.h - Apple IIgs 2MG disk image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// 2MG is a container for Apple II/IIgs disk images (sector-based).
// It wraps raw data (often ProDOS-order) with a small header.
//
// Working format: no timing/weak bits.

#ifndef UFT_2MG_H
#define UFT_2MG_H

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
    uint32_t data_offset;
    uint32_t data_size;
    uint16_t version;
    uint32_t flags;
} TwoMgMeta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* Metadata */
const TwoMgMeta* twomg_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_2MG_H
