/**
 * @file uft_simh.h
 * @brief SIMH disc image format support
 * @version 3.9.0
 * 
 * SIMH format used by the SIMH historical computer simulator.
 * Simple raw format with optional geometry specification in filename
 * or configuration file.
 * 
 * Supports various vintage computer disk formats including:
 * - PDP-11 floppy (RX01, RX02)
 * - VAX floppy
 * - Various CP/M formats
 * 
 * Reference: libdsk drvsimh.c
 */

#ifndef UFT_SIMH_H
#define UFT_SIMH_H

#include "uft/core/uft_unified_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* SIMH disc types */
typedef enum {
    SIMH_DISK_UNKNOWN = 0,
    SIMH_DISK_RX01,          /* DEC RX01: 77 cyls, 1 head, 26 sectors, 128 bytes */
    SIMH_DISK_RX02,          /* DEC RX02: 77 cyls, 1 head, 26 sectors, 256 bytes */
    SIMH_DISK_RX50,          /* DEC RX50: 80 cyls, 1 head, 10 sectors, 512 bytes */
    SIMH_DISK_RX33,          /* DEC RX33: 80 cyls, 2 heads, 15 sectors, 512 bytes */
    SIMH_DISK_PC_360K,       /* PC 5.25" DD */
    SIMH_DISK_PC_720K,       /* PC 3.5" DD */
    SIMH_DISK_PC_1200K,      /* PC 5.25" HD */
    SIMH_DISK_PC_1440K,      /* PC 3.5" HD */
    SIMH_DISK_CUSTOM,        /* User-defined geometry */
} simh_disk_type_t;

/**
 * @brief SIMH geometry definition
 */
typedef struct {
    simh_disk_type_t type;
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    const char *name;
} simh_geometry_t;

/**
 * @brief SIMH read options
 */
typedef struct {
    simh_disk_type_t disk_type;  /* Specify type, or UNKNOWN for auto-detect */
    uint16_t cylinders;          /* For CUSTOM type */
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
} simh_read_options_t;

/**
 * @brief SIMH read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    /* Detected/used geometry */
    simh_disk_type_t disk_type;
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    
    /* Statistics */
    size_t image_size;
    
} simh_read_result_t;

/* Predefined geometries */
extern const simh_geometry_t simh_geometries[];
extern const size_t simh_geometry_count;

/* ============================================================================
 * SIMH File I/O
 * ============================================================================ */

/**
 * @brief Read SIMH file
 */
uft_error_t uft_simh_read(const char *path,
                          uft_disk_image_t **out_disk,
                          const simh_read_options_t *opts,
                          simh_read_result_t *result);

/**
 * @brief Read SIMH from memory
 */
uft_error_t uft_simh_read_mem(const uint8_t *data, size_t size,
                              uft_disk_image_t **out_disk,
                              const simh_read_options_t *opts,
                              simh_read_result_t *result);

/**
 * @brief Write SIMH file
 */
uft_error_t uft_simh_write(const uft_disk_image_t *disk,
                           const char *path);

/**
 * @brief Auto-detect disk type from file size
 */
simh_disk_type_t uft_simh_detect_type(size_t file_size);

/**
 * @brief Get geometry for disk type
 */
const simh_geometry_t* uft_simh_get_geometry(simh_disk_type_t type);

/**
 * @brief Initialize read options with defaults
 */
void uft_simh_read_options_init(simh_read_options_t *opts);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SIMH_H */
