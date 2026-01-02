// SPDX-License-Identifier: MIT
/*
 * xum1541_usb.h - XUM1541 USB Hardware Support Header
 * 
 * @version 2.7.3
 * @date 2024-12-26
 */

#ifndef XUM1541_USB_H
#define XUM1541_USB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct xum1541_handle_t xum1541_handle_t;
typedef struct xum1541_device_info_t xum1541_device_info_t;

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize XUM1541 USB device
 * 
 * @param handle_out Device handle (output)
 * @return 0 on success
 */
int xum1541_init(xum1541_handle_t **handle_out);

/**
 * @brief Close XUM1541 USB device
 * 
 * @param handle Device handle
 */
void xum1541_close(xum1541_handle_t *handle);

/**
 * @brief Read track nibbles from drive
 * 
 * @param handle Device handle
 * @param track Track number (1-42)
 * @param track_data_out Track data (allocated by function)
 * @param track_len_out Track length
 * @return 0 on success
 */
int xum1541_read_track(
    xum1541_handle_t *handle,
    uint8_t track,
    uint8_t **track_data_out,
    size_t *track_len_out
);

/**
 * @brief Write track nibbles to drive
 * 
 * @param handle Device handle
 * @param track Track number (1-42)
 * @param track_data Track data
 * @param track_len Track length
 * @return 0 on success
 */
int xum1541_write_track(
    xum1541_handle_t *handle,
    uint8_t track,
    const uint8_t *track_data,
    size_t track_len
);

/**
 * @brief Control drive motor
 * 
 * @param handle Device handle
 * @param on true = motor on, false = motor off
 * @return 0 on success
 */
int xum1541_motor(xum1541_handle_t *handle, bool on);

/**
 * @brief Detect XUM1541 devices
 * 
 * @param device_list_out Device list (allocated by function)
 * @param count_out Number of devices
 * @return 0 on success
 */
int xum1541_detect_devices(char ***device_list_out, int *count_out);

#ifdef __cplusplus
}
#endif

#endif /* XUM1541_USB_H */
