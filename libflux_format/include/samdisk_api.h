// SPDX-License-Identifier: MIT
/*
 * samdisk_api.h - SAMdisk C API Header
 * 
 * C API for SAMdisk format engine providing support for 57+ disk formats.
 * 
 * @version 2.8.0
 * @date 2024-12-26
 */

#ifndef SAMDISK_API_H
#define SAMDISK_API_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================*
 * TYPES
 *============================================================================*/

/* Opaque engine handle */
typedef struct samdisk_engine_t samdisk_engine_t;

/* Opaque disk handle */
typedef struct samdisk_disk_t samdisk_disk_t;

/* Format types */
#define SAMDISK_FORMAT_UNKNOWN      0
#define SAMDISK_FORMAT_FLUX         1
#define SAMDISK_FORMAT_SECTOR       2
#define SAMDISK_FORMAT_TRACK        3

/* Format information */
typedef struct samdisk_format_info_t {
    const char* name;
    const char* extension;
    const char* description;
    int type;
    int can_read;
    int can_write;
} samdisk_format_info_t;

/*============================================================================*
 * ENGINE MANAGEMENT
 *============================================================================*/

/**
 * @brief Initialize SAMdisk engine
 * 
 * @param engine_out Engine handle (output)
 * @return 0 on success
 */
int samdisk_init(samdisk_engine_t** engine_out);

/**
 * @brief Close SAMdisk engine
 * 
 * @param engine Engine handle
 */
void samdisk_close(samdisk_engine_t* engine);

/*============================================================================*
 * FORMAT DETECTION
 *============================================================================*/

/**
 * @brief Auto-detect format from data
 * 
 * @param engine Engine handle
 * @param data Data buffer
 * @param len Data length
 * @param format_name_out Format name (allocated by function, must be freed)
 * @return 0 on success
 */
int samdisk_detect_format(
    samdisk_engine_t* engine,
    const uint8_t* data,
    size_t len,
    char** format_name_out
);

/**
 * @brief Auto-detect format from file
 * 
 * @param engine Engine handle
 * @param filename File path
 * @param format_name_out Format name (allocated by function, must be freed)
 * @return 0 on success
 */
int samdisk_detect_format_file(
    samdisk_engine_t* engine,
    const char* filename,
    char** format_name_out
);

/*============================================================================*
 * DISK IMAGE I/O
 *============================================================================*/

/**
 * @brief Read disk image from file
 * 
 * @param engine Engine handle
 * @param filename File path
 * @param format Format name (NULL for auto-detect)
 * @param disk_out Disk handle (output)
 * @return 0 on success
 */
int samdisk_read_image(
    samdisk_engine_t* engine,
    const char* filename,
    const char* format,
    samdisk_disk_t** disk_out
);

/**
 * @brief Write disk image to file
 * 
 * @param engine Engine handle
 * @param disk Disk handle
 * @param filename File path
 * @param format Format name
 * @return 0 on success
 */
int samdisk_write_image(
    samdisk_engine_t* engine,
    samdisk_disk_t* disk,
    const char* filename,
    const char* format
);

/**
 * @brief Free disk handle
 * 
 * @param disk Disk handle
 */
void samdisk_free_disk(samdisk_disk_t* disk);

/*============================================================================*
 * CONVERSION
 *============================================================================*/

/**
 * @brief Convert between formats
 * 
 * @param engine Engine handle
 * @param input_file Input file path
 * @param input_format Input format (NULL for auto-detect)
 * @param output_file Output file path
 * @param output_format Output format
 * @return 0 on success
 */
int samdisk_convert(
    samdisk_engine_t* engine,
    const char* input_file,
    const char* input_format,
    const char* output_file,
    const char* output_format
);

/*============================================================================*
 * FORMAT INFORMATION
 *============================================================================*/

/**
 * @brief List all supported formats
 * 
 * @param engine Engine handle
 * @param formats_out Format array (allocated by function)
 * @param count_out Number of formats
 * @return 0 on success
 */
int samdisk_list_formats(
    samdisk_engine_t* engine,
    samdisk_format_info_t** formats_out,
    int* count_out
);

/**
 * @brief Free format list
 * 
 * @param formats Format array
 */
void samdisk_free_formats(samdisk_format_info_t* formats);

/**
 * @brief Get format info by name
 * 
 * @param engine Engine handle
 * @param format_name Format name
 * @param info_out Format info (output)
 * @return 0 on success
 */
int samdisk_get_format_info(
    samdisk_engine_t* engine,
    const char* format_name,
    samdisk_format_info_t* info_out
);

/*============================================================================*
 * DISK INFO
 *============================================================================*/

/**
 * @brief Get disk geometry
 * 
 * @param disk Disk handle
 * @param tracks_out Number of tracks (output)
 * @param sides_out Number of sides (output)
 * @return 0 on success
 */
int samdisk_get_geometry(
    samdisk_disk_t* disk,
    int* tracks_out,
    int* sides_out
);

/**
 * @brief Get disk format name
 * 
 * @param disk Disk handle
 * @return Format name string (do not free)
 */
const char* samdisk_get_format_name(samdisk_disk_t* disk);

/**
 * @brief Read sector data
 * 
 * @param disk Disk handle
 * @param track Track number
 * @param side Side number
 * @param sector_id Sector ID
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @param bytes_read_out Bytes read (output)
 * @return 0 on success
 */
int samdisk_read_sector(
    samdisk_disk_t* disk,
    int track,
    int side,
    int sector_id,
    uint8_t* buffer,
    size_t buffer_size,
    size_t* bytes_read_out
);

/**
 * @brief Write sector data
 * 
 * @param disk Disk handle
 * @param track Track number
 * @param side Side number
 * @param sector_id Sector ID
 * @param data Sector data
 * @param data_size Data size
 * @return 0 on success
 */
int samdisk_write_sector(
    samdisk_disk_t* disk,
    int track,
    int side,
    int sector_id,
    const uint8_t* data,
    size_t data_size
);

#ifdef __cplusplus
}
#endif

#endif /* SAMDISK_API_H */
