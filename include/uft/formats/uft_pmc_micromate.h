#ifndef UFT_PMC_MICROMATE_H
#define UFT_PMC_MICROMATE_H
#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint8_t *data; size_t size;
    int tracks, sectors, heads, sector_size;
} uft_pmc_image_t;

int uft_pmc_probe(const uint8_t *data, size_t size);
int uft_pmc_read(const char *path, uft_pmc_image_t **image);
void uft_pmc_free(uft_pmc_image_t *image);
int uft_pmc_get_info(uft_pmc_image_t *img, char *buf, size_t size);
#endif
