/**
 * @file uft_snes.h
 * @brief Super Nintendo / Super Famicom ROM Support
 * 
 * Support for SNES ROM formats:
 * - SFC (.sfc) - Headerless ROM
 * - SMC (.smc) - 512-byte copier header
 * - SWC (.swc) - Super Wild Card format
 * - FIG (.fig) - Pro Fighter format
 * 
 * Features:
 * - Header detection and removal
 * - Internal header parsing
 * - Checksum verification
 * - HiROM/LoROM detection
 * - Special chip detection (SA-1, SuperFX, DSP, etc.)
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_SNES_H
#define UFT_SNES_H

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

/** Copier header size */
#define SNES_COPIER_HEADER      512

/** Internal header locations */
#define SNES_LOROM_HEADER       0x7FC0
#define SNES_HIROM_HEADER       0xFFC0
#define SNES_EXHIROM_HEADER     0x40FFC0
#define SNES_EXLOROM_HEADER     0x407FC0

/** Internal header size */
#define SNES_INTERNAL_HEADER    32

/** ROM mapping modes */
typedef enum {
    SNES_MAP_UNKNOWN    = 0,
    SNES_MAP_LOROM      = 0x20,     /**< Mode 20: LoROM */
    SNES_MAP_HIROM      = 0x21,     /**< Mode 21: HiROM */
    SNES_MAP_LOROM_SA1  = 0x23,     /**< Mode 23: SA-1 */
    SNES_MAP_LOROM_SDD1 = 0x32,     /**< Mode 32: SDD-1 */
    SNES_MAP_LOROM_FAST = 0x30,     /**< Mode 30: LoROM + FastROM */
    SNES_MAP_HIROM_FAST = 0x31,     /**< Mode 31: HiROM + FastROM */
    SNES_MAP_EXHIROM    = 0x35,     /**< Mode 35: ExHiROM */
    SNES_MAP_EXLOROM    = 0x25      /**< Mode 25: ExLoROM */
} snes_mapping_t;

/** ROM types / special chips */
typedef enum {
    SNES_CHIP_NONE      = 0x00,
    SNES_CHIP_RAM       = 0x01,
    SNES_CHIP_SRAM      = 0x02,
    SNES_CHIP_DSP       = 0x03,
    SNES_CHIP_DSP_RAM   = 0x04,
    SNES_CHIP_DSP_SRAM  = 0x05,
    SNES_CHIP_FX        = 0x13,     /**< SuperFX */
    SNES_CHIP_FX_RAM    = 0x14,
    SNES_CHIP_FX_SRAM   = 0x15,
    SNES_CHIP_FX2       = 0x1A,     /**< SuperFX2 */
    SNES_CHIP_OBC1      = 0x25,
    SNES_CHIP_SA1       = 0x34,
    SNES_CHIP_SA1_SRAM  = 0x35,
    SNES_CHIP_SDD1      = 0x43,
    SNES_CHIP_SDD1_SRAM = 0x45,
    SNES_CHIP_SPC7110   = 0xF5,
    SNES_CHIP_ST018     = 0xF6,
    SNES_CHIP_CX4       = 0xF9
} snes_chip_t;

/** Region codes */
typedef enum {
    SNES_REGION_JAPAN       = 0x00,
    SNES_REGION_USA         = 0x01,
    SNES_REGION_EUROPE      = 0x02,
    SNES_REGION_SWEDEN      = 0x03,
    SNES_REGION_FINLAND     = 0x04,
    SNES_REGION_DENMARK     = 0x05,
    SNES_REGION_FRANCE      = 0x06,
    SNES_REGION_NETHERLANDS = 0x07,
    SNES_REGION_SPAIN       = 0x08,
    SNES_REGION_GERMANY     = 0x09,
    SNES_REGION_ITALY       = 0x0A,
    SNES_REGION_CHINA       = 0x0B,
    SNES_REGION_KOREA       = 0x0D,
    SNES_REGION_INTERNATIONAL = 0x0E,
    SNES_REGION_CANADA      = 0x0F,
    SNES_REGION_BRAZIL      = 0x10,
    SNES_REGION_AUSTRALIA   = 0x11
} snes_region_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief SNES internal header (32 bytes at $xxFFC0)
 */
