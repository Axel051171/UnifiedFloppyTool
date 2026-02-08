#ifndef UFT_KC85_H
#define UFT_KC85_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_kc85_image_t;

int uft_kc85_probe(const uint8_t *data, size_t size);
int uft_kc85_read(const char *path, uft_kc85_image_t **image);
void uft_kc85_free(uft_kc85_image_t *image);
int uft_kc85_get_info(uft_kc85_image_t *img, char *buf, size_t size);
#endif
