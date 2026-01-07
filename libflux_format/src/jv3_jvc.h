// jv3_jvc.h - JV3 / JVC (PC-98 / Japanese floppy images) (C11)
// UFT - Unified Floppy Tooling
//
// JV3/JVC are sector-dump working formats used by Japanese systems (PC-98, X68000 tools).
// Typically 77 tracks, 2 heads, 8 or 9 sectors, 1024 bytes/sector (varies).

#ifndef UFT_JV3_JVC_H
#define UFT_JV3_JVC_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint32_t tracks, heads, sectors, sectorSize;
    bool flux_supported;
    void (*log_callback)(const char* msg);
    void *internal_ctx;
} FloppyDevice;

int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

#endif
