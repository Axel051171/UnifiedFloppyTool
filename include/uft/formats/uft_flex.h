/**
 * @file uft_flex.h
 * @brief FLEX/UniFLEX Disk Format Interface
 * @version 4.1.2
 */

#ifndef UFT_FLEX_H
#define UFT_FLEX_H

#include "uft/core/uft_unified_types.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* FLEX Image Structure */
typedef struct {
    uint8_t *data;
    size_t size;
    int tracks;
    int sectors;
    int heads;
    int sector_size;
    char volume_name[12];
    uint16_t free_sectors;
    uint8_t max_track;
    uint8_t max_sector;
} uft_flex_image_t;

/* Directory Entry */
typedef struct {
    char filename[12];
    char extension[4];
    uint8_t attributes;
    uint8_t start_track;
    uint8_t start_sector;
    uint8_t end_track;
    uint8_t end_sector;
    uint16_t sector_count;
    uint8_t random_flag;
    uint8_t month;
    uint8_t day;
    uint8_t year;
} uft_flex_dir_entry_t;

/* File attributes */
#define FLEX_ATTR_WRITE_PROTECT   0x80
#define FLEX_ATTR_DELETE_PROTECT  0x40
#define FLEX_ATTR_CATALOG_PROTECT 0x20
#define FLEX_ATTR_RANDOM          0x02

/* Functions */
int uft_flex_probe(const uint8_t *data, size_t size, int *tracks, int *sectors, 
                   int *heads, const char **name);
int uft_flex_read(const char *path, uft_flex_image_t **image);
void uft_flex_free(uft_flex_image_t *image);
int uft_flex_read_directory(uft_flex_image_t *image, uft_flex_dir_entry_t *entries,
                            int max_entries, int *count);
int uft_flex_extract_file(uft_flex_image_t *image, const uft_flex_dir_entry_t *entry,
                          const char *output_path);
int uft_flex_get_info(uft_flex_image_t *image, char *buf, size_t size);
int uft_flex_create(const char *path, int tracks, int sectors, int heads,
                    const char *volume_name);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FLEX_H */
