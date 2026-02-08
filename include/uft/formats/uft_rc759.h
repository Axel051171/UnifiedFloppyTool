#ifndef UFT_RC759_H
#define UFT_RC759_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_rc759_image_t;

int uft_rc759_probe(const uint8_t *data, size_t size);
int uft_rc759_read(const char *path, uft_rc759_image_t **image);
void uft_rc759_free(uft_rc759_image_t *image);
int uft_rc759_get_info(uft_rc759_image_t *img, char *buf, size_t size);
#endif
