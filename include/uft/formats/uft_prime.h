#ifndef UFT_PRIME_H
#define UFT_PRIME_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_prime_image_t;

int uft_prime_probe(const uint8_t *data, size_t size);
int uft_prime_read(const char *path, uft_prime_image_t **image);
void uft_prime_free(uft_prime_image_t *image);
int uft_prime_get_info(uft_prime_image_t *img, char *buf, size_t size);
#endif
