/**
 * @file uft_operations.h
 * @brief High-Level API für Tool-unabhängige Operationen
 * 
 * Diese API ist STABIL und abwärtskompatibel.
 */

#ifndef UFT_OPERATIONS_H
#define UFT_OPERATIONS_H

#include "uft_unified_image.h"
#include "uft_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Version
// ============================================================================

#define UFT_OPS_API_VERSION_MAJOR   1
#define UFT_OPS_API_VERSION_MINOR   0

bool uft_ops_api_compatible(uint32_t client_version);

// ============================================================================
// Simple Operations (Auto-select tools)
// ============================================================================

/**
 * @brief Read disk from hardware
 */
uft_error_t uft_read_disk(int device_id,
                           const uft_tool_read_params_t* params,
                           uft_unified_image_t* output);

/**
 * @brief Write disk to hardware
 */
uft_error_t uft_write_disk(int device_id,
                            const uft_tool_write_params_t* params,
                            const uft_unified_image_t* input);

/**
 * @brief Open image file (auto-detect format)
 */
uft_error_t uft_open_image(const char* path, uft_unified_image_t* output);

/**
 * @brief Save image file
 */
uft_error_t uft_save_image(const uft_unified_image_t* image,
                            const char* path,
                            uft_format_t format);

/**
 * @brief Convert image format
 */
uft_error_t uft_convert_image(const uft_unified_image_t* input,
                               uft_format_t target_format,
                               uft_unified_image_t* output);

// ============================================================================
// Device Management
// ============================================================================

typedef struct uft_device_info {
    int         index;
    char        name[64];
    char        port[32];
    char        firmware[16];
    uint32_t    capabilities;
    bool        connected;
} uft_device_info_t;

uft_error_t uft_scan_devices(uft_device_info_t* devices, size_t max_devices, size_t* actual_count);
uft_error_t uft_select_device(int device_index);

// ============================================================================
// Format Detection
// ============================================================================

uft_error_t uft_detect_format(const char* path, uft_format_t* format, int* confidence);
uft_error_t uft_detect_format_from_data(const uint8_t* data, size_t size, uft_format_t* format, int* confidence);

#ifdef __cplusplus
}
#endif

#endif // UFT_OPERATIONS_H
