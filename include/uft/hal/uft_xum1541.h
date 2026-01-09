/**
 * @file uft_xum1541.h
 * @brief XUM1541/ZoomFloppy Interface for Commodore Drives
 * 
 * Supports direct communication with Commodore disk drives (1541, 1571, 1581)
 * via the XUM1541/ZoomFloppy USB adapter using the OpenCBM protocol.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_XUM1541_H
#define UFT_XUM1541_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** Commodore drive types */
typedef enum {
    UFT_CBM_DRIVE_AUTO = 0,
    UFT_CBM_DRIVE_1541,     /**< Single-sided, 35 tracks, GCR */
    UFT_CBM_DRIVE_1541_II,  /**< Single-sided, 35 tracks, GCR */
    UFT_CBM_DRIVE_1570,     /**< Single-sided, 35 tracks */
    UFT_CBM_DRIVE_1571,     /**< Double-sided, 35 tracks, GCR */
    UFT_CBM_DRIVE_1581,     /**< Double-sided, 80 tracks, MFM, 3.5" */
    UFT_CBM_DRIVE_SFD1001,  /**< 77 tracks, 5.25" */
    UFT_CBM_DRIVE_8050,     /**< 77 tracks, dual drive */
    UFT_CBM_DRIVE_8250,     /**< 77 tracks, dual drive, DS */
} uft_cbm_drive_t;

/** IEC bus commands */
typedef enum {
    UFT_IEC_LISTEN      = 0x20,
    UFT_IEC_UNLISTEN    = 0x3F,
    UFT_IEC_TALK        = 0x40,
    UFT_IEC_UNTALK      = 0x5F,
    UFT_IEC_OPEN        = 0xF0,
    UFT_IEC_CLOSE       = 0xE0,
    UFT_IEC_DATA        = 0x60,
} uft_iec_cmd_t;

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct uft_xum_config_s uft_xum_config_t;

/**
 * @brief Track read result
 */
typedef struct {
    int track;              /**< Track (1-35/40/77) */
    int side;               /**< Side (0 or 1, 0 for 1541) */
    uint8_t *gcr_data;      /**< Raw GCR data */
    size_t gcr_size;        /**< GCR data size */
    uint8_t *decoded;       /**< Decoded sector data (optional) */
    size_t decoded_size;    /**< Decoded size */
    int sector_count;       /**< Number of sectors */
    uint8_t sector_errors[21]; /**< Error codes per sector */
    bool success;
    const char *error;
} uft_xum_track_t;

typedef int (*uft_xum_callback_t)(const uft_xum_track_t *track, void *user);

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_xum_config_t* uft_xum_config_create(void);
void uft_xum_config_destroy(uft_xum_config_t *cfg);

int uft_xum_open(uft_xum_config_t *cfg, int device_num);
void uft_xum_close(uft_xum_config_t *cfg);
bool uft_xum_is_connected(const uft_xum_config_t *cfg);

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_xum_detect(int *device_count);
int uft_xum_identify_drive(uft_xum_config_t *cfg, uft_cbm_drive_t *type);
int uft_xum_get_status(uft_xum_config_t *cfg, char *status, size_t max_len);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_xum_set_device(uft_xum_config_t *cfg, int device_num);
int uft_xum_set_track_range(uft_xum_config_t *cfg, int start, int end);
int uft_xum_set_side(uft_xum_config_t *cfg, int side);
int uft_xum_set_retries(uft_xum_config_t *cfg, int count);

/*============================================================================
 * CAPTURE
 *============================================================================*/

/**
 * @brief Read track (raw GCR)
 */
int uft_xum_read_track_gcr(uft_xum_config_t *cfg, int track, int side,
                            uint8_t **gcr, size_t *size);

/**
 * @brief Read track (decoded sectors)
 */
int uft_xum_read_track(uft_xum_config_t *cfg, int track, int side,
                        uint8_t *sectors, int *sector_count,
                        uint8_t *errors);

/**
 * @brief Read entire disk
 */
int uft_xum_read_disk(uft_xum_config_t *cfg, uft_xum_callback_t callback,
                       void *user);

/**
 * @brief Write track
 */
int uft_xum_write_track(uft_xum_config_t *cfg, int track, int side,
                         const uint8_t *data, size_t size);

/*============================================================================
 * LOW-LEVEL IEC
 *============================================================================*/

int uft_xum_iec_listen(uft_xum_config_t *cfg, int device, int secondary);
int uft_xum_iec_talk(uft_xum_config_t *cfg, int device, int secondary);
int uft_xum_iec_unlisten(uft_xum_config_t *cfg);
int uft_xum_iec_untalk(uft_xum_config_t *cfg);
int uft_xum_iec_write(uft_xum_config_t *cfg, const uint8_t *data, size_t len);
int uft_xum_iec_read(uft_xum_config_t *cfg, uint8_t *data, size_t max_len);

/*============================================================================
 * UTILITIES
 *============================================================================*/

const char* uft_xum_get_error(const uft_xum_config_t *cfg);
const char* uft_xum_drive_name(uft_cbm_drive_t type);
int uft_xum_tracks_for_drive(uft_cbm_drive_t type);
int uft_xum_sectors_for_track(uft_cbm_drive_t type, int track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_XUM1541_H */
