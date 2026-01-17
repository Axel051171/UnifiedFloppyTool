/**
 * @file uft_pce.h
 * @brief NEC TurboGrafx-16 / PC Engine ROM Support
 * 
 * Support for NEC/Hudson console ROM formats:
 * - PC Engine HuCard (.pce)
 * - TurboGrafx-16 HuCard
 * - SuperGrafx (.sgx)
 * - PC Engine CD-ROM System (header only)
 * 
 * Features:
 * - ROM size detection and validation
 * - Header parsing (where available)
 * - Region detection (Japan/US)
 * - SuperGrafx detection
 * - Checksum calculation
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_PCE_H
#define UFT_PCE_H

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

/** Standard ROM sizes */
#define PCE_SIZE_256K           (256 * 1024)
#define PCE_SIZE_384K           (384 * 1024)
#define PCE_SIZE_512K           (512 * 1024)
#define PCE_SIZE_768K           (768 * 1024)
#define PCE_SIZE_1M             (1024 * 1024)
#define PCE_SIZE_2M             (2 * 1024 * 1024)

/** Header location (some ROMs) */
#define PCE_HEADER_OFFSET       0x1FFF0
#define PCE_HEADER_SIZE         16

/** ROM types */
typedef enum {
    PCE_TYPE_UNKNOWN        = 0,
    PCE_TYPE_HUCARD         = 1,    /**< Standard HuCard */
    PCE_TYPE_SUPERGRAFX     = 2,    /**< SuperGrafx only */
    PCE_TYPE_POPULOUS       = 3,    /**< Populous (special mapper) */
    PCE_TYPE_SF2            = 4,    /**< Street Fighter II (20Mbit) */
    PCE_TYPE_CDROM          = 5     /**< CD-ROM System */
} pce_type_t;

/** Region */
typedef enum {
    PCE_REGION_UNKNOWN      = 0,
    PCE_REGION_JAPAN        = 1,    /**< PC Engine (Japan) */
    PCE_REGION_USA          = 2     /**< TurboGrafx-16 (USA) */
} pce_region_t;

/** Mapper types */
typedef enum {
    PCE_MAPPER_NONE         = 0,    /**< No mapper (up to 1MB) */
    PCE_MAPPER_SF2          = 1,    /**< Street Fighter II */
    PCE_MAPPER_POPULOUS     = 2     /**< Populous */
} pce_mapper_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief PCE ROM info
 */
typedef struct {
    pce_type_t  type;               /**< ROM type */
    pce_region_t region;            /**< Region */
    pce_mapper_t mapper;            /**< Mapper type */
    size_t      file_size;          /**< File size */
    size_t      rom_size;           /**< ROM size (power of 2) */
    bool        has_header;         /**< Has 512-byte header */
    uint32_t    crc32;              /**< CRC32 checksum */
    char        title[64];          /**< Title (if detectable) */
} pce_info_t;

/**
 * @brief PCE ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    pce_type_t  type;               /**< ROM type */
    bool        has_header;         /**< Has 512-byte header */
    bool        owns_data;          /**< true if we allocated */
} pce_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect ROM type
 * @param data ROM data
 * @param size Data size
 * @return ROM type
 */
pce_type_t pce_detect_type(const uint8_t *data, size_t size);

/**
 * @brief Check for 512-byte header
 * @param data ROM data
 * @param size Data size
 * @return true if header present
 */
bool pce_has_header(const uint8_t *data, size_t size);

/**
 * @brief Detect region from ROM
 * @param data ROM data
 * @param size Data size
 * @return Region
 */
pce_region_t pce_detect_region(const uint8_t *data, size_t size);

/**
 * @brief Get type name
 * @param type ROM type
 * @return Type name
 */
const char *pce_type_name(pce_type_t type);

/**
 * @brief Get region name
 * @param region Region
 * @return Region name
 */
const char *pce_region_name(pce_region_t region);

/**
 * @brief Validate PCE ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid
 */
bool pce_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open PCE ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int pce_open(const uint8_t *data, size_t size, pce_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int pce_load(const char *filename, pce_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void pce_close(pce_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int pce_get_info(const pce_rom_t *rom, pce_info_t *info);

/**
 * @brief Get ROM data without header
 * @param rom ROM context
 * @return Pointer to ROM data
 */
const uint8_t *pce_get_rom_data(const pce_rom_t *rom);

/**
 * @brief Get ROM size without header
 * @param rom ROM context
 * @return ROM size
 */
size_t pce_get_rom_size(const pce_rom_t *rom);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Calculate CRC32
 * @param data ROM data
 * @param size Data size
 * @return CRC32 value
 */
uint32_t pce_calc_crc32(const uint8_t *data, size_t size);

/**
 * @brief Strip 512-byte header if present
 * @param rom ROM context
 * @return 0 on success
 */
int pce_strip_header(pce_rom_t *rom);

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void pce_print_info(const pce_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_PCE_H */
