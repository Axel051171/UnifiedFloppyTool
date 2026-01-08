/**
 * @file types.h
 * @brief UFT Compatibility Types Header
 * 
 * This header provides type definitions for legacy code compatibility.
 * It bridges HxCFloppyEmulator types to UFT native types.
 */

#ifndef UFT_COMPAT_TYPES_H
#define UFT_COMPAT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Basic Types
 *===========================================================================*/

/* Signed integers */
typedef int8_t   int8;
typedef int16_t  int16;
typedef int32_t  int32;
typedef int64_t  int64;

/* Unsigned integers */
typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

/* Pointer-sized integers */
typedef intptr_t  intptr;
typedef uintptr_t uintptr;

/*===========================================================================
 * Floppy-Specific Types
 *===========================================================================*/

/** Track side enumeration */
typedef enum {
    SIDE_0 = 0,
    SIDE_1 = 1,
    SIDE_BOTH = 2
} track_side_t;

/** Track encoding types */
typedef enum {
    ENCODING_UNKNOWN = 0,
    ENCODING_FM,
    ENCODING_MFM, 
    ENCODING_GCR,
    ENCODING_APPLE_GCR,
    ENCODING_C64_GCR
} track_encoding_t;

/** Sector status */
typedef enum {
    SECTOR_OK = 0,
    SECTOR_BAD_CRC,
    SECTOR_MISSING,
    SECTOR_WEAK,
    SECTOR_DELETED
} sector_status_t;

/*===========================================================================
 * Track/Sector Structures
 *===========================================================================*/

/** Basic sector descriptor */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint8_t  size_code;      /* 0=128, 1=256, 2=512, 3=1024 */
    uint16_t data_crc;
    uint8_t  status;
    uint8_t  *data;
    size_t   data_size;
} sector_t;

/** Track descriptor */
typedef struct {
    uint8_t  cylinder;
    uint8_t  head;
    track_encoding_t encoding;
    uint32_t bitrate;
    uint32_t rpm;
    size_t   track_len;      /* in bits or bytes depending on format */
    uint8_t  *track_data;
    sector_t *sectors;
    size_t   sector_count;
} track_t;

/** Floppy image descriptor */
typedef struct {
    char     *filename;
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors_per_track;
    uint16_t bytes_per_sector;
    uint32_t total_size;
    track_t  **tracks;
    void     *format_specific;
} floppy_t;

/*===========================================================================
 * Legacy HxC Type Aliases (for compatibility)
 *===========================================================================*/

typedef floppy_t HXCFE_FLOPPY;
typedef track_t  HXCFE_SIDE;
typedef sector_t HXCFE_SECTCFG;

/*===========================================================================
 * Utility Macros
 *===========================================================================*/

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

/* Byte order */
#ifndef LITTLE_ENDIAN
#define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
#define BIG_ENDIAN 4321
#endif

#ifdef __cplusplus
}
#endif

#endif /* UFT_COMPAT_TYPES_H */
