#pragma once
/*
 * flux_format.h — Plugin-Schnittstelle für Disk-Image-Formate.
 *
 * Dieses Layer ist strikt getrennt vom Hardware-Capture (libflux_hw) und
 * vom mathematischen Modell (libflux_core). Es kann:
 *   - "Flux-native" Container (SCP, STREAM, KF stream, ...)
 *   - "Sector-native" Container (DSK, IMD, D88, ...) -> später via Decoder nach Flux/Bitstream
 *
 * Für diesen Sprint: wir fokussieren auf saubere Modularität + Probe/Read/Write Hooks.
 */

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#include "flux_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum fluxfmt_cap {
    FLUXFMT_CAN_READ  = 1u << 0,
    FLUXFMT_CAN_WRITE = 1u << 1,
    FLUXFMT_HIDDEN    = 1u << 2
} fluxfmt_cap_t;

typedef struct fluxfmt_plugin {
    const char *name;      /* e.g. "DSK" */
    const char *ext;       /* file extension hint, e.g. "dsk" */
    uint32_t caps;

    /* Quick signature check. Return true if file seems to match. */
    bool (*probe)(const uint8_t *buf, size_t len);

    /* Parse from stream to ufm_disk_t OR to an intermediate later.
     * Return 0 on success, <0 on error.
     */
    int  (*read)(FILE *fp, ufm_disk_t *out);

    /* Write from ufm_disk_t. Return 0 on success, <0 on error. */
    int  (*write)(FILE *fp, const ufm_disk_t *in);
} fluxfmt_plugin_t;

/* Registry (generated) */
const fluxfmt_plugin_t* fluxfmt_registry(size_t *count);

#ifdef __cplusplus
}
#endif