typedef struct {
    char        title[21];          /**< Game title (space-padded) */
    uint8_t     map_mode;           /**< Map mode */
    uint8_t     rom_type;           /**< ROM type / chip */
    uint8_t     rom_size;           /**< ROM size (2^N KB) */
    uint8_t     sram_size;          /**< SRAM size (2^N KB, 0=none) */
    uint8_t     region;             /**< Region code */
    uint8_t     developer_id;       /**< Developer ID */
    uint8_t     version;            /**< ROM version */
    uint16_t    checksum_comp;      /**< Checksum complement */
    uint16_t    checksum;           /**< Checksum */
} snes_header_t;

/**
 * @brief SNES ROM info
 */
typedef struct {
    char        title[22];          /**< Title (null-terminated) */
    size_t      file_size;          /**< File size */
    size_t      rom_size;           /**< ROM size (without header) */
    bool        has_copier_header;  /**< Has 512-byte copier header */
    snes_mapping_t mapping;         /**< ROM mapping mode */
    const char  *mapping_name;      /**< Mapping name */
    snes_chip_t chip;               /**< Special chip */
    const char  *chip_name;         /**< Chip name */
    uint32_t    sram_size;          /**< SRAM size in bytes */
    snes_region_t region;           /**< Region code */
    const char  *region_name;       /**< Region name */
    uint8_t     version;            /**< ROM version */
    uint16_t    checksum;           /**< Header checksum */
    uint16_t    calculated;         /**< Calculated checksum */
    bool        checksum_valid;     /**< Checksum matches */
    bool        is_hirom;           /**< HiROM mapping */
    bool        is_fastrom;         /**< FastROM (3.58 MHz) */
} snes_info_t;

/**
 * @brief SNES ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data */
    size_t      size;               /**< Data size */
    bool        has_copier_header;  /**< Has 512-byte header */
    size_t      header_offset;      /**< Internal header offset */
    snes_header_t header;           /**< Parsed internal header */
    bool        owns_data;          /**< true if we allocated */
} snes_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect if ROM has copier header
 * @param data ROM data
 * @param size Data size
 * @return true if has 512-byte header
 */
bool snes_has_copier_header(const uint8_t *data, size_t size);

/**
 * @brief Find internal header offset
 * @param data ROM data
 * @param size Data size
 * @param has_copier true if copier header present
 * @return Header offset or 0 if not found
 */
size_t snes_find_header(const uint8_t *data, size_t size, bool has_copier);

/**
 * @brief Validate SNES ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid
 */
bool snes_validate(const uint8_t *data, size_t size);

/**
 * @brief Get mapping mode name
 * @param mapping Mapping mode
 * @return Mapping name
 */
const char *snes_mapping_name(snes_mapping_t mapping);

/**
 * @brief Get chip name
 * @param chip Chip type
 * @return Chip name
 */
const char *snes_chip_name(snes_chip_t chip);

/**
 * @brief Get region name
 * @param region Region code
 * @return Region name
 */
const char *snes_region_name(snes_region_t region);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open SNES ROM
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int snes_open(const uint8_t *data, size_t size, snes_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int snes_load(const char *filename, snes_rom_t *rom);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void snes_close(snes_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int snes_get_info(const snes_rom_t *rom, snes_info_t *info);

/* ============================================================================
 * API Functions - Checksum
 * ============================================================================ */

/**
 * @brief Calculate ROM checksum
 * @param rom ROM context
 * @return Checksum value
 */
uint16_t snes_calculate_checksum(const snes_rom_t *rom);

/**
 * @brief Verify ROM checksum
 * @param rom ROM context
 * @return true if valid
 */
bool snes_verify_checksum(const snes_rom_t *rom);

/**
 * @brief Fix ROM checksum
 * @param rom ROM context
 * @return 0 on success
 */
int snes_fix_checksum(snes_rom_t *rom);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Get ROM data without copier header
 * @param rom ROM context
 * @return Pointer to ROM data
 */
const uint8_t *snes_get_rom_data(const snes_rom_t *rom);

/**
 * @brief Get ROM size without copier header
 * @param rom ROM context
 * @return ROM size
 */
size_t snes_get_rom_size(const snes_rom_t *rom);

/**
 * @brief Strip copier header (in-place)
 * @param rom ROM context
 * @return 0 on success
 */
int snes_strip_header(snes_rom_t *rom);

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void snes_print_info(const snes_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SNES_H */
