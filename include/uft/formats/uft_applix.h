#ifndef UFT_APPLIX_H
#define UFT_APPLIX_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_applix_image_t;

int uft_applix_probe(const uint8_t *data, size_t size);
int uft_applix_read(const char *path, uft_applix_image_t **image);
void uft_applix_free(uft_applix_image_t *image);
int uft_applix_get_info(uft_applix_image_t *img, char *buf, size_t size);
#endif
