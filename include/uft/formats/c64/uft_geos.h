/**
 * @file uft_geos.h
 * @brief GEOS Filesystem Support
 * 
 * GEOS-specific structures and file handling:
 * - Parse GEOS file headers (VLIR/SEQ)
 * - Extract GEOS files from D64/D71/D81
 * - Handle GEOS icons and metadata
 * - Convert to/from CVT format
 * 
 * GEOS File Types:
 * - SEQ: Sequential files (like normal C64 files)
 * - VLIR: Variable Length Index Record files
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_GEOS_H
#define UFT_GEOS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** GEOS file type markers */
#define GEOS_TYPE_NON_GEOS      0x00    /**< Not a GEOS file */
#define GEOS_TYPE_BASIC         0x01    /**< BASIC */
#define GEOS_TYPE_ASSEMBLER     0x02    /**< Assembler */
#define GEOS_TYPE_DATA          0x03    /**< Data file */
#define GEOS_TYPE_SYSTEM        0x04    /**< System file */
#define GEOS_TYPE_DESK_ACC      0x05    /**< Desk accessory */
#define GEOS_TYPE_APPLICATION   0x06    /**< Application */
#define GEOS_TYPE_PRINTER       0x07    /**< Printer driver */
#define GEOS_TYPE_INPUT         0x08    /**< Input driver */
#define GEOS_TYPE_DISK          0x09    /**< Disk driver */
#define GEOS_TYPE_BOOT          0x0A    /**< Boot loader */
#define GEOS_TYPE_TEMP          0x0B    /**< Temporary */
#define GEOS_TYPE_AUTO_EXEC     0x0C    /**< Auto-exec */
#define GEOS_TYPE_INPUT_128     0x0D    /**< Input driver (C128) */
#define GEOS_TYPE_NUMERATOR     0x0E    /**< Numerator font */
#define GEOS_TYPE_FONT          0x0F    /**< Font file */

/** GEOS structure types */
#define GEOS_STRUCT_SEQ         0x00    /**< Sequential */
#define GEOS_STRUCT_VLIR        0x01    /**< VLIR */

/** GEOS directory entry offsets */
#define GEOS_DIR_ENTRY_SIZE     30      /**< Extended directory entry */
#define GEOS_ICON_WIDTH         24      /**< Icon width in pixels */
#define GEOS_ICON_HEIGHT        21      /**< Icon height in pixels */
#define GEOS_ICON_SIZE          63      /**< Icon data size (24x21/8 = 63 bytes) */

/** GEOS info block size */
#define GEOS_INFO_BLOCK_SIZE    256     /**< Full info block */

/** CVT format constants */
#define CVT_MAGIC               "PRG formatted GEOS file V1.0"
#define CVT_MAGIC_LEN           28

/** Maximum VLIR records */
#define GEOS_MAX_VLIR_RECORDS   127

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief GEOS timestamp
 */
typedef struct {
    uint8_t     year;               /**< Year (0-99, add 1900) */
    uint8_t     month;              /**< Month (1-12) */
    uint8_t     day;                /**< Day (1-31) */
    uint8_t     hour;               /**< Hour (0-23) */
    uint8_t     minute;             /**< Minute (0-59) */
} geos_timestamp_t;

/**
 * @brief GEOS icon data
 */
typedef struct {
    uint8_t     width;              /**< Width in bytes (3 = 24 pixels) */
    uint8_t     height;             /**< Height in pixels (21) */
    uint8_t     data[63];           /**< Bitmap data */
} geos_icon_t;

/**
 * @brief GEOS file info (from info block)
 */
typedef struct {
    uint8_t     info_id[3];         /**< Info block ID (0x00, 0xFF, 0x00) */
    geos_icon_t icon;               /**< File icon */
    uint8_t     dos_type;           /**< C64 DOS file type */
    uint8_t     geos_type;          /**< GEOS file type */
    uint8_t     structure;          /**< SEQ or VLIR */
    uint16_t    load_address;       /**< Load address */
    uint16_t    end_address;        /**< End address */
    uint16_t    exec_address;       /**< Execution address */
    char        class_name[20];     /**< Class name */
    char        author[20];         /**< Author name */
    char        parent_name[20];    /**< Parent application */
    char        application[20];    /**< Application for this file */
    uint8_t     version[4];         /**< Version (major.minor.patch.build) */
    geos_timestamp_t created;       /**< Creation date */
    geos_timestamp_t modified;      /**< Modification date */
    char        description[96];    /**< File description */
} geos_info_t;

/**
 * @brief VLIR record entry
 */
typedef struct {
    uint8_t     track;              /**< Track (0 = empty, 0xFF = deleted) */
    uint8_t     sector;             /**< Sector or size indicator */
    size_t      size;               /**< Record size in bytes */
    uint8_t     *data;              /**< Record data (optional) */
} geos_vlir_record_t;

/**
 * @brief GEOS file context
 */
typedef struct {
    char        filename[17];       /**< C64 filename */
    geos_info_t info;               /**< GEOS info block */
    bool        is_vlir;            /**< true if VLIR structure */
    int         num_records;        /**< Number of VLIR records */
    geos_vlir_record_t *records;    /**< VLIR records (or NULL for SEQ) */
    uint8_t     *seq_data;          /**< SEQ file data */
    size_t      seq_size;           /**< SEQ file size */
} geos_file_t;

