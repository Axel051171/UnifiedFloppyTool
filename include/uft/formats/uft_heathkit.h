#ifndef UFT_HEATHKIT_H
#define UFT_HEATHKIT_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
    int hard_sectored;
} uft_heathkit_image_t;

int uft_heathkit_probe(const uint8_t *data, size_t size);
int uft_heathkit_read(const char *path, uft_heathkit_image_t **image);
void uft_heathkit_free(uft_heathkit_image_t *image);
int uft_heathkit_get_info(uft_heathkit_image_t *img, char *buf, size_t size);
#endif
