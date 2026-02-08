#ifndef UFT_HITACHI_S1_H
#define UFT_HITACHI_S1_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_hitachi_s1_image_t;

int uft_hitachi_s1_probe(const uint8_t *data, size_t size);
int uft_hitachi_s1_read(const char *path, uft_hitachi_s1_image_t **image);
void uft_hitachi_s1_free(uft_hitachi_s1_image_t *image);
int uft_hitachi_s1_get_info(uft_hitachi_s1_image_t *img, char *buf, size_t size);
#endif
