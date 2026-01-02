// SPDX-License-Identifier: MIT
/*
 * greaseweazle_usb.h - Greaseweazle USB Driver Header
 * 
 * @version 2.0.0
 * @date 2025-01-01
 */

#ifndef GREASEWEAZLE_USB_H
#define GREASEWEAZLE_USB_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * TYPES
 *============================================================================*/

typedef struct gw_device gw_device_t;
typedef int gw_error_t;

#define GW_OK               0
#define GW_ERR_NOT_FOUND    -1
#define GW_ERR_ACCESS       -2
#define GW_ERR_USB          -3
#define GW_ERR_TIMEOUT      -4
#define GW_ERR_PROTOCOL     -5
#define GW_ERR_NO_INDEX     -6
#define GW_ERR_WRPROT       -7
#define GW_ERR_NO_MEM       -8

/* Bus types */
#define GW_BUS_NONE         0x00
#define GW_BUS_IBMPC        0x01
#define GW_BUS_SHUGART      0x02
#define GW_BUS_APPLE2       0x03

/*============================================================================*
 * DEVICE MANAGEMENT
 *============================================================================*/

gw_error_t gw_open(gw_device_t **dev_out);
void gw_close(gw_device_t *dev);
gw_error_t gw_reset(gw_device_t *dev);

/*============================================================================*
 * DRIVE CONTROL
 *============================================================================*/

gw_error_t gw_select_drive(gw_device_t *dev, uint8_t drive);
gw_error_t gw_deselect_drive(gw_device_t *dev);
gw_error_t gw_set_bus_type(gw_device_t *dev, uint8_t bus_type);
gw_error_t gw_motor(gw_device_t *dev, uint8_t drive, int on);
gw_error_t gw_seek(gw_device_t *dev, int8_t cylinder);
gw_error_t gw_head(gw_device_t *dev, uint8_t head);

/*============================================================================*
 * FLUX OPERATIONS
 *============================================================================*/

gw_error_t gw_read_flux(gw_device_t *dev, uint32_t ticks, uint16_t max_index,
                        uint8_t **flux_out, size_t *flux_len_out);

gw_error_t gw_write_flux(gw_device_t *dev, const uint8_t *flux, size_t flux_len,
                         int cue_at_index, int terminate_at_index);

/*============================================================================*
 * FLUX ENCODING/DECODING
 *============================================================================*/

size_t gw_decode_flux(const uint8_t *raw, size_t raw_len, 
                      uint32_t *ticks_out, size_t max_ticks);

size_t gw_encode_flux(const uint32_t *ticks, size_t tick_count, 
                      uint8_t *raw_out, size_t max_raw);

/*============================================================================*
 * INFO
 *============================================================================*/

const char *gw_get_model_name(gw_device_t *dev);
void gw_print_info(gw_device_t *dev);
const char *gw_error_string(gw_error_t err);

#ifdef __cplusplus
}
#endif

#endif /* GREASEWEAZLE_USB_H */
