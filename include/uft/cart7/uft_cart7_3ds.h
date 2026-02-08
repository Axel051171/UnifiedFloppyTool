/**
 * @file uft_cart7_3ds.h
 * @brief Nintendo 3DS HAL Provider API for Cart8
 */

#ifndef UFT_CART7_3DS_H
#define UFT_CART7_3DS_H

#include "cart7_3ds_protocol.h"
#include "uft_cart7.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Slot selection */
uft_cart7_error_t uft_cart7_3ds_select(uft_cart7_device_t *device);

/* Header reading */
uft_cart7_error_t uft_cart7_3ds_read_ncsd(uft_cart7_device_t *device, ctr_ncsd_header_t *header);
uft_cart7_error_t uft_cart7_3ds_read_ncch(uft_cart7_device_t *device, int partition, ctr_ncch_header_t *header);
uft_cart7_error_t uft_cart7_3ds_read_exefs_header(uft_cart7_device_t *device, int partition, ctr_exefs_header_t *header);
uft_cart7_error_t uft_cart7_3ds_read_smdh(uft_cart7_device_t *device, int partition, ctr_smdh_t *smdh);

/* Cartridge info */
uft_cart7_error_t uft_cart7_3ds_get_info(uft_cart7_device_t *device, cart7_3ds_info_t *info);

/* ROM dumping */
uft_cart7_error_t uft_cart7_3ds_dump(uft_cart7_device_t *device, const char *filename, bool trimmed, uft_cart7_progress_cb progress, void *user_data);
uft_cart7_error_t uft_cart7_3ds_dump_partition(uft_cart7_device_t *device, int partition, const char *filename, uft_cart7_progress_cb progress, void *user_data);

/* Save data */
int64_t uft_cart7_3ds_read_save(uft_cart7_device_t *device, uint64_t offset, void *buffer, size_t length);
int64_t uft_cart7_3ds_write_save(uft_cart7_device_t *device, uint64_t offset, const void *buffer, size_t length);
ctr_save_type_t uft_cart7_3ds_detect_save(uft_cart7_device_t *device);
uint32_t ctr_save_size_bytes(ctr_save_type_t type);

/* Icon extraction */
uft_cart7_error_t uft_cart7_3ds_extract_icon(uft_cart7_device_t *device, int partition, uint8_t *icon_48x48_rgb565);

/* Debug */
void uft_cart7_3ds_print_ncsd(const ctr_ncsd_header_t *ncsd);
void uft_cart7_3ds_print_ncch(const ctr_ncch_header_t *ncch);
void uft_cart7_3ds_print_info(const cart7_3ds_info_t *info);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CART7_3DS_H */
