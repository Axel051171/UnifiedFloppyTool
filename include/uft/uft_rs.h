/**
 * @file uft_rs.h
 * @brief Reed-Solomon GF(256) helper (recovery addon).
 *
 * This is a small, self-contained RS codec for formats that actually use RS.
 * Most floppy formats rely on CRC + redundancy, so this is typically optional.
 */

#ifndef UFT_RS_H
#define UFT_RS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int nsyms; /* number of parity symbols */
} uft_rs_t;

/* init RS codec for a given number of parity symbols (2..64 recommended) */
int uft_rs_init(uft_rs_t *rs, int nsyms);

/* decode in-place. returns number of corrected symbols, or -1 on failure */
int uft_rs_decode(uft_rs_t *rs, uint8_t *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RS_H */
