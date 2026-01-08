/**
 * @file uft_encoding_types.h
 * @brief UFT Encoding Type Definitions
 * 
 * Provides type definitions and macros for compatibility with
 * HxCFloppyEmulator encoding routines.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_ENCODING_TYPES_H
#define UFT_ENCODING_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Basic Types (HxC style)
 * ═══════════════════════════════════════════════════════════════════════════════ */

#ifndef UFT_ENC_TYPES_DEFINED
#define UFT_ENC_TYPES_DEFINED

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t  SDWORD;
typedef uint64_t QWORD;
typedef int8_t   SBYTE;
typedef int16_t  SWORD;

typedef unsigned char  uchar;
typedef unsigned short ushort;
typedef unsigned int   uint;
typedef unsigned long  ulong;

#endif /* UFT_ENC_TYPES_DEFINED */

/* ═══════════════════════════════════════════════════════════════════════════════
 * HxC Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_ENC_NOERROR           0
#define UFT_ENC_INTERNALERROR    -1
#define UFT_ENC_BADPARAMETER     -2
#define UFT_ENC_ACCESSERROR      -3
#define UFT_ENC_FILECORRUPTED    -4
#define UFT_ENC_UNSUPPORTEDFILE  -5
#define UFT_ENC_VALIDFILE         1

/* Track encodings */
#define ISOIBM_MFM_ENCODING     0x00
#define AMIGA_MFM_ENCODING      0x01
#define ISOIBM_FM_ENCODING      0x02
#define EMU_FM_ENCODING         0x03
#define UNKNOWN_ENCODING        0xFF

/* MFM/FM bit cell times (in nanoseconds) */
#define MFM_BITCELL_NS          2000  /* 2µs = 2000ns */
#define FM_BITCELL_NS           4000  /* 4µs = 4000ns */

/* Track types */
#define IBMPC_DD_FLOPPYMODE     0x00
#define IBMPC_HD_FLOPPYMODE     0x01
#define ATARIST_DD_FLOPPYMODE   0x02
#define ATARIST_HD_FLOPPYMODE   0x03
#define AMIGA_DD_FLOPPYMODE     0x04
#define AMIGA_HD_FLOPPYMODE     0x05
#define CPC_DD_FLOPPYMODE       0x06
#define GENERIC_SHUGGART_DD_FLOPPYMODE 0x07

/* Side encoding */
#define IBMFORMAT_SD            0x00  /* Single density (FM) */
#define IBMFORMAT_DD            0x01  /* Double density (MFM) */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Bit Manipulation Macros
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define BIT_SET(val, bit)     ((val) |= (1U << (bit)))
#define BIT_CLR(val, bit)     ((val) &= ~(1U << (bit)))
#define BIT_TST(val, bit)     (((val) >> (bit)) & 1U)
#define BIT_FLIP(val, bit)    ((val) ^= (1U << (bit)))

/* ═══════════════════════════════════════════════════════════════════════════════
 * Endianness
 * ═══════════════════════════════════════════════════════════════════════════════ */

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define UFT_ENC_BIG_ENDIAN 1
#else
#define UFT_ENC_LITTLE_ENDIAN 1
#endif

static inline uint16_t uft_enc_swap16(uint16_t v) {
    return (v >> 8) | (v << 8);
}

static inline uint32_t uft_enc_swap32(uint32_t v) {
    return ((v >> 24) & 0xFF) | ((v >> 8) & 0xFF00) |
           ((v << 8) & 0xFF0000) | ((v << 24) & 0xFF000000);
}

#ifdef UFT_ENC_BIG_ENDIAN
#define UFT_ENC_LE16(x) uft_enc_swap16(x)
#define UFT_ENC_LE32(x) uft_enc_swap32(x)
#define UFT_ENC_BE16(x) (x)
#define UFT_ENC_BE32(x) (x)
#else
#define UFT_ENC_LE16(x) (x)
#define UFT_ENC_LE32(x) (x)
#define UFT_ENC_BE16(x) uft_enc_swap16(x)
#define UFT_ENC_BE32(x) uft_enc_swap32(x)
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_ENCODING_TYPES_H */
