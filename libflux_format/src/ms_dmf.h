// ms_dmf.h - Microsoft DMF (Distribution Media Format) 1.68MB (C11)
// UFT - Unified Floppy Tooling
//
// DMF (Microsoft) uses non-standard sector layouts (21 sectors/track).
// This is NOT MSX DMF.

#ifndef UFT_MS_DMF_H
#define UFT_MS_DMF_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t tracks, heads, sectors, sectorSize;
    bool flux_supported;
    void (*log_callback)(const char* msg);
    void *internal_ctx;
} FloppyDevice;

int floppy_open(FloppyDevice *dev, const char *path);
int floppy_close(FloppyDevice *dev);
int floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int floppy_analyze_protection(FloppyDevice *dev);

#endif
