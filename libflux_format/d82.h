#ifndef UFT_D82_H
#define UFT_D82_H

/*
 * UFT (Universal Floppy Tool) - D82 (Commodore 8250 / SFD-1001) - v2.8.7
 *
 * D82 is a logical image for CBM 8" drives (8250 / SFD-1001).
 *
 * Standard geometry:
 *  - 77 tracks
 *  - 2 sides
 *  - 29 sectors per track
 *  - 256 bytes per sector
 *  - Total: 1016832 bytes
 *
 * This module:
 *  - READ: logical sector access
 *  - WRITE: modify sectors
 *  - CONVERT: export to raw-sector stream
 *  - FLUX-READY: compatible with GCRRAW → Flux pipeline
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_d82_rc {
    UFT_D82_SUCCESS      = 0,
    UFT_D82_ERR_ARG      = -1,
    UFT_D82_ERR_IO       = -2,
    UFT_D82_ERR_FORMAT   = -3,
    UFT_D82_ERR_RANGE    = -4
} uft_d82_rc_t;

typedef struct uft_d82_ctx {
    uint8_t* image;
    size_t   image_size;
    char*    path;
    bool     writable;
} uft_d82_ctx_t;

/* Detect D82 by size */
bool d82_detect(const uint8_t* buffer, size_t size);

/* Open image */
int d82_open(uft_d82_ctx_t* ctx, const char* path, bool writable);

/* Read logical sector */
int d82_read_sector(uft_d82_ctx_t* ctx,
                    uint8_t side,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len);

/* Write logical sector */
int d82_write_sector(uft_d82_ctx_t* ctx,
                     uint8_t side,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* Convert to raw sector stream */
int d82_to_raw(uft_d82_ctx_t* ctx, const char* output_path);

/* Close */
void d82_close(uft_d82_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D82_H */
