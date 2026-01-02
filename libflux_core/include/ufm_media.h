#pragma once
/*
 * ufm_media.h — Physical/Logical disk "preset profiles"
 *
 * These profiles describe *expected* media parameters (rpm, bitrate, encoding),
 * without forcing the capture/decoder to conform. They are hints for:
 *  - windowing defaults
 *  - PLL base frequency / nominal cell size
 *  - sector geometry expectations (where applicable)
 *
 * Archive principle: profiles are metadata + hints, never truth.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ufm_encoding {
    UFM_ENC_UNKNOWN = 0,
    UFM_ENC_FM_IBM,
    UFM_ENC_MFM_IBM,
    UFM_ENC_MFM_AMIGA,
    UFM_ENC_GCR_C64,
    UFM_ENC_GCR_APPLE2,
    UFM_ENC_SPIRAL_QUICKDISK, /* Mitsumi Quick Disk style */
} ufm_encoding_t;

typedef struct ufm_media_profile {
    const char *name;        /* stable id */
    const char *title;       /* UI label */
    ufm_encoding_t encoding;

    /* Physical timing expectations */
    uint16_t rpm;            /* 300, 360, 600 ... */
    uint32_t bitrate_kbps;   /* 250, 300, 500, 1000 ... (data rate) */

    /* Geometry hints (0 means "unknown / don't care") */
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors_per_track;
    uint16_t sector_size;    /* bytes */

    /* Extra knobs */
    bool has_index;
    bool variable_spt;       /* e.g. C64 zone layout */
} ufm_media_profile_t;

#ifdef __cplusplus
}
#endif
