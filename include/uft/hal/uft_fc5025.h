/**
 * @file uft_fc5025.h
 * @brief FC5025 USB Floppy Controller Interface
 * 
 * Device Software Archive (DSA) FC5025 support for reading 5.25" and 8" disks.
 * Supports FM, MFM, and various legacy formats.
 * 
 * @version 1.0.0
 * @date 2025-01-08
 */

#ifndef UFT_FC5025_H
#define UFT_FC5025_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** FC5025 USB identifiers */
#define UFT_FC5025_VID  0x16C0
#define UFT_FC5025_PID  0x06D6

/** Supported disk formats */
typedef enum {
    UFT_FC_FMT_AUTO = 0,
    UFT_FC_FMT_APPLE_DOS32,     /**< Apple DOS 3.2 (13 sectors) */
    UFT_FC_FMT_APPLE_DOS33,     /**< Apple DOS 3.3 (16 sectors) */
    UFT_FC_FMT_APPLE_PRODOS,    /**< Apple ProDOS */
    UFT_FC_FMT_IBM_FM,          /**< IBM 3740 FM (8" SSSD) */
    UFT_FC_FMT_IBM_MFM,         /**< IBM MFM (various) */
    UFT_FC_FMT_TRS80_SSSD,      /**< TRS-80 SSSD */
    UFT_FC_FMT_TRS80_SSDD,      /**< TRS-80 SSDD */
    UFT_FC_FMT_TRS80_DSDD,      /**< TRS-80 DSDD */
    UFT_FC_FMT_KAYPRO,          /**< Kaypro */
    UFT_FC_FMT_OSBORNE,         /**< Osborne */
    UFT_FC_FMT_NORTHSTAR,       /**< North Star */
    UFT_FC_FMT_TI994A,          /**< TI-99/4A */
    UFT_FC_FMT_ATARI_FM,        /**< Atari 810 FM */
    UFT_FC_FMT_ATARI_MFM,       /**< Atari 1050 MFM */
    UFT_FC_FMT_RAW_FM,          /**< Raw FM capture */
    UFT_FC_FMT_RAW_MFM,         /**< Raw MFM capture */
} uft_fc_format_t;

/** Drive types */
typedef enum {
    UFT_FC_DRIVE_525_48TPI,     /**< 5.25" 48 TPI (40 track) */
    UFT_FC_DRIVE_525_96TPI,     /**< 5.25" 96 TPI (80 track) */
    UFT_FC_DRIVE_8_SSSD,        /**< 8" SS/SD */
    UFT_FC_DRIVE_8_DSDD,        /**< 8" DS/DD */
} uft_fc_drive_t;

/*============================================================================
 * TYPES
 *============================================================================*/

typedef struct uft_fc_config_s uft_fc_config_t;

typedef struct {
    int track;
    int side;
    uint8_t *data;
    size_t size;
    int sector_count;
    uint8_t *sector_status;  /**< Per-sector error flags */
    bool success;
    const char *error;
} uft_fc_track_t;

typedef int (*uft_fc_callback_t)(const uft_fc_track_t *track, void *user);

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

uft_fc_config_t* uft_fc_config_create(void);
void uft_fc_config_destroy(uft_fc_config_t *cfg);

int uft_fc_open(uft_fc_config_t *cfg);
void uft_fc_close(uft_fc_config_t *cfg);
bool uft_fc_is_connected(const uft_fc_config_t *cfg);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

int uft_fc_set_format(uft_fc_config_t *cfg, uft_fc_format_t format);
int uft_fc_set_drive(uft_fc_config_t *cfg, uft_fc_drive_t drive);
int uft_fc_set_track_range(uft_fc_config_t *cfg, int start, int end);
int uft_fc_set_side(uft_fc_config_t *cfg, int side);
int uft_fc_set_retries(uft_fc_config_t *cfg, int count);
int uft_fc_set_double_step(uft_fc_config_t *cfg, bool enable);

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

int uft_fc_detect(int *device_count);
int uft_fc_get_firmware_version(uft_fc_config_t *cfg, int *version);

/*============================================================================
 * CAPTURE
 *============================================================================*/

int uft_fc_read_track(uft_fc_config_t *cfg, int track, int side,
                       uint8_t **data, size_t *size);
int uft_fc_read_disk(uft_fc_config_t *cfg, uft_fc_callback_t callback, void *user);

/*============================================================================
 * UTILITIES
 *============================================================================*/

const char* uft_fc_get_error(const uft_fc_config_t *cfg);
const char* uft_fc_format_name(uft_fc_format_t format);
const char* uft_fc_drive_name(uft_fc_drive_t drive);
int uft_fc_tracks_for_format(uft_fc_format_t format);
int uft_fc_sectors_for_format(uft_fc_format_t format);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FC5025_H */
