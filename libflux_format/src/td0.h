// td0.h - Teledisk TD0 disk image (C11, no deps)
// UFT - Unified Floppy Tooling
//
// TD0 is a compressed disk image format used by Teledisk.
// It can store sector dumps plus error information (CRC errors, weak reads).
// Decompression historically used LZSS variants; this module focuses on
// container identification and analysis hooks.
//
// Sector-based access may be available after full decompression (not included).

#ifndef UFT_TD0_H
#define UFT_TD0_H

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
    uint16_t version;
    bool has_crc_errors;
    bool has_deleted_data;
    bool has_weak_reads;
} Td0Meta;

/* Unified API */
int uft_floppy_open(FloppyDevice *dev, const char *path);
int uft_floppy_close(FloppyDevice *dev);
int uft_floppy_read_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
int uft_floppy_write_sector(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
int uft_floppy_analyze_protection(FloppyDevice *dev);

/* Metadata */
const Td0Meta* td0_get_meta(const FloppyDevice *dev);

#ifdef __cplusplus
}
#endif

#endif // UFT_TD0_H
