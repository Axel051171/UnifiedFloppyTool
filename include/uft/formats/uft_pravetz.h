#ifndef UFT_PRAVETZ_H
#define UFT_PRAVETZ_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_pravetz_image_t;

int uft_pravetz_probe(const uint8_t *data, size_t size);
int uft_pravetz_read(const char *path, uft_pravetz_image_t **image);
void uft_pravetz_free(uft_pravetz_image_t *image);
int uft_pravetz_get_info(uft_pravetz_image_t *img, char *buf, size_t size);
#endif
