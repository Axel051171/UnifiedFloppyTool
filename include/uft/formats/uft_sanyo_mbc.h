#ifndef UFT_SANYO_MBC_H
#define UFT_SANYO_MBC_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_sanyo_mbc_image_t;

int uft_sanyo_mbc_probe(const uint8_t *data, size_t size);
int uft_sanyo_mbc_read(const char *path, uft_sanyo_mbc_image_t **image);
void uft_sanyo_mbc_free(uft_sanyo_mbc_image_t *image);
int uft_sanyo_mbc_get_info(uft_sanyo_mbc_image_t *img, char *buf, size_t size);
#endif
