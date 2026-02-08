/**
 * @file uft_reu.h
 * @brief REU and GeoRAM Image Support
 * 
 * RAM expansion image handling for C64:
 * - REU (RAM Expansion Unit): 1700/1764/1750 (128K/256K/512K)
 * - GeoRAM: 512K-4MB
 * - BBGRAM: 64K-128K
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_REU_H
#define UFT_REU_H

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

/** REU sizes */
#define REU_SIZE_1700           (128 * 1024)    /**< 128KB (1700) */
#define REU_SIZE_1764           (256 * 1024)    /**< 256KB (1764) */
#define REU_SIZE_1750           (512 * 1024)    /**< 512KB (1750) */
#define REU_SIZE_1MB            (1024 * 1024)   /**< 1MB (SuperCPU) */
#define REU_SIZE_2MB            (2 * 1024 * 1024)
#define REU_SIZE_4MB            (4 * 1024 * 1024)
#define REU_SIZE_8MB            (8 * 1024 * 1024)
#define REU_SIZE_16MB           (16 * 1024 * 1024)

/** GeoRAM sizes */
#define GEORAM_SIZE_512K        (512 * 1024)
#define GEORAM_SIZE_1MB         (1024 * 1024)
#define GEORAM_SIZE_2MB         (2 * 1024 * 1024)
#define GEORAM_SIZE_4MB         (4 * 1024 * 1024)

/** Page sizes */
#define REU_PAGE_SIZE           256             /**< REU page size */
#define GEORAM_PAGE_SIZE        256             /**< GeoRAM page size */
#define GEORAM_BLOCK_SIZE       16384           /**< GeoRAM block (64 pages) */

/** REU types */
typedef enum {
    REU_TYPE_1700   = 0,        /**< 1700 (128KB) */
    REU_TYPE_1764   = 1,        /**< 1764 (256KB) */
    REU_TYPE_1750   = 2,        /**< 1750 (512KB) */
    REU_TYPE_1MB    = 3,        /**< 1MB extension */
    REU_TYPE_2MB    = 4,        /**< 2MB extension */
    REU_TYPE_4MB    = 5,        /**< 4MB extension */
    REU_TYPE_8MB    = 6,        /**< 8MB extension */
    REU_TYPE_16MB   = 7,        /**< 16MB extension */
    REU_TYPE_GEORAM = 16,       /**< GeoRAM */
    REU_TYPE_BBGRAM = 17,       /**< BBGRam */
    REU_TYPE_UNKNOWN = 255
} reu_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief REU image info
 */
typedef struct {
    reu_type_t  type;               /**< REU type */
    size_t      size;               /**< Image size */
    size_t      num_pages;          /**< Number of 256-byte pages */
    size_t      num_banks;          /**< Number of 64KB banks */
    bool        is_georam;          /**< true if GeoRAM format */
} reu_info_t;

/**
 * @brief REU image context
 */
typedef struct {
    uint8_t     *data;              /**< RAM data */
    size_t      size;               /**< Total size */
    reu_type_t  type;               /**< REU type */
    bool        owns_data;          /**< true if we allocated */
    bool        modified;           /**< Modified flag */
} reu_image_t;

/**
 * @brief GeoRAM state
 */
typedef struct {
    uint8_t     block;              /**< Current block (0-63 for 1MB) */
    uint8_t     page;               /**< Current page (0-255) */
} georam_state_t;

/* ============================================================================
 * API Functions - Image Management
 * ============================================================================ */

/**
 * @brief Create new REU image
 * @param type REU type
 * @param image Output image
 * @return 0 on success
 */
int reu_create(reu_type_t type, reu_image_t *image);

/**
 * @brief Create REU image with specific size
 * @param size Size in bytes
 * @param image Output image
 * @return 0 on success
 */
int reu_create_sized(size_t size, reu_image_t *image);

/**
 * @brief Open REU image from data
 * @param data REU data
 * @param size Data size
 * @param image Output image
 * @return 0 on success
 */
int reu_open(const uint8_t *data, size_t size, reu_image_t *image);

/**
 * @brief Load REU from file
 * @param filename File path
 * @param image Output image
 * @return 0 on success
 */
int reu_load(const char *filename, reu_image_t *image);

/**
 * @brief Save REU to file
 * @param image REU image
 * @param filename Output path
 * @return 0 on success
 */
int reu_save(const reu_image_t *image, const char *filename);

/**
 * @brief Close REU image
 * @param image Image to close
 */
void reu_close(reu_image_t *image);