/* ============================================================================
 * API Functions - GEOS Detection
 * ============================================================================ */

/**
 * @brief Check if directory entry is GEOS file
 * @param dir_entry 32-byte directory entry
 * @return true if GEOS file
 */
bool geos_is_geos_file(const uint8_t *dir_entry);

/**
 * @brief Get GEOS file type name
 * @param type GEOS file type code
 * @return Type name string
 */
const char *geos_type_name(uint8_t type);

/**
 * @brief Get GEOS structure name
 * @param structure Structure type (SEQ/VLIR)
 * @return Structure name string
 */
const char *geos_structure_name(uint8_t structure);

/* ============================================================================
 * API Functions - Info Block
 * ============================================================================ */

/**
 * @brief Parse GEOS info block
 * @param data Info block data (256 bytes)
 * @param info Output info structure
 * @return 0 on success
 */
int geos_parse_info(const uint8_t *data, geos_info_t *info);

/**
 * @brief Write GEOS info block
 * @param info Info structure
 * @param data Output buffer (256 bytes)
 * @return 0 on success
 */
int geos_write_info(const geos_info_t *info, uint8_t *data);

/**
 * @brief Format timestamp to string
 * @param ts Timestamp
 * @param buffer Output buffer (20+ bytes)
 */
void geos_format_timestamp(const geos_timestamp_t *ts, char *buffer);

/* ============================================================================
 * API Functions - VLIR Handling
 * ============================================================================ */

/**
 * @brief Parse VLIR index
 * @param data VLIR index sector (254 bytes of t/s pairs)
 * @param records Output records array (127 max)
 * @param num_records Output: number of records
 * @return 0 on success
 */
int geos_parse_vlir_index(const uint8_t *data, geos_vlir_record_t *records,
                          int *num_records);

/**
 * @brief Write VLIR index
 * @param records Records array
 * @param num_records Number of records
 * @param data Output buffer (254 bytes)
 * @return 0 on success
 */
int geos_write_vlir_index(const geos_vlir_record_t *records, int num_records,
                          uint8_t *data);

/**
 * @brief Check if VLIR record is empty
 * @param record VLIR record
 * @return true if empty
 */
bool geos_vlir_record_empty(const geos_vlir_record_t *record);

/**
 * @brief Check if VLIR record is deleted
 * @param record VLIR record
 * @return true if deleted
 */
bool geos_vlir_record_deleted(const geos_vlir_record_t *record);

/* ============================================================================
 * API Functions - GEOS File Operations
 * ============================================================================ */

/**
 * @brief Create GEOS file structure
 * @param filename C64 filename
 * @param type GEOS file type
 * @param is_vlir true for VLIR, false for SEQ
 * @param file Output file structure
 * @return 0 on success
 */
int geos_file_create(const char *filename, uint8_t type, bool is_vlir,
                     geos_file_t *file);

/**
 * @brief Free GEOS file structure
 * @param file File to free
 */
void geos_file_free(geos_file_t *file);

/**
 * @brief Set GEOS file icon
 * @param file GEOS file
 * @param icon_data Icon bitmap (63 bytes)
 */
void geos_file_set_icon(geos_file_t *file, const uint8_t *icon_data);

/**
 * @brief Set GEOS file description
 * @param file GEOS file
 * @param class_name Class name
 * @param author Author
 * @param description Description text
 */
void geos_file_set_description(geos_file_t *file, const char *class_name,
                               const char *author, const char *description);

/* ============================================================================
 * API Functions - CVT Format
 * ============================================================================ */

/**
 * @brief Detect CVT format
 * @param data File data
 * @param size Data size
 * @return true if CVT
 */
bool geos_cvt_detect(const uint8_t *data, size_t size);

/**
 * @brief Parse CVT file
 * @param data CVT data
 * @param size Data size
 * @param file Output GEOS file
 * @return 0 on success
 */
int geos_cvt_parse(const uint8_t *data, size_t size, geos_file_t *file);

/**
 * @brief Create CVT from GEOS file
 * @param file GEOS file
 * @param cvt_data Output buffer
 * @param max_size Maximum buffer size
 * @param cvt_size Output: CVT size
 * @return 0 on success
 */
int geos_cvt_create(const geos_file_t *file, uint8_t *cvt_data,
                    size_t max_size, size_t *cvt_size);

/**
 * @brief Load CVT from file
 * @param filename File path
 * @param file Output GEOS file
 * @return 0 on success
 */
int geos_cvt_load(const char *filename, geos_file_t *file);

/**
 * @brief Save as CVT file
 * @param file GEOS file
 * @param filename Output path
 * @return 0 on success
 */
int geos_cvt_save(const geos_file_t *file, const char *filename);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print GEOS file info
 * @param file GEOS file
 * @param fp Output file
 */
void geos_print_info(const geos_file_t *file, FILE *fp);

/**
 * @brief Print GEOS icon as ASCII art
 * @param icon Icon data
 * @param fp Output file
 */
void geos_print_icon(const geos_icon_t *icon, FILE *fp);

/**
 * @brief Get default icon for file type
 * @param type GEOS file type
 * @param icon Output icon
 */
void geos_get_default_icon(uint8_t type, geos_icon_t *icon);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GEOS_H */
