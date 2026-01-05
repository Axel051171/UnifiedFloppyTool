// SPDX-License-Identifier: MIT
/*
 * applesauce_hw.h - Applesauce FDC Hardware Support Header
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#ifndef APPLESAUCE_HW_H
#define APPLESAUCE_HW_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct applesauce_handle_t applesauce_handle_t;

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize Applesauce device
 * 
 * @param port_path Serial port path (e.g. "/dev/ttyUSB0")
 * @param handle_out Device handle (output)
 * @return 0 on success
 */
int applesauce_init(const char *port_path, applesauce_handle_t **handle_out);

/**
 * @brief Close Applesauce device
 * 
 * @param handle Device handle
 */
void applesauce_close(applesauce_handle_t *handle);

/**
 * @brief Connect to floppy drive
 * 
 * @param handle Device handle
 * @return 0 on success
 */
int applesauce_connect(applesauce_handle_t *handle);

/**
 * @brief Seek to track
 * 
 * @param handle Device handle
 * @param track Track number (0-79)
 * @return 0 on success
 */
int applesauce_seek(applesauce_handle_t *handle, int track);

/**
 * @brief Get rotational period (RPM measurement)
 * 
 * @param handle Device handle
 * @param period_us_out Period in microseconds (output)
 * @return 0 on success
 */
int applesauce_get_rpm(
    applesauce_handle_t *handle,
    double *period_us_out
);

/**
 * @brief Read flux data from track
 * 
 * @param handle Device handle
 * @param side Side (0 or 1)
 * @param flux_data_out Flux data (allocated by function)
 * @param flux_len_out Flux data length
 * @return 0 on success
 */
int applesauce_read_flux(
    applesauce_handle_t *handle,
    int side,
    uint8_t **flux_data_out,
    size_t *flux_len_out
);

/**
 * @brief Write flux data to track
 * 
 * @param handle Device handle
 * @param side Side (0 or 1)
 * @param flux_data Flux data
 * @param flux_len Flux data length
 * @return 0 on success
 */
int applesauce_write_flux(
    applesauce_handle_t *handle,
    int side,
    const uint8_t *flux_data,
    size_t flux_len
);

/**
 * @brief Detect Applesauce devices
 * 
 * @param device_list_out Device list (allocated by function)
 * @param count_out Number of devices
 * @return 0 on success
 */
int applesauce_detect_devices(char ***device_list_out, int *count_out);

/**
 * @brief Get statistics
 * 
 * @param handle Device handle
 * @param bytes_read Bytes read (output, optional)
 * @param bytes_written Bytes written (output, optional)
 */
void applesauce_get_stats(
    applesauce_handle_t *handle,
    unsigned long *bytes_read,
    unsigned long *bytes_written
);

#ifdef __cplusplus
}
#endif

#endif /* APPLESAUCE_HW_H */
