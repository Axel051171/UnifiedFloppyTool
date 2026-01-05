#pragma once
/*
 * ufm_meta_export.h — Full-disk metadata sidecar exporter (JSON)
 *
 * Purpose:
 *  - Export what we *observed* during capture + decode:
 *      hw meta, retry stats, quality, cp flags, media profile
 *      per-track decode stats (if available)
 *      per-sector map incl. IDAM/DAM, CRC, confidence (if available)
 *
 * This is NOT a file-format writer. It is preservation metadata.
 */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ufm_disk;

/* Export a human+machine readable JSON sidecar.
 * Returns 0 on success.
 */
int ufm_export_meta_json(const struct ufm_disk *d, const char *json_path);

#ifdef __cplusplus
}
#endif
