/**
 * @file uft_mfm_encoder.h
 * @brief Shared MFM encoder — IBM System 34 track layout synthesis.
 *
 * Produces a raw MFM bitstream from an array of uft_sector_t entries,
 * matching the IBM PC / Atari ST / Amiga (OFS-non-track), Spectrum, etc.
 * "System 34" layout:
 *
 *   Track prolog:   [Gap4a:80×0x4E] [Sync:12×0x00]
 *                   [IAM:  3×C2/sync + 1×FC]
 *                   [Gap1: 50×0x4E]
 *
 *   Per sector:     [Sync: 12×0x00]
 *                   [IDAM: 3×A1/sync + 1×FE + {C,H,R,N} + CRC16]
 *                   [Gap2: 22×0x4E + 12×0x00]
 *                   [DAM:  3×A1/sync + 1×FB + data[2^(N+7)] + CRC16]
 *                   [Gap3: N×0x4E]
 *
 *   Track epilog:   [Gap4b:fill with 0x4E to track end]
 *
 * Consumers: the KFX / MFI / PRI / UDI plugins' write_track hooks. Each
 * of those wraps the MFM bitstream into its own container.
 *
 * Author: UFT project.
 */
#ifndef UFT_MFM_ENCODER_H
#define UFT_MFM_ENCODER_H

#include "uft/uft_format_plugin.h"   /* uft_sector_t */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UFT_MFM_DENSITY_FM,     /**< FM single-density (historical, untested) */
    UFT_MFM_DENSITY_MFM,    /**< Standard MFM double-density */
} uft_mfm_density_t;

/**
 * @brief Parameters for the IBM System 34 layout.
 *
 * Reasonable defaults in UFT_MFM_PARAMS_DEFAULT_DD / _HD. Caller can
 * override gap sizes if the target hardware needs non-standard layout.
 */
typedef struct {
    uft_mfm_density_t  density;
    uint32_t           bit_rate;          /**< bits per second (250k, 500k, …) */
    uint16_t           gap4a;             /**< pre-IAM filler (typ. 80) */
    uint16_t           gap1;              /**< after IAM (typ. 50) */
    uint16_t           gap2;              /**< IDAM→DAM (typ. 22) */
    uint16_t           gap3;              /**< after DAM (typ. 84 HD, 40 DD) */
    uint8_t            gap_byte;          /**< 0x4E for MFM, 0xFF for FM */
    uint8_t            iam;               /**< 1 = emit IAM, 0 = skip */
} uft_mfm_encode_params_t;

#define UFT_MFM_PARAMS_DEFAULT_DD  { UFT_MFM_DENSITY_MFM, 250000, 80, 50, 22, 40, 0x4E, 1 }
#define UFT_MFM_PARAMS_DEFAULT_HD  { UFT_MFM_DENSITY_MFM, 500000, 80, 50, 22, 84, 0x4E, 1 }

/**
 * @brief Encode a sector array into an MFM bitstream.
 *
 * Output is raw MFM bits, packed 8 to a byte, MSB first per byte. Each
 * MFM bit is one cell (so an HD track produces ~12500 bytes of output).
 *
 * @param sectors       Array of sectors, in physical order on the track.
 * @param sector_count  Number of entries in sectors.
 * @param cylinder      Physical cylinder (for IDAM C-field).
 * @param head          Physical head (for IDAM H-field).
 * @param params        Layout parameters (gap sizes etc.).
 * @param out           Destination buffer.
 * @param out_capacity  Bytes available in out.
 * @return Number of MFM bytes written on success, 0 on error
 *         (bad arg, buffer too small, invalid sector size).
 *
 * Sector size handling:
 *   sectors[i].id.size_code selects 128<<N bytes. Code 0..3 covers
 *   128/256/512/1024 which are the only sizes seen in the wild.
 *   sectors[i].data is expected to be exactly that many bytes.
 *
 * CRC: CCITT polynomial 0x1021, initial 0xFFFF, computed over each
 *      address-mark prefix (three 0xA1 + DAM/IDAM byte) + payload.
 *
 * Address-mark sync: the three 0xA1 bytes before IDAM and DAM have
 *   their clock bit between data bits 4 and 5 suppressed, which is
 *   the universal MFM "missing clock" pattern that every FDC uses to
 *   lock onto addressed sectors. This encoder emits the suppressed
 *   clock correctly.
 */
size_t uft_mfm_encode_track(const uft_sector_t *sectors, size_t sector_count,
                             uint8_t cylinder, uint8_t head,
                             const uft_mfm_encode_params_t *params,
                             uint8_t *out, size_t out_capacity);

/**
 * @brief Convenience: encode from a uft_track_t, picking density by bitrate.
 *
 * Uses UFT_MFM_PARAMS_DEFAULT_HD if track->bitrate >= 400000, else _DD.
 */
size_t uft_mfm_encode_from_track(const uft_track_t *track,
                                  uint8_t *out, size_t out_capacity);

#ifdef __cplusplus
}
#endif
#endif /* UFT_MFM_ENCODER_H */
