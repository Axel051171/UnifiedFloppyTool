/*
 *
 * Copyright (C) 2025 UFT Project (compatibility layer)
 *
 * This compatibility layer provides the minimal types and functions
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef UFT_LIBFLUX_COMPAT_H
#define UFT_LIBFLUX_COMPAT_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 *============================================================================*/

#ifndef FALSE
#define FALSE 0x00
#endif

#ifndef TRUE
#define TRUE  0xFF
#endif

/* Endianness Macros */
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
 * UFT Error Codes (from liblibflux_ctx.h)
 *============================================================================*/

#define LIBFLUX_VALIDFILE                  1
#define LIBFLUX_NOERROR                    0
#define LIBFLUX_ACCESSERROR               -1
#define LIBFLUX_BADFILE                   -2
#define LIBFLUX_FILECORRUPTED             -3
#define LIBFLUX_BADPARAMETER              -4
#define LIBFLUX_INTERNALERROR             -5
#define LIBFLUX_UNSUPPORTEDFILE           -6

/*============================================================================
 * UFT Opaque Types (from liblibflux_ctx.h)
 *============================================================================*/

#ifndef _LIBFLUX_
typedef void LIBFLUX_CTX;
#define _LIBFLUX_
#endif

#ifndef _LIBFLUX_FLOPPY_
typedef void LIBFLUX_FLOPPY;
#define _LIBFLUX_FLOPPY_
#endif

/*============================================================================
 * LIBFLUX_SIDE Structure (from internal_floppy.h)
 *============================================================================*/

#define VARIABLEBITRATE                  -1
#define VARIABLEENCODING                 1

/* Forward declaration for stream dump */
typedef struct _LIBFLUX_TRKSTREAM LIBFLUX_TRKSTREAM;

#ifndef _LIBFLUX_SIDE_
typedef struct _LIBFLUX_SIDE
{
    int32_t         number_of_sector;        /* Number of sectors per track (-1 if unknown) */
    uint8_t       * databuffer;              /* Data buffer */
    int32_t         bitrate;                 /* Bitrate (VARIABLEBITRATE if timing buffer used) */
    uint32_t      * timingbuffer;            /* Bitrate buffer */
    uint8_t       * flakybitsbuffer;         /* Flakey/weak bits (NULL if none) */
    uint8_t       * indexbuffer;             /* Index signal (1=asserted, 0=not) */
    uint8_t       * track_encoding_buffer;   /* Track encoding indication buffer */

    int32_t         track_encoding;

    int32_t         tracklen;                /* Buffer length in bits */

    LIBFLUX_TRKSTREAM * stream_dump;

    uint32_t      * cell_to_tick;
    int             tick_freq;

} LIBFLUX_SIDE;
#define _LIBFLUX_SIDE_
#endif

/*============================================================================
 * Track Generator Structure (from track_generator.h)
 *============================================================================*/

#define DEFAULT_HD_BITRATE    500000
#define DEFAULT_DD_BITRATE    250000
#define DEFAULT_AMIGA_BITRATE 253360

#define DEFAULT_DD_RPM        300
#define DEFAULT_AMIGA_RPM     300

typedef struct track_generator_
{
    int32_t  last_bit_offset;
    uint16_t mfm_last_bit;

    void * disk_formats_LUT[256];
} track_generator;

/*============================================================================
 * Sector Configuration (from track_generator.h)
 *============================================================================*/

#define TRACKGEN_NO_DATA 0x00000001

