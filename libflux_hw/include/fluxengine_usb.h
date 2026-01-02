// SPDX-License-Identifier: MIT
/*
 * fluxengine_usb.h - FluxEngine USB Hardware Support Header
 * 
 * @version 2.7.4
 * @date 2024-12-26
 */

#ifndef FLUXENGINE_USB_H
#define FLUXENGINE_USB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct fluxengine_handle_t fluxengine_handle_t;

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * @brief Initialize FluxEngine USB device
 * 
 * @param handle_out Device handle (output)
 * @return 0 on success
 */
int fluxengine_init(fluxengine_handle_t **handle_out);

/**
 * @brief Close FluxEngine USB device
 * 
 * @param handle Device handle
 */
void fluxengine_close(fluxengine_handle_t *handle);

/**
 * @brief Seek to track
 * 
 * @param handle Device handle
 * @param track Track number (0-83)
 * @return 0 on success
 */
int fluxengine_seek(fluxengine_handle_t *handle, uint8_t track);

/**
 * @brief Read flux data from track
 * 
 * @param handle Device handle
 * @param side Side (0 or 1)
 * @param read_time_ms Read time in milliseconds
 * @param flux_data_out Flux data (allocated by function)
 * @param flux_len_out Flux data length
 * @return 0 on success
 */
int fluxengine_read_flux(
    fluxengine_handle_t *handle,
    uint8_t side,
    uint32_t read_time_ms,
    uint8_t **flux_data_out,
    size_t *flux_len_out
);

/**
 * @brief Set drive parameters
 * 
 * @param handle Device handle
 * @param drive Drive number (0 or 1)
 * @param high_density true = HD, false = DD
 * @return 0 on success
 */
int fluxengine_set_drive(
    fluxengine_handle_t *handle,
    uint8_t drive,
    bool high_density
);

/**
 * @brief Recalibrate drive (seek to track 0)
 * 
 * @param handle Device handle
 * @return 0 on success
 */
int fluxengine_recalibrate(fluxengine_handle_t *handle);

/**
 * @brief Detect FluxEngine devices
 * 
 * @param device_list_out Device list (allocated by function)
 * @param count_out Number of devices
 * @return 0 on success
 */
int fluxengine_detect_devices(char ***device_list_out, int *count_out);

#ifdef __cplusplus
}
#endif

#endif /* FLUXENGINE_USB_H */
