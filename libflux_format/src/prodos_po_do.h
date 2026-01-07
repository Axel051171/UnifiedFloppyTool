// prodos_po_do.h - Apple II ProDOS .PO / .DO sector images (C11, no deps)
// UFT - Unified Floppy Tooling
//
// .PO (ProDOS order) and .DO (DOS order) are sector-dump working formats
// for Apple II disks (typically 5.25", 35 tracks, 16 sectors, 256 bytes).
//
// These are NOT preservation formats (no timing/weak bits).

#ifndef UFT_PRODOS_PO_DO_H
#define UFT_PRODOS_PO_DO_H

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

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_PRODOS_PO_DO_H
