/**
 * @file uft_scp_v3.h
 * @brief SCP Format Parser v3 - Public API
 */

#ifndef UFT_SCP_V3_H
#define UFT_SCP_V3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct scp_disk scp_disk_t;
typedef struct scp_params scp_params_t;
typedef struct scp_diagnosis_list scp_diagnosis_list_t;

bool scp_parse(const uint8_t* data, size_t size, scp_disk_t* disk, scp_params_t* params);
uint8_t* scp_write(const scp_disk_t* disk, scp_params_t* params, size_t* out_size);
bool scp_detect_protection(const scp_disk_t* disk, char* protection_name, size_t name_size);
void scp_disk_free(scp_disk_t* disk);
void scp_get_default_params(scp_params_t* params);
char* scp_diagnosis_to_text(const scp_diagnosis_list_t* list, const scp_disk_t* disk);
uint16_t scp_get_expected_bitcell(uint8_t disk_type);
uint8_t scp_get_expected_sectors(uint8_t disk_type, uint8_t track);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SCP_V3_H */
