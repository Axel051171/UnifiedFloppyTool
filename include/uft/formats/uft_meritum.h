#ifndef UFT_MERITUM_H
#define UFT_MERITUM_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_meritum_image_t;

int uft_meritum_probe(const uint8_t *data, size_t size);
int uft_meritum_read(const char *path, uft_meritum_image_t **image);
void uft_meritum_free(uft_meritum_image_t *image);
int uft_meritum_get_info(uft_meritum_image_t *img, char *buf, size_t size);
#endif