#ifndef _LIBFLUX_SECTCFG_
typedef struct _LIBFLUX_SECTCFG
{
    int32_t        head;
    int32_t        sector;
    int32_t        sectorsleft;
    int32_t        cylinder;

    int32_t        sectorsize;

    int32_t        use_alternate_sector_size_id;
    int32_t        alternate_sector_size_id;

    int32_t        missingdataaddressmark;

    int32_t        use_alternate_header_crc;    /* 0x1 = Bad crc, 0x2 = alternate crc */
    uint32_t       data_crc;

    int32_t        use_alternate_data_crc;      /* 0x1 = Bad crc, 0x2 = alternate crc */
    uint32_t       header_crc;

    int32_t        use_alternate_datamark;
    int32_t        alternate_datamark;

    int32_t        use_alternate_addressmark;
    int32_t        alternate_addressmark;

    int32_t        startsectorindex;
    int32_t        startdataindex;
    int32_t        endsectorindex;

    int32_t        trackencoding;

    int32_t        gap3;

    int32_t        bitrate;

    uint8_t      * input_data;
    int32_t      * input_data_index;

    uint8_t      * weak_bits_mask;

    uint8_t        fill_byte;
    uint8_t        fill_byte_used;              /* Set if sector filled with fill_byte */

    uint32_t       flags;
} LIBFLUX_SECTCFG;
#define _LIBFLUX_SECTCFG_
#endif

/*============================================================================
 * Track Utilities (from trackutils.h)
 *============================================================================*/

/**
 * @brief Get a single bit from a bit buffer
 * @param input_data Pointer to bit buffer
 * @param bit_offset Bit offset (0 = MSB of first byte)
 * @return 0 or 1
 */
static inline int getbit(unsigned char * input_data, int bit_offset)
{
    return (input_data[bit_offset >> 3] >> (7 - (bit_offset & 7))) & 1;
}

/**
 * @brief Set a single bit in a bit buffer
 * @param input_data Pointer to bit buffer
 * @param bit_offset Bit offset (0 = MSB of first byte)
 * @param state 0 or 1
 */
static inline void setbit(unsigned char * input_data, int bit_offset, int state)
{
    if (state)
        input_data[bit_offset >> 3] |= (0x80 >> (bit_offset & 7));
    else
        input_data[bit_offset >> 3] &= ~(0x80 >> (bit_offset & 7));
}

/**
 * @brief Set a field of bits in a bit buffer
 * @param dstbuffer Destination bit buffer
 * @param byte Byte value to write
 * @param bitoffset Starting bit offset
 * @param size Number of bits to write (max 8)
 */
static inline void setfieldbit(unsigned char * dstbuffer, unsigned char byte, 
                                int bitoffset, int size)
{
    int i;
    for (i = 0; i < size && i < 8; i++)
    {
        setbit(dstbuffer, bitoffset + i, (byte >> (7 - i)) & 1);
    }
}

/*============================================================================
 * Track Encoding Constants
 *============================================================================*/

#define ISOIBM_MFM_ENCODING          0x00
#define AMIGA_MFM_ENCODING           0x01
#define ISOIBM_FM_ENCODING           0x02
#define EMU_FM_ENCODING              0x03
#define TYCOM_FM_ENCODING            0x04
#define MEMBRAIN_MFM_ENCODING        0x05
#define APPLEII_GCR1_ENCODING        0x06
#define APPLEII_GCR2_ENCODING        0x07
#define APPLEII_HDDD_A2_GCR1_ENCODING 0x08
#define APPLEII_HDDD_A2_GCR2_ENCODING 0x09
#define ARBURGDAT_ENCODING           0x0A
#define ARBURGSYS_ENCODING           0x0B
#define AED6200P_MFM_ENCODING        0x0C
#define NORTHSTAR_HS_MFM_ENCODING    0x0D
#define HEATHKIT_HS_FM_ENCODING      0x0E
#define DEC_RX02_M2FM_ENCODING       0x0F
#define APPLEMAC_GCR_ENCODING        0x10
#define QD_MO5_ENCODING              0x11
#define C64_GCR_ENCODING             0x12
#define VICTOR9K_GCR_ENCODING        0x13
#define MICRALN_HS_FM_ENCODING       0x14
#define CENTURION_MFM_ENCODING       0x15
#define UNKNOWN_ENCODING             0xFF

/*============================================================================
 * Stream/Flux Constants
 *============================================================================*/

/* Internal stream tick frequency */
#define TICKFREQ 250000000  /* 250MHz tick */

#ifdef __cplusplus
}
#endif

#endif /* UFT_LIBFLUX_COMPAT_H */
