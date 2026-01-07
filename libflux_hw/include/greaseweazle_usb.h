// SPDX-License-Identifier: MIT
/*
 * 
 * @version 2.0.0
 * @date 2025-01-01
 */

#ifndef UFT_GW_USB_H
#define UFT_GW_USB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * TYPES
 *============================================================================*/

typedef struct uft_gw_device uft_gw_device_t;
typedef int uft_gw_error_t;

#define UFT_GW_OK               0
#define UFT_GW_ERR_NOT_FOUND    -1
#define UFT_GW_ERR_ACCESS       -2
#define UFT_GW_ERR_USB          -3
#define UFT_GW_ERR_TIMEOUT      -4
#define UFT_GW_ERR_PROTOCOL     -5
#define UFT_GW_ERR_NO_INDEX     -6
#define UFT_GW_ERR_WRPROT       -7
#define UFT_GW_ERR_NO_MEM       -8

/* Bus types */
#define UFT_GW_BUS_NONE         0x00
#define UFT_GW_BUS_IBMPC        0x01
#define UFT_GW_BUS_SHUGART      0x02
#define UFT_GW_BUS_APPLE2       0x03

/*============================================================================*
 * DEVICE MANAGEMENT
 *============================================================================*/

uft_gw_error_t uft_gw_open(uft_gw_device_t **dev_out);
void uft_gw_close(uft_gw_device_t *dev);
uft_gw_error_t uft_gw_reset(uft_gw_device_t *dev);

/*============================================================================*
 * DRIVE CONTROL
 *============================================================================*/

uft_gw_error_t uft_gw_select_drive(uft_gw_device_t *dev, uint8_t drive);
uft_gw_error_t uft_gw_deselect_drive(uft_gw_device_t *dev);
uft_gw_error_t uft_gw_set_bus_type(uft_gw_device_t *dev, uint8_t bus_type);
uft_gw_error_t uft_gw_motor(uft_gw_device_t *dev, uint8_t drive, int on);
uft_gw_error_t uft_gw_seek(uft_gw_device_t *dev, int8_t cylinder);
uft_gw_error_t uft_gw_head(uft_gw_device_t *dev, uint8_t head);

/*============================================================================*
 * FLUX OPERATIONS
 *============================================================================*/

uft_gw_error_t uft_gw_read_flux(uft_gw_device_t *dev, uint32_t ticks, uint16_t max_index,
                        uint8_t **flux_out, size_t *flux_len_out);

uft_gw_error_t uft_gw_write_flux(uft_gw_device_t *dev, const uint8_t *flux, size_t flux_len,
                         int cue_at_index, int terminate_at_index);

/*============================================================================*
 * FLUX ENCODING/DECODING
 *============================================================================*/

size_t uft_gw_decode_flux(const uint8_t *raw, size_t raw_len, 
                      uint32_t *ticks_out, size_t max_ticks);

size_t uft_gw_encode_flux(const uint32_t *ticks, size_t tick_count, 
                      uint8_t *raw_out, size_t max_raw);

/*============================================================================*
 * INFO
 *============================================================================*/

const char *uft_gw_get_model_name(uft_gw_device_t *dev);
void uft_gw_print_info(uft_gw_device_t *dev);
const char *uft_gw_error_string(uft_gw_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GW_USB_H */
