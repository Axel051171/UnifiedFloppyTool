/**
 * @file floppy_utils.h
 * @brief Compatibility wrapper for floppy utilities
 * 
 * This header provides compatibility macros for legacy code.
 * Delegates to libflux_compat.h for bit manipulation functions.
 */

#ifndef UFT_PRIVATE_FLOPPY_UTILS_H
#define UFT_PRIVATE_FLOPPY_UTILS_H

/* Include the canonical bit manipulation functions from libflux_compat */
#include "libflux_compat.h"

/* Also include general floppy utilities */
#include "uft/uft_floppy_utils.h"

/* Mark that we have getbit/setbit available */
#ifndef HAVE_GETBIT
#define HAVE_GETBIT 1
#endif

#endif /* UFT_PRIVATE_FLOPPY_UTILS_H */