/* ============================================================================
 * API Functions - REU Info
 * ============================================================================ */

/**
 * @brief Get REU info
 * @param image REU image
 * @param info Output info
 * @return 0 on success
 */
int reu_get_info(const reu_image_t *image, reu_info_t *info);

/**
 * @brief Get REU type name
 * @param type REU type
 * @return Type name
 */
const char *reu_type_name(reu_type_t type);

/**
 * @brief Detect REU type from size
 * @param size Image size
 * @return REU type
 */
reu_type_t reu_detect_type(size_t size);

/**
 * @brief Get size for REU type
 * @param type REU type
 * @return Size in bytes
 */
size_t reu_type_size(reu_type_t type);

/* ============================================================================
 * API Functions - Memory Access
 * ============================================================================ */

/**
 * @brief Read byte from REU
 * @param image REU image
 * @param address Address (0 to size-1)
 * @return Byte value
 */
uint8_t reu_read_byte(const reu_image_t *image, uint32_t address);

/**
 * @brief Write byte to REU
 * @param image REU image
 * @param address Address
 * @param value Value to write
 */
void reu_write_byte(reu_image_t *image, uint32_t address, uint8_t value);

/**
 * @brief Read block from REU
 * @param image REU image
 * @param address Start address
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return Bytes read
 */
size_t reu_read_block(const reu_image_t *image, uint32_t address,
                      uint8_t *buffer, size_t size);

/**
 * @brief Write block to REU
 * @param image REU image
 * @param address Start address
 * @param buffer Data to write
 * @param size Bytes to write
 * @return Bytes written
 */
size_t reu_write_block(reu_image_t *image, uint32_t address,
                       const uint8_t *buffer, size_t size);

/**
 * @brief Read page (256 bytes)
 * @param image REU image
 * @param bank Bank number
 * @param page Page within bank
 * @param buffer Output buffer (256 bytes)
 * @return 0 on success
 */
int reu_read_page(const reu_image_t *image, int bank, int page, uint8_t *buffer);

/**
 * @brief Write page (256 bytes)
 * @param image REU image
 * @param bank Bank number
 * @param page Page within bank
 * @param buffer Input buffer (256 bytes)
 * @return 0 on success
 */
int reu_write_page(reu_image_t *image, int bank, int page, const uint8_t *buffer);

/* ============================================================================
 * API Functions - GeoRAM Emulation
 * ============================================================================ */

/**
 * @brief Create GeoRAM image
 * @param size Size in bytes (512K-4MB)
 * @param image Output image
 * @return 0 on success
 */
int georam_create(size_t size, reu_image_t *image);

/**
 * @brief Read from GeoRAM window
 * @param image REU/GeoRAM image
 * @param state GeoRAM state (block/page)
 * @param offset Offset within page (0-255)
 * @return Byte value
 */
uint8_t georam_read(const reu_image_t *image, const georam_state_t *state,
                    uint8_t offset);

/**
 * @brief Write to GeoRAM window
 * @param image REU/GeoRAM image
 * @param state GeoRAM state
 * @param offset Offset within page
 * @param value Value to write
 */
void georam_write(reu_image_t *image, const georam_state_t *state,
                  uint8_t offset, uint8_t value);

/**
 * @brief Set GeoRAM block register
 * @param state GeoRAM state
 * @param block Block number
 */
void georam_set_block(georam_state_t *state, uint8_t block);

/**
 * @brief Set GeoRAM page register
 * @param state GeoRAM state
 * @param page Page number
 */
void georam_set_page(georam_state_t *state, uint8_t page);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Fill REU with pattern
 * @param image REU image
 * @param pattern Fill pattern
 */
void reu_fill(reu_image_t *image, uint8_t pattern);

/**
 * @brief Clear REU (fill with 0)
 * @param image REU image
 */
void reu_clear(reu_image_t *image);

/**
 * @brief Compare REU contents
 * @param image1 First image
 * @param image2 Second image
 * @return 0 if equal, non-zero if different
 */
int reu_compare(const reu_image_t *image1, const reu_image_t *image2);

/**
 * @brief Print REU info
 * @param image REU image
 * @param fp Output file
 */
void reu_print_info(const reu_image_t *image, FILE *fp);

/**
 * @brief Dump REU memory map
 * @param image REU image
 * @param fp Output file
 * @param start Start address
 * @param length Bytes to dump
 */
void reu_dump(const reu_image_t *image, FILE *fp, uint32_t start, size_t length);

#ifdef __cplusplus
}
#endif

#endif /* UFT_REU_H */
