#ifndef UFT_IMD_V3_H
#define UFT_IMD_V3_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct imd_disk imd_disk_t;
typedef struct imd_stats imd_stats_t;
bool imd_parse(const uint8_t* data, size_t size, imd_disk_t* disk);
void imd_disk_free(imd_disk_t* disk);
bool imd_get_sector(const uint8_t* data, size_t size, int cyl, int head, int sec, uint8_t* out, size_t* out_size);
void imd_calculate_stats(const imd_disk_t* disk, imd_stats_t* stats);
const char* imd_mode_str(uint8_t mode);
#ifdef __cplusplus
}
#endif
#endif
