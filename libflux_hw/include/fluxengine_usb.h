// SPDX-License-Identifier: MIT
/*
 * 
 * @version 2.7.4
 * @date 2024-12-26
 */

#ifndef UFT_FE_USB_H
#define UFT_FE_USB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration */
typedef struct uft_fe_handle_t uft_fe_handle_t;

/*============================================================================*
 * PUBLIC API
 *============================================================================*/

/**
 * 
 * @param handle_out Device handle (output)
 * @return 0 on success
 */
int uft_fe_init(uft_fe_handle_t **handle_out);

/**
 * 
 * @param handle Device handle
 */
void uft_fe_close(uft_fe_handle_t *handle);

/**
 * @brief Seek to track
 * 
 * @param handle Device handle
 * @param track Track number (0-83)
 * @return 0 on success
 */
int uft_fe_seek(uft_fe_handle_t *handle, uint8_t track);

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
int uft_fe_read_flux(
    uft_fe_handle_t *handle,
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
int uft_fe_set_drive(
    uft_fe_handle_t *handle,
    uint8_t drive,
    bool high_density
);

/**
 * @brief Recalibrate drive (seek to track 0)
 * 
 * @param handle Device handle
 * @return 0 on success
 */
int uft_fe_recalibrate(uft_fe_handle_t *handle);

/**
 * 
 * @param device_list_out Device list (allocated by function)
 * @param count_out Number of devices
 * @return 0 on success
 */
int uft_fe_detect_devices(char ***device_list_out, int *count_out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FE_USB_H */
