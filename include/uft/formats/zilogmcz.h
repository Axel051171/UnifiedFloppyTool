#ifndef UFT_FORMATS_ZILOGMCZ_H
#define UFT_FORMATS_ZILOGMCZ_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t tracks;
    uint32_t sectors;
    uint32_t sectorSize;
    void    *internal_ctx;
} ZilogMczDevice;

int zilogmcz_probe(const uint8_t *data, size_t size);
int zilogmcz_open(ZilogMczDevice *dev, const char *path);
int zilogmcz_close(ZilogMczDevice *dev);
int zilogmcz_read_sector(ZilogMczDevice *dev, uint32_t track, uint32_t sector, uint8_t *buf);

#ifdef __cplusplus
}
#endif
#endif
