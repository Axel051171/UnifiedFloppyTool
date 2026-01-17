#ifndef UFT_SHARP_X1_H
#define UFT_SHARP_X1_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_sharp_x1_image_t;

int uft_sharp_x1_probe(const uint8_t *data, size_t size);
int uft_sharp_x1_read(const char *path, uft_sharp_x1_image_t **image);
void uft_sharp_x1_free(uft_sharp_x1_image_t *image);
int uft_sharp_x1_get_info(uft_sharp_x1_image_t *img, char *buf, size_t size);
#endif
