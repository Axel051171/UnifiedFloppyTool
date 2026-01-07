// SPDX-License-Identifier: MIT
/*
 * unified_api.h - Unified Floppy Preservation API Header
 * 
 * THE ULTIMATE API - One API to rule them all!
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#ifndef UNIFIED_API_H
#define UNIFIED_API_H

#include <stdint.h>
#include <stddef.h>

/* Include sub-APIs */
#include "parameter_compensation.h"
#include "uft_sam_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * HARDWARE TYPES
 *============================================================================*/

typedef enum {
    HW_TYPE_NONE,
    HW_TYPE_KRYOFLUX,
    HW_TYPE_FLUXENGINE,
    HW_TYPE_APPLESAUCE,
    HW_TYPE_XUM1541,
    HW_TYPE_HXC,
    HW_TYPE_ZOOMFLOPPY
} hardware_type_t;

/*============================================================================*
 * UNIFIED HANDLE
 *============================================================================*/

typedef struct unified_handle_t unified_handle_t;

/*============================================================================*
 * INITIALIZATION
 *============================================================================*/

/**
 * @brief Initialize unified system
 * 
 * @param handle_out Handle (output)
 * @param hw_type Hardware type (HW_TYPE_NONE for software-only)
 * @return 0 on success
 */
int unified_init(unified_handle_t **handle_out, hardware_type_t hw_type);

/**
 * @brief Close unified system
 * 
 * @param handle Handle
 */
void unified_close(unified_handle_t *handle);

/*============================================================================*
 * HARDWARE OPERATIONS
 *============================================================================*/

/**
 * @brief Connect to hardware
 * 
 * @param handle Handle
 * @return 0 on success
 */
int unified_connect_hardware(unified_handle_t *handle);

/**
 * @brief Seek to track
 * 
 * @param handle Handle
 * @param track Track number
 * @return 0 on success
 */
int unified_seek(unified_handle_t *handle, int track);

/**
 * @brief Read flux data from hardware
 * 
 * @param handle Handle
 * @param track Track number
 * @param side Side number
 * @param flux_data_out Flux data (allocated by function)
 * @param flux_len_out Flux length
 * @return 0 on success
 */
int unified_read_flux(
    unified_handle_t *handle,
    int track,
    int side,
    uint8_t **flux_data_out,
    size_t *flux_len_out
);

/*============================================================================*
 * FORMAT OPERATIONS
 *============================================================================*/

/**
 * @brief Read disk image from file
 * 
 * @param handle Handle
 * @param filename File path
 * @param format Format name (NULL for auto-detect)
 * @return 0 on success
 */
int unified_read_image(
    unified_handle_t *handle,
    const char *filename,
    const char *format
);

/**
 * @brief Write disk image to file
 * 
 * @param handle Handle
 * @param filename File path
 * @param format Format name
 * @return 0 on success
 */
int unified_write_image(
    unified_handle_t *handle,
    const char *filename,
    const char *format
);

/*============================================================================*
 * HIGH-LEVEL OPERATIONS
 *============================================================================*/

/**
 * @brief Convert any format to any format
 * 
 * THE ULTIMATE FUNCTION!
 * 
 * @param input_file Input file path
 * @param output_file Output file path
 * @param output_format Output format name
 * @return 0 on success
 * 
 * Example:
 *   unified_convert("disk.kf", "disk.d64", "d64");
 */
int unified_convert(
    const char *input_file,
    const char *output_file,
    const char *output_format
);

/**
 * @brief Read from hardware and save to file
 * 
 * @param handle Handle
 * @param output_file Output file path
 * @param output_format Output format name
 * @param start_track Start track
 * @param end_track End track
 * @return 0 on success
 */
int unified_read_disk_to_file(
    unified_handle_t *handle,
    const char *output_file,
    const char *output_format,
    int start_track,
    int end_track
);

/*============================================================================*
 * COMPENSATION CONTROL
 *============================================================================*/

/**
 * @brief Set compensation mode
 * 
 * @param handle Handle
 * @param mode Compensation mode
 * @return 0 on success
 */
int unified_set_compensation_mode(
    unified_handle_t *handle,
    compensation_mode_t mode
);

/**
 * @brief Get compensation mode
 * 
 * @param handle Handle
 * @return Compensation mode
 */
compensation_mode_t unified_get_compensation_mode(unified_handle_t *handle);

/*============================================================================*
 * INFORMATION
 *============================================================================*/

/**
 * @brief List supported formats
 * 
 * @param handle Handle
 * @param formats_out Format array (allocated by function)
 * @param count_out Number of formats
 * @return 0 on success
 */
int unified_list_formats(
    unified_handle_t *handle,
    uft_sam_format_info_t **formats_out,
    int *count_out
);

/**
 * @brief Detect hardware type
 * 
 * @param hw_type_out Hardware type (output)
 * @return 0 on success
 */
int unified_detect_hardware(hardware_type_t *hw_type_out);

/**
 * @brief Get hardware type name
 * 
 * @param hw_type Hardware type
 * @return Hardware name string
 */
const char* unified_get_hardware_name(hardware_type_t hw_type);

/**
 * @brief Get statistics
 * 
 * @param handle Handle
 * @param flux_bytes_read Flux bytes read (output, optional)
 * @param flux_bytes_written Flux bytes written (output, optional)
 * @param conversions_done Conversions done (output, optional)
 */
void unified_get_stats(
    unified_handle_t *handle,
    unsigned long *flux_bytes_read,
    unsigned long *flux_bytes_written,
    unsigned long *conversions_done
);

#ifdef __cplusplus
}
#endif

#endif /* UNIFIED_API_H */
