#ifndef UFT_ROBOTRON_H
#define UFT_ROBOTRON_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
    char variant[64];
} uft_robotron_image_t;

int uft_robotron_probe(const uint8_t *data, size_t size);
int uft_robotron_read(const char *path, uft_robotron_image_t **image);
void uft_robotron_free(uft_robotron_image_t *image);
int uft_robotron_get_info(uft_robotron_image_t *img, char *buf, size_t size);
#endif
