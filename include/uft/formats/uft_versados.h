#ifndef UFT_VERSADOS_H
#define UFT_VERSADOS_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_versados_image_t;

int uft_versados_probe(const uint8_t *data, size_t size);
int uft_versados_read(const char *path, uft_versados_image_t **image);
void uft_versados_free(uft_versados_image_t *image);
int uft_versados_get_info(uft_versados_image_t *img, char *buf, size_t size);
#endif
