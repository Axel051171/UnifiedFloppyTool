/**
 * @file uft_tc.h
 * @brief Transcopy TC Disk Image Format
 * 
 * Transcopy is a DOS-era disk copying program that could
 * preserve copy protection by storing raw track data.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_TC_H
#define UFT_TC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * TC FORMAT DEFINITIONS
 *===========================================================================*/

#define UFT_TC_MAX_TRACKS   84
#define UFT_TC_MAX_SIDES    2

#pragma pack(push, 1)

/**
 * @brief TC file header
 */
typedef struct {
    uint8_t  signature;         /**< 0x5A ('Z') for Transcopy */
    uint8_t  version;           /**< Version number */
    uint8_t  num_tracks;        /**< Number of tracks */
    uint8_t  num_sides;         /**< Number of sides */
    uint16_t track_size;        /**< Bytes per track (typically 6250) */
    uint8_t  density;           /**< 0=DD, 1=HD */
    uint8_t  reserved[9];       /**< Reserved */
} uft_tc_header_t;

/**
 * @brief TC track header
 */
typedef struct {
    uint8_t  track_num;         /**< Track number */
    uint8_t  side;              /**< Side number */
    uint16_t data_length;       /**< Actual data length */
    uint8_t  flags;             /**< Track flags */
    uint8_t  reserved[3];
} uft_tc_track_header_t;

#pragma pack(pop)

/* Signature */
#define UFT_TC_SIGNATURE    0x5A

/* Density */
#define UFT_TC_DENSITY_DD   0
#define UFT_TC_DENSITY_HD   1

/* Track flags */
#define UFT_TC_FLAG_ERROR       0x01
#define UFT_TC_FLAG_NONSTANDARD 0x02
#define UFT_TC_FLAG_WEAK_BITS   0x04

/*===========================================================================
 * CONTEXT
 *===========================================================================*/

typedef struct uft_tc_context uft_tc_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Probe file for TC format
 */
bool uft_tc_probe(const char *path);

/**
 * @brief Open TC file
 */
uft_tc_t* uft_tc_open(const char *path);

/**
 * @brief Create new TC file
 */
uft_tc_t* uft_tc_create(const char *path, int tracks, int sides, int density);

/**
 * @brief Close TC file
 */
void uft_tc_close(uft_tc_t *tc);

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

/**
 * @brief Get file header
 */
const uft_tc_header_t* uft_tc_get_header(uft_tc_t *tc);

/**
 * @brief Get number of tracks
 */
int uft_tc_get_tracks(uft_tc_t *tc);

/**
 * @brief Get number of sides
 */
int uft_tc_get_sides(uft_tc_t *tc);

/**
 * @brief Check if high density
 */
bool uft_tc_is_hd(uft_tc_t *tc);

/*===========================================================================
 * TRACK OPERATIONS
 *===========================================================================*/

/**
 * @brief Get track header
 */
int uft_tc_get_track_header(uft_tc_t *tc, int track, int side,
                             uft_tc_track_header_t *header);

/**
 * @brief Read track data
 */
int uft_tc_read_track(uft_tc_t *tc, int track, int side,
                       uint8_t *data, size_t max_size);

/**
 * @brief Write track data
 */
int uft_tc_write_track(uft_tc_t *tc, int track, int side,
                        const uint8_t *data, size_t size);

/**
 * @brief Get track length
 */
size_t uft_tc_get_track_length(uft_tc_t *tc, int track, int side);

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

/**
 * @brief Convert TC to IMG
 */
int uft_tc_to_img(const char *tc_path, const char *img_path);

/**
 * @brief Convert IMG to TC
 */
int uft_img_to_tc(const char *img_path, const char *tc_path);

/**
 * @brief Convert TC to raw flux
 */
int uft_tc_to_flux(const char *tc_path, const char *flux_path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_TC_H */
