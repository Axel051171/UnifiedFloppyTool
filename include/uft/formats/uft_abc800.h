/**
 * @file uft_abc800.h
 * @brief Luxor ABC 80/800 Disk Format Interface
 * @version 4.1.2
 * 
 * Swedish home/office computers (1978-1985)
 * Uses ABC-DOS and UFD-DOS filesystems
 */

#ifndef UFT_ABC800_H
#define UFT_ABC800_H

#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ABC 80/800 Image Structure */
typedef struct {
    uint8_t *data;
    size_t size;
    int tracks;
    int sectors;
    int heads;
    int sector_size;
} uft_abc800_image_t;

/* Functions */
int uft_abc800_probe(const uint8_t *data, size_t size, int *tracks, int *heads,
                     const char **name);
int uft_abc800_read(const char *path, uft_abc800_image_t **image);
void uft_abc800_free(uft_abc800_image_t *image);
int uft_abc800_read_sector(uft_abc800_image_t *image, int track, int head,
                           int sector, uint8_t *buffer);
int uft_abc800_write_sector(uft_abc800_image_t *image, int track, int head,
                            int sector, const uint8_t *buffer);
int uft_abc800_get_info(uft_abc800_image_t *image, char *buf, size_t size);
int uft_abc800_create(const char *path, int tracks, int heads);
int uft_abc800_write(uft_abc800_image_t *image, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ABC800_H */
