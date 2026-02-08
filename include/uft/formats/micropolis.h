#ifndef UFT_FORMATS_MICROPOLIS_H
#define UFT_FORMATS_MICROPOLIS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MICROPOLIS_MOD1 = 0,
    MICROPOLIS_MOD2 = 1,
    MICROPOLIS_METAFLOPPY = 2,
    MICROPOLIS_VECTOR_GRAPHIC = 3
} MicropolisType;

#define MICROPOLIS_SECTOR_SIZE_STD  266
#define MICROPOLIS_SECTOR_SIZE_VG   275

typedef struct {
    uint32_t tracks;
    uint32_t sectors;
    uint32_t sectorSize;
    bool     double_density;
    MicropolisType type;
    void    *internal_ctx;
} MicropolisDevice;

int micropolis_probe(const uint8_t *data, size_t size);
int micropolis_open(MicropolisDevice *dev, const char *path);
int micropolis_close(MicropolisDevice *dev);
int micropolis_read_sector(MicropolisDevice *dev, uint32_t track, uint32_t sector, uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif
