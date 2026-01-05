/**
 * @file uft_recovery.h
 * @brief Sector recovery API
 */

#ifndef UFT_DECODER_RECOVERY_H
#define UFT_DECODER_RECOVERY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Recovery pipeline stages
 */
typedef enum {
    RECOVERY_STAGE_PLL = 0,
    RECOVERY_STAGE_SYNC,
    RECOVERY_STAGE_DECODE,
    RECOVERY_STAGE_CRC,
    RECOVERY_STAGE_FUSION,
    RECOVERY_STAGE_VERIFY
} uft_recovery_stage_t;

/**
 * @brief Recovery result
 */
typedef struct {
    bool recovered;             /**< Recovery successful */
    int stages_tried;           /**< Number of stages attempted */
    char method[64];            /**< Recovery method used */
    double final_confidence;    /**< Final confidence (0.0-1.0) */
    uint8_t data[512];          /**< Recovered data */
    size_t data_size;           /**< Size of recovered data */
} uft_recovery_result_t;

/**
 * @brief Get stage name
 */
const char* uft_recovery_stage_name(uft_recovery_stage_t stage);

/**
 * @brief Attempt sector recovery
 */
int uft_recover_sector(const uint32_t* flux, size_t flux_count, 
                       int encoding, uft_recovery_result_t* result);

/**
 * @brief Generate recovery report
 */
void uft_recovery_report(const uft_recovery_result_t* result, 
                         char* buffer, size_t size);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DECODER_RECOVERY_H */
