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
/* MF-1158: hier stand `cpm_close(CpmDevice *)`. Definiert sind zwei
 * andere Dinge unter demselben Namen: `cpm_close(cpm_disk_t *)` in
 * `src/detect/mfm/cpm_fs.c:393` mit aeusserer Bindung, und ein
 * dateilokales `static void cpm_close(uft_disk_t *)` in
 * `src/formats/cpm/uft_cpm_diskdef.c:1080` — letzteres ist legitim, weil
 * `static`. Fuer `CpmDevice` gibt es keine Definition; dieser Header hat
 * 0 Einbinder. Umbenannt statt entfernt (MF-1077); P3-413.
 *
 * Am Rand gemessen: DREI Namen dieses Headers haben im ganzen Baum
 * ueberhaupt keine Definition — `cpm_detect_format`, `cpm_probe` und
 * `cpm_write_sector`. Das ist die Klasse MF-366 (Phantom-API) und nicht
 * Teil dieser Umbenennung. */
int cpm_device_close(CpmDevice *dev);
int cpm_read_sector(CpmDevice *dev, uint32_t track, uint32_t head, uint32_t sector, uint8_t *buf);
int cpm_write_sector(CpmDevice *dev, uint32_t track, uint32_t head, uint32_t sector, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_CPM_H */
