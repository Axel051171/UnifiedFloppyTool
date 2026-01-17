#ifndef UFT_DG_NOVA_H
#define UFT_DG_NOVA_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_dg_nova_image_t;

int uft_dg_nova_probe(const uint8_t *data, size_t size);
int uft_dg_nova_read(const char *path, uft_dg_nova_image_t **image);
void uft_dg_nova_free(uft_dg_nova_image_t *image);
int uft_dg_nova_get_info(uft_dg_nova_image_t *img, char *buf, size_t size);
#endif
