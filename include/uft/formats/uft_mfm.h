/**
 * @file uft_mfm.h
 * @brief HxC MFM Format Handler
 * 
 * Raw MFM bitstream format used by HxC tools.
 * Contains unprocessed MFM data with timing information.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_MFM_H
#define UFT_MFM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * MFM FILE STRUCTURES
 *===========================================================================*/

#pragma pack(push, 1)

/**
 * @brief MFM file header
 */
typedef struct {
    char signature[8];          /* "HXCMFM\0\0" */
    uint16_t format_revision;   /* Format version */
    uint16_t num_tracks;        /* Number of tracks */
    uint16_t num_sides;         /* Number of sides (1 or 2) */
    uint16_t rpm;               /* Drive RPM (300 or 360) */
    uint32_t bitrate;           /* Bitrate in bits/second */
    uint16_t track_encoding;    /* Encoding type */
    uint16_t interface_mode;    /* Interface mode */
    uint32_t track_list_offset; /* Offset to track list */
} uft_mfm_header_t;

/**
 * @brief MFM track descriptor
 */
typedef struct {
    uint16_t track_number;      /* Physical track number */
    uint16_t side_number;       /* Side (0 or 1) */
    uint32_t data_offset;       /* Offset to track data */
    uint32_t data_length;       /* Length of track data in bytes */
    uint32_t bitrate;           /* Track-specific bitrate (0 = use header) */
} uft_mfm_track_t;

#pragma pack(pop)

/* Signature */
#define UFT_MFM_SIGNATURE "HXCMFM\0\0"

/* Encoding types */
#define UFT_MFM_ENC_MFM     0x0000  /* MFM encoding */
#define UFT_MFM_ENC_FM      0x0001  /* FM encoding */
#define UFT_MFM_ENC_GCR     0x0002  /* GCR encoding */

/* Interface modes */
#define UFT_MFM_IF_IBM_PC   0x0000
#define UFT_MFM_IF_AMIGA    0x0001
#define UFT_MFM_IF_ATARI_ST 0x0002
#define UFT_MFM_IF_C64      0x0003

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

typedef struct uft_mfm_context uft_mfm_context_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Probe file for MFM format
 */
bool uft_mfm_probe(const char *path);

/**
 * @brief Open MFM file for reading
 */
uft_mfm_context_t* uft_mfm_open(const char *path);

/**
 * @brief Create new MFM file
 */
uft_mfm_context_t* uft_mfm_create(const char *path, 
                                   int num_tracks, int num_sides,
                                   int rpm, uint32_t bitrate);

/**
 * @brief Close MFM file
 */
void uft_mfm_close(uft_mfm_context_t *ctx);

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

/**
 * @brief Get file header
 */
const uft_mfm_header_t* uft_mfm_get_header(uft_mfm_context_t *ctx);

/**
 * @brief Get number of tracks
 */
int uft_mfm_get_num_tracks(uft_mfm_context_t *ctx);

/**
 * @brief Get number of sides
 */
int uft_mfm_get_num_sides(uft_mfm_context_t *ctx);

/**
 * @brief Get track descriptor
 */
const uft_mfm_track_t* uft_mfm_get_track_info(uft_mfm_context_t *ctx,
                                               int track, int side);

/*===========================================================================
 * TRACK I/O
 *===========================================================================*/

/**
 * @brief Read track MFM data
 * 
 * @param ctx Context
 * @param track Track number
 * @param side Side number
 * @param data Output buffer (caller allocated)
 * @param max_size Buffer size
 * @return Bytes read, or -1 on error
 */
int uft_mfm_read_track(uft_mfm_context_t *ctx, int track, int side,
                        uint8_t *data, size_t max_size);

/**
 * @brief Write track MFM data
 */
int uft_mfm_write_track(uft_mfm_context_t *ctx, int track, int side,
                         const uint8_t *data, size_t size);

/**
 * @brief Get track data length
 */
size_t uft_mfm_get_track_length(uft_mfm_context_t *ctx, int track, int side);

/*===========================================================================
 * BITSTREAM OPERATIONS
 *===========================================================================*/

/**
 * @brief Convert MFM bits to flux transitions
 * 
 * @param mfm_data MFM bitstream
 * @param mfm_bits Number of bits
 * @param bitrate Bitrate in bits/second
 * @param flux_ns Output: flux transition times in nanoseconds
 * @param max_flux Maximum flux values
 * @return Number of flux transitions
 */
int uft_mfm_to_flux(const uint8_t *mfm_data, size_t mfm_bits,
                    uint32_t bitrate,
                    uint32_t *flux_ns, size_t max_flux);

/**
 * @brief Convert flux transitions to MFM bits
 */
int uft_flux_to_mfm(const uint32_t *flux_ns, size_t flux_count,
                    uint32_t bitrate,
                    uint8_t *mfm_data, size_t max_bytes);

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

/**
 * @brief Convert MFM to HFE format
 */
int uft_mfm_to_hfe(const char *mfm_path, const char *hfe_path);

/**
 * @brief Convert HFE to MFM format
 */
int uft_hfe_to_mfm(const char *hfe_path, const char *mfm_path);

/**
 * @brief Convert MFM to SCP format
 */
int uft_mfm_to_scp(const char *mfm_path, const char *scp_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_H */
