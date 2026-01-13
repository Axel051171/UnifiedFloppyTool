/**
 * @file uft_d64_v3.h
 * @brief D64 Format Parser v3 - Public API
 */

#ifndef UFT_D64_V3_H
#define UFT_D64_V3_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Forward Declarations (internal types defined in .c)
 * ═══════════════════════════════════════════════════════════════════════════════ */

typedef struct d64_disk_v3 d64_disk_v3_t;
typedef struct d64_params d64_params_t;
typedef struct d64_diagnosis_list d64_diagnosis_list_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Core Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Parse D64 disk image
 */
bool d64_parse(const uint8_t* data, size_t size, d64_disk_v3_t* disk, d64_params_t* params);

/**
 * @brief Write D64 disk image
 */
uint8_t* d64_write(const d64_disk_v3_t* disk, d64_params_t* params, size_t* out_size);

/**
 * @brief Verify D64 write
 */
bool d64_verify(const uint8_t* original, size_t original_size,
                const uint8_t* written, size_t written_size,
                d64_params_t* params, d64_diagnosis_list_t* differences);

/**
 * @brief Detect copy protection
 */
bool d64_detect_protection(const d64_disk_v3_t* disk, char* protection_name, size_t name_size);

/**
 * @brief Free disk resources
 */
void d64_disk_free(d64_disk_v3_t* disk);

/**
 * @brief Get default parameters
 */
void d64_get_default_params(d64_params_t* params);

/**
 * @brief Get diagnosis text
 */
char* d64_diagnosis_to_text(const d64_diagnosis_list_t* list, const d64_disk_v3_t* disk);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief GCR encode block
 */
void d64_gcr_encode_block(const uint8_t* data, uint8_t* gcr);

/**
 * @brief GCR decode block
 */
bool d64_gcr_decode_block(const uint8_t* gcr, uint8_t* data, uint8_t* errors);

/**
 * @brief File type to string
 */
const char* d64_file_type_str(uint8_t type);

/**
 * @brief Merge sector revolutions
 */
bool d64_merge_sector_revs(const uint8_t** revs, const uint8_t* errors,
                           int rev_count, uint8_t* merged, uint8_t* merge_errors);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_V3_H */
