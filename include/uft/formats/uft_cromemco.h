#ifndef UFT_CROMEMCO_H
#define UFT_CROMEMCO_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_cromemco_image_t;

int uft_cromemco_probe(const uint8_t *data, size_t size);
int uft_cromemco_read(const char *path, uft_cromemco_image_t **image);
void uft_cromemco_free(uft_cromemco_image_t *image);
int uft_cromemco_get_info(uft_cromemco_image_t *img, char *buf, size_t size);
#endif
