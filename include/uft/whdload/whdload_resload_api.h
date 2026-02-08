#pragma once
/*
 * whdload_resload_api.h — extracted resload_* API list from WHDLoad autodoc.
 * This is a string/metadata table for diagnostics and workflow mapping.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct uft_whd_resload_api {
    const char *name;              /* e.g. "resload_DiskLoadDev" */
    const char *purpose;           /* short: from NAME line */
    const char *function_firstline;/* first line of FUNCTION section */
    uint8_t disk_related;          /* heuristic flag */
} uft_whd_resload_api_t;

const uft_whd_resload_api_t *uft_whd_resload_api_list(size_t *out_count);

#ifdef __cplusplus
}
#endif
