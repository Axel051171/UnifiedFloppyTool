/**
 * @file uft_g64_v3.h
 * @brief G64 Format Parser v3 - Public API
 */

#ifndef UFT_G64_V3_H
#define UFT_G64_V3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct g64_disk g64_disk_t;
typedef struct g64_params g64_params_t;
typedef struct g64_diagnosis_list g64_diagnosis_list_t;

bool g64_parse(const uint8_t* data, size_t size, g64_disk_t* disk, g64_params_t* params);
uint8_t* g64_write(const g64_disk_t* disk, g64_params_t* params, size_t* out_size);
bool g64_detect_protection(const g64_disk_t* disk, char* protection_name, size_t name_size);
void g64_disk_free(g64_disk_t* disk);
void g64_get_default_params(g64_params_t* params);
char* g64_diagnosis_to_text(const g64_diagnosis_list_t* list, const g64_disk_t* disk);
uint8_t* g64_export_d64(const g64_disk_t* g64, size_t* out_size);
uint8_t g64_checksum(const uint8_t* data, size_t len);
void g64_gcr_encode_block(const uint8_t* data, uint8_t* gcr);
uint32_t g64_get_bitcell_ns(uint8_t speed_zone);

#ifdef __cplusplus
}
#endif

#endif /* UFT_G64_V3_H */
