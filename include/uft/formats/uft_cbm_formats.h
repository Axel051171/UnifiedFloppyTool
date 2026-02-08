/**
 * @file uft_cbm_formats.h
 * @brief Commodore File Format Support (D64/D71/D81/T64/Lynx)
 * 
 * @version 3.8.0
 * @date 2026-01-14
 */

#ifndef UFT_CBM_FORMATS_H
#define UFT_CBM_FORMATS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define UFT_CBM_NAME_LENGTH     16

/* Error codes */
#define UFT_CBM_OK              0
#define UFT_CBM_ERROR           -1
#define UFT_CBM_INVALID         -2
#define UFT_CBM_NOMEM           -3
#define UFT_CBM_IO_ERROR        -4

/* ═══════════════════════════════════════════════════════════════════════════════
 * Data Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief CBM DOS file types
 */
typedef enum {
    UFT_CBM_DEL = 0,    /**< Deleted file */
    UFT_CBM_SEQ = 1,    /**< Sequential file */
    UFT_CBM_PRG = 2,    /**< Program file */
    UFT_CBM_USR = 3,    /**< User file */
    UFT_CBM_REL = 4,    /**< Relative file */
    UFT_CBM_CBM = 5,    /**< CBM partition (1581) */
    UFT_CBM_DIR = 6     /**< Directory (1581) */
} uft_cbm_file_type_t;

/**
 * @brief Disk image type
 */
typedef enum {
    UFT_CBM_IMG_UNKNOWN = 0,
    UFT_CBM_IMG_D64,
    UFT_CBM_IMG_D64_40,
    UFT_CBM_IMG_D71,
    UFT_CBM_IMG_D81,
    UFT_CBM_IMG_T64,
    UFT_CBM_IMG_G64,
    UFT_CBM_IMG_G71
} uft_cbm_image_type_t;

/**
 * @brief CBM file entry
 */
typedef struct uft_cbm_file {
    uint8_t name[UFT_CBM_NAME_LENGTH];
    size_t name_length;
    uft_cbm_file_type_t type;
    uint16_t start_address;
    uint16_t end_address;
    size_t length;
    uint8_t *data;
    uint8_t record_length;
    int start_track;
    int start_sector;
    int block_count;
    struct uft_cbm_file *next;
} uft_cbm_file_t;

/**
 * @brief Disk image context
 */
typedef struct {
    uft_cbm_image_type_t type;
    uint8_t *data;
    size_t size;
    int tracks;
    int sectors_total;
    int dir_track;
    int dir_sector;
    int bam_track;
    int bam_sector;
    uint8_t disk_name[UFT_CBM_NAME_LENGTH];
    uint8_t disk_id[5];
    int blocks_free;
    int files_count;
    uft_cbm_file_t *files;
} uft_cbm_disk_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * Function Declarations
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Detect image type from file size
 */
uft_cbm_image_type_t uft_cbm_detect_type_by_size(size_t size);

/**
 * @brief Get file type string
 */
const char *uft_cbm_file_type_str(uft_cbm_file_type_t type);

/**
 * @brief Allocate CBM file structure
 */
uft_cbm_file_t *uft_cbm_file_alloc(void);

/**
 * @brief Free CBM file structure
 */
void uft_cbm_file_free(uft_cbm_file_t *file);

/**
 * @brief Free file list
 */
void uft_cbm_files_free(uft_cbm_file_t *files);

/**
 * @brief Open disk image file
 */
int uft_cbm_disk_open(uft_cbm_disk_t *disk, const char *filename);

/**
 * @brief Close disk image
 */
void uft_cbm_disk_close(uft_cbm_disk_t *disk);

/**
 * @brief Save disk image to file
 */
int uft_cbm_disk_save(const uft_cbm_disk_t *disk, const char *filename);

/**
 * @brief Create empty D64 image
 */
int uft_cbm_disk_create_d64(uft_cbm_disk_t *disk, const char *disk_name);

/**
 * @brief Read T64 tape image
 */
int uft_cbm_t64_read(const char *filename, uft_cbm_file_t **files);

/**
 * @brief Write T64 tape image
 */
int uft_cbm_t64_write(const char *filename, uft_cbm_file_t *files);

/**
 * @brief Get ASCII filename from CBM file
 */
void uft_cbm_get_ascii_name(char *dest, const uft_cbm_file_t *file, size_t dest_size);

/**
 * @brief Print disk directory
 */
void uft_cbm_print_directory(const uft_cbm_disk_t *disk, FILE *out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_CBM_FORMATS_H */
