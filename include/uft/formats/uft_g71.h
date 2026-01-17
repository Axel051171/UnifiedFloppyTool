/**
 * @file uft_g71.h
 * @brief G71 (GCR Track Image for 1571) Format Interface
 * @version 4.1.0
 */

#ifndef UFT_G71_H
#define UFT_G71_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe file for G71 format
 */
bool uft_g71_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read G71 file
 */
int uft_g71_read(const char *path, uft_disk_image_t **out);

/**
 * @brief Write G71 file
 */
int uft_g71_write(const char *path, const uft_disk_image_t *disk);

/**
 * @brief Create blank G71 image
 */
int uft_g71_create_blank(uft_disk_image_t **out);

/**
 * @brief Get G71 file info
 */
int uft_g71_get_info(const char *path, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_G71_H */
