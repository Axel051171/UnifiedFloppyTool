#ifndef UFT_KFSTREAM_H
#define UFT_KFSTREAM_H

/*
 * UFT (Universal Floppy Tool) - KryoFlux Stream (RAW STREAM) - v2.8.7
 *
 * KryoFlux stream files contain raw flux transition timings.
 * Files are usually named like:
 *   trackNN.raw
 *   trackNN.0.raw / trackNN.1.raw
 *
 * Properties:
 *  - PURE FLUX (no sectors, no encoding)
 *  - Variable timing resolution
 *
 * This module:
 *  - READ: parses KryoFlux stream records
 *  - FLUX-READY: native
 *  - CONVERT: exports normalized delta stream
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_kfs_rc {
    UFT_KFS_SUCCESS      = 0,
    UFT_KFS_ERR_ARG      = -1,
    UFT_KFS_ERR_IO       = -2,
    UFT_KFS_ERR_FORMAT   = -3,
    UFT_KFS_ERR_NOMEM    = -4
} uft_kfs_rc_t;

/* KryoFlux stream chunk types */
typedef enum uft_kfs_chunk_type {
    KFS_CHUNK_FLUX   = 0x01,
    KFS_CHUNK_OOB    = 0x02,
    KFS_CHUNK_INDEX  = 0x03
} uft_kfs_chunk_type_t;

#pragma pack(push,1)
typedef struct uft_kfs_chunk_hdr {
    uint8_t type;
    uint8_t length;
} uft_kfs_chunk_hdr_t;
#pragma pack(pop)

typedef struct uft_kfs_flux {
    uint32_t* deltas;
    size_t count;
} uft_kfs_flux_t;

typedef struct uft_kfs_ctx {
    uft_kfs_flux_t flux;
    char* path;
} uft_kfs_ctx_t;

/* Detect if file looks like KryoFlux stream */
bool uft_kfs_detect(const uint8_t* buffer, size_t size);

/* Open and parse stream */
int uft_kfs_open(uft_kfs_ctx_t* ctx, const char* path);

/* Get flux deltas */
int uft_kfs_get_flux(uft_kfs_ctx_t* ctx, uint32_t** out_deltas, size_t* out_count);

/* Convert to normalized flux */
int uft_kfs_to_flux(uft_kfs_ctx_t* ctx, const char* output_path);

/* Close */
void uft_kfs_close(uft_kfs_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_KFSTREAM_H */
