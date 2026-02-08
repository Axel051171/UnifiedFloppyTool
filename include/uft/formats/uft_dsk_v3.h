#ifndef UFT_DSK_V3_H
#define UFT_DSK_V3_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct dsk_disk dsk_disk_t;
bool dsk_parse(const uint8_t* data, size_t size, dsk_disk_t* disk);
void dsk_disk_free(dsk_disk_t* disk);
#ifdef __cplusplus
}
#endif
#endif
