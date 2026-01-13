#ifndef UFT_STX_V3_H
#define UFT_STX_V3_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct stx_disk stx_disk_t;
bool stx_parse(const uint8_t* data, size_t size, stx_disk_t* disk);
void stx_disk_free(stx_disk_t* disk);
#ifdef __cplusplus
}
#endif
#endif
