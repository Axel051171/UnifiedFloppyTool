/**
 * @file uft_n64.h
 * @brief Nintendo 64 ROM Support
 * 
 * Support for Nintendo 64 ROM formats:
 * - .z64 - Big-endian (native N64 format)
 * - .v64 - Byte-swapped (Doctor V64)
 * - .n64 - Little-endian (word-swapped)
 * 
 * Features:
 * - ROM format detection and conversion
 * - Header parsing (title, game code, region, CIC)
 * - CRC verification and calculation
 * - Save type detection (EEPROM, SRAM, Flash)
 * - CIC chip detection
 * 
 * @author UFT Project
 * @date 2026-01-17
 */

#ifndef UFT_N64_H
#define UFT_N64_H

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

/** ROM header size */
#define N64_HEADER_SIZE         0x40

/** Format magic bytes (first 4 bytes) */
#define N64_MAGIC_Z64           0x80371240  /**< Big-endian .z64 */
#define N64_MAGIC_V64           0x37804012  /**< Byte-swapped .v64 */
#define N64_MAGIC_N64           0x40123780  /**< Little-endian .n64 */

/** Boot code / CIC types */
typedef enum {
    N64_CIC_UNKNOWN     = 0,
    N64_CIC_6101        = 6101,     /**< Starfox 64 */
    N64_CIC_6102        = 6102,     /**< Most common */
    N64_CIC_6103        = 6103,     /**< Banjo-Kazooie, Paper Mario */
    N64_CIC_6105        = 6105,     /**< Zelda OOT/MM */
    N64_CIC_6106        = 6106,     /**< F-Zero X */
    N64_CIC_7101        = 7101,     /**< NTSC variant */
    N64_CIC_7102        = 7102,     /**< PAL variant */
    N64_CIC_7103        = 7103,
    N64_CIC_7105        = 7105,
    N64_CIC_7106        = 7106
} n64_cic_t;

/** ROM byte order formats */
typedef enum {
    N64_FORMAT_UNKNOWN  = 0,
    N64_FORMAT_Z64      = 1,        /**< Big-endian (native) */
    N64_FORMAT_V64      = 2,        /**< Byte-swapped */
    N64_FORMAT_N64      = 3         /**< Little-endian */
} n64_format_t;

/** Region codes */
typedef enum {
    N64_REGION_UNKNOWN  = 0,
    N64_REGION_NTSC     = 'N',      /**< NTSC (USA) */
    N64_REGION_PAL      = 'P',      /**< PAL (Europe) */
    N64_REGION_JAPAN    = 'J',      /**< Japan */
    N64_REGION_GATEWAY  = 'G',      /**< Gateway 64 (NTSC) */
    N64_REGION_PAL_X    = 'X',      /**< PAL (other) */
    N64_REGION_PAL_Y    = 'Y',      /**< PAL (other) */
    N64_REGION_PAL_D    = 'D',      /**< Germany */
    N64_REGION_PAL_F    = 'F',      /**< France */
    N64_REGION_PAL_I    = 'I',      /**< Italy */
    N64_REGION_PAL_S    = 'S'       /**< Spain */
} n64_region_t;

/** Save types */
typedef enum {
    N64_SAVE_NONE       = 0,
    N64_SAVE_EEPROM_4K  = 1,        /**< 4Kbit EEPROM (512 bytes) */
    N64_SAVE_EEPROM_16K = 2,        /**< 16Kbit EEPROM (2KB) */
    N64_SAVE_SRAM_256K  = 3,        /**< 256Kbit SRAM (32KB) */
    N64_SAVE_FLASH_1M   = 4,        /**< 1Mbit Flash (128KB) */
    N64_SAVE_CONTROLLER = 5         /**< Controller Pak */
} n64_save_type_t;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/**
 * @brief N64 ROM header (64 bytes, big-endian in .z64)
 */
typedef struct {
    uint32_t    pi_bsd_dom1;        /**< PI BSD Domain 1 register */
    uint32_t    clock_rate;         /**< Clock rate override */
    uint32_t    boot_address;       /**< Boot code entry point */
    uint32_t    release;            /**< libultra version */
    uint32_t    crc1;               /**< CRC1 checksum */
    uint32_t    crc2;               /**< CRC2 checksum */
    uint64_t    reserved1;          /**< Reserved */
    char        title[20];          /**< Internal name (null-padded) */
    uint32_t    reserved2;          /**< Reserved */
    uint32_t    media_format;       /**< Media format */
    char        game_id[2];         /**< Game ID (e.g., "SM" = Super Mario) */
    char        region;             /**< Region code */
    uint8_t     version;            /**< ROM version */
} n64_header_t;

/**
 * @brief N64 ROM info
 */
typedef struct {
    n64_format_t format;            /**< ROM format */
    const char  *format_name;       /**< Format name */
    size_t      rom_size;           /**< ROM size */
    char        title[21];          /**< Title (null-terminated) */
    char        game_id[3];         /**< Game ID */
    char        full_code[5];       /**< Full game code (e.g., NSME) */
    n64_region_t region;            /**< Region */
    const char  *region_name;       /**< Region name */
    uint8_t     version;            /**< ROM version */
    uint32_t    crc1;               /**< CRC1 */
    uint32_t    crc2;               /**< CRC2 */
    uint32_t    calc_crc1;          /**< Calculated CRC1 */
    uint32_t    calc_crc2;          /**< Calculated CRC2 */
    bool        crc_valid;          /**< CRCs match */
    n64_cic_t   cic;                /**< CIC chip type */
    const char  *cic_name;          /**< CIC name */
    n64_save_type_t save_type;      /**< Save type */
    const char  *save_name;         /**< Save type name */
} n64_info_t;

