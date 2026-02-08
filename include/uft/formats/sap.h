#ifndef UFT_FORMATS_SAP_H
#define UFT_FORMATS_SAP_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif

#define SAP_CRYPT_BYTE          0xB3
#define SAP_SIGNATURE           "SYSTEME D'ARCHIVAGE PUISSANT - ARCHIVE V"
#define SAP_SECTORS_PER_TRACK   16

#pragma pack(push, 1)
typedef struct {
    char signature[66];
    uint8_t version;
} SapHeader;
#pragma pack(pop)


#pragma pack(push, 1)
typedef struct {
    uint8_t track;
    uint8_t sector;
    uint16_t size;
    uint8_t data[256];
    uint16_t crc;
} SapSector;
#pragma pack(pop)

typedef struct {
    uint32_t tracks;
    uint32_t sectors;
    uint32_t sectorSize;
    void    *internal_ctx;
} SapDevice;

int sap_probe(const uint8_t *data, size_t size);
int sap_open(SapDevice *dev, const char *path);
int sap_close(SapDevice *dev);
int sap_read_sector(SapDevice *dev, uint32_t track, uint32_t sector, uint8_t *buf);

#ifdef __cplusplus
}
#endif
#endif
