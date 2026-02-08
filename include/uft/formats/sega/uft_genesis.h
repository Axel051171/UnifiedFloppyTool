/**
 * @file uft_genesis.h
 * @brief Sega Genesis/Mega Drive ROM Image Support
 * 
 * Support for Sega Genesis/Mega Drive cartridge formats:
 * - BIN: Raw ROM dump
 * - MD: Mega Drive ROM (same as BIN)
 * - SMD: Super Magic Drive interleaved format
 * - GEN: Genesis ROM (same as BIN)
 * - 32X: 32X addon ROMs
 * - SCD: Sega CD disc images
 * 
 * ROM Header Information:
 * - System type (Genesis, Mega Drive, 32X, Sega CD)
 * - Copyright/Publisher info
 * - Game title (domestic/overseas)
 * - Serial number
 * - ROM checksum
 * - I/O support (controller types)
 * - Region codes (Japan, USA, Europe)
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_GENESIS_H
#define UFT_GENESIS_H

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

/** Header offset and size */
#define GENESIS_HEADER_OFFSET   0x100
#define GENESIS_HEADER_SIZE     256

/** ROM size limits */
#define GENESIS_MIN_ROM_SIZE    0x200       /**< Minimum valid ROM */
#define GENESIS_MAX_ROM_SIZE    0x400000    /**< 4MB max standard */
#define GENESIS_32X_MAX_SIZE    0x400000    /**< 4MB 32X */

/** SMD block size */
#define SMD_BLOCK_SIZE          16384
#define SMD_HEADER_SIZE         512

/** System strings at $100 */
#define GENESIS_SYSTEM_MD       "SEGA MEGA DRIVE "
#define GENESIS_SYSTEM_GEN      "SEGA GENESIS    "
#define GENESIS_SYSTEM_32X      "SEGA 32X        "
#define GENESIS_SYSTEM_PICO     "SEGA PICO       "
#define GENESIS_SYSTEM_CD       "SEGA MEGA-CD    "

/** Region codes */
typedef enum {
    GENESIS_REGION_JAPAN    = 0x01,     /**< J - Japan */
    GENESIS_REGION_USA      = 0x02,     /**< U - Americas */
    GENESIS_REGION_EUROPE   = 0x04,     /**< E - Europe */
    GENESIS_REGION_WORLD    = 0x07      /**< JUE - All regions */
} genesis_region_t;

/** ROM format types */
typedef enum {
    GENESIS_FORMAT_BIN      = 0,        /**< Raw binary */
    GENESIS_FORMAT_SMD      = 1,        /**< Super Magic Drive */
    GENESIS_FORMAT_MD       = 2,        /**< Mega Drive (same as BIN) */
    GENESIS_FORMAT_32X      = 3,        /**< 32X ROM */
    GENESIS_FORMAT_UNKNOWN  = 255
} genesis_format_t;

/** System types */
typedef enum {
    GENESIS_TYPE_MD         = 0,        /**< Mega Drive */
    GENESIS_TYPE_GENESIS    = 1,        /**< Genesis (US) */
    GENESIS_TYPE_32X        = 2,        /**< 32X addon */
    GENESIS_TYPE_SCD        = 3,        /**< Sega CD */
    GENESIS_TYPE_PICO       = 4,        /**< Sega Pico */
    GENESIS_TYPE_UNKNOWN    = 255
} genesis_system_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief ROM header (at offset $100)
 */
typedef struct {
    char        system[16];         /**< System type string */
    char        copyright[16];      /**< Copyright/date */
    char        title_domestic[48]; /**< Japanese title */
    char        title_overseas[48]; /**< International title */
    char        serial[14];         /**< Serial number (GM XXXXXXXX-XX) */
    uint16_t    checksum;           /**< ROM checksum */
    char        io_support[16];     /**< I/O device support */
    uint32_t    rom_start;          /**< ROM start address */
    uint32_t    rom_end;            /**< ROM end address */
    uint32_t    ram_start;          /**< RAM start address */
    uint32_t    ram_end;            /**< RAM end address */
    char        sram_info[12];      /**< SRAM info */
    char        modem_info[12];     /**< Modem info */
    char        memo[40];           /**< Memo/notes */
    char        region[16];         /**< Region codes */
} genesis_header_t;

/**
 * @brief SMD file header
 */
typedef struct {
    uint8_t     blocks;             /**< Number of 16KB blocks */
    uint8_t     flags[2];           /**< Format flags */
    uint8_t     reserved[8];        /**< Reserved */
    uint8_t     marker[3];          /**< 0xAA 0xBB + type */
} smd_header_t;

/**
 * @brief ROM info
 */
