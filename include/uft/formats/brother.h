#ifndef UFT_FORMATS_BROTHER_H
#define UFT_FORMATS_BROTHER_H

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
    bool     is_120_track;
    void    *internal_ctx;
} BrotherDevice;

int brother_probe(const uint8_t *data, size_t size);
int brother_open(BrotherDevice *dev, const char *path);
int brother_close(BrotherDevice *dev);
int brother_read_sector(BrotherDevice *dev, uint32_t track, uint32_t sector, uint8_t *buf);
int brother_write_sector(BrotherDevice *dev, uint32_t track, uint32_t sector, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_BROTHER_H */
