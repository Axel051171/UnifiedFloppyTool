/**
 * @file uft_greaseweazle.h
 * @brief Greaseweazle Hardware Abstraction Layer
 * 
 * Complete API for Greaseweazle flux controller communication.
 * Supports F1, F7, F7 Plus models.
 * 
 * @version 1.0.0
 * @copyright UFT Project 2025-2026
 * SPDX-License-Identifier: MIT
 */

#ifndef UFT_GREASEWEAZLE_H
#define UFT_GREASEWEAZLE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 * CONSTANTS
 * ═══════════════════════════════════════════════════════════════════════════ */

#define UFT_GW_USB_VID              0x1209
#define UFT_GW_USB_PID              0x4D69
#define UFT_GW_SAMPLE_FREQ_HZ       72000000    /* F7: 72 MHz */
#define UFT_GW_SAMPLE_FREQ_F7_PLUS  84000000    /* F7 Plus: 84 MHz */
#define UFT_GW_MAX_CYLINDERS        85
#define UFT_GW_MAX_HEADS            2
#define UFT_GW_MAX_REVOLUTIONS      16

/* ═══════════════════════════════════════════════════════════════════════════
 * ERROR CODES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_GW_OK                   = 0,
    UFT_GW_ERR_NOT_FOUND        = -1,
    UFT_GW_ERR_OPEN_FAILED      = -2,
    UFT_GW_ERR_COMM             = -3,
    UFT_GW_ERR_TIMEOUT          = -4,
    UFT_GW_ERR_NO_INDEX         = -5,
    UFT_GW_ERR_NO_TRK0          = -6,
    UFT_GW_ERR_WRITE_PROTECT    = -7,
    UFT_GW_ERR_OVERFLOW         = -8,
    UFT_GW_ERR_BAD_PARAM        = -9,
    UFT_GW_ERR_NO_MEMORY        = -10,
    UFT_GW_ERR_NOT_CONNECTED    = -11,
} uft_gw_error_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * BUS TYPES
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum {
    UFT_GW_BUS_NONE     = 0,
    UFT_GW_BUS_IBM_PC   = 1,     /* IBM PC (active low select) */
    UFT_GW_BUS_SHUGART  = 2,     /* Shugart (active high select) */
} uft_gw_bus_type_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * STRUCTURES
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Device information
 */
typedef struct {
    uint8_t     fw_major;           /* Firmware major version */
    uint8_t     fw_minor;           /* Firmware minor version */
    uint8_t     hw_model;           /* Hardware model (1=F1, 7=F7) */
    uint8_t     hw_submodel;        /* Hardware sub-model */
    uint32_t    sample_freq;        /* Sample frequency in Hz */
    uint8_t     usb_speed;          /* USB speed (1=Full, 2=High) */
    char        serial[32];         /* Serial number */
} uft_gw_info_t;

/**
 * @brief Drive timing delays
 */
typedef struct {
    uint16_t    select_delay_us;    /* Delay after drive select */
    uint16_t    step_delay_us;      /* Delay after step pulse */
    uint16_t    settle_delay_ms;    /* Head settle delay */
    uint16_t    motor_delay_ms;     /* Motor spin-up delay */
    uint16_t    auto_off_ms;        /* Auto motor-off timeout */
} uft_gw_delays_t;

/**
 * @brief Flux read parameters
 */
typedef struct {
    uint8_t     revolutions;        /* Number of revolutions (1-16) */
    bool        index_sync;         /* Sync to index pulse */
    uint32_t    ticks;              /* Max ticks (0 = use revolutions) */
} uft_gw_read_params_t;

/**
 * @brief Flux write parameters
 */
typedef struct {
    bool        index_sync;         /* Sync write to index */
    bool        erase_empty;        /* Erase before write */
    bool        verify;             /* Verify after write */
    uint32_t    terminate_at_index; /* Stop at Nth index (0 = continuous) */
} uft_gw_write_params_t;

/**
 * @brief Captured flux data
 */
typedef struct {
    uint32_t*   samples;            /* Flux timing samples (ticks) */
    uint32_t    sample_count;       /* Number of samples */
    uint32_t*   index_times;        /* Index pulse times */
    uint8_t     index_count;        /* Number of index pulses */
    uint32_t    total_ticks;        /* Total capture time */
    int         status;             /* Capture status */
    uint32_t    sample_freq;        /* Sample frequency used */
} uft_gw_flux_data_t;

/**
 * @brief Device handle (opaque)
 */
typedef struct uft_gw_device uft_gw_device_t;

/* ═══════════════════════════════════════════════════════════════════════════
 * API: CONNECTION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Open device on specified port
 * @param port Serial port (e.g., "/dev/ttyACM0", "COM3")
 * @param device Output: device handle
 * @return 0 on success, error code on failure
 */
int uft_gw_open(const char* port, uft_gw_device_t** device);

/**
 * @brief Open first available device
 */
int uft_gw_open_first(uft_gw_device_t** device);

/**
 * @brief Close device
 */
void uft_gw_close(uft_gw_device_t* device);

/**
 * @brief Check if connected
 */
bool uft_gw_is_connected(uft_gw_device_t* device);

/**
 * @brief Reset device
 */
int uft_gw_reset(uft_gw_device_t* device);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: INFORMATION
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Get device info
 */
int uft_gw_get_info(uft_gw_device_t* device, uft_gw_info_t* info);

/**
 * @brief Get version string (e.g., "1.4")
 */
const char* uft_gw_get_version_string(uft_gw_device_t* device);

/**
 * @brief Get sample frequency
 */
uint32_t uft_gw_get_sample_freq(uft_gw_device_t* device);

/**
 * @brief Get error string
 */
const char* uft_gw_strerror(int error);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: DRIVE CONTROL
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Set bus type
 */
int uft_gw_set_bus_type(uft_gw_device_t* device, uft_gw_bus_type_t bus_type);

/**
 * @brief Select drive unit (0 or 1)
 */
int uft_gw_select_drive(uft_gw_device_t* device, uint8_t unit);

/**
 * @brief Deselect drive
 */
int uft_gw_deselect_drive(uft_gw_device_t* device);

/**
 * @brief Set motor state
 */
int uft_gw_set_motor(uft_gw_device_t* device, bool on);

/**
 * @brief Seek to cylinder (0-84)
 */
int uft_gw_seek(uft_gw_device_t* device, uint8_t cylinder);

/**
 * @brief Select head (0 or 1)
 */
int uft_gw_select_head(uft_gw_device_t* device, uint8_t head);

/**
 * @brief Get current cylinder
 */
int uft_gw_get_cylinder(uft_gw_device_t* device);

/**
 * @brief Get current head
 */
int uft_gw_get_head(uft_gw_device_t* device);

/**
 * @brief Check write protection
 */
bool uft_gw_is_write_protected(uft_gw_device_t* device);

/**
 * @brief Set drive delays
 */
int uft_gw_set_delays(uft_gw_device_t* device, const uft_gw_delays_t* delays);

/**
 * @brief Get drive delays
 */
int uft_gw_get_delays(uft_gw_device_t* device, uft_gw_delays_t* delays);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: FLUX OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read flux data from current track
 */
int uft_gw_read_flux(uft_gw_device_t* device, 
                     const uft_gw_read_params_t* params,
                     uft_gw_flux_data_t* flux);

/**
 * @brief Write flux data to current track
 */
int uft_gw_write_flux(uft_gw_device_t* device,
                      const uft_gw_write_params_t* params,
                      const uint32_t* samples,
                      uint32_t sample_count);

/**
 * @brief Erase current track
 */
int uft_gw_erase_track(uft_gw_device_t* device, uint8_t revolutions);

/**
 * @brief Free flux data
 */
void uft_gw_free_flux(uft_gw_flux_data_t* flux);

/* ═══════════════════════════════════════════════════════════════════════════
 * API: HIGH-LEVEL OPERATIONS
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Read complete track (seek + read)
 */
int uft_gw_read_track(uft_gw_device_t* device,
                      uint8_t cylinder,
                      uint8_t head,
                      uint8_t revolutions,
                      uft_gw_flux_data_t* flux);

/**
 * @brief Write complete track (seek + write)
 */
int uft_gw_write_track(uft_gw_device_t* device,
                       uint8_t cylinder,
                       uint8_t head,
                       const uint32_t* samples,
                       uint32_t sample_count,
                       bool verify);

/**
 * @brief Recalibrate (seek to track 0)
 */
int uft_gw_recalibrate(uft_gw_device_t* device);

/**
 * @brief Measure disk RPM
 * @return RPM or negative error code
 */
int uft_gw_measure_rpm(uft_gw_device_t* device);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GREASEWEAZLE_H */
