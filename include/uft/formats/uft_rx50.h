/**
 * @file uft_rx50.h
 * @brief DEC RX50 Disk Format Interface
 * @version 4.1.2
 * 
 * DEC RX50 - 5.25" floppy for DEC Rainbow and Professional
 * 80 tracks, 10 sectors, 512 bytes/sector
 * SS: 400 KB, DS: 800 KB
 */

#ifndef UFT_RX50_H
#define UFT_RX50_H

#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Filesystem types */
typedef enum {
    RX50_FS_UNKNOWN = 0,
    RX50_FS_RT11,       /* RT-11 */
    RX50_FS_CPM86,      /* CP/M-86 */
    RX50_FS_MSDOS       /* MS-DOS */
} rx50_fs_type_t;

/* RX50 Image Structure */
typedef struct {
    uint8_t *data;
    size_t size;
    int tracks;
    int sectors;
    int heads;
    int sector_size;
    rx50_fs_type_t fs_type;
} uft_rx50_image_t;

/* Functions */
int uft_rx50_probe(const uint8_t *data, size_t size, int *heads, rx50_fs_type_t *fs);
int uft_rx50_read(const char *path, uft_rx50_image_t **image);
void uft_rx50_free(uft_rx50_image_t *image);
int uft_rx50_read_sector(uft_rx50_image_t *image, int track, int head, int sector,
                         uint8_t *buffer);
int uft_rx50_write_sector(uft_rx50_image_t *image, int track, int head, int sector,
                          const uint8_t *buffer);
int uft_rx50_get_info(uft_rx50_image_t *image, char *buf, size_t size);
int uft_rx50_create(const char *path, int heads, rx50_fs_type_t fs_type);
int uft_rx50_write(uft_rx50_image_t *image, const char *path);
int uft_rx50_logical_to_physical(int logical_sector);
int uft_rx50_physical_to_logical(int physical_sector);

#ifdef __cplusplus
}
#endif

#endif /* UFT_RX50_H */
