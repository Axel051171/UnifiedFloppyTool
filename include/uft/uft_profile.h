/**
 * @file uft_profile.h
 * @brief Export GUI-friendly format profiles (supported outputs + default options).
 *
 * This is meant as a “single source of truth” for GUIs and CLIs:
 * - Which disk formats exist and what their base geometry is
 * - Which output containers make sense for that format
 * - Default JSON option blobs for recovery / per-format tweaks / exporter
 *
 * The GUI can either use the schema APIs (uft_params.h) or simply query a
 * complete profile blob here.
 */

#ifndef UFT_PROFILE_H
#define UFT_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#include "uft/uft_formats.h"
#include "uft/uft_output.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build a JSON profile for a given disk format.
 *
 * The returned string is heap-allocated with malloc() and must be freed by
 * the caller using uft_profile_free().
 *
 * JSON shape (stable keys):
 * {
 *   "format": { "id": 7, "name": "PC 1.44M", "tracks": 80, "heads": 2, ... },
 *   "outputs": [ { "id": 1, "name": "Raw IMG", "ext": "img" }, ... ],
 *   "defaults": {
 *     "recovery": { ... },
 *     "format": { ... },
 *     "export": { "img": { ... }, "adf": { ... } }
 *   }
 * }
 */
char *uft_format_profile_json(uft_disk_format_id_t fmt);

/**
 * @brief Free a profile JSON string.
 */
void uft_profile_free(char *p);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PROFILE_H */
