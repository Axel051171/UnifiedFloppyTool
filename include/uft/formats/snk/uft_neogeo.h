/**
 * @file uft_neogeo.h
 * @brief SNK Neo Geo ROM Support
 * 
 * Support for SNK Neo Geo formats:
 * - AES (Home Console) ROMs
 * - MVS (Arcade) ROMs
 * - Neo Geo CD images
 * - .neo format (modern container)
 * 
 * Features:
 * - Multi-chip ROM set handling
 * - P-ROM, S-ROM, M-ROM, V-ROM, C-ROM identification
 * - Header parsing for .neo format
 * - NGH number extraction
 * - Region detection
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_NEOGEO_H
#define UFT_NEOGEO_H

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

/** NEO format magic */
#define NEO_MAGIC               "NEO\x01"
#define NEO_MAGIC_SIZE          4
#define NEO_HEADER_SIZE         512

/** ROM chip types */
typedef enum {
    NEO_ROM_P               = 0,    /**< Program ROM (68000 code) */
    NEO_ROM_S               = 1,    /**< Fix/Sprite ROM (text layer) */
    NEO_ROM_M               = 2,    /**< Music ROM (Z80 code) */
    NEO_ROM_V               = 3,    /**< Voice ROM (ADPCM samples) */
    NEO_ROM_C               = 4     /**< Character ROM (sprites) */
} neogeo_rom_type_t;

/** System type */
typedef enum {
    NEO_SYSTEM_UNKNOWN      = 0,
    NEO_SYSTEM_MVS          = 1,    /**< Arcade (Multi Video System) */
    NEO_SYSTEM_AES          = 2,    /**< Home console */
    NEO_SYSTEM_CD           = 3,    /**< Neo Geo CD */
    NEO_SYSTEM_CDZ          = 4     /**< Neo Geo CDZ */
} neogeo_system_t;

/** Region */
typedef enum {
    NEO_REGION_UNKNOWN      = 0,
    NEO_REGION_JAPAN        = 1,
    NEO_REGION_USA          = 2,
    NEO_REGION_EUROPE       = 3,
    NEO_REGION_ASIA         = 4
} neogeo_region_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief NEO format header (512 bytes)
 */
typedef struct {
    char        magic[4];           /**< "NEO\x01" */
    uint32_t    p_rom_size;         /**< P-ROM size */
    uint32_t    s_rom_size;         /**< S-ROM size */
    uint32_t    m_rom_size;         /**< M-ROM size */
    uint32_t    v_rom_size;         /**< V-ROM size */
    uint32_t    c_rom_size;         /**< C-ROM size */
    uint32_t    year;               /**< Release year */
    uint32_t    genre;              /**< Game genre */
    uint32_t    screenshot;         /**< Screenshot flag */
    uint32_t    ngh;                /**< NGH number */
    char        name[33];           /**< Game name */
    char        manufacturer[17];   /**< Manufacturer */
    uint8_t     reserved[427];      /**< Reserved */
} neo_header_t;

/**
 * @brief ROM chip info
 */
typedef struct {
    neogeo_rom_type_t type;         /**< ROM type */
    size_t      size;               /**< ROM size */
    size_t      offset;             /**< Offset in file */
    uint32_t    crc32;              /**< CRC32 */
} neogeo_chip_t;

/**
 * @brief Neo Geo ROM info
 */
typedef struct {
    neogeo_system_t system;         /**< System type */
    neogeo_region_t region;         /**< Region */
    bool        is_neo_format;      /**< Is .neo container */
    char        name[64];           /**< Game name */
    char        manufacturer[32];   /**< Manufacturer */
    uint32_t    ngh;                /**< NGH number */
    uint32_t    year;               /**< Release year */
    size_t      total_size;         /**< Total ROM size */
    size_t      p_size;             /**< P-ROM size */
    size_t      s_size;             /**< S-ROM size */
    size_t      m_size;             /**< M-ROM size */
    size_t      v_size;             /**< V-ROM size */
    size_t      c_size;             /**< C-ROM size */
} neogeo_info_t;

/**
 * @brief Neo Geo ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Total size */
    bool        is_neo_format;      /**< Is .neo container */
    neo_header_t header;            /**< NEO header (if applicable) */
    size_t      p_offset;           /**< P-ROM offset */
    size_t      s_offset;           /**< S-ROM offset */
    size_t      m_offset;           /**< M-ROM offset */
    size_t      v_offset;           /**< V-ROM offset */
    size_t      c_offset;           /**< C-ROM offset */
    bool        owns_data;          /**< true if we allocated */
} neogeo_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if data is NEO format
 * @param data ROM data
 * @param size Data size
 * @return true if NEO format
 */
bool neogeo_is_neo_format(const uint8_t *data, size_t size);

/**
 * @brief Detect ROM chip type from filename
 * @param filename Filename
 * @return ROM type
 */
neogeo_rom_type_t neogeo_detect_chip_type(const char *filename);

/**
 * @brief Get system name
 * @param system System type
 * @return System name
 */
const char *neogeo_system_name(neogeo_system_t system);

/**
 * @brief Get region name
 * @param region Region
 * @return Region name
 */
const char *neogeo_region_name(neogeo_region_t region);

/**
 * @brief Get ROM type name
 * @param type ROM type
 * @return Type name
 */
const char *neogeo_rom_type_name(neogeo_rom_type_t type);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open Neo Geo ROM (.neo format)
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int neogeo_open(const uint8_t *data, size_t size, neogeo_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int neogeo_load(const char *filename, neogeo_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void neogeo_close(neogeo_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int neogeo_get_info(const neogeo_rom_t *rom, neogeo_info_t *info);

/* ============================================================================
 * API Functions - ROM Access
 * ============================================================================ */

/**
 * @brief Get P-ROM data
 * @param rom ROM context
 * @param size Output size
 * @return Pointer to P-ROM
 */
const uint8_t *neogeo_get_prom(const neogeo_rom_t *rom, size_t *size);

/**
 * @brief Get S-ROM data
 * @param rom ROM context
 * @param size Output size
 * @return Pointer to S-ROM
 */
const uint8_t *neogeo_get_srom(const neogeo_rom_t *rom, size_t *size);

/**
 * @brief Get M-ROM data
 * @param rom ROM context
 * @param size Output size
 * @return Pointer to M-ROM
 */
const uint8_t *neogeo_get_mrom(const neogeo_rom_t *rom, size_t *size);

/**
 * @brief Get V-ROM data
 * @param rom ROM context
 * @param size Output size
 * @return Pointer to V-ROM
 */
const uint8_t *neogeo_get_vrom(const neogeo_rom_t *rom, size_t *size);

/**
 * @brief Get C-ROM data
 * @param rom ROM context
 * @param size Output size
 * @return Pointer to C-ROM
 */
const uint8_t *neogeo_get_crom(const neogeo_rom_t *rom, size_t *size);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void neogeo_print_info(const neogeo_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_NEOGEO_H */
