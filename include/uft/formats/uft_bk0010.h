/**
 * @file uft_bk0010.h
 * @brief Elektronika BK-0010/0011 Disk Format Interface
 * @version 4.1.2
 * 
 * Soviet 16-bit PDP-11 compatible home computers (1985-1990s)
 * RT-11 compatible disk format
 */

#ifndef UFT_BK0010_H
#define UFT_BK0010_H

#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DOS type detection */
typedef enum {
    BK_DOS_UNKNOWN = 0,
    BK_DOS_RT11,
    BK_DOS_ANDOS,
    BK_DOS_MKDOS,
    BK_DOS_CSIDOS
} bk_dos_type_t;

/* BK-0010/0011 Image Structure */
typedef struct {
    uint8_t *data;
    size_t size;
    int tracks;
    int sectors;
    int heads;
    int sector_size;
    bk_dos_type_t dos_type;
} uft_bk0010_image_t;

/* Functions */
int uft_bk0010_probe(const uint8_t *data, size_t size, int *tracks, int *heads,
                     bk_dos_type_t *dos);
int uft_bk0010_read(const char *path, uft_bk0010_image_t **image);
void uft_bk0010_free(uft_bk0010_image_t *image);
int uft_bk0010_read_sector(uft_bk0010_image_t *image, int track, int head,
                           int sector, uint8_t *buffer);
int uft_bk0010_write_sector(uft_bk0010_image_t *image, int track, int head,
                            int sector, const uint8_t *buffer);
int uft_bk0010_get_info(uft_bk0010_image_t *image, char *buf, size_t size);
int uft_bk0010_create(const char *path, int tracks, int heads, int init_rt11);
int uft_bk0010_write(uft_bk0010_image_t *image, const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_BK0010_H */
