#ifndef UFT_FORMATS_LIF_H
#define UFT_FORMATS_LIF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LIF_TRACKS          77
#define LIF_SECTORS_TRACK   16
#define LIF_SECTOR_SIZE     256

#pragma pack(push, 1)
typedef struct {
    uint16_t lif_id;
    char     label[6];
    uint32_t directory_start;
    uint16_t system_3000;
    uint16_t dummy;
    uint32_t directory_length;
    uint16_t lif_version;
} LifVolumeHeader;
#pragma pack(pop)

typedef struct {
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    uint32_t sectorSize;
    void    *internal_ctx;
} LifDevice;

int lif_probe(const uint8_t *data, size_t size);
int lif_open(LifDevice *dev, const char *path);
int lif_close(LifDevice *dev);

#ifdef __cplusplus
}
#endif

#endif
