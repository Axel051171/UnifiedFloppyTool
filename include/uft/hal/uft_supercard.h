/**
 * @file uft_supercard.h
 * @brief SuperCard Pro Hardware Interface
 * 
 * Full implementation of the SuperCard Pro protocol for disk capture/write.
 * SCP has a documented serial protocol (unlike KryoFlux).
 * 
 * @version 1.0.0
 * @date 2025-01-08
 * 
 * Protocol documentation: http://www.oocities.org/cpcfan/scpdata.txt
 */

#ifndef UFT_SUPERCARD_H
#define UFT_SUPERCARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * CONSTANTS
 *============================================================================*/

/** SCP sample clock: 40 MHz (25ns resolution) */
#define UFT_SCP_SAMPLE_CLOCK    40000000

/** Maximum tracks supported */
#define UFT_SCP_MAX_TRACKS      168  /* 84 tracks × 2 sides */

/** Maximum revolutions per capture */
#define UFT_SCP_MAX_REVOLUTIONS 5

/** USB Vendor/Product IDs */
#define UFT_SCP_VID             0x04D8
#define UFT_SCP_PID             0xFBAB

/*============================================================================
 * COMMAND CODES
 *============================================================================*/

typedef enum {
    SCP_CMD_SELECT_A        = 'a',  /* Select drive A */
    SCP_CMD_SELECT_B        = 'b',  /* Select drive B */
    SCP_CMD_DESELECT        = 'd',  /* Deselect drive */
    SCP_CMD_MOTOR_ON        = 'm',  /* Motor on */
    SCP_CMD_MOTOR_OFF       = 'M',  /* Motor off */
    SCP_CMD_SEEK_CYL        = 's',  /* Seek to cylinder */
    SCP_CMD_SEEK_TRACK      = 'T',  /* Seek to track (cyl*2+head) */
    SCP_CMD_STEP_IN         = '+',  /* Step head in */
    SCP_CMD_STEP_OUT        = '-',  /* Step head out */
    SCP_CMD_READ_TRACK      = 'R',  /* Read track flux */
    SCP_CMD_WRITE_TRACK     = 'W',  /* Write track flux */
    SCP_CMD_READ_MFM        = 'r',  /* Read MFM data */
    SCP_CMD_WRITE_MFM       = 'w',  /* Write MFM data */
    SCP_CMD_GET_INFO        = 'i',  /* Get device info */
    SCP_CMD_GET_PARAMS      = 'p',  /* Get parameters */
    SCP_CMD_SET_PARAMS      = 'P',  /* Set parameters */
    SCP_CMD_RAM_TEST        = 't',  /* RAM self-test */
    SCP_CMD_STATUS          = '?',  /* Get status */
    SCP_CMD_ERASE           = 'E',  /* Erase track */
    SCP_CMD_SIDE_0          = '0',  /* Select side 0 */
    SCP_CMD_SIDE_1          = '1',  /* Select side 1 */
    SCP_CMD_DENSITY_DD      = 'D',  /* Set DD density */
    SCP_CMD_DENSITY_HD      = 'H',  /* Set HD density */
    SCP_CMD_INDEX_WAIT      = 'I',  /* Wait for index */
    SCP_CMD_FIRMWARE_VER    = 'v',  /* Firmware version */
    SCP_CMD_HARDWARE_VER    = 'V',  /* Hardware version */
} uft_scp_cmd_t;

/*============================================================================
 * STATUS CODES
 *============================================================================*/

typedef enum {
    SCP_STATUS_OK           = '0',  /* Success */
    SCP_STATUS_BAD_CMD      = '1',  /* Bad command */
    SCP_STATUS_NO_DRIVE     = '2',  /* No drive selected */
    SCP_STATUS_NO_MOTOR     = '3',  /* Motor not running */
    SCP_STATUS_NO_INDEX     = '4',  /* No index pulse */
    SCP_STATUS_NO_TRK0      = '5',  /* Track 0 not found */
    SCP_STATUS_WRITE_PROT   = '6',  /* Write protected */
    SCP_STATUS_READ_ERR     = '7',  /* Read error */
    SCP_STATUS_WRITE_ERR    = '8',  /* Write error */
    SCP_STATUS_VERIFY_ERR   = '9',  /* Verify error */
    SCP_STATUS_PARAM_ERR    = 'A',  /* Parameter error */
    SCP_STATUS_RAM_ERR      = 'B',  /* RAM error */
    SCP_STATUS_NO_DISK      = 'C',  /* No disk in drive */
    SCP_STATUS_TIMEOUT      = 'D',  /* Timeout */
} uft_scp_status_t;

/*============================================================================
 * DRIVE TYPES
 *============================================================================*/

typedef enum {
    SCP_DRIVE_AUTO = 0,
    SCP_DRIVE_35_DD,        /* 3.5" DD (720K) */
    SCP_DRIVE_35_HD,        /* 3.5" HD (1.44M) */
    SCP_DRIVE_35_ED,        /* 3.5" ED (2.88M) */
    SCP_DRIVE_525_DD,       /* 5.25" DD (360K) */
    SCP_DRIVE_525_HD,       /* 5.25" HD (1.2M) */
    SCP_DRIVE_8_SSSD,       /* 8" SS/SD */
} uft_scp_drive_t;

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

typedef struct uft_scp_config_s uft_scp_config_t;

/**
 * @brief Track capture result
 */
typedef struct {
    int track;              /**< Track number */
    int side;               /**< Side (0 or 1) */
    uint32_t *flux;         /**< Flux data (caller frees) */
    size_t flux_count;      /**< Number of flux transitions */
    uint32_t *index;        /**< Index positions */
    size_t index_count;     /**< Number of index pulses (= revolutions) */
    uint32_t capture_time;  /**< Capture duration in SCP ticks */
    bool success;           /**< true if capture OK */
    const char *error;      /**< Error message if !success */
} uft_scp_track_t;

/**
 * @brief Disk capture callback
 */
typedef int (*uft_scp_callback_t)(const uft_scp_track_t *track, void *user);

/*============================================================================
 * LIFECYCLE
 *============================================================================*/

/**
 * @brief Create SCP configuration
 * @return Config handle or NULL
 */
uft_scp_config_t* uft_scp_config_create(void);

/**
 * @brief Destroy configuration
 */
void uft_scp_config_destroy(uft_scp_config_t *cfg);

/**
 * @brief Open SCP device
 * @param cfg Configuration
 * @param port Serial port (e.g., "COM3", "/dev/ttyUSB0")
 * @return 0 on success
 */
int uft_scp_open(uft_scp_config_t *cfg, const char *port);

/**
 * @brief Close SCP device
 */
void uft_scp_close(uft_scp_config_t *cfg);

/**
 * @brief Check if device is connected
 */
bool uft_scp_is_connected(const uft_scp_config_t *cfg);

/*============================================================================
 * DEVICE INFO
 *============================================================================*/

/**
 * @brief Get firmware version
 */
int uft_scp_get_firmware_version(uft_scp_config_t *cfg, int *major, int *minor);

/**
 * @brief Get hardware version
 */
int uft_scp_get_hardware_version(uft_scp_config_t *cfg, int *version);

/**
 * @brief Run RAM self-test
 * @return 0 on success, -1 on failure
 */
int uft_scp_ram_test(uft_scp_config_t *cfg);

/**
 * @brief Detect connected SCP devices
 * @param ports Array to fill with port names
 * @param max_ports Maximum entries
 * @return Number of devices found
 */
int uft_scp_detect(char ports[][64], int max_ports);

/*============================================================================
 * CONFIGURATION
 *============================================================================*/

/**
 * @brief Set track range
 */
int uft_scp_set_track_range(uft_scp_config_t *cfg, int start, int end);

/**
 * @brief Set side (-1 = both, 0 = bottom, 1 = top)
 */
int uft_scp_set_side(uft_scp_config_t *cfg, int side);

/**
 * @brief Set revolutions per track
 */
int uft_scp_set_revolutions(uft_scp_config_t *cfg, int revs);

/**
 * @brief Set drive type
 */
int uft_scp_set_drive_type(uft_scp_config_t *cfg, uft_scp_drive_t type);

/**
 * @brief Set retry count
 */
int uft_scp_set_retries(uft_scp_config_t *cfg, int count);

/**
 * @brief Enable/disable verify after write
 */
int uft_scp_set_verify(uft_scp_config_t *cfg, bool enable);

/*============================================================================
 * DRIVE CONTROL
 *============================================================================*/

/**
 * @brief Select drive (A or B)
 */
int uft_scp_select_drive(uft_scp_config_t *cfg, int drive);

/**
 * @brief Control motor
 */
int uft_scp_motor(uft_scp_config_t *cfg, bool on);

/**
 * @brief Seek to track
 */
int uft_scp_seek(uft_scp_config_t *cfg, int track);

/**
 * @brief Select side
 */
int uft_scp_select_side(uft_scp_config_t *cfg, int side);

/**
 * @brief Wait for index pulse
 */
int uft_scp_wait_index(uft_scp_config_t *cfg);

/**
 * @brief Check write protect status
 * @return true if write protected
 */
bool uft_scp_is_write_protected(uft_scp_config_t *cfg);

/*============================================================================
 * CAPTURE
 *============================================================================*/

/**
 * @brief Read single track
 */
int uft_scp_read_track(uft_scp_config_t *cfg, int track, int side,
                        uint32_t **flux, size_t *count,
                        uint32_t **index, size_t *index_count);

/**
 * @brief Read entire disk
 */
int uft_scp_read_disk(uft_scp_config_t *cfg, uft_scp_callback_t callback,
                       void *user);

/**
 * @brief Write single track
 */
int uft_scp_write_track(uft_scp_config_t *cfg, int track, int side,
                         const uint32_t *flux, size_t count);

/**
 * @brief Erase track
 */
int uft_scp_erase_track(uft_scp_config_t *cfg, int track, int side);

/*============================================================================
 * UTILITIES
 *============================================================================*/

/**
 * @brief Convert SCP ticks to nanoseconds
 */
double uft_scp_ticks_to_ns(uint32_t ticks);

/**
 * @brief Convert nanoseconds to SCP ticks
 */
uint32_t uft_scp_ns_to_ticks(double ns);

/**
 * @brief Get sample clock frequency
 */
double uft_scp_get_sample_clock(void);

/**
 * @brief Get last error message
 */
const char* uft_scp_get_error(const uft_scp_config_t *cfg);

/**
 * @brief Get status code description
 */
const char* uft_scp_status_string(uft_scp_status_t status);

/**
 * @brief Get drive type name
 */
const char* uft_scp_drive_name(uft_scp_drive_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SUPERCARD_H */
