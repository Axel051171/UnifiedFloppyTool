/**
 * @file uft_ldbs.h
 * @brief LDBS (LibDsk Block Store) format interface
 * @version 4.1.0
 */

#ifndef UFT_LDBS_H
#define UFT_LDBS_H

#include "uft/core/uft_unified_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Probe file for LDBS format
 * @param data File data
 * @param size Data size
 * @param confidence Output confidence (0-100)
 * @return true if LDBS format detected
 */
bool uft_ldbs_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read LDBS file
 * @param path File path
 * @param out Output disk image
 * @return UFT_OK on success
 */
int uft_ldbs_read(const char *path, uft_disk_image_t **out);

/**
 * @brief Write LDBS file
 * @param path File path
 * @param disk Disk image
 * @return UFT_OK on success
 */
int uft_ldbs_write(const char *path, const uft_disk_image_t *disk);

/**
 * @brief Get LDBS file info
 * @param path File path
 * @param buf Output buffer
 * @param buf_size Buffer size
 * @return UFT_OK on success
 */
int uft_ldbs_get_info(const char *path, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LDBS_H */
