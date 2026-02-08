#ifndef UFT_FORMATS_QDOS_H
#define UFT_FORMATS_QDOS_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#pragma pack(push, 1)
typedef struct {
    char     signature[4];
    char     label[20];
    uint16_t sectors_per_track;
    uint16_t sectors_per_cyl;
    uint16_t cyls_per_side;
    uint16_t reserved[4];
} QdosHeader;
#pragma pack(pop)

typedef struct {
    uint32_t cylinders;
    uint32_t heads;
    uint32_t sectors;
    bool     is_hd;
    char     label[21];
    void    *internal_ctx;
} QdosDevice;

int qdos_probe(const uint8_t *data, size_t size);
int qdos_open(QdosDevice *dev, const char *path);
int qdos_close(QdosDevice *dev);

#ifdef __cplusplus
}
#endif
#endif
