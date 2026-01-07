/**
 * @file dnp.h
 * @brief DNP (Commodore 1581 Partition) Format Support
 * 
 * Stub header - implementation in src/formats/commodore/dnp.c
 */

#ifndef UFT_FORMATS_DNP_H
#define UFT_FORMATS_DNP_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* DNP format constants */
#define DNP_SECTOR_SIZE     256
#define DNP_TRACKS          80
#define DNP_SECTORS_PER_TRACK 40

/* Forward declarations - implement in .c file */
struct dnp_image;
typedef struct dnp_image dnp_image_t;

/* Basic API */
int dnp_open(dnp_image_t **img, const char *path);
void dnp_close(dnp_image_t *img);
int dnp_read_sector(dnp_image_t *img, int track, int sector, uint8_t *buf);
int dnp_write_sector(dnp_image_t *img, int track, int sector, const uint8_t *buf);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_DNP_H */