/**
 * @brief N64 ROM context
 */
typedef struct {
    uint8_t     *data;              /**< ROM data (always in z64 format) */
    size_t      size;               /**< Data size */
    n64_format_t original_format;   /**< Original format before conversion */
    n64_header_t header;            /**< Parsed header */
    bool        owns_data;          /**< true if we allocated */
    bool        header_valid;       /**< Header parsed successfully */
} n64_rom_t;

/* ============================================================================
 * API Functions - Detection
 * ============================================================================ */

/**
 * @brief Detect ROM format from data
 * @param data ROM data
 * @param size Data size
 * @return ROM format
 */
n64_format_t n64_detect_format(const uint8_t *data, size_t size);

/**
 * @brief Get format name
 * @param format ROM format
 * @return Format name string
 */
const char *n64_format_name(n64_format_t format);

/**
 * @brief Get region name
 * @param region Region code
 * @return Region name string
 */
const char *n64_region_name(n64_region_t region);

/**
 * @brief Get CIC name
 * @param cic CIC type
 * @return CIC name string
 */
const char *n64_cic_name(n64_cic_t cic);

/**
 * @brief Get save type name
 * @param type Save type
 * @return Save type name string
 */
const char *n64_save_name(n64_save_type_t type);

/**
 * @brief Validate N64 ROM
 * @param data ROM data
 * @param size Data size
 * @return true if valid N64 ROM
 */
bool n64_validate(const uint8_t *data, size_t size);

/* ============================================================================
 * API Functions - ROM Operations
 * ============================================================================ */

/**
 * @brief Open N64 ROM (auto-converts to z64 format)
 * @param data ROM data
 * @param size Data size
 * @param rom Output ROM context
 * @return 0 on success
 */
int n64_open(const uint8_t *data, size_t size, n64_rom_t *rom);

/**
 * @brief Load ROM from file
 * @param filename File path
 * @param rom Output ROM context
 * @return 0 on success
 */
int n64_load(const char *filename, n64_rom_t *rom);

/**
 * @brief Save ROM to file (in z64 format)
 * @param rom ROM context
 * @param filename Output path
 * @return 0 on success
 */
int n64_save(const n64_rom_t *rom, const char *filename);

/**
 * @brief Save ROM in specific format
 * @param rom ROM context
 * @param filename Output path
 * @param format Desired format
 * @return 0 on success
 */
int n64_save_as(const n64_rom_t *rom, const char *filename, n64_format_t format);

/**
 * @brief Close ROM
 * @param rom ROM to close
 */
void n64_close(n64_rom_t *rom);

/**
 * @brief Get ROM info
 * @param rom ROM context
 * @param info Output info
 * @return 0 on success
 */
int n64_get_info(const n64_rom_t *rom, n64_info_t *info);

/* ============================================================================
 * API Functions - Format Conversion
 * ============================================================================ */

/**
 * @brief Convert to z64 (big-endian) format
 * @param data ROM data
 * @param size Data size
 * @param format Source format
 */
void n64_to_z64(uint8_t *data, size_t size, n64_format_t format);

/**
 * @brief Convert from z64 to v64 (byte-swapped) format
 * @param data ROM data (z64)
 * @param size Data size
 * @param output Output buffer (v64)
 */
void n64_z64_to_v64(const uint8_t *data, size_t size, uint8_t *output);

/**
 * @brief Convert from z64 to n64 (little-endian) format
 * @param data ROM data (z64)
 * @param size Data size
 * @param output Output buffer (n64)
 */
void n64_z64_to_n64(const uint8_t *data, size_t size, uint8_t *output);

/* ============================================================================
 * API Functions - CRC / Checksum
 * ============================================================================ */

/**
 * @brief Calculate ROM CRCs
 * @param rom ROM context
 * @param crc1 Output CRC1
 * @param crc2 Output CRC2
 * @return 0 on success
 */
int n64_calculate_crc(const n64_rom_t *rom, uint32_t *crc1, uint32_t *crc2);

/**
 * @brief Verify ROM CRCs
 * @param rom ROM context
 * @return true if CRCs match
 */
bool n64_verify_crc(const n64_rom_t *rom);

/**
 * @brief Fix ROM CRCs
 * @param rom ROM context
 * @return 0 on success
 */
int n64_fix_crc(n64_rom_t *rom);

/**
 * @brief Detect CIC chip from boot code
 * @param rom ROM context
 * @return CIC type
 */
n64_cic_t n64_detect_cic(const n64_rom_t *rom);

/* ============================================================================
 * API Functions - Utilities
 * ============================================================================ */

/**
 * @brief Print ROM info
 * @param rom ROM context
 * @param fp Output file
 */
void n64_print_info(const n64_rom_t *rom, FILE *fp);

#ifdef __cplusplus
}
#endif

#endif /* UFT_N64_H */
