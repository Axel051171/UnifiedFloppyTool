#ifndef UFT_FORMATS_CPM_H
#define UFT_FORMATS_CPM_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CPM_UNKNOWN = 0,
    CPM_8_SSSD,        /* 8" SSSD (IBM 3740) */
    CPM_525_SSDD,      /* 5.25" SSDD */
    CPM_525_DSDD,      /* 5.25" DSDD */
    CPM_35_DSDD,       /* 3.5" DSDD */
    CPM_35_DSHD         /* 3.5" DSHD */
} CpmFormat;

typedef struct {
    uint32_t tracks;
    uint32_t heads;
    uint32_t sectors;
    uint32_t sectorSize;
    uint32_t blockSize;
    uint32_t dirEntries;
    uint32_t reservedTracks;
    CpmFormat format;
    void    *internal_ctx;
} CpmDevice;

CpmFormat cpm_detect_format(const uint8_t *data, size_t size);
int cpm_probe(const uint8_t *data, size_t size);
int cpm_open(CpmDevice *dev, const char *path);
int cpm_close(CpmDevice *dev);
int cpm_read_sector(CpmDevice *dev, uint32_t track, uint32_t head, uint32_t sector, uint8_t *buf);
int cpm_write_sector(CpmDevice *dev, uint32_t track, uint32_t head, uint32_t sector, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_CPM_H */
