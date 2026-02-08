#ifndef UFT_FORMATS_UDI_H
#define UFT_FORMATS_UDI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define UDI_SIGNATURE      "UDI!"
#define UDI_SIGNATURE_COMP "udi!"

#pragma pack(push, 1)
typedef struct {
    char     signature[4];
    uint32_t file_size;
    uint8_t  version;
    uint8_t  max_cyl;
    uint8_t  max_head;
    uint8_t  reserved;
    uint32_t ext_hdr_len;
} UdiHeader;
#pragma pack(pop)

typedef struct {
    uint32_t cylinders;
    uint32_t heads;
    bool     compressed;
    void    *internal_ctx;
} UdiDevice;

int udi_probe(const uint8_t *data, size_t size);
int udi_open(UdiDevice *dev, const char *path);
int udi_close(UdiDevice *dev);
int udi_read_track(UdiDevice *dev, uint32_t c, uint32_t h, uint8_t *buf, size_t *size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_UDI_H */
