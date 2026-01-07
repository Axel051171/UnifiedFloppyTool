#ifndef UFT_GWFLUX_H
#define UFT_GWFLUX_H

/*
 *
 *
 * Characteristics:
 *  - PURE FLUX (delta timings)
 *  - Track/side separated
 *  - No sector or encoding semantics
 *
 * This module provides:
 *  - READ: parse .gwf container and flux deltas
 *  - FLUX-READY: native (already flux)
 *  - CONVERT: export normalized delta stream
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_gwf_rc {
    UFT_GWF_SUCCESS      = 0,
    UFT_GWF_ERR_ARG      = -1,
    UFT_GWF_ERR_IO       = -2,
    UFT_GWF_ERR_FORMAT   = -3,
    UFT_GWF_ERR_NOMEM    = -4
} uft_gwf_rc_t;

#pragma pack(push,1)
typedef struct uft_gwf_header {
    char     sig[4];        /* "GWF\0" */
    uint16_t version;
    uint8_t  track;
    uint8_t  side;
    uint32_t flux_count;
} uft_gwf_header_t;
#pragma pack(pop)

typedef struct uft_gwf_flux {
    uint32_t* deltas;
    size_t count;
} uft_gwf_flux_t;

typedef struct uft_gwf_ctx {
    uft_gwf_header_t hdr;
    uft_gwf_flux_t flux;
    char* path;
} uft_gwf_ctx_t;

bool uft_gwf_detect(const uint8_t* buffer, size_t size);

/* Open and parse */
int uft_gwf_open(uft_gwf_ctx_t* ctx, const char* path);

/* Access flux deltas */
int uft_gwf_get_flux(uft_gwf_ctx_t* ctx, uint32_t** out_deltas, size_t* out_count);

/* Convert to normalized flux stream */
int uft_gwf_to_flux(uft_gwf_ctx_t* ctx, const char* output_path);

/* Close */
void uft_gwf_close(uft_gwf_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GWFLUX_H */
