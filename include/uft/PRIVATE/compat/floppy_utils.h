/**
 * @file floppy_utils.h
 * @brief Compatibility wrapper for floppy utilities
 * 
 * This header provides compatibility macros for legacy code.
 * New code should use uft/uft_floppy_utils.h directly.
 */

#ifndef UFT_PRIVATE_FLOPPY_UTILS_H
#define UFT_PRIVATE_FLOPPY_UTILS_H

#include "uft/uft_floppy_utils.h"

/* Bit manipulation macros for track data
 * Only define if not already provided by libflux_compat.h
 */
#ifndef HAVE_GETBIT
#define HAVE_GETBIT 1
#endif

#ifndef getbit
static inline int getbit(const unsigned char *data, int bit_offset) {
    return (data[bit_offset >> 3] >> (7 - (bit_offset & 7))) & 1;
}
#endif

#ifndef setbit
static inline void setbit(unsigned char *data, int bit_offset, int value) {
    int byte_idx = bit_offset >> 3;
    int bit_pos = 7 - (bit_offset & 7);
    if (value)
        data[byte_idx] |= (1 << bit_pos);
    else
        data[byte_idx] &= ~(1 << bit_pos);
}
#endif

#endif /* UFT_PRIVATE_FLOPPY_UTILS_H */
