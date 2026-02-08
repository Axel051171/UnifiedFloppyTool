/*
 *
 * Copyright (C) 2025 UFT Project (compatibility layer)
 *
 * This compatibility layer wraps the central libflux.h and track_generator.h
 * and adds encoding-specific utilities.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UFT_LIBFLUX_COMPAT_H
#define UFT_LIBFLUX_COMPAT_H

/* Include central UFT headers */
#include "track_generator.h"
#include "libflux.h"

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Boolean Constants
 *============================================================================*/

#ifndef FALSE
#define FALSE 0x00
#endif

#ifndef TRUE
#define TRUE  0xFF
#endif

/*============================================================================
 * Endianness Macros
 *============================================================================*/

#ifdef UFT_BIG_ENDIAN
    /* Big endian host */
    #define BIGENDIAN_WORD(wValue) (wValue)
    #define BIGENDIAN_DWORD(dwValue) (dwValue)
    #define LITTLEENDIAN_WORD(wValue) \
        ( (((wValue)<<8)&0xFF00) | (((wValue)>>8)&0xFF) )
    #define LITTLEENDIAN_DWORD(dwValue) \
        ( (((dwValue)<<24)&0xFF000000) | (((dwValue)<<8)&0xFF0000) | \
          (((dwValue)>>8)&0xFF00) | (((dwValue)>>24)&0xFF) )
#else
    /* Little endian host (default) */
    #define BIGENDIAN_WORD(wValue) \
        ( (((wValue)<<8)&0xFF00) | (((wValue)>>8)&0xFF) )
    #define BIGENDIAN_DWORD(dwValue) \
        ( (((dwValue)<<24)&0xFF000000) | (((dwValue)<<8)&0xFF0000) | \
          (((dwValue)>>8)&0xFF00) | (((dwValue)>>24)&0xFF) )
    #define LITTLEENDIAN_WORD(wValue) (wValue)
    #define LITTLEENDIAN_DWORD(dwValue) (dwValue)
#endif

/*============================================================================
 * UFT Error Codes
 *============================================================================*/

#ifndef LIBFLUX_NOERROR
#define LIBFLUX_VALIDFILE                  1
#define LIBFLUX_NOERROR                    0
#define LIBFLUX_ACCESSERROR               -1
#define LIBFLUX_BADFILE                   -2
#define LIBFLUX_FILECORRUPTED             -3
#define LIBFLUX_BADPARAMETER              -4
#define LIBFLUX_INTERNALERROR             -5
#define LIBFLUX_UNSUPPORTEDFILE           -6
#endif

/*============================================================================
 * Encoding Constants
 *============================================================================*/

#ifndef ISOIBM_MFM_ENCODING
#define ISOIBM_MFM_ENCODING    0x00
#define AMIGA_MFM_ENCODING     0x01
#define ISOIBM_FM_ENCODING     0x02
#define EMU_FM_ENCODING        0x03
#define TYCOM_FM_ENCODING      0x04
#define MEMBRAIN_MFM_ENCODING  0x05
#define APPLEII_GCR1_ENCODING  0x06
#define APPLEII_GCR2_ENCODING  0x07
#define APPLEII_HDDD_A2_GCR1_ENCODING 0x08
#define APPLEII_HDDD_A2_GCR2_ENCODING 0x09
#define APPLEMAC_GCR_ENCODING  0x0A
#define QD_MO5_ENCODING        0x0B
#define C64_GCR_ENCODING       0x0C
#define VICTOR9K_GCR_ENCODING  0x0D
#define MICRALN_HS_FM_ENCODING 0x0E
#define NORTHSTAR_HS_MFM_ENCODING 0x0F
#define HEATHKIT_HS_FM_ENCODING 0x10
#define DEC_RX02_M2FM_ENCODING  0x11
#define AED6200P_MFM_ENCODING   0x12
#define CENTURION_MFM_ENCODING  0x13
#define ARBURGDAT_ENCODING      0x14
#define ARBURGSYS_ENCODING      0x15
#endif

/*============================================================================
 * Bit Manipulation Functions
 * Compatible with include/uft/PRIVATE/compat/floppy_utils.h
 *============================================================================*/

/* Only define if not already provided by floppy_utils.h */
#ifndef HAVE_GETBIT
#define HAVE_GETBIT
#ifndef getbit
static inline uint8_t getbit(const uint8_t* buffer, uint32_t bit_offset) {
    if (!buffer) return 0;
    return (buffer[bit_offset >> 3] >> (7 - (bit_offset & 7))) & 1;
}
#endif
#endif

#ifndef HAVE_SETBIT
#define HAVE_SETBIT
#ifndef setbit
static inline void setbit(uint8_t* buffer, uint32_t bit_offset, uint8_t value) {
    if (!buffer) return;
    uint32_t byte_idx = bit_offset >> 3;
    uint8_t bit_mask = (uint8_t)(0x80 >> (bit_offset & 7));
    if (value)
        buffer[byte_idx] |= bit_mask;
    else
        buffer[byte_idx] &= (uint8_t)(~bit_mask);
}
#endif
#endif

/* Legacy aliases for HxC compatibility */
#define uft_getbit getbit
#define uft_setbit setbit

/*============================================================================
 * Track Utility Functions (HxC compatibility)
 *============================================================================*/

/* HxC-style us2index with fill and marge parameters */
static inline uint32_t hxc_us2index(uint32_t start_index, LIBFLUX_SIDE* track, 
                                uint32_t us, uint8_t fill, int marge)
{
    if (!track || !track->databuffer) return start_index;
    
    uint32_t byte_index = start_index >> 3;
    uint32_t tracklen_bytes = track->tracklen >> 3;
    
    /* Calculate number of bytes for the given time */
    uint32_t bitrate = (uint32_t)track->bitrate;
    if (bitrate == 0) bitrate = 250000;
    
    uint32_t nbbit = (us * bitrate) / 1000000UL;
    uint32_t nbbyte = nbbit >> 3;
    
    if (marge && nbbyte > 0) nbbyte--;
    
    /* Fill bytes if requested */
    if (fill != 0 && tracklen_bytes > 0) {
        for (uint32_t i = 0; i < nbbyte && byte_index < tracklen_bytes; i++) {
            track->databuffer[byte_index] = fill;
            byte_index++;
            if (byte_index >= tracklen_bytes) byte_index = 0;
        }
    }
    
    return (byte_index << 3);
}

/* Map us2index to HxC version for compatibility */
#ifndef us2index
#define us2index hxc_us2index
#endif

/*============================================================================
 * MFM/FM Encoding Declarations
 *============================================================================*/

/* BuildMFMCylinder and BuildFMCylinder are declared in their respective
 * headers (mfm_encoding.h and fm_encoding.h). Do not duplicate here. */

/*============================================================================
 * Debug/Logging
 *============================================================================*/

#ifndef MSG_DEBUG
#define MSG_DEBUG   0
#define MSG_INFO    1
#define MSG_WARNING 2
#define MSG_ERROR   3
#endif

/* Stub for libflux_printf - can be overridden */
#ifndef HAVE_LIBFLUX_PRINTF
static inline void libflux_printf(void* ctx, int level, const char* fmt, ...) {
    (void)ctx; (void)level; (void)fmt;
}
#define HAVE_LIBFLUX_PRINTF
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_LIBFLUX_COMPAT_H */
