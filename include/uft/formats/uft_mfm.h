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
 * @brief Close MFM file
 */
/* MF-1158: die vier `uft_mfm_*`-Namen dieses Headers kollidieren mit der
 * zweiten MFM-Abbild-Schnittstelle in
 * `src/formats/mfm_native/uft_mfm_image.h`, die als einzige Definitionen
 * hat (`uft_mfm_image.c`) — dort heisst der Typ `uft_mfm_ctx_t`, hier
 * `uft_mfm_context_t`. Zwei von vier (`close`, `read_track`) haben dabei
 * DIESELBE Aritaet und waren damit fuer das Aritaets-Tor unsichtbar; die
 * anderen zwei sieht `extern_decl_conflicts.py`. Fuer
 * `uft_mfm_context_t` gibt es keine Definition; dieser Header hat 1
 * Einbinder. Alle vier umbenannt statt entfernt (MF-1077) — welche der
 * zwei Schnittstellen die kanonische ist, entscheidet der Eigentuemer
 * (P3-413, gehoert zu „One Disk Model"). */
void uft_mfm_context_close(uft_mfm_context_t *ctx);

/*===========================================================================
 * INFORMATION
 *===========================================================================*/





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
int uft_mfm_context_read_track(uft_mfm_context_t *ctx, int track, int side,
                        uint8_t *data, size_t max_size);

/**
 * @brief Write track MFM data
 */
int uft_mfm_context_write_track(uft_mfm_context_t *ctx, int track, int side,
                         const uint8_t *data, size_t size);

/**
 * @brief Get track data length
 */
size_t uft_mfm_context_get_track_length(uft_mfm_context_t *ctx, int track, int side);

/*===========================================================================
 * BITSTREAM OPERATIONS
 *===========================================================================*/



/*===========================================================================
 * CONVERSION
 *===========================================================================*/




#ifdef __cplusplus
}
#endif

#endif /* UFT_MFM_H */
