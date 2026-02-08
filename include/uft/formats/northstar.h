#ifndef UFT_FORMATS_NORTHSTAR_H
#define UFT_FORMATS_NORTHSTAR_H
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
    bool     double_density;
    void    *internal_ctx;
} NorthStarDevice;

int northstar_probe(const uint8_t *data, size_t size);
int northstar_open(NorthStarDevice *dev, const char *path);
int northstar_close(NorthStarDevice *dev);
int northstar_read_sector(NorthStarDevice *dev, uint32_t track, uint32_t sector, uint8_t *buf);

#ifdef __cplusplus
}
#endif
#endif
