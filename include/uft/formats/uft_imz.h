/**
 * @file uft_imz.h
 * @brief IMZ Format Handler (WinImage Compressed Disk Image)
 * 
 * IMZ is simply a ZIP-compressed IMG file.
 * 
 * @version 1.0.0
 * @date 2026-01-15
 */

#ifndef UFT_IMZ_H
#define UFT_IMZ_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * TYPES
 *===========================================================================*/

typedef struct uft_imz_context uft_imz_t;

/*===========================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * @brief Open IMZ file for reading
 */
uft_imz_t* uft_imz_open(const char *path);

/**
 * @brief Create new IMZ file
 */
uft_imz_t* uft_imz_create(const char *path, size_t image_size);

/**
 * @brief Close IMZ file
 */
void uft_imz_close(uft_imz_t *imz);

/*===========================================================================
 * I/O OPERATIONS
 *===========================================================================*/

/**
 * @brief Read sector from image
 */
int uft_imz_read_sector(uft_imz_t *imz, int track, int head, int sector,
                        uint8_t *buffer, size_t size);

/**
 * @brief Write sector to image
 */
int uft_imz_write_sector(uft_imz_t *imz, int track, int head, int sector,
                         const uint8_t *buffer, size_t size);

/**
 * @brief Read entire decompressed image
 */
int uft_imz_read_all(uft_imz_t *imz, uint8_t **data, size_t *size);

/**
 * @brief Write entire image and compress
 */
int uft_imz_write_all(uft_imz_t *imz, const uint8_t *data, size_t size);

/*===========================================================================
 * CONVERSION
 *===========================================================================*/

/**
 * @brief Convert IMG to IMZ
 */
int uft_imz_compress(const char *img_path, const char *imz_path);

/**
 * @brief Convert IMZ to IMG
 */
int uft_imz_decompress(const char *imz_path, const char *img_path);

/*===========================================================================
 * INFORMATION
 *===========================================================================*/

/**
 * @brief Get decompressed image size
 */
size_t uft_imz_get_size(uft_imz_t *imz);

/**
 * @brief Get compressed file size
 */
size_t uft_imz_get_compressed_size(uft_imz_t *imz);

/**
 * @brief Get compression ratio (0.0 - 1.0)
 */
float uft_imz_get_ratio(uft_imz_t *imz);

/**
 * @brief Check if file is valid IMZ
 */
bool uft_imz_probe(const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_IMZ_H */