typedef struct {
    genesis_format_t format;        /**< File format */
    genesis_system_t system;        /**< System type */
    char        title[49];          /**< Game title */
    char        serial[15];         /**< Serial number */
    char        copyright[17];      /**< Copyright info */
    uint16_t    checksum;           /**< Header checksum */
    uint16_t    calculated_checksum;/**< Calculated checksum */
    bool        checksum_valid;     /**< Checksum matches */
    uint32_t    rom_size;           /**< ROM size */
    genesis_region_t regions;       /**< Supported regions */
    bool        has_sram;           /**< Has save RAM */
    uint32_t    sram_start;         /**< SRAM start address */
    uint32_t    sram_end;           /**< SRAM end address */
} genesis_info_t;

/**
 * @brief ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    genesis_format_t format;        /**< Detected format */
    genesis_system_t system;        /**< System type */
    genesis_header_t header;        /**< Parsed header */
    bool        owns_data;          /**< true if we allocated */
} genesis_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect ROM format
 * @param data ROM data
 * @param size Data size
 * @return ROM format
 */
genesis_format_t genesis_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Detect system type from header
 * @param data ROM data
 * @param size Data size
 * @return System type
 */
genesis_system_t genesis_detect_system(const uint8_t *data, size_t size);

/**
 * @brief Check if data is SMD format
 * @param data File data
 * @param size Data size
 * @return true if SMD
 */
bool genesis_is_smd(const uint8_t *data, size_t size);

/**
 * @brief Get format name
 * @param format ROM format
 * @return Format name string
 */
const char *genesis_format_name(genesis_format_t format);

/**
 * @brief Get system name
 * @param system System type
 * @return System name string
 */
const char *genesis_system_name(genesis_system_t system);

/**
 * @brief Validate ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid Genesis ROM
 */
bool genesis_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open Genesis ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int genesis_open(const uint8_t *data, size_t size, genesis_rom_t *rom);

/**
 * @brief Load Genesis ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int genesis_load(const char *filename, genesis_rom_t *rom);

/**
 * @brief Save Genesis ROM
 * @param rom ROM context
 * @param filename Output path
 * @param format Output format
 * @return 0 on success
 */
int genesis_save(const genesis_rom_t *rom, const char *filename,
                 genesis_format_t format);

/**
 * @brief Close Genesis ROM
 * @param rom ROM to close
 */
void genesis_close(genesis_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int genesis_get_info(const genesis_rom_t *rom, genesis_info_t *info);

/* ============================================================================
 * API Functions - Format Conversion
 * ============================================================================ */

/**
 * @brief Convert SMD to BIN format
 * @param smd_data SMD format data
 * @param smd_size SMD data size
 * @param bin_data Output buffer
 * @param max_size Maximum output size
 * @param bin_size Output: actual size
 * @return 0 on success
 */
int genesis_smd_to_bin(const uint8_t *smd_data, size_t smd_size,
                       uint8_t *bin_data, size_t max_size, size_t *bin_size);

/**
 * @brief Convert BIN to SMD format
 * @param bin_data BIN format data
 * @param bin_size BIN data size
 * @param smd_data Output buffer
 * @param max_size Maximum output size
 * @param smd_size Output: actual size
 * @return 0 on success
 */
int genesis_bin_to_smd(const uint8_t *bin_data, size_t bin_size,
                       uint8_t *smd_data, size_t max_size, size_t *smd_size);

/* ============================================================================
 * API Functions - Checksum
 * ============================================================================ */

/**
 * @brief Calculate ROM checksum
 * @param data ROM data
 * @param size ROM size
 * @return Calculated checksum
 */
uint16_t genesis_calculate_checksum(const uint8_t *data, size_t size);

/**
 * @brief Verify ROM checksum
 * @param rom ROM context
 * @return true if checksum valid
 */
bool genesis_verify_checksum(const genesis_rom_t *rom);

/**
 * @brief Fix ROM checksum
 * @param rom ROM context
 * @return 0 on success
 */
int genesis_fix_checksum(genesis_rom_t *rom);

/* ============================================================================
 * API Functions - Region
 * ============================================================================ */

/**
 * @brief Parse region string
 * @param region_str Region string from header
 * @return Region flags
 */
genesis_region_t genesis_parse_regions(const char *region_str);

/**
 * @brief Get region name
 * @param region Region flags
 * @param buffer Output buffer
 * @param size Buffer size
 */
void genesis_region_string(genesis_region_t region, char *buffer, size_t size);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void genesis_print_info(const genesis_rom_t *rom, FILE *fp);

/**
 * @brief Get game title
 * @param rom ROM context
 * @param overseas true for international title
 * @return Title string
 */
const char *genesis_get_title(const genesis_rom_t *rom, bool overseas);

#ifdef __cplusplus
}
#endif

#endif /* UFT_GENESIS_H */
