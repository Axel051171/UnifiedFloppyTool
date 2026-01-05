/**
 * @file uft_catalog.h
 * @brief Export complete format catalogs and parameter schemas as JSON.
 *
 * This is the “one call” API for GUIs:
 *  - list all formats
 *  - list supported output containers per format
 *  - provide default option blobs
 *  - provide parameter schemas (types/ranges/enums)
 *
 * The JSON is intentionally simple and stable: GUIs can cache it and only
 * refresh when the backend version changes.
 */

#ifndef UFT_CATALOG_H
#define UFT_CATALOG_H

#include "uft/uft_formats.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Build a JSON array of format profiles.
 *
 * Shape:
 * {
 *   "version": "1.0",
 *   "formats": [ <uft_format_profile_json(fmt)> , ... ]
 * }
 *
 * The returned string is heap-allocated with malloc() and must be freed
 * using uft_catalog_free().
 */
char *uft_catalog_profiles_json(void);

/**
 * @brief Build a JSON blob describing all known parameter schemas.
 *
 * Shape:
 * {
 *   "version": "1.0",
 *   "recovery": [ { key,label,type,help,default,min,max,step,enum }, ... ],
 *   "outputs": { "img":[...], "adf":[...], ... },
 *   "formats": { "1":[...], "2":[...], ... }
 * }
 */
char *uft_catalog_schemas_json(void);

/**
 * @brief Free JSON allocated by this module.
 */
void uft_catalog_free(char *p);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CATALOG_H */
