#ifndef UFT_FORMATS_VICTOR9K_H
#define UFT_FORMATS_VICTOR9K_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t tracks;
    uint32_t heads;
    bool     flux_supported;
    void    *internal_ctx;
} Victor9kDevice;

int victor9k_probe(const uint8_t *data, size_t size);
int victor9k_open(Victor9kDevice *dev, const char *path);
int victor9k_close(Victor9kDevice *dev);

#ifdef __cplusplus
}
#endif
#endif
