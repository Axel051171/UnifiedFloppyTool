/**
 * @file vdk.h
 * @brief VDK (TRS-80 Virtual Disk) Format Support
 * 
 * Stub header - implementation in src/formats/trs80/vdk.c
 */

#ifndef UFT_FORMATS_VDK_H
#define UFT_FORMATS_VDK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* VDK format constants */
#define VDK_HEADER_SIZE     12
#define VDK_SECTOR_SIZE     256
#define VDK_SIGNATURE       "vdk"

/* Forward declarations */
struct vdk_image;
typedef struct vdk_image vdk_image_t;

/* Basic API */
int vdk_open(vdk_image_t **img, const char *path);
void vdk_close(vdk_image_t *img);
int vdk_read_sector(vdk_image_t *img, int track, int head, int sector, uint8_t *buf);
int vdk_write_sector(vdk_image_t *img, int track, int head, int sector, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_VDK_H */
