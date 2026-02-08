#ifndef UFT_CALCOMP_H
#define UFT_CALCOMP_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_calcomp_image_t;

int uft_calcomp_probe(const uint8_t *data, size_t size);
int uft_calcomp_read(const char *path, uft_calcomp_image_t **image);
void uft_calcomp_free(uft_calcomp_image_t *image);
int uft_calcomp_get_info(uft_calcomp_image_t *img, char *buf, size_t size);
#endif
