#ifndef UFT_D71_H
#define UFT_D71_H

/*
 * UFT (Universal Floppy Tool) - D71 (Commodore 1571 Disk Image) - v2.8.7
 *
 * D71 is a logical double-sided extension of D64 for the C1571 drive.
 * Geometry:
 *  - Side 0: same as D64 (35 tracks)
 *  - Side 1: same geometry, appended
 *  - Optional error-info table (+512 bytes)
 *
 * This module:
 *  - READ: logical sector access (track/sector/side)
 *  - WRITE: modify sectors
 *  - CONVERT: export to raw-sector stream
 *  - FLUX-READY: compatible with GCRRAW decoder pipeline
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum uft_d71_rc {
    UFT_D71_SUCCESS      = 0,
    UFT_D71_ERR_ARG      = -1,
    UFT_D71_ERR_IO       = -2,
    UFT_D71_ERR_FORMAT   = -3,
    UFT_D71_ERR_RANGE    = -4
} uft_d71_rc_t;

typedef struct uft_d71_ctx {
    uint8_t* image;
    size_t   image_size;
    bool     has_error_table;
    char*    path;
    bool     writable;
} uft_d71_ctx_t;

/* Detect D71 */
bool uft_d71_detect(const uint8_t* buffer, size_t size);

/* Open image */
int uft_d71_open(uft_d71_ctx_t* ctx, const char* path, bool writable);

/* Read logical sector */
int uft_d71_read_sector(uft_d71_ctx_t* ctx,
                    uint8_t side,
                    uint8_t track,
                    uint8_t sector,
                    uint8_t* out_data,
                    size_t out_len);

/* Write logical sector */
int uft_d71_write_sector(uft_d71_ctx_t* ctx,
                     uint8_t side,
                     uint8_t track,
                     uint8_t sector,
                     const uint8_t* in_data,
                     size_t in_len);

/* Convert to raw sector stream */
int uft_d71_to_raw(uft_d71_ctx_t* ctx, const char* output_path);

/* Close */
void uft_d71_close(uft_d71_ctx_t* ctx);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D71_H */
