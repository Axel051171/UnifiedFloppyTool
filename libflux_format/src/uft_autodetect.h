// uft_autodetect.h - Unified format auto-detection & vtable routing (C11)
// UFT - Unified Floppy Tooling
//
// Drop this into your build where all format modules are compiled.
// It provides:
//  - Format identification (magic/extension/size heuristics)
//  - A single open() that routes to the correct module via a unified vtable
//
// Design goal: keep your GUI simple: one include, one vtable list, one open().
//
// NOTE: This file does not pull any external dependencies. It relies on
// format modules exporting the unified symbols:
//
//   int uft_floppy_open(FloppyDevice*, const char*);
//   int uft_floppy_close(FloppyDevice*);
//   int uft_floppy_read_sector(FloppyDevice*, uint32_t, uint32_t, uint32_t, uint8_t*);
//   int uft_floppy_write_sector(FloppyDevice*, uint32_t, uint32_t, uint32_t, const uint8_t*);
//   int uft_floppy_analyze_protection(FloppyDevice*);
//
// Because each module uses the SAME function names, you cannot link them all into
// one binary without namespacing. Therefore, in the UFT library you should compile
// each module with a prefix macro, or wrap them in per-format translation units.
//
// This autodetect module assumes you provide per-format namespaced wrappers, e.g.:
//   pcimg_floppy_open(), d88_floppy_open(), fdi_floppy_open(), ...
// See uft_autodetect.c for expected wrapper symbol names.

#ifndef UFT_AUTODETECT_H
#define UFT_AUTODETECT_H

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

typedef enum {
    UFT_FMT_UNKNOWN = 0,
    UFT_FMT_PC_IMG,
    UFT_FMT_D88,
    UFT_FMT_FDI,
    UFT_FMT_IMD,
    UFT_FMT_ATX,
    UFT_FMT_86F,
    UFT_FMT_SCP,
    UFT_FMT_GWRAW,
    UFT_FMT_HDM,
    UFT_FMT_ATR,
    UFT_FMT_D64,
    UFT_FMT_G64,
    UFT_FMT_ST,
    UFT_FMT_MSA,
    UFT_FMT_ADF,
    UFT_FMT_IPF
} UftFormatId;

typedef struct {
    UftFormatId id;
    const char *name;
    const char *ext_primary; /* lowercase without dot, optional */
    /* vtable */
    int (*open)(FloppyDevice *dev, const char *path);
    int (*close)(FloppyDevice *dev);
    int (*read_sector)(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf);
    int (*write_sector)(FloppyDevice *dev, uint32_t t, uint32_t h, uint32_t s, const uint8_t *buf);
    int (*analyze_protection)(FloppyDevice *dev);
} UftFormatVtable;

/* Detect format by content + extension heuristics. Returns UFT_FMT_UNKNOWN if unsure. */
UftFormatId uft_detect_format(const char *path);

/* Get compiled-in vtable by id, or NULL. */
const UftFormatVtable* uft_get_vtable(UftFormatId id);

/* Convenience: detect then open. Returns 0 on success (propagates module errors). */
int uft_open_auto(FloppyDevice *dev, const char *path, UftFormatId *out_id);

#ifdef __cplusplus
}
#endif

#endif // UFT_AUTODETECT_H
