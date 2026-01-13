#ifndef UFT_ADF_V3_H
#define UFT_ADF_V3_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct adf_disk adf_disk_t;
typedef struct adf_params adf_params_t;
bool adf_parse(const uint8_t* data, size_t size, adf_disk_t* disk, adf_params_t* params);
void adf_disk_free(adf_disk_t* disk);
void adf_get_default_params(adf_params_t* params);
#ifdef __cplusplus
}
#endif
#endif
